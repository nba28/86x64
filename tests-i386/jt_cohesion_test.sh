#!/bin/bash
#
# jt_cohesion_test.sh — A/B guard for the JUMP-TABLE COHESION gate
# (ParseEnv::code_alias_run_contradicted, src/core/parse.cc).
#
# THE BLIND SPOT. The function-entry gate (code_alias_is_constant) and the
# code-entry gate (code_alias_lacks_entry_evidence) are DELIBERATELY disarmed
# for __TEXT-resident data: switch jump tables live in __TEXT,__const and target
# MID-function basic-block heads carrying neither an nlist nor a `55 89 e5`
# prologue, so demanding entry evidence there would reject every real switch
# table. That leaves only code_interior_alias, which fires only for a value
# strictly INSIDE a decoded instruction. A u32 INTEGER aliasing an exact
# instruction BOUNDARY therefore passes every gate and is silently rebased.
#
# MEASURED (Halo CE #45): the audio engine's sample-rate table {22050, 44100} in
# __TEXT,__const became {0x1000573c, 44100}. The mixer computes its ring size as
# channels * rateTable[idx] * 6 (2*44100*6 = 529200, exactly the DirectSound
# ring measured in two independent runs), so the corrupted entry overflows that
# arithmetic, IDirectSoundBuffer::Lock returns E_INVALIDARG and the mixer takes
# its silent-skip branch -- 7432 consecutive buffers of silence with a healthy
# play cursor. ★The #35 record-field gate does not cover this: that one
# discriminates (small,small) u16 PAIRS in one word; this is a u32 pair of two
# individually plausible values.
#
# ★THE GATE RUNS AT TRANSLATE TIME, so both arms must TRANSLATE, not merely run.
# (An earlier version of this script set the kill switch when executing the
# already-translated fixture, which is a no-op and made the OFF arm look clean.)
#
# ON  = gate armed: the table survives, the fixture exits 0, and the translated
#       image still contains the intact {22050, 44100} byte pair.
# OFF = M64_NO_JT_COHESION=1 during translation: 22050 is rebased into a
#       translated code address, the intact pair DISAPPEARS from the image and
#       the fixture exits 1. A clean OFF arm is a FAIL, not a pass.
#
# ★PRECONDITION ASSERTION: the test rests on 0x5622 being an instruction
# BOUNDARY and 0xAC44 being MID-instruction in the fixture's own __text. Those
# are properties of the linked layout, so they are re-derived from the
# disassembly every run -- a layout change FAILS LOUDLY instead of going inert.
set -u
cd "$(dirname "$0")"
ROOT=$(cd .. && pwd)

MACHO_TOOL="$ROOT/build/src/macho-tool/macho-tool"
LIBABICONV="$ROOT/build/src/abiconv/libabiconv.dylib"
LIBWRAPPER="$ROOT/build/src/86x64/libwrapper.a"
LIBINTERPOSE="$ROOT/build/src/86x64/libinterpose.dylib"
PIPELINE="$ROOT/src/86x64/86x64.sh"

I386=build/96_jt_cohesion.i386
if [ ! -f "$I386" ]; then
  echo "jt-cohesion: SKIP (build/96_jt_cohesion.i386 missing;"
  echo "                   run \`make jt-cohesion\` first)"
  exit 0
fi

fail=0

# --- precondition: the two constants really do alias code as claimed ---------
dis=$(otool -tV "$I386" 2>/dev/null)
bnd=$(printf '%s\n' "$dis"    | awk '$1=="00005622"{print "yes"; exit}')
ac44_starts=$(printf '%s\n' "$dis" | awk '$1=="0000ac44"{print "yes"; exit}')
ac43_starts=$(printf '%s\n' "$dis" | awk '$1=="0000ac43"{print "yes"; exit}')
if [ "$bnd" = "yes" ] && [ -z "$ac44_starts" ] && [ "$ac43_starts" = "yes" ]; then
  echo "  precondition:         0x5622 is an instruction BOUNDARY, 0xAC44 is"
  echo "                        MID-instruction (its insn starts at 0xAC43)  OK"
else
  echo "  precondition:         FAILED — the fixture layout moved, so the constants"
  echo "                        no longer alias code as required. Re-derive them;"
  echo "                        do NOT trust the arms below."
  fail=1
fi

# translate <output> [env...] — runs the real pipeline, same as the Makefile.
translate() {
  local out=$1; shift
  rm -f "$out" "$out.dylib"
  "$@" bash "$PIPELINE" -m "$MACHO_TOOL" -l "$LIBABICONV" -w "$LIBWRAPPER" \
       -i "$LIBINTERPOSE" -o "$out" "$I386" >/dev/null 2>&1
  chmod +x "$out" 2>/dev/null
}

# The intact pair, little-endian: 22050 = 0x00005622, 44100 = 0x0000AC44.
INTACT='2256000044ac0000'
pair_present() {   # <image> -> prints the count of intact pairs
  xxd -p "$1" 2>/dev/null | tr -d '\n' | grep -o "$INTACT" | wc -l | tr -d ' '
}

translate build/96_jt_cohesion.on.x86_64  env
translate build/96_jt_cohesion.off.x86_64 env M64_NO_JT_COHESION=1

for arm in on off; do
  if [ ! -x "build/96_jt_cohesion.$arm.x86_64" ]; then
    echo "  $arm arm:              translation FAILED"
    fail=1
  fi
done
[ "$fail" = 0 ] || { echo "jt-cohesion: FAIL"; exit 1; }

./build/96_jt_cohesion.on.x86_64  >/dev/null 2>&1; on_rc=$?
./build/96_jt_cohesion.off.x86_64 >/dev/null 2>&1; off_rc=$?
on_pairs=$(pair_present build/96_jt_cohesion.on.x86_64.dylib)
off_pairs=$(pair_present build/96_jt_cohesion.off.x86_64.dylib)

if [ "$on_rc" = 0 ] && [ "${on_pairs:-0}" -ge 1 ]; then
  echo "  ON  (gate armed):     {22050, 44100} intact in the translated image"
  echo "                        ($on_pairs occurrence(s)); fixture exit 0"
else
  case "$on_rc" in
    1) echo "  ON  (gate armed):     the BOUNDARY word (22050) was rebased anyway —"
       echo "                        the gate did not fire (pairs=$on_pairs)" ;;
    2) echo "  ON  (gate armed):     the INTERIOR word (44100) was rebased — that is"
       echo "                        the control and must never happen" ;;
    *) echo "  ON  (gate armed):     rc=$on_rc pairs=$on_pairs" ;;
  esac
  fail=1
fi

if [ "$off_rc" = 1 ] && [ "${off_pairs:-1}" -eq 0 ]; then
  echo "  OFF (kill switch):    reproduced the defect — the intact pair is GONE from"
  echo "                        the image (22050 rebased into a code address) and the"
  echo "                        fixture exits 1  OK"
elif [ "$off_rc" = 0 ]; then
  echo "  OFF (kill switch):    expected the disarmed gate to corrupt the table and it"
  echo "                        did NOT (rc=0, pairs=$off_pairs). This guard is not"
  echo "                        exercising code_alias_run_contradicted."
  fail=1
else
  echo "  OFF (kill switch):    rc=$off_rc pairs=$off_pairs — arms not comparable"
  fail=1
fi

if [ "$fail" = 0 ]; then
  echo "jt-cohesion: PASS"
else
  echo "jt-cohesion: FAIL"
fi
exit $fail
