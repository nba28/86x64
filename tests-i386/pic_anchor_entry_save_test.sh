#!/bin/bash
#
# pic_anchor_entry_save_test.sh — A/B guard for the PIC-anchor ENTRY-SAVE gate
# (src/core/section.cc, DetectPicAnchoredDisps step 2b).
#
# THE BLIND SPOT. DetectPicAnchoredDisps walks __text LINEARLY, not as a CFG.
# Step (3) already refuses to drop an anchor on a plain register write, because a
# function's epilogue lies lexically BEFORE every block reached by a branch taken
# earlier. Step (2b)'s frame-slot tracker had no such exemption: any load from a
# non-anchor slot did anchors.erase(dst) — so the ordinary epilogue
# `movl -0xc(%ebp),%ebx` killed the live PIC anchor for the whole rest of the
# function. branch_anchor_snap repairs blocks reached by a DIRECT forward branch,
# but it is keyed on branch DISPLACEMENTS and can never see a block reached only
# through an INDIRECT jump-table dispatch.
#
# MEASURED on Civ IV's Python 2.6 (task #45), i386 function @0x96fd1, anchor
# 0x96fe5: the case body at 0x97100 kept its raw i386 displacement, so at runtime
# translated_anchor + 0x51033 landed 0x1d87 inside __TEXT,__cstring and the
# following `movl (%eax),%eax` dereferenced the ASCII "e AS" -> SIGSEGV at
# 0x53412065. Four anchored refs were lost in that one function (three char*
# `lea`s into __cstring plus the __nl_symbol_ptr GOT load).
#
# ★THE GATE RUNS AT TRANSLATE TIME, so this script TRANSLATES both arms itself.
# A/B'ing the env var while merely RUNNING an already-translated fixture is a
# no-op and would make this guard inert.
#
# ON  = gate armed: the case body's anchored load is rewritten rip-relative, it
#       reads _magic, the fixture exits 0.
# OFF = M64_NO_PIC_ANCHOR_ENTRY_SAVE=1 during translation: the load keeps its
#       i386 displacement, reads the __text nop padding, the fixture exits 7
#       (or faults). A clean OFF arm is a FAIL, not a pass.
set -u
cd "$(dirname "$0")"
ROOT=$(cd .. && pwd)

MACHO_TOOL="$ROOT/build/src/macho-tool/macho-tool"
LIBABICONV="$ROOT/build/src/abiconv/libabiconv.dylib"
LIBWRAPPER="$ROOT/build/src/86x64/libwrapper.a"
LIBINTERPOSE="$ROOT/build/src/86x64/libinterpose.dylib"
PIPELINE="$ROOT/src/86x64/86x64.sh"

I386=build/99_pic_anchor_entry_save.i386
if [ ! -f "$I386" ] || [ ! -f build/libabiconv.dylib ]; then
  echo "pic-anchor-entry-save: SKIP (build/99_pic_anchor_entry_save.i386 or"
  echo "                             build/libabiconv.dylib missing; run"
  echo "                             \`make pic-anchor-entry-save\` first)"
  exit 0
fi

fail=0
dis=$(otool -tV "$I386" 2>/dev/null)

# --- preconditions: the fixture really has all THREE ingredients -------------
# Re-derived from the linked layout every run, so a layout change fails loudly
# instead of leaving the guard exercising nothing.
#
#   (i)   an entry save `movl %ebx,-0xc(%ebp)` BEFORE the `popl %ebx` anchor,
#   (ii)  an epilogue restore `movl -0xc(%ebp),%ebx` AFTER it,
#   (iii) the anchored load UNDER TEST after the restore, reached only by a
#         BACKWARD branch (no snapshot; see the fixture header).
# ⚠ strtonum() is a GAWK extension: macOS ships BSD awk, which answers
# "calling undefined function strtonum" and yields an EMPTY address for every
# probe below — so the precondition block failed unconditionally and this guard
# could never render a verdict on the machine it ships to. awk therefore only
# MATCHES (its regex engine does handle \t); bash does the hex conversion.
addr_of() {   # first matching instruction's address, DECIMAL; empty if no match
   local hex
   hex=$(printf '%s\n' "$dis" | awk "$1"' {print $1; exit}')
   [ -n "$hex" ] && printf '%d' "$((16#$hex))"
}
save_at=$(addr_of '/movl[ \t]+%ebx, -0xc\(%ebp\)/')
pop_at=$(addr_of  '/popl[ \t]+%ebx/')
rest_at=$(addr_of '/movl[ \t]+-0xc\(%ebp\), %ebx/')
# the anchored load under test: `movl <disp32>(%ebx), %eax`
load_at=$(addr_of '/movl[ \t]+0x[0-9a-f]+\(%ebx\), %eax/')
# exactly ONE branch may target it and it must be BACKWARD: only forward
# branches record anchor snapshots, so a forward one would rescue the anchor
# and the OFF arm would silently pass.
load_hex=$(printf '%x' "${load_at:-0}")
br_hex=$(printf '%s\n' "$dis" | awk -v t="0x$load_hex" '$2 ~ /^j/ && $3 == t {print $1}')
br_n=$(printf '%s' "$br_hex" | grep -c .)
br_at=$([ "$br_n" = 1 ] && printf '%d' "$((16#$br_hex))")

