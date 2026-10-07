#!/usr/bin/env python3
"""shimdb_legacy_scan.py — classify a legacy framework's exports against modern macOS
and our existing libabiconv shims, to drive proactive shimdb expansion.

Background: legacy i386/Carbon apps bind framework symbols that modern macOS has
either DELETED (-> bare stub jumps through a null lazy pointer, rip=0 crash) or
KEPT-but-stripped-the-header-for (-> abigen never generated a shim, so the native
8-byte `ret` over-pops the 4-byte i386 frame). Pulling the *old* framework
(binary + Headers) out of a high-compatibility OS (Snow Leopard / Mojave) lets us
enumerate both classes up front instead of discovering them one crash at a time.

For one extracted legacy framework dylib, every exported symbol lands in a bucket:

  DELETED           in legacy, NOT in modern  -> null-stub crash class.
                    Needs a curated impl in shimdb/impl/ (or a correct graceful
                    return — value depends on the return type, from the old header;
                    NEVER a blind `return 0`, see the BlockMoveData lesson).
  SURVIVED_UNSHIMMED in legacy AND modern, but libabiconv has no ___<sym> shim
                    -> over-pop class. HIGHEST LEVERAGE: feed the old header to
                    abigen and it emits a real shim that calls the live native fn.
  SURVIVED_SHIMMED  in legacy AND modern AND already has a libabiconv ___<sym>
                    shim -> already handled.

Usage:
  shimdb_legacy_scan.py --legacy-dylib <path> --modern-fw <path-or-name> \
                        [--libabiconv <path>] [--arch x86_64] [-o <json>]

  --modern-fw  either an absolute framework binary path
               (/System/Library/Frameworks/Foo.framework/Foo) or a bare name
               (Foo) resolved under /System/Library/Frameworks. dlopen works even
               when the file lives only in the dyld shared cache.
"""
import argparse, ctypes, ctypes.util, json, os, subprocess, sys

def nm_defined_exports(dylib, arch):
    """External *defined* symbols (T/D/S/B...) of a (possibly fat) dylib, arch slice."""
    out = subprocess.run(["nm", "-gU", "-arch", arch, dylib],
                         capture_output=True, text=True).stdout
    syms = set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[1] not in ("U",):   # addr type name
            syms.add(parts[2])
    return syms

def nm_libabiconv_exports(libabiconv):
    out = subprocess.run(["nm", "-gU", libabiconv], capture_output=True, text=True).stdout
    return {p[2] for p in (l.split() for l in out.splitlines()) if len(p) >= 3}

def open_modern(modern):
    if "/" not in modern:
        modern = f"/System/Library/Frameworks/{modern}.framework/{modern}"
    h = ctypes.CDLL(modern, mode=ctypes.RTLD_GLOBAL)
    return h, modern

def present_in_modern(handle, sym):
    # dlsym wants the symbol WITHOUT exactly one leading underscore.
    name = sym[1:] if sym.startswith("_") else sym
    try:
        return ctypes.cast(getattr(handle, name), ctypes.c_void_p).value is not None
    except AttributeError:
        return False

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--legacy-dylib", required=True)
    ap.add_argument("--modern-fw", required=True)
    ap.add_argument("--libabiconv",
                    default=os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                     "../../build/src/abiconv/libabiconv.dylib")))
    ap.add_argument("--arch", default="x86_64")
    ap.add_argument("-o", "--out")
    a = ap.parse_args()

    legacy = nm_defined_exports(a.legacy_dylib, a.arch)
    abi = nm_libabiconv_exports(a.libabiconv) if os.path.exists(a.libabiconv) else set()
    handle, modern_path = open_modern(a.modern_fw)

    buckets = {"DELETED": [], "SURVIVED_UNSHIMMED": [], "SURVIVED_SHIMMED": []}
    for s in sorted(legacy):
        if not present_in_modern(handle, s):
            buckets["DELETED"].append(s)
        elif ("__" + s) in abi:          # libabiconv export is "__" + original (=> ___sym)
            buckets["SURVIVED_SHIMMED"].append(s)
        else:
            buckets["SURVIVED_UNSHIMMED"].append(s)

    print(f"legacy={a.legacy_dylib}")
    print(f"modern={modern_path}")
    print(f"  exports(legacy)={len(legacy)}  libabiconv_shims={len(abi)}")
    for k in ("DELETED", "SURVIVED_UNSHIMMED", "SURVIVED_SHIMMED"):
        print(f"  {k:18} {len(buckets[k])}")
    if a.out:
        json.dump(buckets, open(a.out, "w"), indent=1)
        print(f"  wrote {a.out}")
    # a small sample of the two actionable buckets to eyeball
    for k in ("DELETED", "SURVIVED_UNSHIMMED"):
        print(f"  --- {k} sample ---")
        for s in buckets[k][:25]:
            print("    ", s)

if __name__ == "__main__":
    main()
