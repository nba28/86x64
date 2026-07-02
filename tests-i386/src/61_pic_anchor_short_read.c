/* 61_pic_anchor_short_read.c — PIC regression for the anchor+disp mid-blob
 * SHORT/interior read skew AND the anchor-relative indirect call.
 *
 * Compiled PIC (the DEFAULT %.c rule — NO -fno-pic), clang reaches file-scope
 * globals through a get_pc-style anchor (`call $+0; pop %reg`) and
 * `disp(%anchor)` displacements. Two i386 idioms this test pins:
 *
 *  1. INTERIOR read of a multi-byte __DATA word: reading the HIGH short of a
 *     `{short,short}` global emits `movswl kSize+2(%anchor)`. That mid-blob
 *     target resolved through the PIC-anchored disp path, which — before the
 *     fix — lacked the `resolve_containing` containing-blob fallback the
 *     absolute-[disp32]/[rip+disp] paths have. Section::Parse2 then parked the
 *     placeholder BEFORE the next blob, so the emitted rip-relative disp skewed
 *     to the NEIGHBOURING datum (+2): `size 4 <garbage>` (it read _fp's bytes).
 *
 *  2. Anchor-relative indirect call: the static fn-ptr call `fp()` lowers to
 *     `call *disp(%anchor)` (CALL_NEAR_MEMv). The anchor register is a DEAD
 *     low-32 artifact in x86_64, so the reg-relative narrowing had to be
 *     rewritten rip-relative (drop the anchor base) — otherwise the load lands
 *     on a garbage address and the call jumps into hyperspace (SIGSEGV).
 *
 * The fix lives in the CORE translator: section.cc DetectPicAnchoredDisps adds
 * the writable-__DATA containing fallback (+ memdisp_offset), instruction.cc's
 * pic_anchored load/store transform propagates memdisp_offset, and the
 * CALL_NEAR_MEMv / JMP_MEMv reg-relative narrowing honours pic_anchored by
 * reaching the resolved blob rip-relative.
 *
 * Expected: "size 4 4" (both shorts, no skew) then "cb called".
 */
#include <stdio.h>
#include <stdlib.h>

static void cb(void) { puts("cb called"); }

static void (*fp)(void) = cb;                    /* anchored indirect call    */
static struct { short w, h; } kSize = { 4, 4 };  /* mid-blob interior short read */

int main(void) {
   printf("size %d %d\n", kSize.w, kSize.h);
   fp();
   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
