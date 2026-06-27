/*
 * Hand-written i386->x86_64 shims for the pthread SYNCHRONIZATION primitives
 * (mutex / cond / rwlock). These replace abigen's auto-generated marshalling,
 * which is fundamentally WRONG for these types.
 *
 * Why abigen's default is wrong (universal; found via Portal 2's Source thread
 * pool, s19): abigen treats a `pthread_mutex_t *` / `pthread_cond_t *` argument
 * as a pointer to a value struct, so each call COPIES the i386 struct into a
 * fresh native-sized stack buffer (widening `long __sig` 4->8 and byte-copying
 * the opaque body), runs the native pthread call on the COPY, then copies the
 * mutated state back into the i386 struct. That round-trip survives single-
 * threaded sequential lock/unlock, but POSIX synchronization objects require a
 * STABLE, SHARED address: the kernel/runtime keys the owner and the waiter
 * queue off the object's address. Two threads contending on "the same" i386
 * mutex actually operate on two different native stack copies, so:
 *   - pthread_cond_(timed)wait atomically unlocks a COPY of the mutex, leaving
 *     the real shared mutex held -> any other thread deadlocks acquiring it;
 *   - pthread_cond_signal/broadcast bumps a COPY of the cond -> the waiter on a
 *     different copy is never woken (lost wakeup).
 * Net: every multithreaded i386 program deadlocks at its first real cross-thread
 * rendezvous (Portal 2: CThreadPool startup; main waits on a CThreadEvent while
 * a CThread worker deadlocks on the pool mutex). See tests-i386/35.
 *
 * The fix (universal, structural — keyed on "this is a kernel-backed pthread
 * sync handle", not on any app): maintain ONE stable native pthread object per
 * i386 object, allocated at the correct native size and shared across all
 * threads/calls. We store the native object pointer INSIDE the i386 struct
 * (which the i386 program treats as opaque): the i386 sync structs are all >=
 * 16 bytes (i386 sizes: mutex 44, cond 28, rwlock 128), so the first 16 bytes
 * hold {magic(8), native_ptr(8)}. A statically-initialised object (e.g.
 * PTHREAD_MUTEX_INITIALIZER, whose first 8 bytes are 0x00000000_32AAABA7) does
 * not carry our magic, so first use lazily creates+default-inits the native
 * object under a real (native) registry lock. Attributes (recursive mutex etc.)
 * are value config — reconstructed by value from the i386 attr, which is the
 * one case where abigen's copy semantics are correct.
 *
 * Wiring: MTSHIM ___pthread_<fn> -> _shim_pthread_<fn> in maptable_tramp.asm;
 * the _pthread_<fn> names are listed in custom.syms so abigen does NOT also
 * emit them. The MTSHIM trampoline hands us `a = &args[0]` (i386-cdecl, 4-byte
 * slots) and returns our int32 result in eax while unwinding the i386 frame.
 * Our own internal pthread_* calls resolve to NATIVE libsystem_pthread (the
 * shims have the extra-underscore name ___pthread_*), so there is no recursion.
 */
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static int psx_trace(void) {
   static int v = -1;
   if (v < 0) v = getenv("ABICONV_PTHREAD_TRACE") ? 1 : 0;
   return v;
}

/* In-struct header we impose on the (opaque-to-the-client) i386 sync object.
 * magic distinguishes "adopted" objects per type so a misuse is caught. */
#define PSX_MAGIC_MUTEX  0x8642ABC0DE5A4D55ULL  /* ...'MU' */
#define PSX_MAGIC_COND   0x8642ABC0DE5A434FULL  /* ...'CO' */
#define PSX_MAGIC_RWLOCK 0x8642ABC0DE5A5257ULL  /* ...'RW' */

struct psx_hdr {
   uint64_t magic;
   void    *native;
};

/* Registry lock — a REAL native mutex (this file's pthread_* calls bind to
 * libsystem_pthread, never to our shims). Held only across the brief
 * lookup/create, never across a blocking native lock/wait. */
static pthread_mutex_t g_reg_lock = PTHREAD_MUTEX_INITIALIZER;

