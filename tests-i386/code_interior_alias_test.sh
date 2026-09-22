#!/bin/bash
#
# Regression test for the CODE-INTERIOR ALIAS gate
# (core: section.cc DataParser + ParseEnv::code_interior_alias; Civ IV STEAM
# static-init SIGSEGV at slide_objc -> abiconv_call_init, 2026-07-26).
#
# A locals-stripped, reloc-less, fixed-address (-no_pie) i386 executable gives
# macho-tool no way to know which 4-byte __DATA words are pointers, so its
# in-range heuristic falsely "rebases" any integer/string constant that aliases
# __text. Civ IV's strtok delimiter string " ._" (= 0x005F2E20) aliased the last
# byte of a 5-byte `call` and was rewritten to a translated __text address whose
# bytes are "mT\xa9\x10"; strtok then split the registry path "Game" on 'm', the
# lookup failed, and the NULL node was dereferenced at +0x88.
#
# The fix: a genuine code pointer always targets an instruction BOUNDARY, so a
# value landing strictly INSIDE a decoded instruction is a constant.
#
# A/B (the env kill-switch M64_NO_CODE_INTERIOR_GATE=1 disarms the gate):
#   gate ON  -> the interior-aliasing __DATA word survives byte-identical
#   gate OFF -> the same word is rebased (proves the guard actually bites)
# Both directions are asserted, so the test cannot silently pass for the wrong
# reason. Needs the SL i386 sysroot; SKIPs without it (like the suite).
# i386 linking needs the Snow Leopard ld64-95 wrapper: modern ld dropped -arch i386
# (same resolution as the Makefile's LD). Override with LD=... in the environment.
LD="${LD:-$HOME/projects/Library/Toolchains/sl-ld64/ld-i386}"; [ -x "$LD" ] || LD=ld

set -u
cd "$(dirname "$0")"

PROJ_ROOT="$(cd .. && pwd)"
MT="${1:-$PROJ_ROOT/build/src/macho-tool/macho-tool}"
LIBABICONV="$PROJ_ROOT/build/src/abiconv/libabiconv.dylib"
LIBWRAPPER="$PROJ_ROOT/build/src/86x64/libwrapper.a"
LIBINTERPOSE="$PROJ_ROOT/build/src/86x64/libinterpose.dylib"
PIPELINE="$PROJ_ROOT/src/86x64/86x64.sh"
SYSROOT=/tmp/i386-sysroot

if [ ! -f "$SYSROOT/usr/lib/libSystem.dylib" ] && [ ! -f "$SYSROOT/usr/lib/libSystem.B.dylib" ]; then
   echo "SKIP code-interior-alias (no i386 sysroot at $SYSROOT; run 'make sysroot')"
   exit 0
fi
for f in "$MT" "$LIBABICONV" "$LIBWRAPPER" "$LIBINTERPOSE"; do
   [ -e "$f" ] || { echo "SKIP code-interior-alias (missing $f; build first)"; exit 0; }
done

mkdir -p build
fail() { echo "FAIL code-interior-alias ($1)"; exit 1; }

clang -arch i386 -isysroot "$SYSROOT" -mmacosx-version-min=10.6 \
   -c src/code_interior_alias.s -o build/code_interior_alias.o \
   2>/dev/null || fail assemble
# non-PIE MH_EXECUTE: the fixed-load-address, reloc-less shape that disarms
# every other pointer-detection discriminator.
"$LD" -arch i386 -macos_version_min 10.6 -no_pie -syslibroot "$SYSROOT" \
   -lSystem -e _main -o build/code_interior_alias.i386 \
   build/code_interior_alias.o 2>/dev/null || fail link

# _probe_pad is a single 6-byte instruction; +2 is strictly interior to it.
PROBE="$(nm build/code_interior_alias.i386 2>/dev/null | awk '$3=="_probe_pad"{print $1}')"
[ -n "$PROBE" ] || fail "could not read _probe_pad vmaddr"
INTERIOR=$(( 0x$PROBE + 2 ))
# The heuristic only considers values in [0x1000, 0x80000000); below that the
# fixture would prove nothing.
[ "$INTERIOR" -ge 4096 ] || fail "probe vmaddr $PROBE too low to exercise the heuristic"

# Patch the interior address into the sentinel-bracketed __DATA slot as a plain
# integer literal (no relocation) — exactly Civ's shape.
python3 - "$INTERIOR" build/code_interior_alias.i386 <<'PY' || fail "patch"
import sys, struct
val = int(sys.argv[1]); path = sys.argv[2]
d = bytearray(open(path, 'rb').read())
lead = struct.pack('<I', 0xA5A51111); tail = struct.pack('<I', 0xA5A52222)
needle = lead + struct.pack('<I', 0) + tail
i = d.find(needle)
if i < 0:
    sys.exit("sentinel slot not found in linked i386 binary")
