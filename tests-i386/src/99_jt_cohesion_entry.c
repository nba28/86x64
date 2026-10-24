/* 99_jt_cohesion_entry.c — A/B fixture for the entry-evidence arm of the
 * jump-table cohesion gate (ParseEnv::code_alias_run_contradicted).
 *
 * A packed integer table in __TEXT,__const whose word happens to equal a real
 * function entry (nlist + `55 89 e5` prologue) used to be rebased: the gate
 * refused to judge any value carrying entry evidence. Civ IV's static zlib lost
 * two static_ltree {code,len} pairs this way and wrote corrupt caches.
 *
 * jce_fn is laid out by hand so its interior is known: 55 | 89 e5 | b8 imm32 |
 * 5d | c3. jce_fn+4..+7 are strictly inside the mov, so they are proven
 * mid-instruction aliases. jce_tab puts jce_fn itself between three such words
 * on each side; read as integers, every word minus jce_tab[3] must be
 * {4,5,6,0,5,6,7}. ON keeps that; OFF (M64_NO_JT_COHESION_ENTRY=1) rebases
 * jce_tab[3] to a translated code address.
 *
 * Positive control: jce_ftab is a genuine function-pointer table (zero
 * neighbours, entry targets). It must stay a pointer table: calling through it
 * returns the functions' constants.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

__asm__(".text\n"
        ".globl _jce_fn\n"
        "_jce_fn:\n"
        "  pushl %ebp\n"
        "  movl %esp, %ebp\n"
        "  movl $0x11223344, %eax\n"
        "  popl %ebp\n"
        "  ret\n"
        ".globl _jce_fn2\n"
        "_jce_fn2:\n"
        "  pushl %ebp\n"
        "  movl %esp, %ebp\n"
        "  movl $0x55667788, %eax\n"
        "  popl %ebp\n"
        "  ret\n"
        ".const\n"
        ".align 2\n"
        ".globl _jce_tab\n"
        "_jce_tab:\n"
        "  .long _jce_fn+4, _jce_fn+5, _jce_fn+6, _jce_fn, _jce_fn+5, _jce_fn+6, _jce_fn+7\n"
        "  .long 0, 0\n"
        ".globl _jce_ftab\n"
        "_jce_ftab:\n"
        "  .long _jce_fn, _jce_fn2\n"
        "  .long 0, 0\n");

extern const uint32_t jce_tab[7];
typedef uint32_t (*fn_t)(void);
extern const fn_t jce_ftab[2];

int main(void) {
   static const int want[7] = { 4, 5, 6, 0, 5, 6, 7 };
   int bad = 0;
   for (int i = 0; i < 7; i++) {
      if ((int)(jce_tab[i] - jce_tab[3]) != want[i]) {
         printf("table[%d]: %#x - %#x != %d\n", i, jce_tab[i], jce_tab[3], want[i]);
         bad = 1;
      }
   }
   if (jce_ftab[0]() != 0x11223344u || jce_ftab[1]() != 0x55667788u) {
      printf("fn-ptr table broken\n");
      bad = 1;
   }
   printf(bad ? "FAIL\n" : "OK\n");
   exit(bad ? 1 : 42);
}
