#!/usr/bin/env python3
"""abigen_modern_manifest.py — drive the MODERN abigen pass's symbol set + header
set by AUTO-DISCOVERY from the targets' actual imports (per-symbol → framework →
umbrella header), replacing the hand-curated ABICONV_SYM_SOURCES framework list.

The modern abigen pass emits i386-cdecl → x86_64-SysV ABI-bridge shims for
present-native C functions a translated i386 binary calls. Historically its symbol
set was (exports of a HAND-CURATED framework list) ∩ (declarations in includes.h),
grown reactively each time a target crashed on an unshimmed symbol. Anything no
listed framework covered (mach_timebase_info from libSystem, CGImageCreateWith-
ImageInRect from CoreGraphics, ...) bound straight to the native callee — i386
cdecl stack args vs the native fn's SysV registers → garbage → the native 8-byte
ret over-pops the i386 4-byte frame → fused PC. Those gaps were patched with
per-target shims (halo_shim.c). This script makes the pass UNIVERSAL and
import-driven so no per-target shim is needed.

Per-symbol auto-discovery:
  1. `nm -m -arch i386 <target>` lists every undefined external WITH its source
     dylib leaf, e.g. `_CGImageCreateWithImageInRect (from ApplicationServices)`.
     The leaf is exactly the two-level bind's source — authoritative per binary.
  2. The consider set = union of all targets' imports − already-shimmed. We do NOT
     intersect with a framework export list: nm -m already proves each symbol is
     imported from a real source, and abigen SELF-FILTERS — it emits a shim only
     for symbols it finds a prototype for in the headers below. A removed-in-64-bit
     symbol (classic Carbon) has no modern declaration → abigen skips it → it falls
     to the SEPARATE legacy pass (includes_legacy.h, 10.6 SDK). A re-exported symbol
     (CG via the ApplicationServices umbrella) is declared transitively by the
     umbrella, so no re-export chain resolution is needed.
  3. Each discovered framework leaf contributes its umbrella header to a generated
     includes file, so abigen has a declaration to marshal from. Leaves with no
     umbrella mapping are reported (their symbols stay unshimmed → Phase 2 generic
     fallback), so coverage gaps are visible rather than silent.

Outputs:
  --out-syms FILE      one `_sym` per line, ready for `abigen -s` (the consider set)
  --out-includes FILE  a C header #including each discovered framework's umbrella
  --report FILE        JSON: per-leaf import counts, unmapped leaves

Usage:
  abigen_modern_manifest.py --target <i386 bin> [--target ...]
      [--exclude-asm abiconv.asm ...] --out-syms abiconv.syms
      --out-includes includes_auto.h [--report report.json]
"""
import argparse, json, os, re, subprocess, sys

# Framework install_name leaf  ->  umbrella header abigen should parse. A generic,
# stable Apple convention (not per-target / per-symbol curation): an umbrella
# transitively includes its sub-headers AND its re-exported sub-frameworks'
# headers (ApplicationServices.h pulls in CoreGraphics/CGImage.h; mach/mach.h pulls
# mach_time.h), so this is enough for abigen to find a prototype for any present-
# native symbol bound through that leaf. Removed-in-64-bit symbols are #if-gated
# out of these modern headers, so abigen self-skips them here (→ legacy pass).
UMBRELLA = {
    "Foundation":            ["<Foundation/Foundation.h>"],
    "AppKit":                ["<AppKit/AppKit.h>"],
    "CoreFoundation":        ["<CoreFoundation/CoreFoundation.h>"],
    "CoreServices":          ["<CoreServices/CoreServices.h>"],
    "ApplicationServices":   ["<ApplicationServices/ApplicationServices.h>"],
    "CoreGraphics":          ["<CoreGraphics/CoreGraphics.h>"],
    "QuartzCore":            ["<QuartzCore/QuartzCore.h>"],
    "ImageIO":               ["<ImageIO/ImageIO.h>"],
    "OpenGL":                ["<OpenGL/gl.h>", "<OpenGL/OpenGL.h>", "<OpenGL/glu.h>"],
    "Security":              ["<Security/Security.h>"],
    "SystemConfiguration":   ["<SystemConfiguration/SystemConfiguration.h>"],
    "DiskArbitration":       ["<DiskArbitration/DiskArbitration.h>"],
    "IOKit":                 ["<IOKit/IOKitLib.h>"],
    "AudioToolbox":          ["<AudioToolbox/AudioToolbox.h>"],
    "Carbon":                ["<Carbon/Carbon.h>"],   # present-native Carbon Events etc.
    # libSystem umbrella: mach + the C library. mach/mach.h transitively declares
    # the unbridged-native time family (mach_timebase_info/mach_absolute_time/host_*).
    "libSystem":             ["<mach/mach.h>", "<mach/mach_time.h>",
                              "<sys/sysctl.h>", "<time.h>"],
}

# Leaves we deliberately do NOT umbrella-map here: handled elsewhere or no modern
# header (so their symbols are reported as gaps, not silently shimmed).
#   libobjc   -> objc runtime, hand-shimmed (objc_shim.c)
#   libcrypto -> openssl headers removed from the SDK (LibreSSL); generic-fallback
#   libSystem data crt slots -> patch_import_pointers / data-shadow machinery


