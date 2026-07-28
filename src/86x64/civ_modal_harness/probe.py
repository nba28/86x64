import lldb

def ev(expr):
    f = lldb.debugger.GetSelectedTarget().GetProcess().GetSelectedThread().GetFrameAtIndex(0)
    v = f.EvaluateExpression(expr)
    err = v.GetError().GetCString()
    print("%-62s => %s%s" % (expr, v.GetValue() or v.GetSummary(),
                             ("   ERR:" + err.strip()) if err else ""))

def run(debugger, command, result, internal_dict):
    print("========== PARKED-PROCESS PROBE ==========")
    ev("(long)[NSApp modalWindow]")
    ev("(const char*)object_getClassName((id)[NSApp modalWindow])")
    ev("(int)[[NSApp modalWindow] isVisible]")
    ev("(long)[[NSApp modalWindow] windowNumber]")
    ev("(long)[[NSApp modalWindow] level]")
    ev("(int)[NSApp isRunning]")
    ev("(int)[NSApp isActive]")
    ev("(int)[NSApp modalWindow] == 0")
    ev("(unsigned long)[[NSApp windows] count]")
    ev("(long)[NSApp keyWindow]")
    ev("(long)[NSApp mainWindow]")
    ev("(const char*)[[[NSApp windows] description] UTF8String]")
    t = lldb.debugger.GetSelectedTarget().GetProcess().GetSelectedThread()
    print("---------- main thread frames ----------")
    for i in range(0, min(14, t.GetNumFrames())):
        fr = t.GetFrameAtIndex(i)
        print("  #%-2d 0x%012x  %-14s %s" % (i, fr.GetPC(),
              fr.GetModule().GetFileSpec().GetFilename(), fr.GetFunctionName()))
    print("========== END PROBE ==========")

def __lldb_init__(debugger, internal_dict):
    debugger.HandleCommand('command script add -f probe.run probe')
