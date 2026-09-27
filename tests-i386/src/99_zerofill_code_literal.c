/* 99_zerofill_code_literal — a __data pointer into zero-fill memory that the
 * CODE addresses absolutely must be relocated, even in symbol-free space.
 *
 * Plants vs. Zombies is non-PIE and locals-stripped; its only __common symbols
 * are Mach notify exports at the END of the section. A __data table of
 * pointers to its globals therefore sat in "symbol-free" zero-fill space, the
 * zero-fill target gate (99_zerofill_target_pair, Halo) left every one raw,
 * and the game SIGSEGVed on the first deref after "Click to start".
 * The code's own `movl %eax, 0x374250` attests the object as well as a symbol.
 *
 * Linked with -x; g_anchor (exported, survives -x) sits ABOVE g_var so the
 * section has a symbol yet g_var is below it (PvZ's shape). -mdynamic-no-pic
 * so main stores to g_var through an absolute address, as PvZ's code does.
 * ON: ok=1. OFF (M64_NO_ZF_CODE_LITERAL=1 at TRANSLATE time): the slot keeps
 * its i386 value and the deref faults.
 */
extern int  printf(const char *, ...);
extern void exit(int);

static int g_var[4];
unsigned char g_anchor[16] __attribute__((section("__DATA,__bss")));
int *g_slot = &g_var[1];   /* the __data pointer slot */

int main(void)
{
   g_var[1] = 7;
   printf("ok=%d anchor=%d\n", *g_slot == 7, g_anchor[0] == 0);
   exit(0);
}
