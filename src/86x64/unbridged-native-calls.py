#!/usr/bin/env python3
"""List the NATIVE functions a translated image calls with NO abigen bridge.

THE DEFECT THIS FINDS (Portal 2 2026-09-14)
-------------------------------------------
A translated i386 call site does not use the x86_64 call instruction.  The
translator emits:

    leaq  <ret>(%rip), %r11      ; the return address
    pushw %ax ; pushw %ax        ; reserve FOUR bytes
    movl  %r11d, (%rsp)          ; store a 4-BYTE return address
    jmp   <callee>

so the callee must return the 4-byte way.  Every abigen bridge does (its
epilogue is `mov r11d,[rsp]; add rsp,4; jmp r11`), and translated-to-translated
calls do too.  A NATIVE function does not: its `ret` pops EIGHT bytes.  Control
then returns to

    (adjacent stack word << 32) | (the real 4-byte return address)

Portal 2 died at rip=0xb03_051044eb -- low half a valid libsteam_api address,
high half leftover stack, err=0x14 (instruction fetch).  The arguments are wrong
too (i386 pushes them on the stack; SysV expects registers), but the return
width is what makes it fatal and unmistakable.

★ HOW THE BIND GETS THERE.  static-interpose rewrites a translated image's binds
to libabiconv for every symbol libabiconv actually provides, and LEAVES THE REST
POINTING AT THE ORIGINAL FRAMEWORK.  So a MISSING BRIDGE is not a missing
feature -- it silently degrades into a raw cross-ABI call.  And a bridge can be
missing for a boring reason: `_bootstrap_look_up` was already in abigen's
consider set, but no parsed header declared it, so abigen had no prototype and
emitted nothing.  One `#include <servers/bootstrap.h>` fixed it.

WHY A TREE, NOT A FILE.  A bind onto a sibling TRANSLATED module (libtier0,
libvstdlib, ...) is perfectly fine -- both sides use the 4-byte convention.  Only
binds onto genuinely NATIVE dylibs are findings, so the tool must know which
dylibs in the tree are ours.  It decides the same way the runtime does: a
translated image's __TEXT is based at 0x10000000.

usage:
    unbridged-native-calls.py <tree-or-file>...      # functions (the ABI hazard)
    unbridged-native-calls.py --data <tree>...       # data imports as well
    unbridged-native-calls.py --list <tree>...       # every symbol, not a summary
exit: 0 = nothing unbridged, 1 = findings, 2 = usage/tool error

⚠ A HIT IS NOT PROOF OF A CRASH, and this is the tool's main limitation: it
cannot tell whether the app ever CALLS the symbol.  Portal 2 imports 14
ForceFeedback entry points it may never reach on a machine with no joystick.
Rank by how likely the path is, and treat libSystem/libstdc++ hits as urgent.

⚠ A NORETURN callee is a partial false positive: its `ret` never executes, so the
return-width half of the hazard cannot fire.  The ARGUMENTS are still wrong --
i386 pushed them on the stack, the native callee reads registers -- so
`_exit(status)` still exits with a garbage status.  The two hits this tool reports
against tests-i386/build are exactly that class (___stack_chk_fail, _exit): worth
bridging for the argument, never a mystery crash.
"""
import os
import re
import subprocess
import sys

