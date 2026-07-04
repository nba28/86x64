/* 73_cpufreq_check.c — repro guard for Halo's InsufficientCPUSpeed capability
 * self-check firing under translation despite adequate hardware.
 *
 * Halo (i386, 0x2aa57c) gates launch on CPU clock speed:
 *     sysctlbyname("hw.cpufrequency_max", &freq /*u64*, &len=8, 0, 0);
 *     if (r != 0 || freq == 0)  pass;                 // vendor fallback (675MHz)
 *     else { MHz = ___udivdi3(freq, 1000000);         // 64-bit divide
 *            if (MHz + 10 > 674) pass;                 // >= ~665 MHz
 *            else if (GetKeys 'p' held) pass;          // documented bypass
 *            else  show "InsufficientCPUSpeed" nag + _exit(0); }
 *
 * On real Apple-Silicon hardware the tester saw the nag despite hw.cpufrequency_max
 * reporting 2.4 GHz (and with the sysctl_shim CPU-freq floor deployed). This
 * test reproduces the exact path through the FULL translation pipeline so we can
 * isolate WHICH stage misbehaves without burning display time:
 *   (1) ___udivdi3 in isolation on a fixed value (2.4e9 / 1e6 must be 2400), and
 *   (2) the whole sysctlbyname 64-bit copy-back -> ___udivdi3 -> compare chain.
 *
 * Operands are volatile so clang emits the real ___udivdi3 call (a constant
 * divisor would otherwise fold to a reciprocal multiply and skip the shim),
 * matching Halo's own `call ___udivdi3`.
 *
 * Deterministic assertions (diffed):
 *   udivdi3_2400: 2400            — 64-bit division is correct through the shim
 *   cpufreq_check: PASS           — the gate passes (any real Mac is >665 MHz;
 *                                    the floor guarantees >=2.4GHz even if the
 *                                    key is absent/low)
 * The freq_lo/hi/mhz line is diagnostic (machine-dependent) and printed too.
 */

extern int  printf(const char *, ...);
extern void exit(int);
extern int  sysctlbyname(const char *name, void *oldp, unsigned long *oldlenp,
                         void *newp, unsigned long newlen);

int main(void) {
   /* (1) isolate ___udivdi3 on a known value. (Narrowed for printf: our i386
    * printf bridge rejects %llu; the DIVISION stays 64-bit.) */
   volatile unsigned long long a = 2400000000ULL, b = 1000000ULL;
   unsigned long long q = a / b;
   printf("udivdi3_2400: %d\n", (int)q);

   /* (2) full Halo path: 64-bit sysctl copy-back -> divide -> gate. */
   unsigned long long freq = 0;
   unsigned long      len  = sizeof freq;          /* 8 */
   int r = sysctlbyname("hw.cpufrequency_max", &freq, &len, 0, 0);

   unsigned long long mhz  = 0;
   int                pass;
   if (r == 0 && freq != 0) {
      volatile unsigned long long vf = freq, vd = 1000000ULL;
      mhz  = vf / vd;                               /* ___udivdi3 */
      pass = (mhz + 10 > 674);
   } else {
      pass = 1;                                     /* Halo's >0 fallback */
   }

   /* Deterministic on any real Mac (the sysctl_shim CPU-freq floor forces r=0
    * and freq>=2.4GHz even if the key is absent/low, so hi(=high dword nonzero)
    * is 0 and the gate passes). A copy-back width/offset bug would flip hi to 1
    * or drop MHz below 665 (pass=0); a udivdi3 bug would break line 1. */
   (void)len; (void)mhz;
   printf("cpufreq: r=%d hi=%d pass=%d\n",
          r, (int)((freq >> 32) != 0), pass);
   printf("cpufreq_check: %s\n", pass ? "PASS" : "FAIL");
   exit(0);
}
