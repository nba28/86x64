#!/bin/bash
#
# Regression test for the FLAG-COLUMN rule (core: section.cc DataParser +
# ParseEnv::flag_column_power_of_two; Call of Duty 4's "separate alpha blend,
# glow will be disabled"). A single-bit caps mask that equals a function entry
# in a stripped, non-PIE, reloc-less executable must stay a constant when its
# record column holds single-bit flags.
#
# A/B (M64_NO_FLAG_COLUMN=1 disarms the rule):
#   ON  -> the mask word survives byte-identical (0x8000)
#   OFF -> the same word is relocated (proves the guard bites)
# Needs the SL i386 sysroot; SKIPs without it.
. "$(dirname "$0")/../src/86x64/paths.sh"   # M64_* local paths
LD="${LD:-$M64_I386_LD}"; [ -x "$LD" ] || LD=ld

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
   echo "SKIP flag-column-entry (no i386 sysroot at $SYSROOT; run 'make sysroot')"
   exit 0
fi
for f in "$MT" "$LIBABICONV" "$LIBWRAPPER" "$LIBINTERPOSE"; do
   [ -e "$f" ] || { echo "SKIP flag-column-entry (missing $f; build first)"; exit 0; }
done

mkdir -p build
fail() { echo "FAIL flag-column-entry ($1)"; exit 1; }

clang -arch i386 -isysroot "$SYSROOT" -mmacosx-version-min=10.6 \
   -c src/flag_column_entry.s -o build/flag_column_entry.o \
   2>/dev/null || fail assemble
# non-PIE MH_EXECUTE: the fixed-load-address, reloc-less shape that disarms
# every other pointer-detection discriminator.
"$LD" -arch i386 -macos_version_min 10.6 -no_pie -syslibroot "$SYSROOT" \
   -lSystem -e _main -o build/flag_column_entry.i386 \
   build/flag_column_entry.o 2>/dev/null || fail link

PROBE="$(nm build/flag_column_entry.i386 2>/dev/null | awk '$3=="_probe_fn"{print $1}')"
[ "$PROBE" = "00008000" ] || fail "_probe_fn at 0x$PROBE, not 0x8000"
TARGET=$(( 0x8000 ))

# Strip local symbols: no func_syms for _probe_fn, so code_alias_is_constant
# stays disarmed — the Civ IV shape, and the only shape this gate must cover.
strip -x build/flag_column_entry.i386 2>/dev/null || fail strip
if nm build/flag_column_entry.i386 2>/dev/null | grep -q " _probe_fn$"; then
   fail "_probe_fn still symboled after strip"
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
        lead = struct.pack('<I', 0xA5A53333); tail = struct.pack('<I', 0xA5A54444)
        i = d.find(lead)
        while i >= 0:
            if d[i+124:i+128] == tail:
                print(struct.unpack('<I', d[i+32:i+36])[0]); sys.exit(0)
            i = d.find(lead, i + 1)
print('NOTFOUND')
PY
}

translate() {   # $1 = output stem ; env already set by caller
   rm -f "$1" "$1.dylib"
   bash "$PIPELINE" -m "$MT" -l "$LIBABICONV" -w "$LIBWRAPPER" -i "$LIBINTERPOSE" \
      -o "$1" build/flag_column_entry.i386 >/dev/null 2>&1
}

# --- A: gate ON (the fix) -------------------------------------------------
translate build/flag_column_entry.on.x86_64 || fail "translate (gate on)"
GOT_ON="$(readback build/flag_column_entry.on.x86_64)"
[ "$GOT_ON" != "NOTFOUND" ] || fail "sentinel slot not found in translated output (gate on)"

# --- B: gate OFF (pre-fix behavior) --------------------------------------
M64_NO_FLAG_COLUMN=1 translate build/flag_column_entry.off.x86_64 \
   || fail "translate (gate off)"
GOT_OFF="$(readback build/flag_column_entry.off.x86_64)"
[ "$GOT_OFF" != "NOTFOUND" ] || fail "sentinel slot not found in translated output (gate off)"

if [ "$GOT_ON" != "$TARGET" ]; then
   printf 'FAIL flag-column-entry (flag 0x%x was relocated to 0x%x)\n' \
      "$TARGET" "$GOT_ON"
   exit 1
fi
if [ "$GOT_OFF" = "$TARGET" ]; then
   echo "FAIL flag-column-entry (guard is inert: the constant survives even with"
   echo "     M64_NO_FLAG_COLUMN=1, so it does not actually exercise the gate)"
   exit 1
fi

printf 'PASS flag-column-entry (0x%x preserved with the gate; rebased to 0x%x without)\n' \
   "$TARGET" "$GOT_OFF"
exit 0
