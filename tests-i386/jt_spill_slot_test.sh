#!/bin/bash
# jt_spill_slot_test.sh — A/B guard: a PIC relative-offset switch jump table whose
# TABLE BASE IS SPILLED TO A FRAME SLOT must still be recognised and relocated.
#
# Under register pressure GCC computes the PIC table base once, parks it in a
# local (`mov %eax,-0x14(%ebp)`) and reloads it into a different register at the
# dispatch (`mov %edi,-0x14(%ebp); mov %eax,(%edi,%eax,4); add %eax,%ebx; jmp *%eax`).
# DetectJumpTables tracked the base only through REGISTERS, lost it across the
# spill, and never recognised the dispatch — so the table kept its stale i386
# relative offsets (the translation makes the code LONGER, so every one is wrong)
# and the linear sweep additionally disassembled the inline table AS CODE.
#
# Real-world: Civ IV "Init Python". Python 2.6's marshal r_object switch (126
# entries) is this shape; the mangled entry produced `anchor + 0x02530000` —
# an address with the anchor's low half intact and its high half wrong — and the
# jump to unmapped memory was the SIGBUS. 27 of Python's 99 PIC switch dispatches
# use the spill form.
#
# The fixture carries TWO dispatches so a green ON arm cannot mean "nothing changed":
#   CONTROL — base kept in a register throughout. Detected before this fix, so its
#             table must be relocated IDENTICALLY in BOTH arms.
#   SPILL   — base parked in a frame slot. Relocated only when the detector
#             follows it through the slot.
#
# ON  : both dispatches land on their intended case; binary exits 0.
# OFF : M64_NO_JT_SPILL_SLOTS=1 — the SPILL table keeps its stale i386 deltas and
#       the dispatch lands somewhere else (the defect reproduces).
set -u
cd "$(dirname "$0")"
ROOT=..
BUILD=build
NAME=99_jt_spill_slot
I386=$BUILD/$NAME.i386
MT=$ROOT/build/src/macho-tool/macho-tool

if [ ! -x "$MT" ]; then echo "jt-spill-slot: SKIP (no macho-tool)"; exit 0; fi
if [ ! -f "$I386" ]; then echo "jt-spill-slot: SKIP (no $I386 — sysroot missing?)"; exit 0; fi

fail=0

# ---------------------------------------------------------------------------
# Part 1 (always runs): does the DETECTOR see the spilled table?
# `transform` alone needs no libabiconv, so this half works even without a
# full runtime build.
# ---------------------------------------------------------------------------
det() { # $1 = extra env ; prints the number of tables detected
  env $1 MACHO_TRACE_JUMPTABLE=1 "$MT" transform "$I386" "$BUILD/$NAME.det.out" 2>&1 \
    | grep -c '^\[jumptable\] dispatch@'
}
n_on=$(det "")
n_off=$(env M64_NO_JT_SPILL_SLOTS=1 MACHO_TRACE_JUMPTABLE=1 "$MT" transform "$I386" \
        "$BUILD/$NAME.det.off.out" 2>&1 | grep -c '^\[jumptable\] dispatch@')

if [ "$n_on" = 2 ]; then
  echo "  ON  detector:       CONTROL + SPILL tables both detected (2)             OK"
else
  echo "  ON  detector:       expected 2 tables, got $n_on"; fail=1
fi
if [ "$n_off" = 1 ]; then
  echo "  OFF detector:       only CONTROL detected (1) — spilled table missed     OK"
else
  echo "  OFF detector:       expected 1 table (defect), got $n_off"; fail=1
fi

