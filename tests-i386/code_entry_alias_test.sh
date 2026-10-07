#!/bin/bash
#
# Regression test for the CODE-ENTRY gate
# (core: section.cc DataParser + ParseEnv::code_alias_lacks_entry_evidence;
# Civ IV STEAM static-init SIGSEGV at slide_objc -> abiconv_call_init,
# 2026-07-26).
#
# A locals-stripped, reloc-less, fixed-address (-no_pie) i386 executable gives
# macho-tool no metadata for deciding which 4-byte __DATA words are pointers, so
# its in-range heuristic falsely "rebases" any constant that aliases __text.
# Civ IV's strtok delimiter string " ._" (= 0x005F2E20) was rewritten to a
# translated __text address whose bytes are "mT\xa9\x10"; strtok then split the
# registry path "Game" on the 'm', the lookup failed, and the NULL node was
# dereferenced at +0x88.
#
# The instruction-BOUNDARY gate (code_interior_alias) does NOT catch this:
# measured on the real binary, 0x005F2E20 is a genuine boundary — the
# `subl $0x18,%esp` three bytes into the function entered at 0x5F2E1D.
#
# The discriminator that does: a genuine DATA-resident code pointer targets a
# function ENTRY, and entry-ness survives a locals-strip as either an nlist at
# the value (globals survive `strip -x`) or the `55 89 e5` frame-setup prologue.
#
# A/B (the env kill-switch M64_NO_CODE_ENTRY_GATE=1 disarms the gate):
#   gate ON  -> the mid-function-aliasing __DATA word survives byte-identical
#   gate OFF -> the same word is rebased (proves the guard actually bites)
# Both directions are asserted, so the test cannot silently pass for the wrong
# reason. Needs the SL i386 sysroot; SKIPs without it (like the suite).
# i386 linking needs the Snow Leopard ld64-95 wrapper: modern ld dropped -arch i386
# (same resolution as the Makefile's LD). Override with LD=... in the environment.
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
   echo "SKIP code-entry-alias (no i386 sysroot at $SYSROOT; run 'make sysroot')"
   exit 0
fi
for f in "$MT" "$LIBABICONV" "$LIBWRAPPER" "$LIBINTERPOSE"; do
   [ -e "$f" ] || { echo "SKIP code-entry-alias (missing $f; build first)"; exit 0; }
done

mkdir -p build
fail() { echo "FAIL code-entry-alias ($1)"; exit 1; }

clang -arch i386 -isysroot "$SYSROOT" -mmacosx-version-min=10.6 \
   -c src/code_entry_alias.s -o build/code_entry_alias.o \
   2>/dev/null || fail assemble
# non-PIE MH_EXECUTE: the fixed-load-address, reloc-less shape that disarms
# every other pointer-detection discriminator.
"$LD" -arch i386 -macos_version_min 10.6 -no_pie -syslibroot "$SYSROOT" \
   -lSystem -e _main -o build/code_entry_alias.i386 \
   build/code_entry_alias.o 2>/dev/null || fail link

# _probe_fn opens with the `55 89 e5` prologue; +3 is the `subl $0x18,%esp`
# boundary — a REAL instruction start, but mid-function with no nlist and no
# prologue, i.e. exactly Civ IV's 0x005F2E20.
PROBE="$(nm build/code_entry_alias.i386 2>/dev/null | awk '$3=="_probe_fn"{print $1}')"
[ -n "$PROBE" ] || fail "could not read _probe_fn vmaddr"
TARGET=$(( 0x$PROBE + 3 ))
# The heuristic only considers values in [0x1000, 0x80000000); below that the
# fixture would prove nothing.
[ "$TARGET" -ge 4096 ] || fail "probe vmaddr $PROBE too low to exercise the heuristic"

# Patch the target address into the sentinel-bracketed __DATA slot as a plain
# integer literal (no relocation) — exactly Civ's shape.
python3 - "$TARGET" build/code_entry_alias.i386 <<'PY' || fail "patch"
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

# Strip local symbols: no func_syms for _probe_fn, so code_alias_is_constant
# stays disarmed — the Civ IV shape, and the only shape this gate must cover.
strip -x build/code_entry_alias.i386 2>/dev/null || fail strip
if nm build/code_entry_alias.i386 2>/dev/null | grep -q " _probe_fn$"; then
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
      -o "$1" build/code_entry_alias.i386 >/dev/null 2>&1
}

# --- A: gate ON (the fix) -------------------------------------------------
translate build/code_entry_alias.on.x86_64 || fail "translate (gate on)"
GOT_ON="$(readback build/code_entry_alias.on.x86_64)"
[ "$GOT_ON" != "NOTFOUND" ] || fail "sentinel slot not found in translated output (gate on)"

# --- B: gate OFF (pre-fix behavior) --------------------------------------
M64_NO_CODE_ENTRY_GATE=1 translate build/code_entry_alias.off.x86_64 \
   || fail "translate (gate off)"
GOT_OFF="$(readback build/code_entry_alias.off.x86_64)"
[ "$GOT_OFF" != "NOTFOUND" ] || fail "sentinel slot not found in translated output (gate off)"

if [ "$GOT_ON" != "$TARGET" ]; then
   printf 'FAIL code-entry-alias (mid-instruction constant 0x%x was rebased to 0x%x)\n' \
      "$TARGET" "$GOT_ON"
   exit 1
fi
if [ "$GOT_OFF" = "$TARGET" ]; then
   echo "FAIL code-entry-alias (guard is inert: the constant survives even with"
   echo "     M64_NO_CODE_ENTRY_GATE=1, so it does not actually exercise the gate)"
   exit 1
fi

printf 'PASS code-entry-alias (0x%x preserved with the gate; rebased to 0x%x without)\n' \
   "$TARGET" "$GOT_OFF"
exit 0
