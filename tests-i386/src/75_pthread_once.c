/*
 * 75_pthread_once — exercises the _pthread_once shim (pthread_sync_shim.c).
 *
 * The i386 pthread_once_t is `{ long __sig; char __opaque[] }` — an 8-byte
 * control block with a 4-byte sig — which is NOT a valid native os_once_t.
 * Forwarding _pthread_once straight to native pthread_once (abigen's default)
 * makes native's gate machinery read a corrupt token and call
 * _os_once_gate_corruption_abort; and the init routine is an i386 void(void)
 * fn ptr native pthread_once would invoke with the wrong ABI (Civ IV s30, the
 * translated QuickTime internalGetPerThreadStorage -> _pthread_once path).
 *
 * The shim runs the once semantics itself, keyed on the stable i386 control
 * address, and calls the i386 init routine via _86x64_call_i386. This test
 * verifies (a) the init routine runs, (b) it runs EXACTLY ONCE across many
 * calls on the same control block, and (c) two DISTINCT control blocks each
 * fire independently. On a correct shim it exits 42; a corrupt-os_once abort
 * (native forward) crashes, and a run-twice / never-run bug fails the count.
 *
 * pthread_once_t on i386 = PTHREAD_ONCE_INIT = { _PTHREAD_ONCE_SIG_init, {0} }.
 */
#include <pthread.h>

extern void exit(int status);

static int g_count_a = 0;
static int g_count_b = 0;

static void init_a(void) { g_count_a++; }
static void init_b(void) { g_count_b++; }

int main(void) {
   pthread_once_t once_a = PTHREAD_ONCE_INIT;
   pthread_once_t once_b = PTHREAD_ONCE_INIT;

   /* Same control block, many times: init_a must run exactly once. */
   for (int i = 0; i < 5; i++) {
      if (pthread_once(&once_a, init_a) != 0) { exit(1); }
   }
   if (g_count_a != 1) { exit(10 + g_count_a); }  /* 0=never, >1=re-ran */

   /* A distinct control block fires independently (not shadowed by once_a). */
   if (pthread_once(&once_b, init_b) != 0) { exit(2); }
   if (pthread_once(&once_b, init_b) != 0) { exit(3); }
   if (g_count_b != 1) { exit(20 + g_count_b); }

   exit(42);
}