/* Get (or lazily create) the stable native object for an i386 sync object.
 * `native_sz` is the native struct size; `magic` selects the type; when a fresh
 * object is created and `do_init` is non-NULL it runs the native initializer. */
static void *psx_resolve(void *i386obj, uint64_t magic, size_t native_sz,
                         void (*do_init)(void *native, void *ctx), void *ctx) {
   struct psx_hdr *h = (struct psx_hdr *)i386obj;
   void *nat;
   pthread_mutex_lock(&g_reg_lock);
   if (h->magic == magic) {
      nat = h->native;
   } else {
      nat = calloc(1, native_sz);        /* zeroed, native-sized + aligned */
      if (nat && do_init) do_init(nat, ctx);
      h->native = nat;
      h->magic  = magic;                 /* publish under the lock */
   }
   pthread_mutex_unlock(&g_reg_lock);
   return nat;
}

/* Drop the native object for an i386 object (destroy path). */
static void *psx_take(void *i386obj, uint64_t magic) {
   struct psx_hdr *h = (struct psx_hdr *)i386obj;
   void *nat = NULL;
   pthread_mutex_lock(&g_reg_lock);
   if (h->magic == magic) { nat = h->native; h->native = NULL; h->magic = 0; }
   pthread_mutex_unlock(&g_reg_lock);
   return nat;
}

/* Reconstruct a native attr (sig8 + opaque) from an i386 attr (sig4 + opaque),
 * exactly mirroring abigen's input marshalling (which native pthread already
 * accepts). `out` must be >= 8 + native_opaque; copies min(i386_opaque,
 * native_opaque) opaque bytes and zero-fills the rest. */
static void psx_build_attr(void *out, const void *i386attr,
                           size_t i386_opaque, size_t native_opaque) {
   const unsigned char *src = (const unsigned char *)i386attr;
   unsigned char *dst = (unsigned char *)out;
   int32_t sig;
   int64_t sig8;
   size_t n;
   memset(dst, 0, 8 + native_opaque);
   memcpy(&sig, src, 4);
   sig8 = (int64_t)sig;                   /* sign-extend (movslq), like abigen */
   memcpy(dst, &sig8, 8);
   n = i386_opaque < native_opaque ? i386_opaque : native_opaque;
   memcpy(dst + 8, src + 4, n);
}

/* i386/x86_64 darwin opaque sizes (sig is `long`: 4 vs 8). */
#define I386_MUTEX_OPAQUE 40
#define I386_COND_OPAQUE  24
#define I386_RWLOCK_OPAQUE 124
#define I386_MUTEXATTR_OPAQUE 8
#define I386_CONDATTR_OPAQUE  4

/* ---- mutex ---------------------------------------------------------------- */

struct mtx_init_ctx { void *i386attr; };
static void mtx_do_init(void *native, void *vctx) {
   struct mtx_init_ctx *c = (struct mtx_init_ctx *)vctx;
   if (c && c->i386attr) {
      char nattr[8 + 8];                  /* native pthread_mutexattr_t = 16 */
      psx_build_attr(nattr, c->i386attr, I386_MUTEXATTR_OPAQUE, 8);
      pthread_mutex_init((pthread_mutex_t *)native, (pthread_mutexattr_t *)nattr);
   } else {
      pthread_mutex_init((pthread_mutex_t *)native, NULL);
   }
}

static pthread_mutex_t *mtx_native(uint32_t i386mutex, void *i386attr) {
   struct mtx_init_ctx c; c.i386attr = i386attr;
   return (pthread_mutex_t *)psx_resolve((void *)(uintptr_t)i386mutex,
            PSX_MAGIC_MUTEX, sizeof(pthread_mutex_t), mtx_do_init, &c);
}

