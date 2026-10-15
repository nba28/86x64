#!/bin/bash
# pic_anchor_xchg_test.sh — A/B guard for src/99_pic_anchor_xchg.s: the PIC
# anchor follows `xchg` (the cpuid save/restore idiom).
# ON  = exit 42. OFF = M64_NO_PIC_ANCHOR_XCHG=1 at translate time: the store
#       keeps its raw i386 displacement (a fault or exit 3).
set -u
cd "$(dirname "$0")"
ROOT=..
I386=build/99_pic_anchor_xchg.i386
[ -f "$I386" ] || { echo "pic-anchor-xchg: FAIL (no $I386)"; exit 1; }
for arm in on off; do
  e=""; [ $arm = off ] && e="M64_NO_PIC_ANCHOR_XCHG=1"
  rm -f build/99_pic_anchor_xchg.$arm.x86_64*
  env $e bash $ROOT/src/86x64/86x64.sh -m $ROOT/build/src/macho-tool/macho-tool \
      -l $ROOT/build/src/abiconv/libabiconv.dylib -w $ROOT/build/src/86x64/libwrapper.a \
      -i $ROOT/build/src/86x64/libinterpose.dylib -o build/99_pic_anchor_xchg.$arm.x86_64 "$I386" >/dev/null 2>&1
done
build/99_pic_anchor_xchg.on.x86_64 >/dev/null 2>&1; on=$?
build/99_pic_anchor_xchg.off.x86_64 >/dev/null 2>&1; off=$?
[ $on = 42 ] && [ $off != 42 ] && { echo "pic-anchor-xchg: PASS (on=$on off=$off)"; exit 0; }
echo "pic-anchor-xchg: FAIL (on=$on off=$off)"; exit 1
