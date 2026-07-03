/*
 * Hand-written i386->x86_64 shims for sysctl() / sysctlbyname().
 *
 * abigen can't generate correct shims for these: it deep-copies `int *name` as
 * a SINGLE int (sysctl's name is a mib ARRAY of `namelen` ints) and copies only
 * the low 4 bytes of a `size_t *` into an 8-byte local with an uninitialised
 * high half. Both corrupt the call. (megsOfPhysicalMemory -> sysctl(CTL_HW,...)
 * crashed iPhoto here, and absent any shim the bind stayed on real libsystem,
 * so the i386-cdecl call hit x86_64 sysctl with empty arg registers -> name=NULL.)
 *
 * All of sysctl's pointer args already point at i386 memory that lives in the
 * low 4GB and is directly usable by x86_64, so they pass straight through
 * (zero-extended). The only real ABI difference is `size_t`: 4 bytes on i386,
 * 8 on x86_64. `newlen` (by value) zero-extends for free; `*oldlenp` is in/out
 * and must be widened to a local size_t for the call, then written back narrow.
 *
 * These are reached via the ___sysctl / ___sysctlbyname trampolines in
 * maptable_tramp.asm (static-interpose redirects the binary's _sysctl bind).
 */

#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* int sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
 *            void *newp, size_t newlen);
 * i386 frame: name[0] namelen[1] oldp[2] oldlenp[3] newp[4] newlen[5]. */
int shim_sysctl(uint32_t *a) {
   int          *name    = (int *)(uintptr_t)a[0];
   unsigned      namelen = a[1];
   void         *oldp    = (void *)(uintptr_t)a[2];
   uint32_t     *ol32    = (uint32_t *)(uintptr_t)a[3];   /* i386 size_t *oldlenp */
   void         *newp    = (void *)(uintptr_t)a[4];
   size_t        newlen  = a[5];                          /* zero-extended */

   size_t  oldlen  = 0;
   size_t *oldlenp = NULL;
   if (ol32) { oldlen = *ol32; oldlenp = &oldlen; }

   int r = sysctl(name, namelen, oldp, oldlenp, newp, newlen);

   if (ol32) { *ol32 = (uint32_t)oldlen; }

   if (getenv("ABICONV_SYSCTL_TRACE")) {
      unsigned long long v = 0;
      if (oldp && oldlen >= 4) v = (oldlen >= 8) ? *(unsigned long long *)oldp
                                                  : *(uint32_t *)oldp;
      fprintf(stderr, "[sysctl] mib[%u]={%d,%d} -> r=%d oldlen=%zu val=%llu\n",
              namelen, (namelen > 0 && name) ? name[0] : -1,
              (namelen > 1 && name) ? name[1] : -1, r, oldlen, v);
   }
   return r;
}

/* hw.cpufrequency{,_max,_min}: legacy apps gate on CPU clock speed (Halo's
 * "InsufficientCPUSpeed" capability check reads hw.cpufrequency_max, divides by
 * 1e6, and refuses to launch — showing a modal "hold 'p' to bypass" nag that
 * then _exit()s — when the result is implausibly low). Under Rosetta on Apple
 * Silicon this key is unreliable: depending on the macOS version it is absent
 * (ENOENT), reports 0, or reports a small nonzero value, and the branch that
 * shows the nag fires precisely on the small-nonzero case (absent/0 both take
 * the app's own >0 fallback and pass). Universal, structural fix: for the
 * hw.cpufrequency* family guarantee a sane modern floor so any i386 CPU-speed
 * gate always sees a plausible value. Triggers on the sysctl key name, never on
 * any app identity; benefits every legacy binary that queries CPU frequency. */
static int cpufreq_key(const char *n) {
   return n && (!strcmp(n, "hw.cpufrequency")     ||
                !strcmp(n, "hw.cpufrequency_max") ||
                !strcmp(n, "hw.cpufrequency_min"));
}

/* int sysctlbyname(const char *name, void *oldp, size_t *oldlenp,
 *                  void *newp, size_t newlen);
 * i386 frame: name[0] oldp[1] oldlenp[2] newp[3] newlen[4]. */
int shim_sysctlbyname(uint32_t *a) {
   const char *name   = (const char *)(uintptr_t)a[0];
   void       *oldp   = (void *)(uintptr_t)a[1];
   uint32_t   *ol32   = (uint32_t *)(uintptr_t)a[2];
   void       *newp   = (void *)(uintptr_t)a[3];
   size_t      newlen = a[4];

   size_t  reqlen  = 0;                 /* caller's buffer size (i386 size_t) */
   size_t  oldlen  = 0;
   size_t *oldlenp = NULL;
   if (ol32) { reqlen = oldlen = *ol32; oldlenp = &oldlen; }

   int r = sysctlbyname(name, oldp, oldlenp, newp, newlen);

   if (ol32) { *ol32 = (uint32_t)oldlen; }

   /* CPU-frequency sanity floor. Only touch a real value buffer (>=4 bytes),
    * never the oldp==NULL size-probe form. FLOOR (2.4 GHz) fits in 32 bits so
    * it is representable whether the caller asked for 4 or 8 bytes. */
   int      nat_r   = r;                /* native return, kept for the trace */
   uint64_t nat_val = 0;
   int      floored = 0;
   if (cpufreq_key(name) && oldp && reqlen >= 4) {
      const uint64_t FLOOR = 2400000000ULL;
      uint64_t cur = 0;
      if (r == 0) {
         if (oldlen >= 8)      cur = *(uint64_t *)oldp;
         else if (oldlen >= 4) cur = *(uint32_t *)oldp;
      }
      nat_val = cur;
      if (r != 0 || cur < FLOOR) {
         if (reqlen >= 8) { *(uint64_t *)oldp = FLOOR;            oldlen = 8; }
         else             { *(uint32_t *)oldp = (uint32_t)FLOOR; oldlen = 4; }
         if (ol32) { *ol32 = (uint32_t)oldlen; }
         r = 0;
         floored = 1;
      }
   }

   /* Diagnostic: show BOTH the native return and what we delivered, so a
    * display run can prove whether the CPU-speed gate reads a low freq (fixed
    * here) or a plausible one (the failing check is elsewhere). */
   if (getenv("ABICONV_SYSCTL_TRACE") && cpufreq_key(name)) {
      unsigned long long v = (oldp && oldlen >= 8) ? *(unsigned long long *)oldp
                           : (oldp && oldlen >= 4) ? *(uint32_t *)oldp : 0ULL;
      fprintf(stderr, "[sysctl] %s native(r=%d val=%llu) delivered(r=%d val=%llu "
              "MHz=%llu) reqlen=%zu%s\n",
              name, nat_r, (unsigned long long)nat_val, r, v, v / 1000000ULL,
              reqlen, floored ? " [FLOORED]" : "");
   }
   return r;
}