if d.find(needle, i + 1) >= 0:
    sys.exit("sentinel slot is not unique")
d[i+4:i+8] = struct.pack('<I', val)
open(path, 'wb').write(bytes(d))
PY

# Strip local symbols: no func_syms for _probe_pad, so code_alias_is_constant
# stays disarmed — the Civ IV shape, and the only shape this gate must cover.
strip -x build/code_interior_alias.i386 2>/dev/null || fail strip
if nm build/code_interior_alias.i386 2>/dev/null | grep -q " _probe_pad$"; then
   fail "_probe_pad still symboled after strip"
fi

# Read the middle slot back out of a translated artifact.
readback() {
   python3 - "$1" <<'PY'
import sys, struct, glob, os
stem = sys.argv[1]
cands = [stem + '.dylib', stem]
data = None
for c in cands:
    if os.path.exists(c):
        d = open(c, 'rb').read()
        lead = struct.pack('<I', 0xA5A51111); tail = struct.pack('<I', 0xA5A52222)
        i = d.find(lead)
        while i >= 0:
            if d[i+8:i+12] == tail:
                print(struct.unpack('<I', d[i+4:i+8])[0]); sys.exit(0)
            i = d.find(lead, i + 1)
print('NOTFOUND')
PY
}

translate() {   # $1 = output stem ; env already set by caller
   rm -f "$1" "$1.dylib"
   bash "$PIPELINE" -m "$MT" -l "$LIBABICONV" -w "$LIBWRAPPER" -i "$LIBINTERPOSE" \
      -o "$1" build/code_interior_alias.i386 >/dev/null 2>&1
}

# The CODE-ENTRY gate (code_entry_alias_test.sh) rejects the SAME fixture word
# for a different reason (mid-function, no nlist / no `55 89 e5` prologue), so
# disabling only this gate is not enough to observe the pre-fix behavior. Three
# arms isolate this gate exactly:
#   A  both gates on              -> preserved
#   B  ENTRY gate off, this on    -> preserved  (this gate alone suffices)
#   C  both gates off             -> rebased    (the fixture really is detected)

# --- A: both gates on -----------------------------------------------------
translate build/code_interior_alias.on.x86_64 || fail "translate (both on)"
GOT_ON="$(readback build/code_interior_alias.on.x86_64)"
[ "$GOT_ON" != "NOTFOUND" ] || fail "sentinel slot not found in translated output (both on)"

# --- B: entry gate off, interior gate ON (isolates THIS gate) -------------
M64_NO_CODE_ENTRY_GATE=1 translate build/code_interior_alias.iso.x86_64 \
   || fail "translate (entry gate off)"
GOT_ISO="$(readback build/code_interior_alias.iso.x86_64)"
[ "$GOT_ISO" != "NOTFOUND" ] || fail "sentinel slot not found in translated output (entry off)"

# --- C: both gates off (pre-fix behavior) --------------------------------
M64_NO_CODE_INTERIOR_GATE=1 M64_NO_CODE_ENTRY_GATE=1 \
   translate build/code_interior_alias.off.x86_64 || fail "translate (both off)"
GOT_OFF="$(readback build/code_interior_alias.off.x86_64)"
[ "$GOT_OFF" != "NOTFOUND" ] || fail "sentinel slot not found in translated output (both off)"

if [ "$GOT_ON" != "$INTERIOR" ]; then
   printf 'FAIL code-interior-alias (mid-instruction constant 0x%x was rebased to 0x%x)\n' \
      "$INTERIOR" "$GOT_ON"
   exit 1
fi
if [ "$GOT_ISO" != "$INTERIOR" ]; then
   printf 'FAIL code-interior-alias (with only the ENTRY gate disabled the constant 0x%x\n' "$INTERIOR"
   printf '     was rebased to 0x%x -- this gate is not catching it on its own)\n' "$GOT_ISO"
   exit 1
fi
if [ "$GOT_OFF" = "$INTERIOR" ]; then
   echo "FAIL code-interior-alias (guard is inert: the constant survives with BOTH"
   echo "     gates disabled, so it does not actually exercise pointer detection)"
   exit 1
fi

printf 'PASS code-interior-alias (0x%x preserved with the gate; rebased to 0x%x without)\n' \
   "$INTERIOR" "$GOT_OFF"
exit 0
