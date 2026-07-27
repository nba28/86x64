#!/bin/bash
#
# Regression test for the CODE-ALIAS gate on BASE-REGISTER memory displacements
# (core: instruction.cc memdisp_code_alias_is_constant;
#  Civ IV STEAM "launch in window" SIGBUS at 0x10382008, 2026-07-28).
#
# For a fixed-load-address (-no_pie MH_EXECUTE) i386 image, instruction.cc reads
# a `[base + disp32]` displacement as an ABSOLUTE GLOBAL-TABLE vmaddr whenever
# disp32 lands inside a real segment, and relocates it to the translated layout.
# With a base register live, disp32 is far more often an ordinary INTEGER, and a
# large __text (Civ IV: 0x23b0 .. 0xdd2176) is aliased by ordinary small
# integers constantly.
#
# Civ IV's GMemory free-list builder computes its loop terminator with
#   lea 0x27ec(%ecx),%edx        (0x27ec == 0x2800 - 0x14 = chunk minus one node)
# which got relocated to `lea r11,[rip+..]; lea (%rcx,%r11),%edx`. The iterator
# never met the terminator, the node-zeroing loop ran off the end of the chunk
# and faulted writing the first non-writable page (KERN_PROTECTION_FAILURE,
# 8 bytes past a 1540K rwx region abutting a read-only file mapping).
#
# The fix reuses the IMMEDIATE family's classifier: a value aliasing an
# instructions-flagged section is an integer CONSTANT unless there is positive
# function-ENTRY evidence at it. Both polarities are asserted so the gate cannot
# pass by simply disarming the whole absolute-table heuristic:
#   POSITIVE  `leal <mid-__text>(%ecx),%edx` -> displacement PRESERVED
#   NEGATIVE  `movl %edx,_g_table(%ecx)`     -> STILL relocated (__DATA target)
#
# A/B via the kill-switch M64_NO_MEMDISP_CODE_ALIAS_GATE=1:
#   gate ON  -> the mid-__text-aliasing displacement survives verbatim
#   gate OFF -> the same displacement is rewritten to lea r11,[rip+..]
# Both directions are asserted, so the test cannot silently pass for the wrong
# reason. Needs the SL i386 sysroot; SKIPs without it (like the suite).
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
   echo "SKIP memdisp-code-alias (no i386 sysroot at $SYSROOT; run 'make sysroot')"
   exit 0
fi
for f in "$MT" "$LIBABICONV" "$LIBWRAPPER" "$LIBINTERPOSE"; do
   [ -e "$f" ] || { echo "SKIP memdisp-code-alias (missing $f; build first)"; exit 0; }
done

mkdir -p build
fail() { echo "FAIL memdisp-code-alias ($1)"; exit 1; }

clang -arch i386 -isysroot "$SYSROOT" -mmacosx-version-min=10.6 \
   -c src/memdisp_code_alias.s -o build/memdisp_code_alias.o \
   2>/dev/null || fail assemble
# non-PIE MH_EXECUTE: the fixed-load-address shape that arms the absolute-table
# heuristic in the first place.
ld -arch i386 -macos_version_min 10.6 -no_pie -syslibroot "$SYSROOT" \
   -lSystem -e _main -o build/memdisp_code_alias.i386 \
   build/memdisp_code_alias.o 2>/dev/null || fail link

# _probe_fn opens with `55 89 e5`; +3 is the `subl $0x18,%esp` boundary — a REAL
# instruction start, but mid-function with no nlist and no entry shape.
PROBE="$(nm build/memdisp_code_alias.i386 2>/dev/null | awk '$3=="_probe_fn"{print $1}')"
[ -n "$PROBE" ] || fail "could not read _probe_fn vmaddr"
TARGET=$(( 0x$PROBE + 3 ))
# The heuristic only considers values in [0x1000, 0x80000000).
[ "$TARGET" -ge 4096 ] || fail "probe vmaddr $PROBE too low to exercise the heuristic"

# _g_table is .globl, so it survives `strip -x` — the NEGATIVE control's target.
GTAB="$(nm build/memdisp_code_alias.i386 2>/dev/null | awk '$3=="_g_table"{print $1}')"
[ -n "$GTAB" ] || fail "could not read _g_table vmaddr"

