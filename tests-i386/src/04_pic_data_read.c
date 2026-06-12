/*
 * 04_pic_data_read — minimal Tessera-shaped PIC anchor test.
 *
 * Written in C (not asm) because real i386 PIC code relies on Mach-O
 * scattered SECTDIFF/PAIR relocations to express `[anchor + (sym - anchor)]`
 * disps cross-section. nasm's macho32 backend doesn't emit scattered
 * SECTDIFF relocs — it emits plain pcrel relocs that the linker
 * miscomputes for `(sym - .anchor)`. clang's i386 codegen does emit
 * SECTDIFF/PAIR, so a C source faithfully reproduces the Tessera-shaped
 * pattern that the translator must handle.
 *
 * Compiled to i386 PIC by clang, _main looks like:
 *
 *   call    .Lpic_anchor
 *  .Lpic_anchor:
 *   pop     %edi                  ; edi = .Lpic_anchor vmaddr
 *   mov     <offset>(%edi), %esi  ; load codes[0]
 *   mov     <offset+4>(%edi), %edx; load codes[1]
 *   ...
 *
 * The translator must detect the call+pop anchor pattern and rewrite
 * each subsequent `[edi+disp]` load as rip-relative. Without the fix,
 * the post-translation disp is the pre-translation disp, which points
 * to a wrong address in the M64 layout — entry 0 would read into
 * __cstring (or similar) and give garbage.
 *
 * Validation via exit code rather than printf to dodge an unrelated
 * libabiconv bug in 4-arg %x varargs marshalling (arg 2 of a 4-arg %x
 * call gets garbled even when source is pure stack-store constants).
 * Sum 1+2+4+8 = 15. If any PIC load is wrong, sum changes.
 */
extern void exit(int status);

static const unsigned int codes[4] = {1U, 2U, 4U, 8U};

int main(void) {
   int sum = (int)(codes[0] + codes[1] + codes[2] + codes[3]);
   exit(sum);
   return 0;
}