TRANSLATED_TEXT_BASE = 0x10000000
MACHO_MAGIC = (b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe",
               b"\xfe\xed\xfa\xcf", b"\xfe\xed\xfa\xce",
               b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca")


def is_macho(path):
    try:
        with open(path, "rb") as fh:
            return fh.read(4) in MACHO_MAGIC
    except (IOError, OSError):
        return False


def run(cmd):
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
        return p.stdout
    except (OSError, subprocess.SubprocessError):
        return ""


def text_vmaddr(path):
    """Preferred __TEXT vmaddr, or None. Identifies OUR translated output."""
    out = run(["otool", "-l", path])
    seg = None
    for line in out.splitlines():
        line = line.strip()
        if line.startswith("segname "):
            seg = line.split()[1]
        elif line.startswith("vmaddr ") and seg == "__TEXT":
            try:
                return int(line.split()[1], 16)
            except ValueError:
                return None
    return None


def collect(paths):
    """Return (files, translated_basenames)."""
    files = []
    for a in paths:
        if os.path.isdir(a):
            for root, _, names in os.walk(a):
                for n in sorted(names):
                    p = os.path.join(root, n)
                    if is_macho(p):
                        files.append(p)
        elif is_macho(a):
            files.append(a)
    translated = set()
    for p in files:
        if text_vmaddr(p) == TRANSLATED_TEXT_BASE:
            translated.add(os.path.basename(p))
    return files, translated


# dyld_info -fixups rows look like:
#   __DATA  __la_symbol_ptr  0x1001020   lazy-bind  libSystem/_bootstrap_look_up
ROW = re.compile(r"^\s+(\S+)\s+(\S+)\s+0x([0-9A-Fa-f]+)\s+"
                 r"(?:lazy-bind|bind|weak-bind)\s+(\S+?)/(\S+)\s*$")


def scan(path, translated, want_data):
    """Yield (dylib, symbol, section) for binds onto NATIVE dylibs."""
    for line in run(["dyld_info", "-fixups", path]).splitlines():
        m = ROW.match(line)
        if not m:
            continue
        _seg, sect, _addr, dylib, sym = m.groups()
        is_data = "symbol_ptr" not in sect or sect == "__nl_symbol_ptr"
        # __la_symbol_ptr = a CALL; __nl_symbol_ptr / __got = usually DATA.
        is_call = sect == "__la_symbol_ptr"
        if not is_call and not want_data:
            continue
        if dylib == "libabiconv":
            continue                      # bridged: correct by construction
        if dylib.startswith("<"):
            continue                      # weak-def-coalesce &c, not a dylib
        base = dylib if dylib.endswith(".dylib") else dylib + ".dylib"
        if base in translated or dylib in translated:
            continue                      # sibling translated module: same ABI
        yield dylib, sym, sect


def main(argv):
    want_data = False
    listing = False
    args = []
    for a in argv:
        if a == "--data":
            want_data = True
        elif a == "--list":
            listing = True
        elif a.startswith("-"):
            sys.stderr.write(__doc__)
            return 2
        else:
            args.append(a)
    if not args:
        sys.stderr.write(__doc__)
        return 2

    files, translated = collect(args)
    if not files:
        print("no Mach-O files found")
        return 2
    print("scanned %d Mach-O file(s); %d of them are TRANSLATED (__TEXT @ %#x)"
          % (len(files), len(translated), TRANSLATED_TEXT_BASE))

    per_dylib = {}
    hits = set()
    for p in files:
        if os.path.basename(p) not in translated:
            continue                      # only translated images can misbehave
        for dylib, sym, sect in scan(p, translated, want_data):
            hits.add((dylib, sym, sect))
            per_dylib.setdefault(dylib, set()).add(sym)

    if listing:
        for dylib, sym, sect in sorted(hits):
            print("  %-22s %-44s %s" % (dylib, sym, sect))
    else:
        for dylib in sorted(per_dylib, key=lambda d: -len(per_dylib[d])):
            syms = sorted(per_dylib[dylib])
            print("  %-22s %3d  %s%s" % (dylib, len(syms), ", ".join(syms[:4]),
                                         " ..." if len(syms) > 4 else ""))
    print()
    print("%d distinct UNBRIDGED native symbol(s) reachable from translated code"
          % len(set((d, s) for d, s, _ in hits)))
    if hits:
        print("⚠ each is a raw cross-ABI call: i386 stack args, and the callee's")
        print("  `ret` pops 8 bytes where the translator pushed 4. Give abigen a")
        print("  PROTOTYPE (add the header to src/abiconv/includes.h) so it emits")
        print("  a bridge, then `m64 resync` + retranslate.")
    return 1 if hits else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
