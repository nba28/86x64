/* 99_zf_mask_imm — an imm32 that lands in symbol-free zero-fill but is also a
 * test/and/or mask in the code is a constant, not an address.
 *
 * Halo passes a 4 MB size as `movl $0x400000, 4(%esp)`; 0x400000 lies in its
 * stripped __bss, so it was relocated and a 4 MB memcpy ran ~100 MB at
 * campaign start. The same value is `testl $0x400000, ...` 42 times.
 *
 * Linked with -x so __bss has no symbol. ON: ok=1. OFF (M64_NO_ZF_MASK_IMM=1
 * at translate time): the size arguments are relocated. The value is not
 * 64K-aligned, so the zf-round-imm rule cannot keep it constant as well.
 */
extern int  printf(const char *, ...);
extern void exit(int);

static char big[0x100000];               /* __bss spans 0x80000 */
volatile unsigned g_flags;

static int consume(int a, int b)
{
   /* compare pieces: any imm32 inside __bss (0x2014..) would be relocated too */
   return (a >> 12) == 0x84 && (a & 0xfff) == 0 && (b >> 12) == 0x84 && (b & 0xfff) == 0;
}

int main(void)
{
   big[0] = 1;
   __asm__ volatile("testl $0x84000, %0" :: "m"(g_flags));
   __asm__ volatile("orl $0x84000, %0" : "+m"(g_flags));
   __asm__ volatile("testl $0x84000, %0" :: "m"(g_flags));
   printf("ok=%d\n", consume(0x84000, 0x84000) && big[0] == 1);
   exit(0);
}