def nm_imports_with_framework(binary, arch):
    """{leaf: set(_sym)} for every undefined external, from `nm -m`.
    Line shape: `... (undefined) external _Sym (from Foundation)`."""
    out = subprocess.run(["nm", "-m", "-arch", arch, binary],
                         capture_output=True, text=True).stdout
    fw = {}
    # nm -m: `(undefined [lazy bound]) external _Sym (from Foundation)`. The
    # bracketed qualifier ([lazy bound]/[weak]/...) is optional, hence [^)]*.
    # `_\S+` skips the i386 `.objc_class_name_X` class refs (handled elsewhere).
    pat = re.compile(r"\(undefined[^)]*\)\s+external\s+(_\S+)\s+\(from\s+([^)]+)\)")
    for line in out.splitlines():
        m = pat.search(line)
        if m:
            fw.setdefault(m.group(2).strip(), set()).add(m.group(1))
    return fw


def asm_emitted_shims(asmpath):
    """`_sym` for every ___sym shim a libabiconv .asm already defines (so we don't
    re-emit a colliding global). Same parse as shimdb_legacy_considerset.py."""
    syms = set()
    try:
        with open(asmpath) as f:
            for line in f:
                s = line.replace(",", " ").split()
                if len(s) >= 2 and s[0] in ("global", "MTSHIM") and s[1].startswith("___"):
                    syms.add(s[1][2:])
    except OSError:
        pass
    return syms


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", action="append", default=[],
                    help="i386 target binary whose imports drive the set (repeatable)")
    ap.add_argument("--arch", default="i386")
    ap.add_argument("--exclude-asm", action="append", default=[],
                    help="libabiconv .asm whose already-emitted shims to exclude")
    ap.add_argument("--out-syms", required=True)
    ap.add_argument("--out-includes", required=True)
    ap.add_argument("--out-ignore",
                    help="write the already-shimmed (hand-shim) symbols, one _sym "
                         "per line, for the modern abigen pass to UNION into its -i "
                         "ignore file. Closes the 'protected only by header omission' "
                         "collision: a hand-shimmed symbol (___sysctl in maptable_tramp.asm) "
                         "that is ALSO a curated-framework export would, once a broader "
                         "header (sys/sysctl.h) makes it parseable, be re-emitted as a "
                         "second `global ___sym` -> duplicate-symbol link error. Ignoring "
                         "every hand-shim symbol prevents that regardless of header coverage.")
    ap.add_argument("--report")
    a = ap.parse_args()

    # 1. aggregate imports per source-framework leaf across all targets
    per_leaf = {}        # leaf -> set(_sym)
    n_targets = 0
    for t in a.target:
        t = os.path.expanduser(t)
        if not os.path.exists(t):
            print("  warning: target not found, skipping: %s" % t, file=sys.stderr)
            continue
        n_targets += 1
        for leaf, syms in nm_imports_with_framework(t, a.arch).items():
            per_leaf.setdefault(leaf, set()).update(syms)

    # 2. consider set = union(all imports) − already-shimmed. abigen self-filters
    #    to the subset it can parse a present-native prototype for.
    already = set()
    for p in a.exclude_asm:
        already |= asm_emitted_shims(p)
    all_imports = set()
    for syms in per_leaf.values():
        all_imports |= syms
    consider = sorted(all_imports - already)

    with open(a.out_syms, "w") as f:
        f.write("# modern import-driven consider set: union(target imports) − already-shimmed\n")
        f.write("# %d targets, %d leaves, %d imports, %d already-shimmed, %d to consider\n"
                % (n_targets, len(per_leaf), len(all_imports), len(already), len(consider)))
        for s in consider:
            f.write(s + "\n")

    # 2b. hand-shim ignore set: the symbols the --exclude-asm files already define.
    #     The modern abigen pass unions this into its -i ignore file so a hand-shimmed
    #     symbol that is ALSO a curated-framework export is never re-emitted.
    if a.out_ignore:
        with open(a.out_ignore, "w") as f:
            f.write("# hand-shim symbols (from --exclude-asm) to add to abigen's -i ignore\n")
            for s in sorted(already):
                f.write(s + "\n")

    # 3. generated includes: umbrella headers for each discovered leaf we can map
    mapped, unmapped = [], []
    seen, lines = set(), []
    for leaf in sorted(per_leaf):
        hs = UMBRELLA.get(leaf)
        if hs:
            mapped.append(leaf)
            for h in hs:
                if h not in seen:
                    seen.add(h)
                    lines.append("#include %s\n" % h)
        else:
            unmapped.append((leaf, len(per_leaf[leaf])))
    with open(a.out_includes, "w") as f:
        f.write("/* AUTO-GENERATED by abigen_modern_manifest.py — do not edit. */\n")
        f.write("/* Umbrella headers for the frameworks the targets import. */\n")
        f.writelines(lines if lines else ["/* (no umbrella-mapped frameworks) */\n"])

    print("targets            : %d" % n_targets)
    print("mapped leaves      : %d  (%s)" % (len(mapped), ", ".join(mapped)))
    if unmapped:
        print("UNMAPPED leaves    : %s"
              % ", ".join("%s(%d)" % (k, v) for k, v in sorted(unmapped, key=lambda x: -x[1])))
        print("                     ^ symbols from these get no modern header → "
              "abigen skips → Phase 2 generic fallback (or add to UMBRELLA).")
    print("considered symbols : %d  -> %s" % (len(consider), a.out_syms))
    print("generated includes : %s  (%d umbrella headers)" % (a.out_includes, len(seen)))

    if a.report:
        json.dump({"n_targets": n_targets,
                   "mapped": mapped,
                   "unmapped": {k: v for k, v in unmapped},
                   "per_leaf_counts": {k: len(v) for k, v in per_leaf.items()},
                   "n_consider": len(consider),
                   "consider": consider},
                  open(a.report, "w"), indent=1)
        print("report             : %s" % a.report)


if __name__ == "__main__":
    main()
