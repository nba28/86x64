/*
 * Hand-written i386->x86_64 shims for the pthread thread-specific-data trio
 * pthread_getspecific / pthread_setspecific, with one correctness guard abigen
 * cannot express: treat key 0 as UNINITIALIZED.
 *
 * Why this is needed (universal, found via Portal 2's libtier0):
 * A `CThreadLocalBase` (Valve's GenericThreadLocals) is a global whose 4-byte
 * `m_index` (the pthread_key_t) is 0 until its constructor calls
 * pthread_key_create. Valve's logging.cpp __GLOBAL__I_a — IDENTICALLY in the
 * i386 original and our translation — registers logging channels (which
 * construct the CLoggingSystem singleton, whose ctor calls
 * RegisterLoggingListener) BEFORE it constructs g_nThreadLocalStateIndex. So
 * during early static init RegisterLoggingListener reads the thread-local
 * through key 0:  Get() == pthread_getspecific(0), and uses the result as an
 * array index:  cell = this[0x7418 + getspecific(0)*0x4c].
 *
 * macOS pthread_key_create never returns 0 — real keys start at 258 — so key 0
 * is ALWAYS an unconstructed CThreadLocalBase. On i386, pthread_getspecific(0)
 * returns TSD slot 0 (the pthread_self pointer); the i386 build tolerates the
 * resulting wild 32-bit index because the address it forms happens to be
 * mapped/benign. Our x86_64 translation forms a non-wrapping 64-bit address
 * from the same garbage and faults (EXC_BAD_ACCESS). Worse, the matching
 * CThreadLocalBase::Set would pthread_setspecific(0, value) and CLOBBER TSD
 * slot 0 = pthread_self.
 *
 * The semantically-correct, allocator-agnostic fix: an uninitialized key (0)
 * has no thread-local value. getspecific(0) -> NULL makes the caller take its
 * "no value yet" default path (exactly the intent for an unconstructed
 * thread-local); setspecific(0, ...) -> no-op (success) so it can't corrupt
 * the reserved slot 0. Once the real key is created (m_index = 258...), both
 * forward to native unchanged. Key 0 is never a valid pthread key for ANY
 * binary, so this is safe and universal — not Portal-2-specific.
 *
 * Wiring: MTSHIM ___pthread_getspecific/___pthread_setspecific in
 * maptable_tramp.asm; _pthread_getspecific/_pthread_setspecific listed in
 * custom.syms so abigen does NOT also emit them (one definition wins).
 * The MTSHIM trampoline hands us `a = &args[0]` (i386-cdecl, 4-byte slots) and
 * returns our uint32_t result in eax while unwinding the i386 4-byte frame.
 */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int tls_trace(void) {
   static int v = -1;
   if (v < 0) { v = getenv("ABICONV_TLS_TRACE") ? 1 : 0; }
   return v;
}

/* void *pthread_getspecific(pthread_key_t key):  a[0] = key. */
int32_t shim_pthread_getspecific(uint32_t *a) {
   uint32_t key = a[0];
   if (key == 0) {                       /* unconstructed thread-local */
      if (tls_trace()) {
         fprintf(stderr, "[tls] getspecific(key=0) -> NULL (uninitialized)\n");
         fflush(stderr);
      }
      return 0;
   }
   void *v = pthread_getspecific((pthread_key_t)key);
   /* The i386 caller stored a low-4GB (translated) pointer; return it
    * truncated to 32 bits, exactly as the abigen shim would. */
   return (int32_t)(uintptr_t)v;
}

/* int pthread_setspecific(pthread_key_t key, const void *value):
 *   a[0] = key, a[1] = value (low-4GB i386 pointer). */
int32_t shim_pthread_setspecific(uint32_t *a) {
   uint32_t key = a[0];
   if (key == 0) {                       /* never clobber reserved TSD slot 0 */
      if (tls_trace()) {
         fprintf(stderr, "[tls] setspecific(key=0) -> no-op\n");
         fflush(stderr);
      }
      return 0;                          /* report success */
   }
   int r = pthread_setspecific((pthread_key_t)key, (void *)(uintptr_t)a[1]);
   return (int32_t)r;
}
