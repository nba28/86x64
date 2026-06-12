import lldb

REVERSE_RET = 0x72d6560          # _86x64_reverse_ret in the low sibling copy
LOWHEAP_LO, LOWHEAP_HI = 0x80000000, 0xF0000000
EXE = "$HOME/Downloads/iLife11/Applications/iPhoto.app/Contents/MacOS/iPhoto"

def dump_window(target, process, hist, n=30):
    err = lldb.SBError()
    print("--- last %d (pc, rsp) steps ---" % min(n, len(hist)))
    for pc, sp in hist[-n:]:
        # try to disassemble one instruction at pc
        txt = ""
        insns = target.ReadInstructions(lldb.SBAddress(pc, target), 1)
        if insns and insns.GetSize() > 0:
            ins = insns.GetInstructionAtIndex(0)
            txt = "%s %s" % (ins.GetMnemonic(target), ins.GetOperands(target))
        print("pc=0x%012x rsp=0x%012x  %s" % (pc, sp, txt))

def main():
    dbg = lldb.debugger
    dbg.SetAsync(False)
    target = dbg.GetSelectedTarget()
    bp_run = target.BreakpointCreateByName("-[NSApplication run]")
    info = lldb.SBLaunchInfo([])
    err = lldb.SBError()
    process = target.Launch(info, err)
    if not err.Success():
        print("launch failed: %s" % err)
        return
    armed = False
    fc_hits = 0
    while True:
        st = process.GetState()
        if st == lldb.eStateExited:
            print("process exited %d" % process.GetExitStatus())
            return
        if st != lldb.eStateStopped:
            print("unexpected state %d" % st)
            return
        thread = None
        for t in process:
            if t.GetStopReason() in (lldb.eStopReasonBreakpoint, lldb.eStopReasonException):
                thread = t
                break
        if thread is None:
            process.Continue()
            continue
        if thread.GetStopReason() == lldb.eStopReasonException:
            f = thread.GetFrameAtIndex(0)
            print("CRASH before capture: thread q=%s pc=0x%x rsp=0x%x"
                  % (thread.GetQueueName(), f.GetPC(), f.GetSP()))
            process.Kill()
            return
        if not armed:
            # -[NSApplication run] reached: arm the address breakpoint
            bp_run.SetEnabled(False)
            bp_addr = target.BreakpointCreateByAddress(REVERSE_RET)
            print("armed reverse_ret bp at 0x%x (%d locs resolved)"
                  % (REVERSE_RET, bp_addr.GetNumLocations()))
            armed = True
            process.Continue()
            continue
        q = thread.GetQueueName() or "?"
        if not q.startswith("com.apple.FileCoordination"):
            process.Continue()
            continue
        fc_hits += 1
        print("FC-queue reverse_ret hit #%d (thread %d) — stepping..."
              % (fc_hits, thread.GetIndexID()))
        hist = []
        found = False
        for i in range(50000):
            thread.StepInstruction(False)
            if process.GetState() != lldb.eStateStopped:
                print("process left stopped state at step %d" % i)
                break
            if thread.GetStopReason() == lldb.eStopReasonException:
                f = thread.GetFrameAtIndex(0)
                print("CRASHED at step %d pc=0x%x rsp=0x%x" % (i, f.GetPC(), f.GetSP()))
                dump_window(target, process, hist)
                process.Kill()
                return
            f = thread.GetFrameAtIndex(0)
            pc, sp = f.GetPC(), f.GetSP()
            hist.append((pc, sp))
            if sp < LOWHEAP_LO:
                print("TRUNCATION at step %d: pc=0x%x rsp=0x%x" % (i, pc, sp))
                dump_window(target, process, hist)
                for r in ("rax","rbx","rcx","rdx","rsi","rdi","rbp","r10","r11","r12","r13","r14","r15"):
                    print("%s = 0x%x" % (r, f.FindRegister(r).GetValueAsUnsigned()))
                found = True
                break
        if found or fc_hits >= 3:
            process.Kill()
            return
        process.Continue()

main()
