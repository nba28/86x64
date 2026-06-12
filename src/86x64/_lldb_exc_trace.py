import lldb

def bp_hit(frame, bp_loc, internal_dict):
    thread = frame.GetThread()
    print("=== objc_exception_throw on thread %d ===" % thread.GetIndexID())
    for i in range(min(thread.GetNumFrames(), 18)):
        f = thread.GetFrameAtIndex(i)
        mod = f.GetModule().GetFileSpec().GetFilename() or "?"
        name = f.GetFunctionName() or "?"
        print("  #%-2d 0x%012x %s`%s" % (i, f.GetPC(), mod, name))
    # the exception object is in rdi
    rdi = frame.FindRegister("rdi").GetValueAsUnsigned()
    lldb.debugger.HandleCommand("expr -l objc -O -- (id)%dULL" % rdi)
    return False  # continue

def __lldb_init_module(debugger, internal_dict):
    target = debugger.GetSelectedTarget()
    bp = target.BreakpointCreateByName("objc_exception_throw")
    bp.SetScriptCallbackFunction("_lldb_exc_trace.bp_hit")
    bp.SetAutoContinue(True)
    print("exception-throw breakpoint armed")
