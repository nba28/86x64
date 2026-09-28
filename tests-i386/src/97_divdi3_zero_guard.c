/* 97_divdi3_zero_guard — the libgcc 64-bit div/mod helpers must not take the
 * whole process down with an uncatchable #DE (SIGFPE) when a translated binary
 * hands them a ZERO divisor.
 *
 * Real trigger: Halo CE's renderer computes elapsed-time conversions by dividing
 * a 64-bit mach_absolute_time value by a timer-frequency global; a separate
 * translator layout bug left that global 0, so the signed 64-bit divide (emitted
 * by GCC as a call to ___divdi3, bridged by libabiconv's libgcc_shim.asm)
 * faulted with #DE the instant the renderer ran (crash rip = ____divdi3+47 at the
 * `idiv r9`, r9=0). The upstream zero is fixed elsewhere; this guard makes the
 * primitive itself robust: a 0 divisor returns 0 (quotient AND remainder) —
 * matching how well-behaved i386 code already guards its own division (Halo does
 * exactly `testl %edx,%edx; jne ..; xorl %eax,%eax` at its OWN guarded sites) —
 * instead of a SIGFPE the caller can't recover from. Signed AND unsigned, div
 * AND mod are all covered.
 *
 * Exit 97 = every zero-divisor form returned 0 without faulting, and the
 * non-zero control divide still computes correctly (the guard is transparent on
 * the normal path). */

#include <stdint.h>

extern int printf(const char *, ...);
extern void exit(int);

/* volatile zero the compiler can't fold, forcing a real ___{u,}{div,mod}di3 call
 * (i386 has no 64-bit hardware divide). */
static volatile int64_t  SZERO = 0;
static volatile uint64_t UZERO = 0;

int main(void) {
   volatile int64_t  sa = (int64_t)0x18E649B01C287EA8LL;   /* the Halo dividend */
   volatile uint64_t ua = 0x18E649B01C287EA8ULL;

   int64_t  sdiv = sa / SZERO;   /* ___divdi3,  divisor 0 -> guard -> 0 */
   int64_t  smod = sa % SZERO;   /* ___moddi3,  divisor 0 -> guard -> 0 */
   uint64_t udiv = ua / UZERO;   /* ___udivdi3, divisor 0 -> guard -> 0 */
   uint64_t umod = ua % UZERO;   /* ___umoddi3, divisor 0 -> guard -> 0 */

   /* Control: a real non-zero divide must still be exact (guard transparent). */
   volatile int64_t  cd = 1000000000000LL;
   volatile int64_t  cn = 7;
   int64_t  cq = cd / cn;        /* 142857142857 = 0x21_43C7A574 */
   int64_t  cr = cd % cn;        /* 1 */

   /* The i386 printf bridge rejects %lld/%llu (see 73_cpufreq_check); the
    * DIVISIONS above stay 64-bit, we just narrow the results for printing. The
    * control quotient is checked against its exact 64-bit value in `ok`; here we
    * print its low 32 bits only. */
   int all_zero = (sdiv == 0) && (smod == 0) && (udiv == 0) && (umod == 0);
   printf("zero_forms_all_returned_zero=%d cq_lo=%d cr=%d\n",
          all_zero, (int)(uint32_t)cq, (int)cr);

   int ok = all_zero && (cq == 142857142857LL) && (cr == 1);
   exit(ok ? 97 : 1);
}
