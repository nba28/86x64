/*
 * 101_literal_abs_disp.c — an absolute [disp32] whose target lives in a LITERAL
 * POOL must still have its displacement relocated.
 *
 * THE DEFECT (found by a whole-image pcmap audit of Halo CE, 2026-08-18).
 * i386 ModR/M mod=00 r/m=101 means "absolute disp32"; the IDENTICAL encoding in
 * 64-bit means "[rip+disp32]". Every absolute memory reference must therefore
 * have its displacement rewritten at translate time, or the translated
 * instruction silently reads whatever byte sits at that offset from rip.
 * Instruction::Parse decides whether to rewrite by asking whether the target
 * section holds data -- and that test enumerated section TYPES (S_REGULAR /
 * S_ZEROFILL / S_GB_ZEROFILL), which excludes every literal pool: __literal4 is
 * S_4BYTE_LITERALS, __literal8 is S_8BYTE_LITERALS, __cstring is
 * S_CSTRING_LITERALS. In Halo four float compares against pooled constants
 * (`cmpss xmm,[abs32],imm8`) kept their i386 displacement and read __text,
 * __DATA,__data and the translator's own __86x64_pcmap instead of 0.0f,
 * -0.05f and 0.0005f.
 *
 * WHY __cstring AND NOT __literal4.  ld64 will not leave a NAMED atom in a
 * coalescable literal section: give a `.literal4` entry a global symbol and ld
 * moves it to __TEXT,__const (S_REGULAR), which the old gate already accepted --
 * the fixture would test nothing. A named `.cstring` entry keeps its section
 * AND its S_CSTRING_LITERALS type, so it is the pool type that can actually be
 * addressed by name. The fix keys on the pool TYPE, not the section name, so
 * __literal4/__literal8/__literal16 ride on the same predicate; this fixture
 * pins the mechanism, and the Halo audit pins the __literal4 instance.
 *
 * THE READ: `cmpb $imm8, <abs32>` -- the [abs32]+imm8 shape whose displacement
 * the gate governs. Four independent sentinel bytes are checked, so an
 * unrelocated read that happens to land on a matching byte costs 2^-32 rather
 * than 2^-8.
 *
 * exit 0 = every absolute displacement relocated correctly.
 *      1 = the literal-pool read was wrong (the defect).
 *      2 = the __DATA control was wrong, i.e. the fixture is no longer
 *          measuring what it claims -- fix the fixture, not the gate.
 */
#include <stdio.h>
#include <stdlib.h>

/* bytes a5 5b c3 7e, then the implicit NUL. Unique, so ld cannot merge it
 * with another string and move the symbol out from under the data. */
__asm__(".cstring\n"
        ".globl _lit_sentinel\n"
        "_lit_sentinel: .asciz \"\\245[\\303~\"\n");

__asm__(".data\n"
        ".globl _data_sentinel\n"
        ".align 2\n"
        "_data_sentinel: .long 0x1234abcd\n");

extern unsigned int data_sentinel;

/* cmpb $imm8, <abs32>  ->  80 3d <disp32> <imm8> */
#define CMPB_ABS(sym, off, val)                                  \
   ({ unsigned char _r;                                          \
      __asm__ volatile("cmpb $" #val ", " #sym "+" #off "\n\t"   \
                       "sete %0" : "=r"(_r) : : "cc");           \
      _r; })

int main(void) {
   const int l0 = CMPB_ABS(_lit_sentinel, 0, 0xa5);
   const int l1 = CMPB_ABS(_lit_sentinel, 1, 0x5b);
   const int l2 = CMPB_ABS(_lit_sentinel, 2, 0xc3);
   const int l3 = CMPB_ABS(_lit_sentinel, 3, 0x7e);
   /* little-endian 0x1234abcd -> bytes cd ab 34 12 */
   const int d0 = CMPB_ABS(_data_sentinel, 0, 0xcd);
   const int d3 = CMPB_ABS(_data_sentinel, 3, 0x12);

   printf("literal: %d%d%d%d  data: %d%d  data_sentinel=%08x\n",
          l0, l1, l2, l3, d0, d3, data_sentinel);

   if (!d0 || !d3) {
      printf("CONTROL FAILED: the __DATA absolute read is wrong, so this "
             "fixture is not measuring the literal-pool gate.\n");
      exit(2);   /* the wrapper JMPs into _main: exit, never return */
   }
   if (!l0 || !l1 || !l2 || !l3) {
      printf("LITERAL-POOL ABS DISP NOT RELOCATED: cmpb $imm8,[abs32] against "
             "a literal-pool section read the wrong memory.\n");
      exit(1);
   }
   printf("ok: literal-pool and __DATA absolute displacements both relocated\n");
   exit(0);
}
