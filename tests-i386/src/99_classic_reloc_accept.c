/* 99_classic_reloc_accept — a data slot that carries a classic local reloc IS
 * a pointer, whatever the heuristic gates think of its target.
 *
 * PvZ's libbass.dylib (classic relocs, based at 0) keeps a __DATA,__const
 * table of {fn, fn} records; entry 0 targets 0x91ac, an -O2 function with no
 * symbol and no `55 89 e5` prologue. The code-entry gate called it a constant,
 * the slot stayed raw and `call *(%edx,%esi,8)` jumped to 0x91ac.
 * Built -O2 -fomit-frame-pointer, linked -x -pie for 10.5 (classic relocs, no
 * LC_DYLD_INFO). ON: exit 42. OFF (M64_NO_CLASSIC_RELOC_ACCEPT=1 at
 * TRANSLATE time): the call through the table faults.
 */
#include <stdlib.h>

__attribute__((noinline)) static int f_add(int x) { return x + 40; }
__attribute__((noinline)) static int f_mul(int x) { return x * 3; }
struct op { int (*fn)(int); int (*alt)(int); };
static const struct op ops[] = { { f_add, f_mul }, { f_mul, f_add } };

int main(void)
{
   volatile int i = 0;
   exit(ops[i].fn(2));
}