int32_t shim_pthread_mutex_init(uint32_t *a) {
   /* a[0]=mutex, a[1]=attr. Re-init: drop any prior native object first. */
   void *old = psx_take((void *)(uintptr_t)a[0], PSX_MAGIC_MUTEX);
   if (old) { pthread_mutex_destroy((pthread_mutex_t *)old); free(old); }
   void *attr = a[1] ? (void *)(uintptr_t)a[1] : NULL;
   pthread_mutex_t *m = mtx_native(a[0], attr);
   if (psx_trace()) fprintf(stderr, "[psx] mutex_init i386=%#x attr=%#x -> %p\n",
                            a[0], a[1], (void *)m);
   return m ? 0 : -1;
}
int32_t shim_pthread_mutex_lock(uint32_t *a) {
   pthread_mutex_t *m = mtx_native(a[0], NULL);
   if (!m) return -1;
   return (int32_t)pthread_mutex_lock(m);
}
int32_t shim_pthread_mutex_trylock(uint32_t *a) {
   pthread_mutex_t *m = mtx_native(a[0], NULL);
   if (!m) return -1;
   return (int32_t)pthread_mutex_trylock(m);
}
int32_t shim_pthread_mutex_unlock(uint32_t *a) {
   pthread_mutex_t *m = mtx_native(a[0], NULL);
   if (!m) return -1;
   return (int32_t)pthread_mutex_unlock(m);
}
int32_t shim_pthread_mutex_destroy(uint32_t *a) {
   void *m = psx_take((void *)(uintptr_t)a[0], PSX_MAGIC_MUTEX);
   if (!m) return 0;                      /* never initialised / already gone */
   int r = pthread_mutex_destroy((pthread_mutex_t *)m);
   free(m);
   return (int32_t)r;
}

/* ---- cond ----------------------------------------------------------------- */

static void cnd_do_init(void *native, void *vi386attr) {
   if (vi386attr) {
      char nattr[8 + 8];                  /* native pthread_condattr_t = 16 */
      psx_build_attr(nattr, vi386attr, I386_CONDATTR_OPAQUE, 8);
      pthread_cond_init((pthread_cond_t *)native, (pthread_condattr_t *)nattr);
   } else {
      pthread_cond_init((pthread_cond_t *)native, NULL);
   }
}
static pthread_cond_t *cnd_native(uint32_t i386cond, void *i386attr) {
   return (pthread_cond_t *)psx_resolve((void *)(uintptr_t)i386cond,
            PSX_MAGIC_COND, sizeof(pthread_cond_t), cnd_do_init, i386attr);
}

int32_t shim_pthread_cond_init(uint32_t *a) {
   void *old = psx_take((void *)(uintptr_t)a[0], PSX_MAGIC_COND);
   if (old) { pthread_cond_destroy((pthread_cond_t *)old); free(old); }
   void *attr = a[1] ? (void *)(uintptr_t)a[1] : NULL;
   pthread_cond_t *c = cnd_native(a[0], attr);
   if (psx_trace()) fprintf(stderr, "[psx] cond_init i386=%#x -> %p\n", a[0], (void *)c);
   return c ? 0 : -1;
}
int32_t shim_pthread_cond_signal(uint32_t *a) {
   pthread_cond_t *c = cnd_native(a[0], NULL);
   return c ? (int32_t)pthread_cond_signal(c) : -1;
}
int32_t shim_pthread_cond_broadcast(uint32_t *a) {
   pthread_cond_t *c = cnd_native(a[0], NULL);
   return c ? (int32_t)pthread_cond_broadcast(c) : -1;
}
int32_t shim_pthread_cond_wait(uint32_t *a) {
   /* a[0]=cond, a[1]=mutex */
   pthread_cond_t  *c = cnd_native(a[0], NULL);
   pthread_mutex_t *m = mtx_native(a[1], NULL);
   if (!c || !m) return -1;
   return (int32_t)pthread_cond_wait(c, m);
}
static void psx_build_timespec(struct timespec *ts, const void *i386ts) {
   const unsigned char *s = (const unsigned char *)i386ts;
   int32_t sec, nsec;
   memcpy(&sec, s, 4);                    /* i386 time_t tv_sec @0 (4 bytes) */
   memcpy(&nsec, s + 4, 4);               /* i386 long  tv_nsec @4 (4 bytes) */
   ts->tv_sec  = (time_t)sec;             /* sign-extend, like abigen */
   ts->tv_nsec = (long)nsec;
}
int32_t shim_pthread_cond_timedwait(uint32_t *a) {
   /* a[0]=cond, a[1]=mutex, a[2]=abstime */
   pthread_cond_t  *c = cnd_native(a[0], NULL);
   pthread_mutex_t *m = mtx_native(a[1], NULL);
   if (!c || !m) return -1;
   if (!a[2]) return (int32_t)pthread_cond_wait(c, m);
   struct timespec ts;
   psx_build_timespec(&ts, (void *)(uintptr_t)a[2]);
   return (int32_t)pthread_cond_timedwait(c, m, &ts);
}
int32_t shim_pthread_cond_timedwait_relative_np(uint32_t *a) {
   /* a[0]=cond, a[1]=mutex, a[2]=reltime */
   pthread_cond_t  *c = cnd_native(a[0], NULL);
   pthread_mutex_t *m = mtx_native(a[1], NULL);
   if (!c || !m) return -1;
   if (!a[2]) return (int32_t)pthread_cond_wait(c, m);
   struct timespec ts;
   psx_build_timespec(&ts, (void *)(uintptr_t)a[2]);
   return (int32_t)pthread_cond_timedwait_relative_np(c, m, &ts);
}
int32_t shim_pthread_cond_destroy(uint32_t *a) {
   void *c = psx_take((void *)(uintptr_t)a[0], PSX_MAGIC_COND);
   if (!c) return 0;
   int r = pthread_cond_destroy((pthread_cond_t *)c);
   free(c);
   return (int32_t)r;
}

