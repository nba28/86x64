#!/bin/bash
# pic_anchor_post_ret_test.sh — A/B guard for src/99_pic_anchor_post_ret.s: a
# PIC store in a block placed after the function's ret keeps its anchor.
# ON  = exit 42. OFF = M64_NO_PIC_POST_RET_BODY=1 at translate time: the store
#       keeps its raw i386 displacement (a fault or exit 3).
set -u
cd "$(dirname "$0")"
ROOT=..
I386=build/99_pic_anchor_post_ret.i386
[ -f "$I386" ] || { echo "pic-anchor-post-ret: FAIL (no $I386)"; exit 1; }
for arm in on off; do
  e=""; [ $arm = off ] && e="M64_NO_PIC_POST_RET_BODY=1"
  rm -f build/99_pic_anchor_post_ret.$arm.x86_64*
  env $e bash $ROOT/src/86x64/86x64.sh -m $ROOT/build/src/macho-tool/macho-tool \
      -l $ROOT/build/src/abiconv/libabiconv.dylib -w $ROOT/build/src/86x64/libwrapper.a \
      -i $ROOT/build/src/86x64/libinterpose.dylib -o build/99_pic_anchor_post_ret.$arm.x86_64 "$I386" >/dev/null 2>&1
done
build/99_pic_anchor_post_ret.on.x86_64 >/dev/null 2>&1; on=$?
build/99_pic_anchor_post_ret.off.x86_64 >/dev/null 2>&1; off=$?
[ $on = 42 ] && [ $off != 42 ] && { echo "pic-anchor-post-ret: PASS (on=$on off=$off)"; exit 0; }
echo "pic-anchor-post-ret: FAIL (on=$on off=$off)"; exit 1
