/*
 * 99_lowmem_big_alloc — a SINGLE allocation larger than one low-4GB heap region.
 *
 * Reproduces the Portal 2 silent-death blocker (2026-09-12). Every pointer the
 * translated i386 program holds must fit in 32 bits, so malloc is served from
 * libabiconv's own arena of regions reserved below 4 GB (malloc_shim.c). A single
 * allocation has to fit inside ONE region, and the region size was a fixed
 * constant (768 MB) — so a request larger than that could NEVER be served, no
 * matter how much total arena was free, and bump() just returned NULL.
 *
 * Portal 2's tier0 CStdMemAlloc reserves 1 GiB in one call during libvstdlib's
 * static initializers. It got NULL, turned that into
 * CStdMemAlloc::SetCRTAllocFailed -> Plat_ExitProcess -> _exit(0), and the game
 * vanished with status 0, no output and no crash report — indistinguishable from
 * a clean exit. add_region() now sizes the mapping to the request (and spills into
 * the anonymous-mmap band when its own window is exhausted).
 *
 * Two arms, so the guard proves the fix rather than the test:
 *   fixed                                  -> 42
 *   M64_HEAP_FIXED_REGIONS=1 (kill switch) -> 7   (the old NULL)
 *
 * Reports through the EXIT CODE only. stdout from translated code is buffered, so
 * a printf'd diagnostic is lost on abnormal termination — and this test allocates
 * near the limits of the address window, which is exactly where that happens.
 *   42 = all good                     7 = a request returned NULL (the defect)
 *    8 = pointer escaped the low 4GB  9 = readback mismatch
 *
 * WARNING: ends with exit(), never `return`. These tests link `-e _main` with no
 * crt0 and the 86x64.sh wrapper enters _main via `jmp`, so there is no return
 * address to `ret` to — a `return` here faults with rip=1 (see Makefile:1259).
 *
 * Only a few pages of each block are touched — the regions are RESERVED, not
 * committed, so this costs address space, not RAM.
 */
#include <stdlib.h>
#include <stdint.h>

/* Larger than one region (256 MB), smaller than the window. */
#define BIG_ONE  (320UL * 1024 * 1024)
/* Exactly what Portal 2's CStdMemAlloc reserves in one call. */
#define BIG_TWO  (1024UL * 1024 * 1024)

/* 0 = ok, else the exit code to report. */
static int check(void *p, unsigned long n)
{
   if (p == NULL) { return 7; }
   /* The whole point of the arena: the block must be representable in the i386
    * program's 32-bit pointer slots. */
   if ((unsigned long)(uintptr_t)p + n > 0xFFFFFFFFUL) { return 8; }
   /* Touch both ends: a region sized by a bogus computation can be handed back
    * and still fault on a write near its top. */
   volatile unsigned char *c = (volatile unsigned char *)p;
   c[0] = 0x5A;
   c[n - 1] = 0xA5;
   if (c[0] != 0x5A || c[n - 1] != 0xA5) { return 9; }
   return 0;
}

int main(void)
{
   int rc;

   void *a = malloc(BIG_ONE);
   rc = check(a, BIG_ONE);
   if (rc != 0) { exit(rc); }

   /* Free it before the second request: the fix must not depend on the first
    * block still being carved out, and free() must accept an oversized block. */
   free(a);

   void *b = malloc(BIG_TWO);
   rc = check(b, BIG_TWO);
   if (rc != 0) { exit(rc); }

   /* Ordinary small allocations must still work once a giant region exists — the
    * oversized mapping must not have consumed the whole window. */
   void *s = malloc(4096);
   rc = check(s, 4096);
   if (rc != 0) { exit(rc); }

   free(b);
   free(s);
   exit(42);
}
