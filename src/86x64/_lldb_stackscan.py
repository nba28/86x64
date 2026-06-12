import lldb

target = lldb.debugger.GetSelectedTarget()
process = target.GetProcess()
thread = process.GetSelectedThread()
frame = thread.GetFrameAtIndex(0)
sp = frame.GetSP()
err = lldb.SBError()
print("== stack scan from sp=%#x ==" % sp)
for i in range(200):
    a = sp + i * 8
    v = process.ReadUnsignedFromMemory(a, 8, err)
    if not err.Success():
        err.Clear()
        continue
    sa = target.ResolveLoadAddress(v)
    mod = sa.GetModule()
    sym = sa.GetSymbol()
    if mod.IsValid() and sym.IsValid():
        start = sym.GetStartAddress().GetLoadAddress(target)
        print("%#x: %#018x  %s`%s + %d"
              % (a, v, mod.GetFileSpec().GetFilename(), sym.GetName(), v - start))
