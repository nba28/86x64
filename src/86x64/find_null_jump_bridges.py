#!/usr/bin/env python3
"""find_null_jump_bridges.py — list every libabiconv bridge that can only jump to NULL.

WHY THIS EXISTS. abigen generates a bridge `___X` for any symbol in its consider
set with a parseable declaration in the 10.6-era headers. That is correct as long
as a NATIVE `_X` still exists to call. When Apple REMOVES the underlying API the
bridge survives, the translate pipeline weakens the now-dangling native bind to
NULL, and the bridge's `call` jumps to 0. static-interpose meanwhile redirects
the app's own bind to that bridge -- so we do not merely fail to help, we
actively REPLACE whatever the app would have bound to with a guaranteed crash.

Measured instances (2026-08-03):
  * SecondsToDate / DateToSeconds / Long* -- removed Date & Time Utilities.
    Halo died with rip=0. Fixed by IMPLEMENTING them (8ba04b3).
  * QTNewDataReferenceFromFSRef -- system QuickTime.framework is ABSENT, yet the
    app's OWN bundled translated QuickTime DEFINES the symbol. Here our bridge
    SHADOWS a working implementation, so the fix is to stay out of the path.

Those two need opposite remedies, which is exactly why this tool reports the
`bundled` column rather than just "dead": it separates
  "nothing provides this"      -> implement it, or supply the real framework
  "the app provides this"      -> stop interposing; let the app's bind resolve

A bridge is a NULL-jump trap when ALL of:
  1. libabiconv EXPORTS ___X                (static-interpose will redirect to it)
  2. libabiconv leaves _X UNDEFINED         (the bridge calls out to a native)
  3. _X does not resolve on this OS         (so that call lands on NULL)

Condition 3 is checked by actually dlopening the frameworks a translated app
loads and dlsym'ing the name -- the same question dyld will ask at run time --
rather than by guessing from header availability.

★THAT PROBE RUNS IN A SEPARATE x86_64 PROCESS (src/86x64/nulljump_probe.c), and
must. Asking it in-process was wrong twice over, measured 2026-08-04:
  * python3 here is arm64, so it answered for the WRONG ARCHITECTURE. 33 symbols
    that exist only on x86_64 were called dead -- objc_msgSend_stret / _fpret /
    Super_stret and the whole $INODE64 stat/readdir/scandir family. Excluding
    those from interposition binds them straight to native x86_64 code invoked
    with i386 4-byte argument slots.
  * the probing process WAS Python, so RTLD_DEFAULT found the Python C-API in the
    interpreter itself and declared 63 `Py*` bridges healthy. They are not: they
    were still being interposed into jump-to-zero, and Civilization IV runs its
    entire game logic through that API.

Hand-written shims are NOT traps: they implement the work themselves and leave
no undefined native, so condition 2 excludes them automatically. That matters --
a blanket "exclude all _QT*" would break quicktime_image.c and the golden
QuickTime work, which are deliberately hand-implemented.

usage: find_null_jump_bridges.py <libabiconv.dylib> [bundle-to-check-for-defs ...]
       find_null_jump_bridges.py --emit <libabiconv.dylib>   # bare symbol list
                                                             # for static-interpose
"""
import subprocess
import sys
import os

# ★The dlsym probe MUST run in an x86_64 process. This script used to answer
# "does _X resolve?" with ctypes in its own interpreter, which on Apple Silicon
# is arm64 -- a different question from the one dyld asks when it binds a
# TRANSLATED x86_64 app. Measured 2026-08-04: 33 of 147 "orphans" were that false
# positive, among them objc_msgSend_stret / _fpret / Super_stret and the entire
# $INODE64 stat/readdir/scandir family. Excluding those from interposition leaves
# them bound straight to native x86_64 code called with i386 4-byte argument
# slots. See src/86x64/nulljump_probe.c.
PROBE_SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "nulljump_probe.c")

# The frameworks a translated classic app actually has loaded. dlopen'ing them
# puts their exports in reach of an RTLD_DEFAULT lookup, so the dlsym below asks
# the same question dyld asks when binding.
FRAMEWORKS = [
    "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation",
    "/System/Library/Frameworks/CoreServices.framework/CoreServices",
    "/System/Library/Frameworks/ApplicationServices.framework/ApplicationServices",
    "/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics",
    "/System/Library/Frameworks/Foundation.framework/Foundation",
    "/System/Library/Frameworks/AppKit.framework/AppKit",
    "/System/Library/Frameworks/Carbon.framework/Carbon",
    "/System/Library/Frameworks/OpenGL.framework/OpenGL",
    "/System/Library/Frameworks/AGL.framework/AGL",
    "/System/Library/Frameworks/ImageIO.framework/ImageIO",
    "/System/Library/Frameworks/AudioToolbox.framework/AudioToolbox",
    "/System/Library/Frameworks/IOKit.framework/IOKit",
    "/usr/lib/libSystem.B.dylib",
    "/usr/lib/libobjc.A.dylib",
]


def system_deps(lib):
    """The system libraries `lib` itself links (otool -L): loaded whenever it is,
    so their exports resolve for its bridges whatever the app loads. Without them
    the probe called every OpenAL/ForceFeedback bridge null although libabiconv
    links both (Portal 2's OpenAL binds stayed raw)."""
    out = subprocess.run(["otool", "-L", lib], capture_output=True, text=True).stdout
    return [l.split()[0] for l in out.splitlines()[1:]
            if l.strip().startswith(("/System/", "/usr/lib/"))]


