#!/bin/bash
# rsrc_datafork_test.sh — regression guard for the CLASSIC RESOURCE FORK ->
# DATA-FORK RESOURCE FILE migration.
#
# THE BUG IT PREVENTS (found on the translated Halo CE, 2026-07-27)
# ----------------------------------------------------------------
# Classic Mac apps keep real content in the HFS RESOURCE FORK: a `*.rsrc` file
# whose DATA fork is 0 bytes and whose whole payload is the com.apple.ResourceFork
# xattr. Modern `codesign` REFUSES to sign any bundle that contains one
#   ("resource fork, Finder information, or similar detritus not allowed", exit 1)
# and on Apple Silicon an unsigned bundle cannot launch, so the pipeline HAS to
# get the fork out of the fork. Stripping it leaves a 0-byte husk and every
# Resource Manager load fails SILENTLY: FSOpenResFile -> -1, Get1Resource -> NULL.
# For Halo that killed the first-launch EULA gate — the dialog was built and
# populated, CreateScrollingTextBoxControl('TEXT' 1024) returned -192
# (resNotFound), and Halo DisposeWindow()d it and exit()ed before its modal loop.
#
# THE FIX, in two halves (this guards both):
#   1. src/86x64/m64 flatten_forks() / `m64 forks --flatten`, run automatically
#      by translate_bundle() and cmd_sign(): fork bytes go into the DATA fork
#      when it is empty, else into the hidden sidecar <dir>/.86x64rsrc/<name>;
#      the ResourceFork + FinderInfo xattrs are then removed.
#   2. src/abiconv/rsrc_datafork_shim.c shim_FSOpenResFile: after the real
#      resource-fork open fails, retry the DATA fork via
#      FSOpenResourceFile(ref, 0, NULL, perm, &refNum) — Apple's own .dfont
#      mechanism — and then the sidecar.
#
# Asserts, WITHOUT needing the i386 sysroot:
#   1. a hand-built classic resource file (real resource-map bytes) in a resource
#      fork is INVISIBLE to a signable bundle: codesign REFUSES it;
#   2. flatten_forks() (the real m64 function, imported as a module) moves the
#      payload byte-for-byte: empty data fork -> in place; used data fork ->
#      .86x64rsrc sidecar, original data fork untouched; no xattrs left; idempotent;
#   3. codesign then ACCEPTS the same bundle;
#   4. the modern Resource Manager still reads the flattened file through exactly
#      the call the shim makes — FSOpenResourceFile(ref, 0, NULL, fsRdPerm, &rn)
#      + Get1Resource — returning the identical resource bytes;
#   5. the shim is WIRED: MTSHIM ___FSOpenResFile, custom.syms _FSOpenResFile
#      (so abigen stops emitting the plain forwarder), CMakeLists source entry;
#      and, if libabiconv is built, it exports _shim_FSOpenResFile and
#      ___FSOpenResFile is the MTSHIM trampoline rather than abigen's forwarder.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ_ROOT="$(cd "$HERE/.." && pwd)"
M64="$PROJ_ROOT/src/86x64/m64"
SHIM="$PROJ_ROOT/src/abiconv/rsrc_datafork_shim.c"

[ -f "$M64" ]  || { echo "m64 not found — SKIP"; exit 0; }
[ -f "$SHIM" ] || { echo "rsrc_datafork_shim.c not found — SKIP"; exit 0; }

fail=0
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# ---- fixture: a REAL classic resource file (data area + resource map), built
# from the documented Resource Manager on-disk format, carrying 'TEXT' 1024.
python3 - "$tmp" <<'PY' || { echo "FAIL: could not build the resource fixture"; exit 1; }
import os, struct, sys
d = sys.argv[1]
body = b"86x64 classic resource payload -- do not lose me.\n" * 7

# data area: 4-byte big-endian length + data
data = struct.pack(">I", len(body)) + body
# resource map
typelist_off = 28                       # from map start: 16 hdr + 4 next + 2 ref + 2 attr + 2 + 2
namelist_off = typelist_off + 2 + 8 + 12
mapbody  = b"\0" * 16                   # copy of the file header (ignored on read)
mapbody += struct.pack(">IhH", 0, 0, 0) # nextMap, fileRefNum, attributes
mapbody += struct.pack(">HH", typelist_off, namelist_off)
mapbody += struct.pack(">H", 0)                       # numTypes - 1  (1 type)
mapbody += struct.pack(">4sHH", b"TEXT", 0, 2 + 8)    # type, count-1, refList off
mapbody += struct.pack(">hhBBBBI", 1024, -1, 0, 0, 0, 0, 0)  # id, name off, attrs, 3-byte data off, handle
hdr = struct.pack(">IIII", 256, 256 + len(data), len(data), len(mapbody))
blob = hdr + b"\0" * (256 - len(hdr)) + data + mapbody

