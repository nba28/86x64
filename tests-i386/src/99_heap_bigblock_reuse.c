/*
 * 99_heap_bigblock_reuse — a freed big block must not be eaten by a tiny request.
 *
 * Reproduces the Quinn network-host crash (2026-09-26). libabiconv's low-4GB
 * malloc (malloc_shim.c) never splits or coalesces blocks, and reused the first
 * free block that was big enough. libabiconv itself mallocs + frees a 4 MB low
 * stack for every native->i386 callback (cb_bridge.c), so the pattern
 *     big = malloc(4 MB); free(big); keep = malloc(16);     (repeat)
 * handed each freed 4 MB stack to the next long-lived 16-byte allocation, and
 * every callback bumped a fresh 4 MB. ~850 rounds exhaust the whole low-4GB
 * window; Quinn's CFSocket callbacks did it in 4 minutes.
 *
 * Two arms (exit code only — translated stdout is lost on abnormal exit):
 *   fixed                                     -> 42
 *   ABICONV_NO_HEAP_FIT_CAP=1 (kill switch)   -> 7   (malloc returned NULL)
 *
 * Ends with exit(), never `return` (no crt0; see 99_lowmem_big_alloc.c).
 */
#include <stdlib.h>

#define BIG    (4UL * 1024 * 1024)   /* = CB_LOWSTACK_SZ */
#define ROUNDS 2000                  /* > window / BIG, with margin */

static void *keep[ROUNDS];

int main(void)
{
   for (int i = 0; i < ROUNDS; ++i) {
      void *big = malloc(BIG);
      if (big == NULL) { exit(7); }
      free(big);
      keep[i] = malloc(16);
      if (keep[i] == NULL) { exit(7); }
   }
   exit(42);
}
