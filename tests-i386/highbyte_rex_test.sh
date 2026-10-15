#!/bin/bash
# highbyte_rex_test.sh — A/B guard for src/99_highbyte_rex.s: a REX-lowered
# operand must not turn %ah/%ch/%dh/%bh into %spl/%bpl/%sil/%dil.
# ON  = exit 42. OFF = M64_NO_HIGHBYTE_REX=1 at translate time: a register dies.
set -u
cd "$(dirname "$0")"
ROOT=..
I386=build/99_highbyte_rex.i386
[ -f "$I386" ] || { echo "highbyte-rex: FAIL (no $I386)"; exit 1; }
for arm in on off; do
  e=""; [ $arm = off ] && e="M64_NO_HIGHBYTE_REX=1"
  rm -f build/99_highbyte_rex.$arm.x86_64*
  env $e bash $ROOT/src/86x64/86x64.sh -m $ROOT/build/src/macho-tool/macho-tool \
      -l $ROOT/build/src/abiconv/libabiconv.dylib -w $ROOT/build/src/86x64/libwrapper.a \
      -i $ROOT/build/src/86x64/libinterpose.dylib -o build/99_highbyte_rex.$arm.x86_64 "$I386" >/dev/null 2>&1
done
build/99_highbyte_rex.on.x86_64 >/dev/null 2>&1; on=$?
build/99_highbyte_rex.off.x86_64 >/dev/null 2>&1; off=$?
[ $on = 42 ] && [ $off != 42 ] && { echo "highbyte-rex: PASS (on=$on off=$off)"; exit 0; }
echo "highbyte-rex: FAIL (on=$on off=$off)"; exit 1
