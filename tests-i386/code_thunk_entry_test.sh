#!/bin/bash
#
# Regression test for ADJUSTOR-THUNK entry evidence
# (core: ParseEnv::code_target_has_entry_evidence, third evidence shape).
#
# Companion to code-entry-alias, asserting the OTHER direction of the same
# gate. code-entry-alias proves a mid-function constant is NOT rebased; this one
# proves a genuine vtable slot IS still rebased when it points at an i386 C++
# ABI adjustor thunk — `add|sub $imm, disp8(%esp)` followed by a tail-`jmp`.
#
# A thunk is a real function ENTRY, but it is a local symbol (gone after
# `strip -x`) and has no `55 89 e5` frame setup, so the ENTRY gate's first two
# evidence forms miss it. Measured on the real i386 Civ IV: without this shape
# the gate demoted 7883 genuine vtable slots to constants, leaving them holding
# i386 addresses -> every multiple-inheritance/covariant virtual call through
# them would jump to an unmapped address.
#
# A/B (the env kill-switch M64_NO_THUNK_ENTRY_EVIDENCE=1 removes the shape):
#   evidence ON  -> the __DATA word pointing at the thunk is RELOCATED
#   evidence OFF -> the same word is demoted to a constant (guard actually bites)
# Both directions are asserted, so the test cannot pass for the wrong reason.
# Needs the SL i386 sysroot; SKIPs without it (like the suite).
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
   echo "SKIP code-thunk-entry (no i386 sysroot at $SYSROOT; run 'make sysroot')"
   exit 0
fi
for f in "$MT" "$LIBABICONV" "$LIBWRAPPER" "$LIBINTERPOSE"; do
   [ -e "$f" ] || { echo "SKIP code-thunk-entry (missing $f; build first)"; exit 0; }
done

mkdir -p build
fail() { echo "FAIL code-thunk-entry ($1)"; exit 1; }

clang -arch i386 -isysroot "$SYSROOT" -mmacosx-version-min=10.6 \
   -c src/code_thunk_entry.s -o build/code_thunk_entry.o \
   2>/dev/null || fail assemble
# non-PIE MH_EXECUTE: the fixed-load-address, reloc-less shape that disarms
# every other pointer-detection discriminator.
ld -arch i386 -macos_version_min 10.6 -no_pie -syslibroot "$SYSROOT" \
   -lSystem -e _main -o build/code_thunk_entry.i386 \
   build/code_thunk_entry.o 2>/dev/null || fail link

TARGET_HEX="$(nm build/code_thunk_entry.i386 2>/dev/null | awk '$3=="_thunk_fn"{print $1}')"
[ -n "$TARGET_HEX" ] || fail "could not read _thunk_fn vmaddr"
TARGET=$(( 0x$TARGET_HEX ))
# The heuristic only considers values in [0x1000, 0x80000000).
[ "$TARGET" -ge 4096 ] || fail "thunk vmaddr $TARGET_HEX too low to exercise the heuristic"

# Confirm the fixture really assembled the thunk shape we claim to recognise:
# 83|81  44|6c  24  <disp8>  <imm>  then E9/EB.
python3 - "$TARGET" build/code_thunk_entry.i386 <<'PY' || fail "fixture is not an adjustor thunk"
import sys
vm = int(sys.argv[1]); d = open(sys.argv[2], 'rb').read()
fo = vm - 0x1000                      # every section here: fileoff = vmaddr-0x1000
t = d[fo:fo+12]
if not (t[0] in (0x83, 0x81) and t[1] in (0x44, 0x6c) and t[2] == 0x24):
    sys.exit("bytes at thunk entry are %s, not an add/sub disp8(%%esp)" % t[:6].hex(' '))
n = 5 if t[0] == 0x83 else 8
if t[n] not in (0xe9, 0xeb):
    sys.exit("no jmp after the adjust: %s" % t[:12].hex(' '))
PY

# Patch the thunk entry into the sentinel-bracketed __DATA slot as a plain
# integer literal with NO relocation — a vtable slot in Civ's reloc-less shape.
python3 - "$TARGET" build/code_thunk_entry.i386 <<'PY' || fail "patch"
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

# Strip local symbols: no nlist for _thunk_fn, so the only entry evidence left
# is its instruction shape — the Civ IV situation exactly.
strip -x build/code_thunk_entry.i386 2>/dev/null || fail strip
if nm build/code_thunk_entry.i386 2>/dev/null | grep -q " _thunk_fn$"; then
   fail "_thunk_fn still symboled after strip"
fi

readback() {
   python3 - "$1" <<'PY'
import sys, struct, os
stem = sys.argv[1]
for c in (stem + '.dylib', stem):
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
      -o "$1" build/code_thunk_entry.i386 >/dev/null 2>&1
}

# --- A: thunk evidence ON (the fix) --------------------------------------
translate build/code_thunk_entry.on.x86_64 || fail "translate (evidence on)"
GOT_ON="$(readback build/code_thunk_entry.on.x86_64)"
[ "$GOT_ON" != "NOTFOUND" ] || fail "sentinel slot not found in translated output (evidence on)"

# --- B: thunk evidence OFF (the regression this guards) ------------------
M64_NO_THUNK_ENTRY_EVIDENCE=1 translate build/code_thunk_entry.off.x86_64 \
   || fail "translate (evidence off)"
GOT_OFF="$(readback build/code_thunk_entry.off.x86_64)"
[ "$GOT_OFF" != "NOTFOUND" ] || fail "sentinel slot not found in translated output (evidence off)"

if [ "$GOT_ON" = "$TARGET" ]; then
   printf 'FAIL code-thunk-entry (vtable slot -> adjustor thunk 0x%x was DEMOTED to a\n' "$TARGET"
   echo   "     constant: the ENTRY gate does not recognise the thunk entry shape)"
   exit 1
fi
if [ "$GOT_OFF" != "$TARGET" ]; then
   echo "FAIL code-thunk-entry (guard is inert: the slot is relocated even with"
   echo "     M64_NO_THUNK_ENTRY_EVIDENCE=1, so it does not exercise the shape)"
   exit 1
fi

printf 'PASS code-thunk-entry (thunk slot 0x%x relocated to 0x%x; demoted to the raw 0x%x without the shape)\n' \
   "$TARGET" "$GOT_ON" "$GOT_OFF"
exit 0