app = os.path.join(d, "F.app", "Contents", "Resources")
os.makedirs(os.path.join(app, "Sub"))
# (a) fork-only file: data fork empty  -> flatten IN PLACE
p = os.path.join(app, "Only.rsrc")
open(p, "wb").close()
open(p + "/..namedfork/rsrc", "wb").write(blob)
# (b) both forks: real data fork + resource fork -> SIDECAR, data fork untouched
q = os.path.join(app, "Sub", "Both.dat")
open(q, "wb").write(b"THE DATA FORK MUST SURVIVE")
open(q + "/..namedfork/rsrc", "wb").write(blob)
open(os.path.join(d, "blob.bin"), "wb").write(blob)

os.makedirs(os.path.join(d, "F.app", "Contents", "MacOS"))
open(os.path.join(d, "F.app", "Contents", "Info.plist"), "w").write(
    '<?xml version="1.0"?><plist version="1.0"><dict>'
    '<key>CFBundleIdentifier</key><string>com.86x64.rsrctest</string>'
    '<key>CFBundleExecutable</key><string>t</string></dict></plist>')
import shutil; shutil.copy("/bin/echo", os.path.join(d, "F.app", "Contents", "MacOS", "t"))
print("fixture: %d-byte resource file in 2 forks" % len(blob))
PY

APP="$tmp/F.app"

# ---- 1: codesign REFUSES a bundle that still carries a resource fork.
out="$(codesign -f -s - "$APP" 2>&1)"; rc=$?
if [ "$rc" -ne 0 ] && printf '%s' "$out" | grep -qi "resource fork"; then
  echo "1. codesign REFUSES the fork-carrying bundle (exit $rc) — PASS"
else
  echo "FAIL(1): codesign did not refuse a bundle with a resource fork (exit $rc)"
  printf '%s\n' "$out" | sed 's/^/      /'
  fail=1
fi

# ---- 2: flatten_forks() — the REAL m64 function, imported as a module.
python3 - "$M64" "$tmp" <<'PY' || fail=1
import importlib.util, os, sys
from importlib.machinery import SourceFileLoader
m64_path, d = sys.argv[1], sys.argv[2]
loader = SourceFileLoader("m64drv", m64_path)
spec = importlib.util.spec_from_file_location("m64drv", m64_path, loader=loader)
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)

app  = os.path.join(d, "F.app")
only = os.path.join(app, "Contents/Resources/Only.rsrc")
both = os.path.join(app, "Contents/Resources/Sub/Both.dat")
side = os.path.join(app, "Contents/Resources/Sub", m.RSRC_SIDECAR_DIR, "Both.dat")
blob = open(os.path.join(d, "blob.bin"), "rb").read()

n = m.flatten_forks(app, verbose=False)
ok = True
if n != 2:
    print("FAIL(2a): flatten_forks migrated %d file(s), expected 2" % n); ok = False
if open(only, "rb").read() != blob:
    print("FAIL(2b): fork-only file's data fork != the original fork bytes"); ok = False
if open(both, "rb").read() != b"THE DATA FORK MUST SURVIVE":
    print("FAIL(2c): a real data fork was OVERWRITTEN by the migration"); ok = False
if not os.path.isfile(side) or open(side, "rb").read() != blob:
    print("FAIL(2d): both-forks file's payload did not reach the sidecar"); ok = False
for p in (only, both):
    if m._rsrc_fork_size(p):
        print("FAIL(2e): resource fork still present on", p); ok = False
# idempotent: a second pass is a no-op
if m.flatten_forks(app, verbose=False) != 0:
    print("FAIL(2f): flatten_forks is not idempotent"); ok = False