/* ---- rwlock --------------------------------------------------------------- */
/* rwlock attrs only carry process-shared, irrelevant in-process: native default
 * init (NULL attr) is correct for any single-process i386 program. */
static void rwl_do_init(void *native, void *unused) {
   (void)unused; pthread_rwlock_init((pthread_rwlock_t *)native, NULL);
}
static pthread_rwlock_t *rwl_native(uint32_t i386rw) {
   return (pthread_rwlock_t *)psx_resolve((void *)(uintptr_t)i386rw,
            PSX_MAGIC_RWLOCK, sizeof(pthread_rwlock_t), rwl_do_init, NULL);
}
int32_t shim_pthread_rwlock_init(uint32_t *a) {
   void *old = psx_take((void *)(uintptr_t)a[0], PSX_MAGIC_RWLOCK);
   if (old) { pthread_rwlock_destroy((pthread_rwlock_t *)old); free(old); }
   pthread_rwlock_t *r = rwl_native(a[0]);
   return r ? 0 : -1;
}
int32_t shim_pthread_rwlock_rdlock(uint32_t *a) {
   pthread_rwlock_t *r = rwl_native(a[0]); return r ? (int32_t)pthread_rwlock_rdlock(r) : -1; }
int32_t shim_pthread_rwlock_wrlock(uint32_t *a) {
   pthread_rwlock_t *r = rwl_native(a[0]); return r ? (int32_t)pthread_rwlock_wrlock(r) : -1; }
int32_t shim_pthread_rwlock_tryrdlock(uint32_t *a) {
   pthread_rwlock_t *r = rwl_native(a[0]); return r ? (int32_t)pthread_rwlock_tryrdlock(r) : -1; }
int32_t shim_pthread_rwlock_trywrlock(uint32_t *a) {
   pthread_rwlock_t *r = rwl_native(a[0]); return r ? (int32_t)pthread_rwlock_trywrlock(r) : -1; }
int32_t shim_pthread_rwlock_unlock(uint32_t *a) {
   pthread_rwlock_t *r = rwl_native(a[0]); return r ? (int32_t)pthread_rwlock_unlock(r) : -1; }
int32_t shim_pthread_rwlock_destroy(uint32_t *a) {
   void *r = psx_take((void *)(uintptr_t)a[0], PSX_MAGIC_RWLOCK);
   if (!r) return 0;
   int rc = pthread_rwlock_destroy((pthread_rwlock_t *)r);
   free(r);
   return (int32_t)rc;
}
