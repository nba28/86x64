#!/usr/bin/env python3
"""shimdb_legacy_considerset.py — build the abigen "consider set" for the LEGACY
shim pass from a whole legacy SDK (default: the extracted MacOSX10.6 SDK).

The legacy pass is a SECOND abigen invocation, isolated from the modern pass by
its own -isysroot, that emits ABI-marshalling shims for every framework symbol
the modern pass does NOT already shim. Two disjoint sub-classes both want a shim
(see the shimdb legacy expansion):

  SURVIVED_UNSHIMMED  present on modern macOS but its HEADER was deleted, so the
                      modern abigen never saw a declaration -> no ___sym shim.
                      The legacy header gives abigen the prototype; the emitted
                      shim's `call _sym` resolves to the LIVE native function.
  DELETED             gone from modern macOS entirely. The shim still marshals
                      the i386 frame correctly; `call _sym` resolves to a
                      graceful stub provided per-target by shimgen/shimdb.

So the consider set = (all legacy SDK framework exports) MINUS (symbols libabiconv
already shims, i.e. ___sym is exported). abigen then only emits for the subset it
can actually parse a prototype for from includes_legacy.h, so the set can be
generous. Output is one `_sym` per line (nm style, single leading underscore),
ready for `abigen -s`.

Usage:
  shimdb_legacy_considerset.py [--sdk <10.6 sdk>] [--libabiconv <dylib>]
        [--arch x86_64] [-o abiconv_legacy.syms] [--report report.json]
"""
import argparse, json, os, subprocess, sys, glob

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _i386_closure import expand_target
import m64_paths

def framework_stub_binaries(sdk):
    """Every <FW>.framework/<FW> Mach-O stub under the SDK (incl. sub-frameworks)."""
    bins = []
    fwroot = os.path.join(sdk, "System/Library/Frameworks")
    for fwdir in sorted(glob.glob(os.path.join(fwroot, "*.framework"))):
        name = os.path.basename(fwdir)[:-len(".framework")]
        # the version-current symlink resolves to Versions/A/<name>
        cand = os.path.join(fwdir, name)
        if os.path.exists(cand):
            bins.append(cand)
        # sub-frameworks (Carbon -> HIToolbox, CarbonCore, ...) under Versions/*/Frameworks
        for sub in sorted(glob.glob(os.path.join(fwdir, "Versions/*/Frameworks/*.framework"))):
            subname = os.path.basename(sub)[:-len(".framework")]
            subcand = os.path.join(sub, subname)
            if os.path.exists(subcand):
                bins.append(subcand)
    return bins

def nm_defined_exports(dylib, arch):
    out = subprocess.run(["nm", "-gU", "-arch", arch, dylib],
                         capture_output=True, text=True).stdout
    syms = set()
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 3 and p[1] not in ("U",):
            syms.add(p[2])
    return syms

def nm_undefined_imports(binary, arch):
    """External *undefined* symbols a target binary imports (its `call _sym`
    references), nm -u. Used to restrict the consider set to symbols some target
    actually calls -- 'only useful symbols' -- instead of the whole framework
    surface (the classic 32-bit-only Carbon umbrella alone is ~5000 functions /
    20MB of asm that no target touches)."""
    out = subprocess.run(["nm", "-u", "-arch", arch, binary],
                         capture_output=True, text=True).stdout
    syms = set()
    for line in out.splitlines():
        s = line.split()
        # `nm -u` lines are just the undefined name (optionally with a "(...)").
        if s and s[0].startswith("_"):
            syms.add(s[0])
    # A translated target that was ALREADY retranslated against the current
    # shim set imports `___X (from libabiconv)` (the baked redirect) instead of
    # the raw `_X`. That import is positive PROOF the target calls `_X`, so
    # normalize it back — otherwise a rebuild AFTER a retranslate drops every
    # shim the previous build created (the consider set no longer sees the raw
    # name), and the next resync deploys a libabiconv missing the very ___X
    # exports the deployed dylib binds -> dyld "Symbol not found". Makes the
    # import-driven set IDEMPOTENT across build->retranslate->build cycles.
    # Scoped to the libabiconv two-level leaf (`nm -m`): a genuine triple-
    # underscore import from elsewhere (libSystem's ___assert_rtn = C
    # __assert_rtn) must NOT contribute a stripped phantom `_assert_rtn` —
    # that name trips abigen's shim-name collision guard and suppresses the
    # REAL ___assert_rtn shim.
    outm = subprocess.run(["nm", "-m", "-arch", arch, binary],
                          capture_output=True, text=True).stdout
    import re as _re
    pat = _re.compile(r"\(undefined[^)]*\)\s+(?:weak\s+)?external\s+(___\S+)\s+"
                      r"\(from\s+libabiconv\)")
    for line in outm.splitlines():
        m = pat.search(line)
        if m:
            syms.add(m.group(1)[2:])
    return syms

