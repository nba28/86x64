#!/bin/bash
# zerofill_code_literal_test.sh — A/B guard: a __data pointer into zero-fill
# memory of a locals-stripped image must be relocated (see
# src/99_zerofill_code_literal.c). ON prints ok=1; OFF (M64_NO_ZF_CODE_LITERAL=1)
# leaves the raw i386 address in the slot and the store faults.
set -u
cd "$(dirname "$0")"
ROOT=..
I386=build/99_zerofill_code_literal.i386
fail=0
for arm in on off; do
  out=build/99_zerofill_code_literal.$arm.x86_64
  ENVV=""; [ "$arm" = off ] && ENVV="M64_NO_ZF_CODE_LITERAL=1"
  env $ENVV bash $ROOT/src/86x64/86x64.sh -m $ROOT/build/src/macho-tool/macho-tool \
      -l $ROOT/build/src/abiconv/libabiconv.dylib -w $ROOT/build/src/86x64/libwrapper.a \
      -i $ROOT/build/src/86x64/libinterpose.dylib -o "$out" "$I386" >/dev/null 2>&1
  r=$("$out" 2>/dev/null | head -1); r=${r%% *}
  if [ "$arm" = on ]; then
    [ "$r" = "ok=1" ] && echo "  ON : $r  OK" || { echo "  ON : expected ok=1, got '$r'"; fail=1; }
  else
    [ "$r" != "ok=1" ] && echo "  OFF: '$r' — the defect reproduces  OK" \
                       || { echo "  OFF: got ok=1 — guard not exercising the gate"; fail=1; }
  fi
done
[ $fail = 0 ] && echo "zerofill-code-literal: PASS" || echo "zerofill-code-literal: FAIL"
exit $fail
