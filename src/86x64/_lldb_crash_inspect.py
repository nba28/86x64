import lldb

def inspect(debugger, command, result, internal_dict):
    target = lldb.debugger.GetSelectedTarget()
    process = target.GetProcess()
    thread = process.GetSelectedThread()
    frame = thread.GetFrameAtIndex(0)
    err = lldb.SBError()

    regs = {}
    for reg in ("rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
                "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"):
        v = frame.FindRegister(reg).GetValueAsUnsigned()
        regs[reg] = v
        print("%-4s = 0x%016x" % (reg, v))

    rdi = regs["rdi"]
    if 0x80000000 <= rdi < 0x80800000:
        real = process.ReadUnsignedFromMemory(rdi, 8, err)
        if err.Success():
            print("ARENA HANDLE rdi=0x%x slot=%d real=0x%016x" % (rdi, (rdi - 0x80000000) // 8, real))
            debugger.HandleCommand("expr -l objc -O -- (Class)object_getClass((id)%dULL)" % real)
        err.Clear()

    # stack scan: symbolize native 8-byte slots, flag plausible translated
    # 4-byte return addresses (translated images live under 0x20000000)
    sp = regs["rsp"]
    print("== stack scan from rsp=0x%x ==" % sp)
    for i in range(192):
        a = sp + i * 8
        v = process.ReadUnsignedFromMemory(a, 8, err)
        if not err.Success():
            err.Clear()
            break
        line = None
        sa = target.ResolveLoadAddress(v)
        if sa.GetModule().IsValid() and sa.GetSymbol().IsValid():
            start = sa.GetSymbol().GetStartAddress().GetLoadAddress(target)
            line = "%s`%s + %d" % (sa.GetModule().GetFileSpec().GetFilename(),
                                   sa.GetSymbol().GetName(), v - start)
        for half, off in ((v & 0xffffffff, 0), (v >> 32, 4)):
            if 0x1000 <= half < 0x20000000 and not line:
                print("0x%x+%d: 0x%08x  (low/translated?)" % (a, off, half))
        if line:
            print("0x%x: 0x%016x  %s" % (a, v, line))
    print("== end scan ==")

def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('command script add -f _lldb_crash_inspect.inspect crashinspect')