# Patch the mid-__text address into the LEA's disp32 sentinel (0x11223344).
python3 - "$TARGET" build/memdisp_code_alias.i386 <<'PY' || fail "patch"
import sys, struct
val = int(sys.argv[1]); path = sys.argv[2]
d = bytearray(open(path, 'rb').read())
needle = bytes.fromhex('8d91') + struct.pack('<I', 0x11223344)   # lea disp32(%ecx),%edx
i = d.find(needle)
if i < 0:
    sys.exit("LEA sentinel disp32 not found in linked i386 binary")
if d.find(needle, i + 1) >= 0:
    sys.exit("LEA sentinel disp32 is not unique")
d[i+2:i+6] = struct.pack('<I', val)
open(path, 'wb').write(bytes(d))
PY

# Strip locals: no func_syms for _probe_fn, so code_alias_is_constant stays
# disarmed and the locals-stripped arm carries the classification — Civ's shape.
strip -x build/memdisp_code_alias.i386 2>/dev/null || fail strip
if nm build/memdisp_code_alias.i386 2>/dev/null | grep -q " _probe_fn$"; then
   fail "_probe_fn still symboled after strip"
fi

translate() {   # $1 = output stem ; env already set by caller
   rm -f "$1" "$1.dylib"
   bash "$PIPELINE" -m "$MT" -l "$LIBABICONV" -w "$LIBWRAPPER" -i "$LIBINTERPOSE" \
      -o "$1" build/memdisp_code_alias.i386 >/dev/null 2>&1
}

# Report whether the preserved-LEA byte pattern `8d 91 <disp32>` (optionally
# 0x67-prefixed) is present in the translated dylib, and whether the NEGATIVE
# control's store was rewritten to the relocated `movl %edx,(%rcx,%r11)` form.
probe() {   # $1 = stem, $2 = target disp ; prints "<lea_kept> <neg_relocated>"
   python3 - "$1.dylib" "$2" <<'PY'
import sys, struct, os
path, target = sys.argv[1], int(sys.argv[2])
if not os.path.exists(path):
    print("MISSING MISSING"); sys.exit(0)
d = open(path, 'rb').read()
disp = struct.pack('<I', target)
lea_kept = (d.find(bytes.fromhex('8d91') + disp) >= 0)
# relocated store form emitted by the transform: lea r11,[rip+..] then
# `movl %edx,(%rcx,%r11)` == 42 89 14 19
neg_reloc = (d.find(bytes.fromhex('42891419')) >= 0)
print(("KEPT" if lea_kept else "GONE"), ("RELOC" if neg_reloc else "RAW"))
PY
}

# --- A: gate ON (the fix) -------------------------------------------------
translate build/memdisp_code_alias.on.x86_64 || fail "translate (gate on)"
read -r LEA_ON NEG_ON <<<"$(probe build/memdisp_code_alias.on.x86_64 "$TARGET")"
[ "$LEA_ON" != "MISSING" ] || fail "no translated dylib (gate on)"

# --- B: gate OFF (pre-fix behavior) --------------------------------------
M64_NO_MEMDISP_CODE_ALIAS_GATE=1 translate build/memdisp_code_alias.off.x86_64 \
   || fail "translate (gate off)"
read -r LEA_OFF NEG_OFF <<<"$(probe build/memdisp_code_alias.off.x86_64 "$TARGET")"
[ "$LEA_OFF" != "MISSING" ] || fail "no translated dylib (gate off)"

if [ "$LEA_ON" != "KEPT" ]; then
   printf 'FAIL memdisp-code-alias (mid-__text constant 0x%x was relocated despite the gate)\n' \
      "$TARGET"
   exit 1
fi
if [ "$LEA_OFF" = "KEPT" ]; then
   echo "FAIL memdisp-code-alias (guard is inert: the displacement survives even with"
   echo "     M64_NO_MEMDISP_CODE_ALIAS_GATE=1, so it does not exercise the gate)"
   exit 1
fi
if [ "$NEG_ON" != "RELOC" ]; then
   echo "FAIL memdisp-code-alias (NEGATIVE control regressed: the genuine __DATA"
   echo "     table base _g_table@0x$GTAB is no longer relocated — the fix must not"
   echo "     disarm the absolute-table heuristic for data targets)"
   exit 1
fi

printf 'PASS memdisp-code-alias (0x%x preserved with the gate, relocated without; __DATA table base still relocated)\n' \
   "$TARGET"
exit 0
