/* 99_bss_zerofill_not_filebacked — a segment that ENDS in a large zerofill
 * (__bss/__common) section must leave that section PURELY zero-filled: its
 * bytes must NOT be file-backed. The translator used to page-round the __DATA
 * segment's filesize UP after laying out the (offset-not-advancing) zerofill
 * sections, which — when the first zerofill section starts at a non-page-aligned
 * vmaddr (right after a file-backed section) — spilled up to a page of file
 * backing INTO __bss/__common. The loader then maps those bytes from the file
 * instead of anonymous zero, so a stale/COW'd file byte can read non-zero.
 *
 * Real impact (Halo): a C++ static-init RUN-ONCE GUARD FLAG lives at __bss[0].
 * If that byte was file-backed and non-zero, the guarded initializer (Halo's
 * timer-frequency calibration) was SKIPPED, leaving the frequency 0 and the
 * renderer dividing by zero.
 *
 * This test reproduces that shape: a large __bss array whose FIRST bytes double
 * as a run-once guard, and a "calibration" that must run exactly once and write
 * a sentinel the rest of the program reads. The whole __bss must be observed as
 * zero at startup (so the guard is 0 and calibration runs). Exit 99 = the guard
 * byte and a probe deep inside the multi-KB zerofill span were both zero at
 * load AND the run-once init fired. The paired structural check
 * (bss_zerofill_test.sh) additionally asserts __DATA filesize stops at/below the
 * first zerofill section (page-rounded) in the translated Mach-O. */

#include <stdint.h>
extern int printf(const char *, ...);
extern void exit(int);

/* Large zerofill __bss (~1.5MB); byte 0 is the run-once guard, like Halo. */
static unsigned char big_bss[0x180000];
static uint64_t calibrated;               /* set once by the guarded init */

/* Mirror Halo's `cmpb [guard],0; jne skip; ...; movb [guard],1` run-once. */
static void run_once_calibrate(void) {
   if (big_bss[0] != 0) { return; }        /* already ran (or a stale file byte!) */
   calibrated = 0x0BADC0DE12345678ULL;     /* the "frequency" */
   big_bss[0] = 1;                          /* mark done */
}

int main(void) {
   /* 1) The guard byte and a deep-interior probe must both be zero at load
    *    (i.e. __bss is genuine anonymous zero-fill, not file-backed). */
   int guard_zero  = (big_bss[0] == 0);
   int deep_zero   = (big_bss[0x120000] == 0);   /* well past any file page */

   /* 2) Run-once init must fire (only possible if the guard read 0). */
   run_once_calibrate();
   run_once_calibrate();                          /* second call must be a no-op */

   int ran = (calibrated == 0x0BADC0DE12345678ULL);
   int marked = (big_bss[0] == 1);

   printf("guard_zero=%d deep_zero=%d ran=%d marked=%d\n",
          guard_zero, deep_zero, ran, marked);

   int ok = guard_zero && deep_zero && ran && marked;
   exit(ok ? 99 : 1);
}