print("2. flatten_forks:", "PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
PY

# ---- 3: codesign now ACCEPTS the same bundle.
if codesign -f -s - "$APP" >/dev/null 2>&1 && codesign -v "$APP" >/dev/null 2>&1; then
  echo "3. codesign ACCEPTS the flattened bundle — PASS"
else
  echo "FAIL(3): codesign still rejects the bundle after flattening"; fail=1
fi

# ---- 4: the modern Resource Manager reads the flattened file through the exact
# call the shim makes. Needs a compiler; SKIPs gracefully without one.
if xcrun --find clang >/dev/null 2>&1; then
  cat > "$tmp/rmprobe.c" <<'EOF'
#include <CoreServices/CoreServices.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    FSRef ref;
    if (FSPathMakeRef((const UInt8 *)argv[1], &ref, NULL) != noErr) { puts("no ref"); return 2; }
    ResFileRefNum rn = -1;
    OSErr e = FSOpenResourceFile(&ref, 0, NULL, fsRdPerm, &rn);   /* the shim's fallback */
    if (e != noErr || rn <= 0) { printf("open err=%d\n", (int)e); return 3; }
    UseResFile(rn);
    Handle h = Get1Resource('TEXT', 1024);
    if (!h) { puts("no TEXT 1024"); return 4; }
    printf("%ld\n", (long)GetHandleSize(h));
    return 0;
}
EOF
  if clang -arch x86_64 -o "$tmp/rmprobe" "$tmp/rmprobe.c" -framework CoreServices \
        -Wno-deprecated-declarations >/dev/null 2>&1; then
    want="$(python3 -c "
import struct,sys
b=open('$tmp/blob.bin','rb').read(); print(struct.unpack('>I', b[256:260])[0])")"
    for f in "$APP/Contents/Resources/Only.rsrc" \
             "$APP/Contents/Resources/Sub/.86x64rsrc/Both.dat"; do
      got="$("$tmp/rmprobe" "$f" 2>&1)"
      if [ "$got" = "$want" ]; then
        echo "4. Resource Manager read $want bytes of 'TEXT' 1024 from $(basename "$(dirname "$f")")/$(basename "$f") — PASS"
      else
        echo "FAIL(4): $f -> '$got' (expected $want)"; fail=1
      fi
    done
  else
    echo "4. rmprobe did not build — skipping the Resource Manager leg"
  fi
else
  echo "4. no clang — skipping the Resource Manager leg"
fi

# ---- 5: the shim stays WIRED (a silent unwiring reintroduces the whole bug).
grep -q 'MTSHIM[[:space:]]*___FSOpenResFile' "$PROJ_ROOT/src/abiconv/maptable_tramp.asm" \
  || { echo "FAIL(5a): no MTSHIM ___FSOpenResFile in maptable_tramp.asm"; fail=1; }
grep -qx '_FSOpenResFile' "$PROJ_ROOT/src/abiconv/custom.syms" \
  || { echo "FAIL(5b): _FSOpenResFile missing from custom.syms (abigen would dup-emit)"; fail=1; }
grep -q 'rsrc_datafork_shim.c' "$PROJ_ROOT/src/abiconv/CMakeLists.txt" \
  || { echo "FAIL(5c): rsrc_datafork_shim.c not in the abiconv CMake sources"; fail=1; }
grep -q 'FSOpenResourceFile' "$SHIM" \
  || { echo "FAIL(5d): the shim no longer uses FSOpenResourceFile"; fail=1; }
[ "$fail" = 0 ] && echo "5. shim wiring (MTSHIM + custom.syms + CMake) — PASS"

LIBAB="$PROJ_ROOT/build/src/abiconv/libabiconv.dylib"
if [ -f "$LIBAB" ]; then
  if nm -gU "$LIBAB" 2>/dev/null | grep -q ' _shim_FSOpenResFile$'; then
    echo "5e. libabiconv exports _shim_FSOpenResFile — PASS"
  else
    echo "FAIL(5e): built libabiconv does not export _shim_FSOpenResFile"; fail=1
  fi
  if objdump -d --disassemble-symbols=___FSOpenResFile "$LIBAB" 2>/dev/null \
       | grep -q '__dyld_stub_binder_flag'; then
    echo "5f. ___FSOpenResFile is the MTSHIM trampoline (not abigen's forwarder) — PASS"
  else
    echo "FAIL(5f): ___FSOpenResFile is not the MTSHIM trampoline"; fail=1
  fi
else
  echo "5e/5f. libabiconv not built — skipping the built-artifact legs"
fi

if [ "$fail" = 0 ]; then echo "rsrc_datafork_test: PASS"; exit 0; fi
echo "rsrc_datafork_test: FAIL"; exit 1
