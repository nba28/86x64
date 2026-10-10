/* 99_zf_abs_neighbour — a __data pointer into symbol-free zero-fill must be
 * relocated when the code addresses memory right beside its target, even if
 * never that exact address.
 *
 * Call of Duty 4 (non-PIE, locals-stripped; its only __common symbols are Mach
 * exports at the section end) keeps an init table of object pointers. Entry
 * 0x92bdc4 is read by `movl 0x92bd84(,%eax,4)` and 0x7a63e0 sits beside
 * `movl $0x1, 0x7a63c0`, but neither address appears in the code itself, so
 * the zero-fill gate left both raw.
 *
 * Linked with -x; g_anchor (exported) sits above g_var, so the section has a
 * symbol and g_var is below it. The code touches g_var[0] absolutely and the
 * slot points at g_var[5], 20 bytes away. ON: ok=1. OFF
 * (M64_NO_ZF_ABS_NEIGHBOUR=1 at TRANSLATE time): the slot keeps its i386 value
 * and the store faults.
 */
extern int  printf(const char *, ...);
extern void exit(int);

static int g_var[8];
unsigned char g_anchor[16] __attribute__((section("__DATA,__bss")));
int *g_slot = &g_var[5];

int main(void)
{
   g_var[0] = 1;              /* the absolute neighbour */
   *g_slot = 7;
   printf("ok=%d anchor=%d\n", *g_slot == 7, g_anchor[0] == 0);
   exit(0);
}
