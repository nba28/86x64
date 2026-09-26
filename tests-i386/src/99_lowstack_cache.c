/* 99_lowstack_cache.c — the per-thread low-stack cache must stay correct.
 *
 * Native->i386 callbacks (here: qsort calling an i386 comparator through
 * cb_bridge) run on low-4GB stacks now cached per thread (lowstack_pool.c).
 * Two ways a cache like that breaks:
 *   1. NESTING: a callback that itself triggers callbacks must get a DIFFERENT
 *      stack — sentinels on the outer comparator's frame must survive the
 *      nested qsort.
 *   2. THREAD EXIT: stacks a thread cached must be freed when it exits — 500
 *      short-lived threads each running callbacks would otherwise strand
 *      500 x 4 MB (> the ~1.6 GB low window) and exhaust the heap.
 * Exit 42 = both hold. ABICONV_NO_LOWSTACK_CACHE=1 (plain malloc/free per
 * entry) must also exit 42: this guards the cache, it is not a bug repro.
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static int inner_cmp(const void *a, const void *b) {
   return *(const int *)a - *(const int *)b;
}
static int nest_bad;
static int outer_cmp(const void *a, const void *b) {
   volatile int sentinel[64];
   for (int i = 0; i < 64; i++) { sentinel[i] = 0x5a5a0000 + i; }
   int arr[8] = { 5, 3, 7, 1, 8, 2, 6, 4 };
   qsort(arr, 8, sizeof arr[0], inner_cmp);          /* nested callbacks */
   for (int i = 0; i < 64; i++) { if (sentinel[i] != 0x5a5a0000 + i) { nest_bad = 1; } }
   for (int i = 0; i < 8; i++) { if (arr[i] != i + 1) { nest_bad = 1; } }
   return *(const int *)a - *(const int *)b;
}
static void *worker(void *arg) {
   int arr[16];
   for (int i = 0; i < 16; i++) { arr[i] = 16 - i; }
   qsort(arr, 16, sizeof arr[0], inner_cmp);
   *(int *)arg = (arr[0] == 1 && arr[15] == 16);
   return NULL;
}

int main(void) {
   int top[12] = { 9, 4, 11, 1, 7, 12, 3, 10, 2, 8, 6, 5 };
   qsort(top, 12, sizeof top[0], outer_cmp);
   int sorted = 1;
   for (int i = 0; i < 12; i++) { if (top[i] != i + 1) { sorted = 0; } }
   printf("nesting: sorted=%d sentinels_ok=%d\n", sorted, !nest_bad);

   int threads_ok = 1;
   for (int t = 0; t < 500; t++) {
      pthread_t th; int ok = 0;
      if (pthread_create(&th, NULL, worker, &ok) != 0) { threads_ok = 0; break; }
      pthread_join(th, NULL);
      if (!ok) { threads_ok = 0; break; }
   }
   void *probe = malloc(64u << 20);                 /* heap still has room */
   printf("threads: ok=%d heap_after=%s\n", threads_ok, probe ? "ok" : "EXHAUSTED");
   exit(sorted && !nest_bad && threads_ok && probe ? 42 : 1);
}
