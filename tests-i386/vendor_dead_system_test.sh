#!/bin/bash
# vendor_dead_system_test.sh — regression guard for the DEAD-ABSOLUTE-SYSTEM-PATH
# auto-vendor in src/86x64/_vendor_deps.py.
#
# The bug it prevents: an i386-era app links a system framework by ABSOLUTE path
# that Apple later REMOVED — the canonical case is Civ IV / iPhoto embedding
# Python 2.6 via /System/Library/Frameworks/Python.framework/Versions/2.6/Python.
# Modern macOS ships no Python 2.x, so the whole framework dir is gone and dyld
# hard-fails at LOAD ("Library not loaded: .../Python … no such file, not in dyld
# cache") before main. _vendor_deps used to `continue` past every /System and
# /usr dep, so it never pulled the removed framework into the bundle. The fix
# auto-vendors a dead absolute /System//usr dep (path resolves to nothing AND has
# no modern native home) exactly like the hollow-shell (QuickTime) case, after
# which `m64 rpath-fix` repoints the consumer at @rpath/... (that half already
# worked). Universal: any i386 app linking a removed system framework hits it.
#
# The subtle trap the fix must NOT fall into: on modern macOS almost every
# /usr/lib/*.dylib and /System framework has NO file on disk — it lives only in
# the dyld shared cache — so a bare os.path.exists() is False even for perfectly
# loadable libs (libSystem.B.dylib, libobjc.A.dylib, Foundation). The oracle is
# dyld's own _dyld_shared_cache_contains_path(); "dead" means neither on disk nor
# in the cache. And a dead /System path whose leaf is ALREADY in the bundle
# (Python's Extras ship libsvn_swig_py-1.0.dylib recorded with its dead /System
# install path) must NOT be re-vendored.
#
# Asserts, WITHOUT needing the i386 sysroot (pure Python-module unit tests
# against the real production functions — no reimplementation to drift):
#   1. system_dep_loadable(): True for a cache-served lib (libSystem.B.dylib),
#      False for a removed framework (Python 2.6). [the oracle]
#   2. vendor() over a synthetic bundle+source pool:
#      a. a DEAD /System/.../Python.framework dep IS vendored from the pool;
#      b. a cache-served /usr/lib/libSystem.B.dylib dep is NOT vendored;
#      c. a dead /System dep whose leaf is already nested in the bundle is NOT
#         re-vendored (no false positive).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ_ROOT="$(cd "$HERE/.." && pwd)"
VDEP="$PROJ_ROOT/src/86x64/_vendor_deps.py"

[ -f "$VDEP" ] || { echo "_vendor_deps.py not found — SKIP"; exit 0; }

fail=0

python3 - "$VDEP" <<'PY' || fail=1
import importlib.util, os, sys, tempfile, shutil
from importlib.machinery import SourceFileLoader
from pathlib import Path

vdep_path = sys.argv[1]
loader = SourceFileLoader("vdep", vdep_path)   # underscore .py: explicit loader
spec = importlib.util.spec_from_file_location("vdep", vdep_path, loader=loader)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

ok = True

# ---- 1: the dyld-cache oracle. libSystem is cache-served (loadable) even with
# no file on disk; the removed Python 2.6 framework is neither.
LIBSYSTEM = "/usr/lib/libSystem.B.dylib"
DEAD_PY   = "/System/Library/Frameworks/Python.framework/Versions/2.6/Python"
if not m.system_dep_loadable(LIBSYSTEM):
    print(f"FAIL(1a): {LIBSYSTEM} reported NOT loadable (cache oracle broken?)"); ok = False
if m.system_dep_loadable(DEAD_PY):
    print(f"FAIL(1b): removed {DEAD_PY} reported loadable"); ok = False

# ---- 2: end-to-end vendor() over a synthetic bundle + source pool.
d = tempfile.mkdtemp()
try:
    app = Path(d) / "T.app"
    macos = app / "Contents/MacOS"
    fw    = app / "Contents/Frameworks"
    macos.mkdir(parents=True); fw.mkdir(parents=True)

    # is_macho() only checks the 4-byte magic and we STUB deps_of below (so otool
    # is never consulted); a bare 64-bit LE Mach-O magic suffices. Don't copy a
    # real system binary — SIP chflags make shutil.copy2 fail under /tmp.
    MACHO64_LE = b'\xcf\xfa\xed\xfe'
    def macho(p):
        Path(p).write_bytes(MACHO64_LE + b'\x00' * 60)

    consumer = macos / "T"
    macho(consumer)

    # A nested, already-in-bundle lib whose recorded dep is a DEAD /System path.
    # It lives under a DIFFERENT (already-present) framework so Python.framework
    # itself stays absent and must be vendored — this leaf tests the "dead
    # /System path but leaf already in the bundle -> don't re-vendor" guard.
    nested = fw / "Other.framework/Versions/A/lib"
    nested.mkdir(parents=True)
    macho(nested / "libalready.dylib")         # present in the bundle by basename

    # Source pool holding the removed framework to vendor.
    pool = Path(d) / "pool"
    poolfw = pool / "Python.framework/Versions/2.6"
    poolfw.mkdir(parents=True)
    macho(poolfw / "Python")
    (pool / "Python.framework/Versions/Current").symlink_to("2.6")
    (pool / "Python.framework" / "Python").symlink_to("Versions/Current/Python")

    # Stub deps_of: the consumer links three things —
    #   • a DEAD removed framework (must be vendored),
    #   • a cache-served system lib (must NOT be vendored),
    #   • a DEAD /System path whose leaf is already nested in the bundle (must
    #     NOT be re-vendored).
    DEPS = {
        str(consumer): [
            DEAD_PY,
            "/usr/lib/libSystem.B.dylib",
            "/System/Library/PrivateFrameworks/Dead.framework/lib/libalready.dylib",
        ],
    }
    m.deps_of = lambda b: DEPS.get(str(b), [])

    vendored, unresolved = m.vendor(app, [pool] + m.DEFAULT_SOURCES, dry=False)

    bundled_py = (fw / "Python.framework").is_dir() and \
                 (fw / "Python.framework/Versions/2.6/Python").exists()
    if not bundled_py:
        print("FAIL(2a): dead /System Python.framework was NOT vendored into the bundle"); ok = False
    if (fw / "libSystem.B.dylib").exists():
        print("FAIL(2b): cache-served libSystem.B.dylib was wrongly vendored"); ok = False
    # (2c) the already-nested libalready.dylib must not have been copied to the
    # top of Frameworks/ as a fresh vendored dylib.
    if (fw / "libalready.dylib").exists():
        print("FAIL(2c): an already-in-bundle nested lib was wrongly re-vendored"); ok = False

    print("vendor dead-system checks:", "PASS" if ok else "FAIL",
          f"(vendored={vendored})")
finally:
    shutil.rmtree(d, ignore_errors=True)

sys.exit(0 if ok else 1)
PY

if [ "$fail" = 0 ]; then echo "vendor_dead_system_test: PASS"; exit 0; fi
echo "vendor_dead_system_test: FAIL"; exit 1
