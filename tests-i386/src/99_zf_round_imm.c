/* 99_zf_round_imm — a 64K-aligned imm32 that lands in symbol-free zero-fill,
 * where the code addresses no memory, is a constant (a size), not an address.
 *
 * Halo passes its game-state sizes 0x440000 / 0x480000 as stack arguments and
 * compares against them; both lie in its stripped __bss / __common, so they were
 * relocated and every checkpoint save/restore copied the wrong length.
 *
 * Linked with -x so __bss has no symbol. ON: ok=1. OFF (M64_NO_ZF_ROUND_IMM=1
 * at translate time): the sizes are relocated.
 */
extern int  printf(const char *, ...);
extern void exit(int);

static char big[0x100000];               /* __bss spans 0x90000 */

static int consume(int a, int b)
{
   /* compare halves: an imm32 0x90000 here would be relocated too */
   return (a >> 16) == 9 && (a & 0xffff) == 0 && (b >> 16) == 9 && (b & 0xffff) == 0;
}

int main(void)
{
   big[0] = 1;
   printf("ok=%d\n", consume(0x90000, 0x90000) && big[0] == 1);
   exit(0);
}