# ---------------------------------------------------------------------------
# Part 2: are the emitted table ENTRIES actually relocated?
# The CONTROL table must be byte-identical between arms; the SPILL table must
# differ (ON = relocated for the longer x86_64 code, OFF = stale i386 deltas).
# ---------------------------------------------------------------------------
tables() { # $1 = binary ; prints "<control entries>|<spill entries>"
  python3 - "$1" <<'PY'
import sys, struct, subprocess, re
d = open(sys.argv[1], 'rb').read()
# Locate each dispatch by its own bytes: `add %ebx,%eax ; jmp *%rax` (01 d8 ff e0).
# The inline table starts immediately after. Order in the fixture: CONTROL, SPILL.
outs = []
for m in re.finditer(rb'\x01\xd8\xff\xe0', d):
    t = m.start() + 4
    outs.append(','.join('0x%x' % struct.unpack_from('<I', d, t + 4*i)[0] for i in range(4)))
print('|'.join(outs[:2]) if len(outs) >= 2 else 'MISSING')
PY
}
t_on=$(tables "$BUILD/$NAME.det.out")
t_off=$(tables "$BUILD/$NAME.det.off.out")
c_on=${t_on%%|*}; s_on=${t_on##*|}
c_off=${t_off%%|*}; s_off=${t_off##*|}

if [ "$t_on" = MISSING ] || [ "$t_off" = MISSING ]; then
  echo "  TABLES:             could not locate both dispatches in the output"; fail=1
else
  if [ "$c_on" = "$c_off" ]; then
    echo "  CONTROL table:      identical in both arms ($c_on)          OK"
  else
    echo "  CONTROL table:      MUST match across arms; on=$c_on off=$c_off"; fail=1
  fi
  if [ "$s_on" != "$s_off" ]; then
    echo "  SPILL table:        ON relocated ($s_on) vs OFF stale ($s_off)  OK"
  else
    echo "  SPILL table:        expected the arms to DIFFER; both = $s_on"
    echo "                      The guard is not exercising the kill switch."; fail=1
  fi
fi

# ---------------------------------------------------------------------------
# Part 3 (needs the runtime): translate fully and RUN both arms.
# ON must exit 0. OFF must not — the stale delta lands mid-instruction.
# ---------------------------------------------------------------------------
LIBABICONV=$ROOT/build/src/abiconv/libabiconv.dylib
if [ ! -f "$LIBABICONV" ]; then
  echo "  RUNTIME:            SKIP (no libabiconv build)"
else
  # the translated binaries load libabiconv via @loader_path, so stage it (and
  # its nulljump sidecar) next to them exactly as the Makefile does
  cp "$LIBABICONV" "$BUILD/libabiconv.dylib" 2>/dev/null
  cp "$(dirname "$LIBABICONV")/libabiconv.nulljump" "$BUILD/" 2>/dev/null
  for arm in on off; do
    ENVV=""; [ "$arm" = off ] && ENVV="M64_NO_JT_SPILL_SLOTS=1"
    rm -f "$BUILD/$NAME.$arm.x86_64"
    env $ENVV bash $ROOT/src/86x64/86x64.sh -m "$MT" \
        -l "$LIBABICONV" \
        -w $ROOT/build/src/86x64/libwrapper.a \
        -i $ROOT/build/src/86x64/libinterpose.dylib \
        -o "$BUILD/$NAME.$arm.x86_64" "$I386" >/dev/null 2>&1
  done
  if [ -x "$BUILD/$NAME.on.x86_64" ]; then
    "$BUILD/$NAME.on.x86_64" >/dev/null 2>&1; rc_on=$?
  else rc_on=NOBUILD; fi
  if [ -x "$BUILD/$NAME.off.x86_64" ]; then
    "$BUILD/$NAME.off.x86_64" >/dev/null 2>&1; rc_off=$?
  else rc_off=NOBUILD; fi

  if [ "$rc_on" = 0 ]; then
    echo "  ON  run:            both dispatches took the intended case (exit 0)    OK"
  else
    echo "  ON  run:            expected exit 0, got $rc_on"
    echo "                      (7 = CONTROL wrong case, 8 = SPILL wrong case)"; fail=1
  fi
  if [ "$rc_off" != 0 ] && [ "$rc_off" != NOBUILD ]; then
    echo "  OFF run:            spilled dispatch went astray (exit $rc_off)        OK"
  else
    echo "  OFF run:            expected a non-zero exit; got $rc_off."
    echo "                      The kill switch is not reproducing the defect."; fail=1
  fi
fi

[ "$fail" = 0 ] && echo "jt-spill-slot: PASS" || echo "jt-spill-slot: FAIL"
exit $fail