def nm_libabiconv_shims(libabiconv):
    out = subprocess.run(["nm", "-gU", libabiconv], capture_output=True, text=True).stdout
    return {p[2] for p in (l.split() for l in out.splitlines()) if len(p) >= 3}

def asm_emitted_shims(asmpath):
    """The `_sym` for every `___sym` shim ALREADY defined by a libabiconv .asm,
    so the legacy pass does not re-emit a colliding `global ___sym`. Two forms:

      "\tglobal\t___CFRetain"               the modern abigen pass (abiconv.asm)
      "\tMTSHIM ___IOServiceClose, _shim_…"  a hand-shim trampoline (maptable_tramp.asm)

    Both name the shim as ___sym (the "__" override prefix + the _sym symbol);
    strip "__" to recover _sym. Source-level + build-order-correct: these files
    exist before the legacy pass runs, avoiding the chicken-and-egg of nm'ing a
    not-yet-(re)built libabiconv. maptable_tramp.asm is the single home of every
    hand shim (NS*/IOKit/Carbon/QuickTime/ImageCapture/FSSpec MTSHIM lines), so
    scanning it catches hand shims that are not also listed in custom.syms."""
    syms = set()
    with open(asmpath) as f:
        for line in f:
            s = line.replace(",", " ").split()
            if len(s) >= 2 and s[0] in ("global", "MTSHIM") and s[1].startswith("___"):
                syms.add(s[1][2:])   # strip the "__" override prefix -> _CFRetain
    return syms

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sdk", default=str(m64_paths.SDK106))
    ap.add_argument("--libabiconv",
                    default=os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                     "../../build/src/abiconv/libabiconv.dylib")))
    ap.add_argument("--exclude-asm", action="append", default=[],
                    help="modern abiconv.asm whose `global ___sym` shims to exclude. "
                         "When given, this is the authoritative exclusion source and "
                         "--libabiconv is ignored (build-order-correct).")
    ap.add_argument("--arch", default="i386,x86_64",
                    help="comma-separated nm slices to UNION exports from. Must include "
                         "i386: the 32-bit-only legacy symbols (QuickTime Movie Toolbox, "
                         "classic QuickDraw/Window Mgr) live ONLY in the i386 stub slice "
                         "-- the x86_64 slice drops them, exactly the deleted class we want "
                         "to marshal. x86_64 is unioned in so nothing present only there is "
                         "lost. (The abigen PARSE arch is separate and is i386.)")
    ap.add_argument("-o", "--out", default="abiconv_legacy.syms")
    ap.add_argument("--report")
    ap.add_argument("--target", action="append", default=[],
                    help="a target i386 binary whose imports define the 'useful' "
                         "symbol set. Repeatable. Each target is expanded to its "
                         "full CO-TRANSLATED dependency closure (_i386_closure.py: "
                         "bundle/pool frameworks + @loader_path siblings), so apps "
                         "whose code lives in companion binaries (iWork SF*.framework) "
                         "contribute their whole legacy import surface, not just the "
                         "main exe's. When given, the consider set is "
                         "restricted to (union of target imports) ∩ (legacy framework "
                         "exports) -- only symbols some target actually calls. Missing "
                         "target files are skipped with a warning. Omit to consider the "
                         "whole legacy framework surface (large).")
    ap.add_argument("--source", action="append", default=[], metavar="DIR",
                    help="extra source pool for closure dep resolution "
                         "(repeatable, searched before the defaults)")
    a = ap.parse_args()

    bins = framework_stub_binaries(a.sdk)
    arches = [x.strip() for x in a.arch.split(",") if x.strip()]
    legacy = set()
    per_fw = {}
    for b in bins:
        e = set()
        for arch in arches:
            e |= nm_defined_exports(b, arch)
        if e:
            per_fw[os.path.basename(b)] = len(e)
            legacy |= e
    # Drop the SDK-stub linker-directive pseudo-symbols ($ld$add$os10.5$_Foo,
    # $ld$hide$..., etc.): they are not C symbols, abigen never emits for them,
    # and they only bloat the consider set. Real symbols always start with '_'.
    legacy = {s for s in legacy if s.startswith("_")}

    # 'only useful symbols': restrict to what our targets actually import —
    # each target expanded to its co-translated dependency closure (main exe +
    # every binary m64 vendors+translates alongside it; the closure walk itself
    # is over the i386 translate-input slice).
    if a.target:
        wanted = set()
        seen_bins = set()
        n_closure = 0
        for t in a.target:
            t = os.path.expanduser(t)
            if not os.path.exists(t):
                print("  warning: target not found, skipping: %s" % t, file=sys.stderr)
                continue
            bins, _skipped = expand_target(t, "i386", a.source or None)
            for b in bins:
                rb = os.path.realpath(str(b))
                if rb in seen_bins:
                    continue
                seen_bins.add(rb)
                n_closure += 1
                for arch in arches:
                    wanted |= nm_undefined_imports(rb, arch)
        before = len(legacy)
        legacy &= wanted
        print("targets             : %d  (%d closure binaries, %d imports -> %d "
              "legacy-framework symbols, from %d framework exports)"
              % (len(a.target), n_closure, len(wanted), len(legacy), before))

    if a.exclude_asm:
        already_shim = set()
        for p in a.exclude_asm:
            already_shim |= asm_emitted_shims(p)
        already = {s for s in legacy if s in already_shim}
    else:
        abi = nm_libabiconv_shims(a.libabiconv) if os.path.exists(a.libabiconv) else set()
        # a legacy symbol _X is already shimmed iff libabiconv exports ___X (== "__"+"_X")
        already = {s for s in legacy if ("__" + s) in abi}
    consider = sorted(legacy - already)

    # only data symbols typically start without a function-ish look; abigen itself
    # filters to parseable function prototypes, so we keep the whole remainder.
    with open(a.out, "w") as f:
        f.write("# legacy consider set: (10.6 SDK framework exports) - (libabiconv ___ shims)\n")
        f.write("# %d frameworks, %d legacy exports, %d already-shimmed, %d to consider\n"
                % (len(per_fw), len(legacy), len(already), len(consider)))
        for s in consider:
            f.write(s + "\n")

    print("frameworks scanned : %d" % len(per_fw))
    print("legacy exports     : %d" % len(legacy))
    print("already shimmed    : %d" % len(already))
    print("consider set       : %d  -> %s" % (len(consider), a.out))

    if a.report:
        json.dump({"frameworks": per_fw,
                   "n_legacy": len(legacy),
                   "n_already_shimmed": len(already),
                   "n_consider": len(consider),
                   "consider": consider},
                  open(a.report, "w"), indent=1)
        print("report             : %s" % a.report)

if __name__ == "__main__":
    main()
