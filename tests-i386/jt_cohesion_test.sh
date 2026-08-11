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
# strictly INSIDE a decoded instruction. A u32 INTEGER that aliases an exact
# instruction BOUNDARY therefore passes every gate and is silently rebased.
#
# MEASURED (Halo CE #45): the sample-rate table {22050, 44100} in __TEXT,__const
# became {0x1000573c, 44100}. The mixer computes its ring size as
# channels * rateTable[idx] * 6 (2*44100*6 = 529200, exactly the measured
# DirectSound ring), so the corrupted entry overflows that arithmetic,
# IDirectSoundBuffer::Lock returns E_INVALIDARG, and the mixer takes its
# silent-skip branch -- 7432 consecutive buffers of silence with a healthy play
# cursor. ★The #35 record-field gate does not cover it: that one discriminates
# (small,small) u16 PAIRS in one word; this is a u32 pair of individually
# plausible values.
#
# THE FIX: a jump table is a RUN of valid code targets, an integer table is not.
# An immediate neighbour that itself aliases code at a NON-boundary proves the
# run is not a table. Conservative: demote only when a neighbour CONTRADICTS and
# neither neighbour CORROBORATES, so a real switch table next to a stray integer
# keeps its other neighbour and survives.
#
# ON  = the gate is armed: both table words survive translation, exit 0.
# OFF = M64_NO_JT_COHESION=1: 22050 is rebased into a translated code address,
#       the fixture's compare fails and it exits 1. A clean OFF arm means this
#       guard is NOT exercising the gate -- that is a FAIL, not a pass.
#
# ★PRECONDITION ASSERTION. The whole test rests on 0x5622 being an instruction
# BOUNDARY and 0xAC44 being MID-instruction in the fixture's own __text. Those
# are properties of the linked layout, so the script re-derives them from the
# disassembly every run: if the layout ever shifts, the guard FAILS LOUDLY
# instead of going quietly inert.
set -u
cd "$(dirname "$0")"

BIN=build/96_jt_cohesion.x86_64
I386=build/96_jt_cohesion.i386
if [ ! -x "$BIN" ]; then
  echo "jt-cohesion: SKIP (build/96_jt_cohesion.x86_64 missing;"
  echo "                   run \`make jt-cohesion\` first)"
  exit 0
fi

fail=0

# --- precondition: the two constants really do alias code the way we claim ----
if [ -r "$I386" ]; then
  dis=$(otool -tV "$I386" 2>/dev/null)
  bnd=$(printf '%s\n' "$dis" | awk '$1=="00005622"{print "yes"; exit}')
  # 0xAC44 is mid-instruction iff NO instruction starts there but one starts at 0xAC43
  mid_no=$(printf '%s\n' "$dis" | awk '$1=="0000ac44"{print "starts"; exit}')
  mid_yes=$(printf '%s\n' "$dis" | awk '$1=="0000ac43"{print "yes"; exit}')
  if [ "$bnd" = "yes" ] && [ -z "$mid_no" ] && [ "$mid_yes" = "yes" ]; then
    echo "  precondition:         0x5622 is an instruction BOUNDARY and 0xAC44 is"
    echo "                        MID-instruction (starts at 0xAC43)  OK"
  else
    echo "  precondition:         FAILED — the fixture's layout moved, so the two"
    echo "                        constants no longer alias code as required."
    echo "                        boundary(0x5622)=${bnd:-no} starts(0xAC44)=${mid_no:-no}"
    echo "                        starts(0xAC43)=${mid_yes:-no}"
    echo "                        Re-derive the values; do NOT trust the arms below."
    fail=1
  fi
else
  echo "  precondition:         SKIP (no $I386 to disassemble)"
fi

run() { "$@" "$BIN" >/dev/null 2>&1; echo $?; }

on_rc=$(run env)
off_rc=$(run env M64_NO_JT_COHESION=1)

case "$on_rc" in
  0) echo "  ON  (gate armed):     both __TEXT,__const words survived translation" ;;
  1) echo "  ON  (gate armed):     the BOUNDARY word (22050) was rebased — the gate"
     echo "                        did not fire"; fail=1 ;;
  2) echo "  ON  (gate armed):     the INTERIOR word (44100) was rebased — that is the"
     echo "                        control and should never happen"; fail=1 ;;
  *) echo "  ON  (gate armed):     unexpected rc=$on_rc"; fail=1 ;;
esac

case "$off_rc" in
  1) echo "  OFF (kill switch):    reproduced the defect — 22050 rebased into a"
     echo "                        translated code address (rc=1)  OK" ;;
  0) echo "  OFF (kill switch):    expected the disarmed gate to corrupt the table and"
     echo "                        it did NOT (rc=0). This guard is not exercising"
     echo "                        code_alias_run_contradicted."; fail=1 ;;
  *) echo "  OFF (kill switch):    unexpected rc=$off_rc"; fail=1 ;;
esac

if [ "$fail" = 0 ]; then
  echo "jt-cohesion: PASS"
else
  echo "jt-cohesion: FAIL"
fi
exit $fail
