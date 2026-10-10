/* 99_zf_small_pair_imm — a stack-stored imm32 that merely LOOKS like an address
 * into symbol-free zero-fill space must stay a constant.
 *
 * Call of Duty 4 (stripped, 16 MB __common) stores kAudioChannelLayoutTag_Stereo
 * (0x650002) to a stack local. 0x650002 lands inside __common with no symbol at
 * or below it, so the translator relocated it as a pointer and CoreAudio got a
 * garbage layout tag. Rule (ParseEnv::zerofill_small_pair_constant): `movl $X,
 * d(%ebp|%esp)` with X in a symbol-free zero-fill range and u16 halves
 * {high != 0, low in 1..0x3f} is a constant.
 *
 * g_zf is static and the fixture links with -x: no symbol in the zero-fill
 * section; 8 MB so it covers 0x650002. ON exits 42; OFF
 * (M64_NO_ZF_SMALL_PAIR_IMM=1 at TRANSLATE time) relocates the value.
 */
extern void exit(int);

static unsigned char g_zf[8 * 1024 * 1024];

int main(void)
{
   volatile unsigned int v;
   g_zf[1] = 1;   /* keep g_zf alive */
   __asm__ volatile("movl $0x650002, %0" : "=m"(v));
   exit(v == 0x650002u ? 42 : 1);
}
