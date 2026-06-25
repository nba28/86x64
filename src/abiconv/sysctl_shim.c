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

/* int sysctlbyname(const char *name, void *oldp, size_t *oldlenp,
 *                  void *newp, size_t newlen);
 * i386 frame: name[0] oldp[1] oldlenp[2] newp[3] newlen[4]. */
int shim_sysctlbyname(uint32_t *a) {
   const char *name   = (const char *)(uintptr_t)a[0];
   void       *oldp   = (void *)(uintptr_t)a[1];
   uint32_t   *ol32   = (uint32_t *)(uintptr_t)a[2];
   void       *newp   = (void *)(uintptr_t)a[3];
   size_t      newlen = a[4];

   size_t  oldlen  = 0;
   size_t *oldlenp = NULL;
   if (ol32) { oldlen = *ol32; oldlenp = &oldlen; }

   int r = sysctlbyname(name, oldp, oldlenp, newp, newlen);

   if (ol32) { *ol32 = (uint32_t)oldlen; }
   return r;
}
