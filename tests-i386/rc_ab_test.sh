#!/bin/bash
# rc_ab_test.sh NAME FIXTURE "OFF_ENV" — A/B guard for a translate-time rule
# whose fixture exits 42 when translated right. ON must exit 42; OFF (the kill
# switches in OFF_ENV, set while translating) must not — a translation that
# fails outright counts as the defect reproducing.
set -u
cd "$(dirname "$0")"
NAME=$1; FIX=$2; OFFENV=$3
ROOT=..
I386=build/$FIX.i386
[ -f "$I386" ] || { echo "$NAME: FAIL ($I386 missing)"; exit 1; }
for arm in on off; do
  out=build/$FIX.$arm.x86_64
  rm -f "$out" "$out.dylib"
  ENVV=""; [ "$arm" = off ] && ENVV="$OFFENV"
  env $ENVV bash $ROOT/src/86x64/86x64.sh -m $ROOT/build/src/macho-tool/macho-tool \
      -l $ROOT/build/src/abiconv/libabiconv.dylib -w $ROOT/build/src/86x64/libwrapper.a \
      -i $ROOT/build/src/86x64/libinterpose.dylib -o "$out" "$I386" >/dev/null 2>&1
  if [ -x "$out" ]; then "$out" >/dev/null 2>&1; else (exit 99); fi
  eval "r_$arm=$?"
done
if [ "$r_on" = 42 ] && [ "$r_off" != 42 ]; then
  echo "$NAME: PASS (on=$r_on off=$r_off)"
else
  echo "$NAME: FAIL (on=$r_on off=$r_off; want 42 / not 42)"; exit 1
fi
