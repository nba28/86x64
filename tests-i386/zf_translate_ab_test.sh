#!/bin/bash
# zf_translate_ab_test.sh NAME FIXTURE "OFF_ENV" — A/B guard for a translate-time
# pointer rule: the fixture prints ok=1 when its data pointers were relocated.
# ON must print ok=1; OFF (the kill switches in OFF_ENV, set while translating)
# must not.
set -u
cd "$(dirname "$0")"
NAME=$1; FIX=$2; OFFENV=$3
ROOT=..
I386=build/$FIX.i386
fail=0
for arm in on off; do
  out=build/$FIX.$arm.x86_64
  ENVV=""; [ "$arm" = off ] && ENVV="$OFFENV"
  env $ENVV bash $ROOT/src/86x64/86x64.sh -m $ROOT/build/src/macho-tool/macho-tool \
      -l $ROOT/build/src/abiconv/libabiconv.dylib -w $ROOT/build/src/86x64/libwrapper.a \
      -i $ROOT/build/src/86x64/libinterpose.dylib -o "$out" "$I386" >/dev/null 2>&1
  r=$("$out" 2>/dev/null | head -1); r=${r%% *}
  if [ "$arm" = on ]; then
    [ "$r" = "ok=1" ] && echo "  ON : $r  OK" || { echo "  ON : expected ok=1, got '$r'"; fail=1; }
  else
    [ "$r" != "ok=1" ] && echo "  OFF: '$r' — the defect reproduces  OK" \
                       || { echo "  OFF: got ok=1 — guard not exercising the rule"; fail=1; }
  fi
done
[ $fail = 0 ] && echo "$NAME: PASS" || echo "$NAME: FAIL"
exit $fail
