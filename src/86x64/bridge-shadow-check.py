#!/usr/bin/env python3
"""bridge-shadow-check.py — ONE job: find native code in libabiconv that binds
to an abigen i386 bridge instead of the real system function.

THE DEFECT CLASS (measured 2026-08-22, root-caused from a Halo SIGSEGV).

abigen names the i386->x86_64 bridge for a symbol `_X` by prepending two
underscores: `_X` -> `___X`.  That output namespace is NOT reserved, and libc
already exports plenty of `___X` names of its own (its internal syscall
wrappers and ctype helpers).  Whenever both exist, the bridge WINS for
libabiconv's own internal calls, because the static linker binds a call to a
symbol defined in the same linkage unit directly to that local definition.

A bridge reads its arguments off the *i386 caller's frame* (`[rbp+12]`, ...)
and returns i386-style.  Called from native x86_64 code it therefore reads
garbage as its arguments and corrupts the stack on the way out.

The one that bit us: clang lowers a large `memset(p, 0, n)` to a call to
`___bzero` -- a name no source file mentions, so excluding `memset`/`memcpy`
/`strlen` from bridging (which abigen does) never covered it.  In
`carbon_dialog_shim.m:run_alert` that produced `bzero(NULL, ...)` and a
SIGSEGV at address 0.  `___tolower` is the same family (the historic
"tolower double-wrap").

WHAT THIS REPORTS
  * every abigen bridge whose name collides with a real system symbol (the
    latent landmines), and
  * every call site inside libabiconv that actually targets one from a
    function that is not itself a bridge -- those are live bugs.

Exit status: 1 if any live call site was found, else 0.

Usage:
    ./src/86x64/bridge-shadow-check.py [libabiconv.dylib] [--asm abiconv.asm]
"""
import argparse
import bisect
import ctypes
import os
import re
import subprocess
import sys


def sh(*cmd):
    return subprocess.run(cmd, capture_output=True, text=True).stdout


def defined_text_externals(lib):
    out = set()
    for line in sh("nm", "-m", lib).splitlines():
        if "__TEXT,__text" not in line:
            continue
        m = re.search(r"\)\s+external\s+(\S+)$", line.rstrip())
        if m:
            out.add(m.group(1))
    return out


def asm_globals(path):
    if not path or not os.path.exists(path):
        return None
    out = set()
    for line in open(path, errors="replace"):
        m = re.match(r"\s*global\s+(\S+)", line)
        if m:
            out.add(m.group(1))
    return out


def system_has(sym):
    """Does the SYSTEM export this Mach-O symbol? (drop one leading underscore)"""
    name = sym[1:] if sym.startswith("_") else sym
    try:
        ctypes.CDLL(None).__getattr__(name)
        return True
    except AttributeError:
        return False


def symbol_index(lib):
    syms = []
    for line in sh("nm", "-n", lib).splitlines():
        p = line.split(None, 2)
        if len(p) == 3 and p[1] in "TtSs":
            try:
                syms.append((int(p[0], 16), p[2].strip()))
            except ValueError:
                pass
    syms.sort()
    return syms, [s[0] for s in syms]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("lib", nargs="?", help="path to libabiconv.dylib")
    ap.add_argument("--asm", help="abiconv.asm (to identify generated bridges)")
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="list every colliding bridge name, not just live hits")
    a = ap.parse_args()

    lib = a.lib
    if not lib:
        for c in ("build/src/abiconv/libabiconv.dylib",
                  os.path.expanduser("~/projects/translations/Apps64/Halo.app/"
                                     "Contents/MacOS/libabiconv.dylib")):
            if os.path.exists(c):
                lib = c
                break
    if not lib or not os.path.exists(lib):
        sys.exit("no libabiconv.dylib found; pass one explicitly")

    asm = a.asm
    if not asm:
        for c in ("build/src/abiconv/abiconv.asm", "src/abiconv/abiconv.asm"):
            if os.path.exists(c):
                asm = c
                break

    defined = defined_text_externals(lib)
    bridges = asm_globals(asm)
    if bridges is None:
        # Fall back: treat every defined ___X as a candidate bridge.
        print("note: no abiconv.asm found; treating all defined ___* as bridges")
        bridges = {s for s in defined if s.startswith("___")}
    bridges &= defined

    collisions = sorted(b for b in bridges if system_has(b))
    print(f"library : {lib}")
    print(f"bridges : {len(bridges)} defined")
    print(f"collide : {len(collisions)} also exist as real system symbols")
    if a.verbose:
        for c in collisions:
            print(f"    {c}")

    # Live call sites: a `callq ___X` whose caller is not itself a bridge.
    coll = set(collisions)
    syms, addrs = symbol_index(lib)
    dis = sh("otool", "-tV", "-arch", "x86_64", lib)
    live = []
    for line in dis.splitlines():
        m = re.match(r"^([0-9a-f]{8,16})\t.*\bcallq\s+(\S+)\s*$", line)
        if not m:
            continue
        target = m.group(2)
        if target not in coll:
            continue
        at = int(m.group(1), 16)
        i = bisect.bisect_right(addrs, at) - 1
        caller = syms[i][1] if i >= 0 else "?"
        if caller in bridges:          # a bridge calling a bridge is by design
            continue
        live.append((at, caller, target))

    if live:
        print(f"\n*** {len(live)} LIVE SHADOWED CALL SITE(S) — these are bugs ***")
        for at, caller, target in live:
            print(f"    0x{at:x}  {caller}  ->  {target}")
        print("\nFix: keep the value off the stack / out of the lowering (e.g. heap-\n"
              "allocate a large struct instead of memset-ing it), or give the bridge\n"
              "a name the system does not also export.")
        return 1
    print("\nno live shadowed call sites")
    return 0


if __name__ == "__main__":
    sys.exit(main())
