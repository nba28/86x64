/*
 * 12_static_byte_flag — minimal repro for the movb $imm8,[abs32] mis-relocation bug.
 *
 * The i386 `movb $1, g_flag` compiles (with -fno-pic) to absolute addressing:
 *   c6 05 <abs32> 01
 * encoding: opcode c6/0 mem8 imm8, mod=00 r/m=101 (disp32 absolute).
 *
 * Without -fno-pic clang emits a PIC call+pop anchor; -fno-pic forces the
 * straight absolute encoding so the translator's `has_small_imm && dest_is_data`
 * branch (instruction.cc ~line 274) is exercised.
 *
 * The bug: resolve(md) for the disp32 resolves to the wrong SectionBlob —
 * reportedly a __TEXT blob instead of the __bss blob — so the translated
 * x86_64 instruction writes into read-only __TEXT → EXC_BAD_ACCESS.
 *
 * Expected: exit_code 0 (set_flag() writes 1 to g_flag, main returns g_flag?0:1).
 */
extern void exit(int status);

static volatile char g_flag;   /* __DATA,__bss / S_ZEROFILL */

__attribute__((noinline)) void set_flag(void) {
    g_flag = 1;   /* movb $1, <abs32>  =>  c6 05 <addr32> 01 */
}

int main(void) {
    set_flag();
    exit(g_flag ? 0 : 1);
    return 0;
}