def resolves_on_x86_64(names, extra_libs=()):
    """Subset of `names` that a translated x86_64 app's dyld would resolve.

    Compiles nulljump_probe.c -arch x86_64 and runs it under `arch -x86_64`.
    There is deliberately NO fallback to an in-process ctypes answer: that is the
    bug this replaced, and a plausible wrong list is worse than a loud failure.
    """
    if os.environ.get("M64_NULLJUMP_PROBE_ARCH") == "native":
        # KILL SWITCH -- reinstates the pre-2026-08-04 in-process answer so the
        # guard can show the invariant breaking. Never use it to build a list.
        import ctypes
        for fw in FRAMEWORKS:
            try:
                ctypes.CDLL(fw)
            except OSError:
                pass
        here, got = ctypes.CDLL(None), set()
        for n in names:
            try:
                getattr(here, n[1:])
                got.add(n)
            except AttributeError:
                pass
        return got

    probe = os.path.join(os.environ.get("TMPDIR", "/tmp"), "nulljump_probe.x86_64")
    if not os.path.exists(probe) or os.path.getmtime(probe) < os.path.getmtime(PROBE_SRC):
        cc = subprocess.run(["clang", "-arch", "x86_64", "-O0", "-o", probe, PROBE_SRC],
                            capture_output=True, text=True)
        if cc.returncode != 0:
            raise SystemExit("find_null_jump_bridges: cannot build the x86_64 probe:\n"
                             + cc.stderr)
    run = subprocess.run(["arch", "-x86_64", probe] + FRAMEWORKS + list(extra_libs),
                         input="\n".join(sorted(names)), capture_output=True, text=True)
    if run.returncode != 0:
        raise SystemExit("find_null_jump_bridges: the x86_64 probe failed (is Rosetta "
                         "installed?):\n" + run.stderr)
    return set(run.stdout.split())


def nm(args, path):
    out = subprocess.run(["nm"] + args + [path], capture_output=True, text=True)
    return out.stdout.splitlines()


def exported_bridges(lib):
    """___X symbols libabiconv defines (static-interpose's redirect targets)."""
    got = set()
    for line in nm(["-gU"], lib):
        p = line.split()
        if len(p) >= 3 and p[1] == "T" and p[2].startswith("___"):
            got.add(p[2])
    return got


def undefined_natives(lib):
    """_X symbols libabiconv expects someone else to define."""
    got = set()
    for line in nm(["-u"], lib):
        s = line.strip().split()
        if not s:
            continue
        name = s[-1]
        if name.startswith("_") and not name.startswith("__"):
            got.add(name)
    return got


def defined_in(path):
    """Exported symbols of a bundled binary (e.g. a TRANSLATED framework)."""
    got = set()
    for line in nm(["-gU"], path):
        p = line.split()
        if len(p) >= 3 and p[1] in ("T", "D", "S", "B"):
            got.add(p[2])
    return got


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    emit = sys.argv[1] == "--emit"
    if emit:
        sys.argv.pop(1)
        if len(sys.argv) < 2:
            print(__doc__)
            return 2
    lib = sys.argv[1]

    bridges = exported_bridges(lib)
    undef = undefined_natives(lib)

    # One batched probe rather than one process per symbol.
    candidates = {br[2:] for br in bridges if br[2:] in undef}
    live = resolves_on_x86_64(candidates, system_deps(lib))

    # Symbols provided by any bundle the caller pointed us at (translated
    # frameworks count -- they are real, working implementations).
    bundled = {}
    for extra in sys.argv[2:]:
        for root, _dirs, files in os.walk(extra):
            for f in files:
                p = os.path.join(root, f)
                try:
                    if os.path.getsize(p) < 4096:
                        continue
                    with open(p, "rb") as fh:
                        if fh.read(4) not in (b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe"):
                            continue
                except OSError:
                    continue
                for s in defined_in(p):
                    bundled.setdefault(s, os.path.basename(p))

    traps = []
    for br in sorted(bridges):
        native = br[2:]                        # ___X -> _X
        if native not in undef:
            continue                           # hand-written shim: no native call
        if native in live:
            continue                           # resolves on x86_64: the bridge is fine
        traps.append((native, bundled.get(native)))

    if emit:
        # Bare list consumed by static-interpose.sh: one symbol per line. Kept
        # deliberately dumb so the consumer needs no parsing.
        for name, _ in traps:
            print(name)
        return 0

    print("=== NULL-JUMP BRIDGES (exported ___X, undefined _X, _X absent on this OS) ===")
    print("total: %d\n" % len(traps))
    shadowing = [t for t in traps if t[1]]
    orphan = [t for t in traps if not t[1]]

    print("--- %d SHADOWING a working implementation the app already ships ---" % len(shadowing))
    print("    (fix: stop interposing -- let the app's own bind resolve)")
    for name, where in shadowing:
        print("    %-46s provided by %s" % (name, where))

    print("\n--- %d with NO provider at all ---" % len(orphan))
    print("    (fix: implement, or supply the real framework)")
    for name, _ in orphan:
        print("    %s" % name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