if [ -n "$save_at" ] && [ -n "$pop_at" ] && [ -n "$rest_at" ] && \
   [ -n "$load_at" ] && [ -n "$br_at" ] && \
   [ "$save_at" -lt "$pop_at" ] && [ "$pop_at" -lt "$rest_at" ] && \
   [ "$rest_at" -lt "$load_at" ] && [ "$load_at" -lt "$br_at" ]; then
  echo "  precondition:         entry save 0x$(printf '%x' $save_at) < anchor pop"
  echo "                        0x$(printf '%x' $pop_at) < epilogue restore 0x$(printf '%x' $rest_at) <"
  echo "                        anchored load 0x$load_hex, reached only by the"
  echo "                        backward branch at 0x$br_hex  OK"
else
  echo "  precondition:         FAILED — the fixture layout moved, so the three"
  echo "                        ingredients are no longer ordered as required."
  echo "                        (save=${save_at:-?} pop=${pop_at:-?} restore=${rest_at:-?}"
  echo "                         load=${load_at:-?} branches-to-load=$br_n at ${br_hex:-?})"
  echo "                        Re-derive them; do NOT trust the arms below."
  fail=1
fi

# translate <output> [env...] — the real pipeline, same as the Makefile rule.
translate() {
  local out=$1; shift
  rm -f "$out" "$out.dylib"
  "$@" bash "$PIPELINE" -m "$MACHO_TOOL" -l "$LIBABICONV" -w "$LIBWRAPPER" \
       -i "$LIBINTERPOSE" -o "$out" "$I386" >/dev/null 2>&1
  chmod +x "$out" 2>/dev/null
}

# Both arms disarm the back-edge join (M64_NO_PIC_BACK_EDGE): it ALSO recovers
# this backward-reached anchor, and would make the OFF arm pass on its own.
translate build/99_pic_anchor_entry_save.on.x86_64  env M64_NO_PIC_BACK_EDGE=1
translate build/99_pic_anchor_entry_save.off.x86_64 env M64_NO_PIC_BACK_EDGE=1 M64_NO_PIC_ANCHOR_ENTRY_SAVE=1

for arm in on off; do
  if [ ! -x "build/99_pic_anchor_entry_save.$arm.x86_64" ]; then
    echo "  $arm arm:              translation FAILED"
    fail=1
  fi
done
[ "$fail" = 0 ] || { echo "pic-anchor-entry-save: FAIL"; exit 1; }

./build/99_pic_anchor_entry_save.on.x86_64  >/dev/null 2>&1; on_rc=$?
./build/99_pic_anchor_entry_save.off.x86_64 >/dev/null 2>&1; off_rc=$?

if [ "$on_rc" = 0 ]; then
  echo "  ON  (gate armed):     the backward-reached block's anchored load is"
  echo "                        rip-relative and reads _magic; exit 0  OK"
else
  case "$on_rc" in
    7) echo "  ON  (gate armed):     the load read the __text padding, not _magic — the"
       echo "                        anchor was still erased at the epilogue restore" ;;
    *) echo "  ON  (gate armed):     rc=$on_rc (99 = the dead early exit ran;"
       echo "                        139 = the stale displacement faulted)" ;;
  esac
  fail=1
fi

if [ "$off_rc" = 7 ]; then
  echo "  OFF (kill switch):    reproduced the defect — the load kept its i386"
  echo "                        displacement and read the __text nop padding"
  echo "                        instead of _magic; exit 7  OK"
elif [ "$off_rc" = 0 ]; then
  echo "  OFF (kill switch):    expected the disarmed gate to lose the anchor and it"
  echo "                        did NOT (rc=0). This guard is not exercising the"
  echo "                        entry-save exemption."
  fail=1
else
  echo "  OFF (kill switch):    rc=$off_rc — the defect reproduced, but not with the"
  echo "                        deterministic padding read (a fault is still a"
  echo "                        reproduction; check the fixture's __text padding)"
fi

if [ "$fail" = 0 ]; then
  echo "pic-anchor-entry-save: PASS"
else
  echo "pic-anchor-entry-save: FAIL"
fi
exit $fail
