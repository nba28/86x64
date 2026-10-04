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
#include <dlfcn.h>
#include <errno.h>
#include <sched.h>
#include <mach/mach.h>
#include <mach-o/getsect.h>
#include <mach-o/loader.h>

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
/* ── CONDVAR CENSUS (env-gated, off by default) ──────────────────────────────
 * `ABICONV_PSX_CENSUS=<seconds>` starts a thread that periodically prints, per
 * i386 condvar, how many times it was WAITED on versus SIGNALLED. It exists for
 * the one failure this shim cannot distinguish by itself:
 *
 *   waits > 0, signals == 0  ->  nobody ever signals it. The bug is UPSTREAM:
 *                                whatever should produce the wakeup never runs.
 *   signals on a DIFFERENT i386 address than the waiter's
 *                            ->  an IDENTITY problem: signaller and waiter are
 *                                not talking about the same object, which is the
 *                                by-value-marshalling family this file cures.
 *
 * A periodic thread rather than atexit(): a parked process is killed by the
 * harness watchdog, so an exit-time report never prints. Counters are plain
 * relaxed atomics on a fixed open-addressed table -- no allocation, no lock, and
 * no ordering imposed on the code being measured, so arming it cannot change the
 * schedule it is trying to observe. */
#define PSX_CENSUS_SLOTS 512
struct psx_cen {
   /* Plain types, touched only through __atomic_* builtins -- those builtins
    * reject _Atomic-qualified operands, and the qualifier would buy nothing here
    * since every access already names its ordering explicitly. */
   uint32_t key;                          /* i386 object address, 0 = free */
   unsigned long waits, timeouts, signals, broadcasts;
   /* WHO is involved. An object that is waited on and never signalled is only
    * half a diagnosis: the other half is which code waits, and whether that is
    * the MAIN thread (so the whole app is parked) or a worker (idle by design).
    * The MTSHIM trampoline hands us `a = &args[0]` on the i386-cdecl stack, so
    * a[-1] is the caller's return address -- and it is a TRANSLATED address, so
    * dladdr + __86x64_pcmap map it back to the original function. */
   uint32_t wait_caller, signal_caller;
   unsigned char waited_by_main;
};
static struct psx_cen g_cen[PSX_CENSUS_SLOTS];
/* timedwait return-value histogram: 0, ETIMEDOUT, EINTR, EINVAL, EPERM, other */
static unsigned long g_tw_ret[6];
static const char *const g_tw_name[6] =
   { "0 (signalled)", "ETIMEDOUT", "EINTR", "EINVAL", "EPERM", "other" };
static int psx_census_secs(void) {
   static int v = -1;
   if (v < 0) {
      const char *e = getenv("ABICONV_PSX_CENSUS");
      v = e ? atoi(e) : 0;
      if (v < 0) v = 0;
   }
   return v;
}
static struct psx_cen *psx_cen_slot(uint32_t key) {
   if (!key) return NULL;
   uint32_t h = (key * 2654435761u) % PSX_CENSUS_SLOTS;
   for (int probe = 0; probe < 64; ++probe) {
      struct psx_cen *c = &g_cen[(h + probe) % PSX_CENSUS_SLOTS];
      uint32_t k = __atomic_load_n(&c->key, __ATOMIC_RELAXED);
      if (k == key) return c;
      if (k == 0) {
         uint32_t expect = 0;
         if (__atomic_compare_exchange_n(&c->key, &expect, key, 0,
                                         __ATOMIC_RELAXED, __ATOMIC_RELAXED))
            return c;
         if (__atomic_load_n(&c->key, __ATOMIC_RELAXED) == key) return c;
      }
   }
   return NULL;                           /* table full: undercount, never wrong */
}
#define PSX_CEN_BUMP(field, key) do {                                          \
   if (psx_census_secs()) {                                                    \
      struct psx_cen *_c = psx_cen_slot((uint32_t)(key));                      \
      if (_c) __atomic_fetch_add(&_c->field, 1ul, __ATOMIC_RELAXED);           \
   }                                                                           \
} while (0)

/* Record the caller (and whether it is the main thread) the FIRST time only, so
 * a hot poll loop cannot turn this into a write storm. */
#define PSX_CEN_WHO(which, key, argp) do {                                      \
   if (psx_census_secs()) {                                                     \
      struct psx_cen *_c = psx_cen_slot((uint32_t)(key));                       \
      if (_c) {                                                                 \
         if (__atomic_load_n(&_c->which, __ATOMIC_RELAXED) == 0) {              \
            __atomic_store_n(&_c->which, ((const uint32_t *)(argp))[-1],        \
                             __ATOMIC_RELAXED);                                 \
         }                                                                      \
         if (pthread_main_np())                                                 \
            __atomic_store_n(&_c->waited_by_main, 1, __ATOMIC_RELAXED);         \
      }                                                                         \
   }                                                                            \
} while (0)

/* image+offset for a TRANSLATED return address, so the report is greppable and
 * feeds straight into pcmap-diff.py. */
static void psx_describe(uint32_t addr, char *out, size_t n) {
   if (!addr) { snprintf(out, n, "-"); return; }
   Dl_info info;
   if (dladdr((void *)(uintptr_t)addr, &info) && info.dli_fname) {
      const char *base = strrchr(info.dli_fname, '/');
      base = base ? base + 1 : info.dli_fname;
      snprintf(out, n, "%#x = %s+%#lx %s", addr, base,
               (unsigned long)((uintptr_t)addr - (uintptr_t)info.dli_fbase),
               info.dli_sname ? info.dli_sname : "");
   } else {
      snprintf(out, n, "%#x = (no image)", addr);
   }
}

/* ONE-SHOT CALL-CHAIN DUMP for the main thread's wait, in steady state.
 *
 * The census names the IMMEDIATE caller (CThreadSyncObject::Wait) but not who
 * called THAT, and the wall we are chasing is "which Source loop is polling".
 * sample(1) cannot help: it stops at the shim because it cannot unwind through
 * translated frames. So scan the i386-cdecl stack upward from the argument block
 * and report words that point into the __text of a non-system image.
 *
 * ⚠Stepping is FOUR bytes: translated code keeps an i386-granular stack even on
 * the native stack (a call is `pushw %ax; pushw %ax; movl %r11d,(%rsp); jmp`), so
 * 8-byte stepping catches only the accidentally-aligned frames.
 * ⚠This is a CANDIDATE SET, not a call chain -- live and dead frames are mixed.
 * Disassemble each hit before believing it: a translated `call $+0; pop` leaves an
 * ANCHOR VALUE on the stack that looks exactly like a return address, and a stale
 * word can point at any instruction at all. */
static void psx_dump_chain(const uint32_t *a) {
   const char *e = getenv("ABICONV_PSX_MAIN_CHAIN");
   if (!e) return;
   unsigned long after = strtoul(e, NULL, 0);   /* dump on the Nth main wait */
   if (after == 0) after = 1;
   static unsigned long seen = 0;
   static int done = 0;
   if (done || !pthread_main_np()) return;
   if (__atomic_fetch_add(&seen, 1ul, __ATOMIC_RELAXED) < after) return;
   done = 1;
   fprintf(stderr, "[psx] main-thread wait #%lu -- stack words pointing into "
                   "__TEXT,__text of a non-system image (CANDIDATES, not a "
                   "chain; verify by disassembly):\n", after);
   int shown = 0;
   for (int i = -1; i < 1024 && shown < 24; ++i) {
      uint32_t v = a[i];
      if (!v) continue;
      Dl_info info;
      if (!dladdr((void *)(uintptr_t)v, &info) || !info.dli_fname) continue;
      if (strstr(info.dli_fname, "/usr/lib/") || strstr(info.dli_fname, "/System/")
          || strstr(info.dli_fname, "libabiconv")) continue;
      const struct mach_header_64 *mh =
         (const struct mach_header_64 *)info.dli_fbase;
      unsigned long tsz = 0;
      const uint8_t *txt = mh ? getsectiondata(mh, "__TEXT", "__text", &tsz) : NULL;
      if (!txt || (const uint8_t *)(uintptr_t)v < txt ||
          (const uint8_t *)(uintptr_t)v >= txt + tsz) continue;   /* not code */
      const char *base = strrchr(info.dli_fname, '/');
      base = base ? base + 1 : info.dli_fname;
      fprintf(stderr, "[psx]     [a%+d] %#10x  %s+%#lx  %s\n", i, v, base,
              (unsigned long)((uintptr_t)v - (uintptr_t)info.dli_fbase),
              info.dli_sname ? info.dli_sname : "(no symbol)");
      ++shown;
   }
   if (!shown) fprintf(stderr, "[psx]     (none found)\n");
   fflush(stderr);
}

static void *psx_census_thread(void *unused) {
   (void)unused;
   const int secs = psx_census_secs();
   for (;;) {
      struct timespec req = { secs, 0 };
      nanosleep(&req, NULL);
      fprintf(stderr, "[psx] ---- condvar census ----\n");
      int shown = 0;
      for (int i = 0; i < PSX_CENSUS_SLOTS; ++i) {
         uint32_t k = __atomic_load_n(&g_cen[i].key, __ATOMIC_RELAXED);
         if (!k) continue;
         unsigned long w = __atomic_load_n(&g_cen[i].waits, __ATOMIC_RELAXED);
         unsigned long t = __atomic_load_n(&g_cen[i].timeouts, __ATOMIC_RELAXED);
         unsigned long sg = __atomic_load_n(&g_cen[i].signals, __ATOMIC_RELAXED);
         unsigned long b = __atomic_load_n(&g_cen[i].broadcasts, __ATOMIC_RELAXED);
         if (!w && !sg && !b) continue;
         uint32_t wc = __atomic_load_n(&g_cen[i].wait_caller, __ATOMIC_RELAXED);
         uint32_t sc = __atomic_load_n(&g_cen[i].signal_caller, __ATOMIC_RELAXED);
         unsigned char bym =
            __atomic_load_n(&g_cen[i].waited_by_main, __ATOMIC_RELAXED);
         char wloc[128] = "-", sloc[128] = "-";
         psx_describe(wc, wloc, sizeof wloc);
         psx_describe(sc, sloc, sizeof sloc);
         fprintf(stderr, "[psx]   cond %#10x  waits=%-7lu signals=%-5lu "
                         "bcasts=%-5lu %s%s\n"
                         "[psx]        waiter %s\n"
                         "[psx]        signaller %s\n",
                 k, w, sg, b, bym ? "MAIN " : "",
                 (w && !sg && !b) ? "  <-- WAITED, NEVER SIGNALLED" : "", wloc, sloc);
         (void)t;
         ++shown;
      }
      if (!shown) fprintf(stderr, "[psx]   (no condvar traffic yet)\n");
      fprintf(stderr, "[psx]   timedwait returns:");
      for (int r = 0; r < 6; ++r) {
         unsigned long v = __atomic_load_n(&g_tw_ret[r], __ATOMIC_RELAXED);
         if (v) fprintf(stderr, "  %s=%lu", g_tw_name[r], v);
      }
      fprintf(stderr, "\n");
      fflush(stderr);
   }
   return NULL;
}
static void psx_census_start(void) {
   if (!psx_census_secs()) return;
   static int started = 0;
   if (started) return;
   started = 1;
   pthread_t th;
   if (pthread_create(&th, NULL, psx_census_thread, NULL) == 0)
      pthread_detach(th);
}

static pthread_cond_t *cnd_native(uint32_t i386cond, void *i386attr) {
   psx_census_start();
   return (pthread_cond_t *)psx_resolve((void *)(uintptr_t)i386cond,
            PSX_MAGIC_COND, sizeof(pthread_cond_t), cnd_do_init, i386attr);
}

int32_t shim_pthread_cond_init(uint32_t *a) {
   void *old = psx_take((void *)(uintptr_t)a[0], PSX_MAGIC_COND);
   if (old) { pthread_cond_destroy((pthread_cond_t *)old); free(old); }
   void *attr = a[1] ? (void *)(uintptr_t)a[1] : NULL;
   pthread_cond_t *c = cnd_native(a[0], attr);
   return c ? 0 : -1;
}
int32_t shim_pthread_cond_signal(uint32_t *a) {
   PSX_CEN_BUMP(signals, a[0]);
   PSX_CEN_WHO(signal_caller, a[0], a);
   pthread_cond_t *c = cnd_native(a[0], NULL);
   return c ? (int32_t)pthread_cond_signal(c) : -1;
}
int32_t shim_pthread_cond_broadcast(uint32_t *a) {
   PSX_CEN_BUMP(broadcasts, a[0]);
   PSX_CEN_WHO(signal_caller, a[0], a);
   pthread_cond_t *c = cnd_native(a[0], NULL);
   return c ? (int32_t)pthread_cond_broadcast(c) : -1;
}
int32_t shim_pthread_cond_wait(uint32_t *a) {
   /* a[0]=cond, a[1]=mutex */
   PSX_CEN_BUMP(waits, a[0]);
   PSX_CEN_WHO(wait_caller, a[0], a);
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
   PSX_CEN_BUMP(waits, a[0]);
   PSX_CEN_WHO(wait_caller, a[0], a);
   psx_dump_chain(a);
   pthread_cond_t  *c = cnd_native(a[0], NULL);
   pthread_mutex_t *m = mtx_native(a[1], NULL);
   if (!c || !m) return -1;
   if (!a[2]) return (int32_t)pthread_cond_wait(c, m);
   struct timespec ts;
   psx_build_timespec(&ts, (void *)(uintptr_t)a[2]);
   int r = pthread_cond_timedwait(c, m, &ts);
   /* The RETURN VALUE is load-bearing here, not just the wakeup: Source's
    * CThreadSyncObject::Wait loops on it (`cmp $4,%eax; je` for EINTR, then a
    * re-check of its own m_cSet), so a wait that keeps returning 0 without anyone
    * signalling turns a timed wait into an unbounded spin. ⚠Sample the WHOLE run,
    * not the first N calls: the first eight here each blocked ~150us and returned
    * 0, which looked benign, while the aggregate rate was ~1us/call -- the
    * degenerate behaviour only appears in the totals. */
   if (psx_census_secs()) {
      unsigned idx = (r == 0) ? 0 : (r == ETIMEDOUT) ? 1 : (r == EINTR) ? 2
                   : (r == EINVAL) ? 3 : (r == EPERM) ? 4 : 5;
      __atomic_fetch_add(&g_tw_ret[idx], 1ul, __ATOMIC_RELAXED);
      /* How long did it actually block? A "successful" wait that returns in
       * under a microsecond never waited at all. */
      static unsigned long n = 0;
      if ((__atomic_fetch_add(&n, 1ul, __ATOMIC_RELAXED) % 2000000ul) == 1999999ul) {
         struct timespec now;
         clock_gettime(CLOCK_REALTIME, &now);
         fprintf(stderr, "[psx] timedwait sample: ret=%d ts={%lld,%ld} "
                         "now={%lld,%ld} (deadline %lld s away)\n",
                 r, (long long)ts.tv_sec, ts.tv_nsec,
                 (long long)now.tv_sec, now.tv_nsec,
                 (long long)(ts.tv_sec - now.tv_sec));
         fflush(stderr);
      }
   }
   return (int32_t)r;
}
int32_t shim_pthread_cond_timedwait_relative_np(uint32_t *a) {
   /* a[0]=cond, a[1]=mutex, a[2]=reltime */
   PSX_CEN_BUMP(waits, a[0]);
   PSX_CEN_WHO(wait_caller, a[0], a);
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

/* ---- pthread_once --------------------------------------------------------- */
/*
 * abigen forwards _pthread_once straight to native pthread_once, which is wrong
 * two ways: (a) the i386 once-control block is `{ long __sig; char __opaque[4] }`
 * (8 bytes, 4-byte sig) — NOT a valid native os_once_t, so native pthread_once's
 * gate machinery reads a corrupt token and calls _os_once_gate_corruption_abort
 * (Civ IV s30, the wall exposed once import_repair cleared s29's rip=0: the
 * translated QuickTime internalGetPerThreadStorage -> _pthread_once path);
 * (b) the init routine is an i386 `void(void)` code pointer that native
 * pthread_once would invoke with the x86_64 ABI + an 8-byte `ret` over-popping
 * the i386 4-byte frame.
 *
 * Fix (same family as the mutex/cond/rwlock shims): run the once semantics
 * ourselves, keyed on the STABLE low-4GB address of the i386 control block, and
 * invoke the i386 init routine through _86x64_call_i386 on a fresh low-4GB stack
 * (identical to the generic callback bridge). Concurrent callers on the same
 * control block block on the entry lock until the first init completes — the
 * POSIX pthread_once contract. Universal: keyed on the sync-handle shape, not on
 * any app; any translated i386 program's pthread_once is corrected.
 */

/* objc_reverse.asm: lay `nwords` i386 cdecl args + a return frame on the
 * provided low-4GB stack, enter the translated fn, return its eax. */
uint32_t _86x64_call_i386(uint64_t fn, uint64_t nwords, const uint32_t *words,
                          uint64_t lowstack_top);

#define PSX_ONCE_STACK_SZ (1u * 1024u * 1024u)

struct psx_once {
   uint32_t        key;        /* i386 control-block address (stable, <4GB) */
   int             done;       /* 1 once the init routine has completed */
   pthread_mutex_t lk;         /* serializes first-init; POSIX once contract */
   struct psx_once *next;
};
static struct psx_once *g_once_head;    /* under g_reg_lock */

static struct psx_once *once_entry(uint32_t key) {
   struct psx_once *e;
   pthread_mutex_lock(&g_reg_lock);
   for (e = g_once_head; e; e = e->next) {
      if (e->key == key) { pthread_mutex_unlock(&g_reg_lock); return e; }
   }
   e = (struct psx_once *)calloc(1, sizeof(*e));
   if (e) {
      e->key = key;
      pthread_mutex_init(&e->lk, NULL);
      e->next = g_once_head;
      g_once_head = e;
   }
   pthread_mutex_unlock(&g_reg_lock);
   return e;
}

int32_t shim_pthread_once(uint32_t *a);
int32_t shim_pthread_once(uint32_t *a) {
   /* a[0] = pthread_once_t*, a[1] = void(*init)(void) — both i386 addresses. */
   const uint32_t once = a[0];
   const uint32_t init = a[1];
   if (once == 0 || init == 0) { return 0; }

   struct psx_once *e = once_entry(once);
   if (e == NULL) { return -1; }
   if (__atomic_load_n(&e->done, __ATOMIC_ACQUIRE)) { return 0; }

   pthread_mutex_lock(&e->lk);
   if (!e->done) {
      void *stk = malloc(PSX_ONCE_STACK_SZ);   /* low-4GB (this copy's malloc) */
      if (stk == NULL) { pthread_mutex_unlock(&e->lk); return -1; }
      const uint64_t top =
         ((uint64_t)(uintptr_t)stk + PSX_ONCE_STACK_SZ) & ~0xfULL;
      _86x64_call_i386((uint64_t)init, 0, NULL, top);
      free(stk);
      __atomic_store_n(&e->done, 1, __ATOMIC_RELEASE);
   }
   pthread_mutex_unlock(&e->lk);
   return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 * pthread_t IDENTITY  (Portal 2 2026-09-14, s20n)
 * ══════════════════════════════════════════════════════════════════════════
 * abigen treats pthread_t (= struct _opaque_pthread_t *) as a POINTER TO A
 * STRUCT and marshals the struct BY VALUE -- the same disease this file already
 * cures for mutex/cond/rwlock -- so pthread_join/detach/equal/kill each receive
 * a pointer to a fresh COPY instead of the real thread, and ___pthread_self
 * returns the native 8-byte pthread_t TRUNCATED to 32 bits.
 *
 * Worse, and this is the one that actually bit: for pthread_create the pthread_t
 * OUT-PARAM is never copied back, so the caller's 4-byte slot keeps whatever it
 * held before (0).
 *
 * ★WHY IT HID FOR SO LONG. Thread creation still WORKS -- the thread runs, its
 * start routine executes, and a start handshake completes -- so every direct
 * test of "can we create threads" passes. But Valve's CThread::Start does
 *     pthread_create( &this->m_hThread   [+0x98], &attr, ThreadProc, this )
 * and CWorkerThread::Call opens with
 *     if ( m_hThread == 0 ) return -1;      // silent, no dispatch
 * so with the handle left at 0 every CallWorker() quietly no-ops. Because
 * CThreadPool::SuspendExecution dispatches in one loop and collects the replies
 * in a SECOND loop, main then waits forever for a reply to a call it never sent.
 * MEASURED (ABICONV_PSX_CENSUS): m_EventSend signals=0 while the worker polls it
 * 233x, m_EventComplete waits=23M signals=0, 100% CPU on the main thread alone.
 * Guards: 99_pthread_create_handle (ON=42, OFF=9) and 99_pthread_create_arg.
 *
 * REPRESENTATION. A native pthread_t is an 8-byte pointer and the i386 slot is 4
 * bytes, so it cannot be stored raw and MUST NOT be truncated (two live threads
 * can share low 32 bits). We hand the i386 side the thread's MACH PORT
 * (pthread_mach_thread_np) as its 32-bit token: Apple's own identity for a
 * thread, unique among live threads, and never 0 -- which matters, because
 * `m_hThread != 0` is exactly the test real code makes. A small table maps
 * token -> pthread_t so join/detach keep working for a thread that has already
 * exited, where pthread_from_mach_thread_np would no longer resolve it.
 *
 * ⚠ A token is only meaningful while we own the thread. We deliberately do NOT
 * invent tokens for threads we never saw: shim_pthread_self registers the
 * calling thread lazily, so a NATIVE thread that calls into translated code still
 * gets a stable, comparable value. */

#define PSX_MAX_THREADS 512
struct psx_thr { uint32_t token; pthread_t nat; };
static struct psx_thr g_thr[PSX_MAX_THREADS];

/* Register `t` and return its 32-bit token. 0 means "cannot represent". */
static uint32_t psx_thread_token(pthread_t t) {
   if (!t) { return 0; }
   uint32_t tok = (uint32_t)pthread_mach_thread_np(t);
   if (!tok) { return 0; }
   pthread_mutex_lock(&g_reg_lock);
   int free_slot = -1;
   for (int i = 0; i < PSX_MAX_THREADS; ++i) {
      if (g_thr[i].token == tok) { g_thr[i].nat = t; free_slot = -2; break; }
      if (g_thr[i].token == 0 && free_slot < 0) { free_slot = i; }
   }
   if (free_slot >= 0) { g_thr[free_slot].token = tok; g_thr[free_slot].nat = t; }
   pthread_mutex_unlock(&g_reg_lock);
   /* Table full: the token still WORKS via pthread_from_mach_thread_np for a
    * live thread, so degrade rather than fail the create. */
   return tok;
}

static pthread_t psx_thread_lookup(uint32_t tok) {
   if (!tok) { return NULL; }
   pthread_t nat = NULL;
   pthread_mutex_lock(&g_reg_lock);
   for (int i = 0; i < PSX_MAX_THREADS; ++i) {
      if (g_thr[i].token == tok) { nat = g_thr[i].nat; break; }
   }
   pthread_mutex_unlock(&g_reg_lock);
   if (!nat) { nat = pthread_from_mach_thread_np((mach_port_t)tok); }
   return nat;
}

static void psx_thread_forget(uint32_t tok) {
   if (!tok) { return; }
   pthread_mutex_lock(&g_reg_lock);
   for (int i = 0; i < PSX_MAX_THREADS; ++i) {
      if (g_thr[i].token == tok) { g_thr[i].token = 0; g_thr[i].nat = NULL; break; }
   }
   pthread_mutex_unlock(&g_reg_lock);
}

/* cb_bridge.c: bind an i386 fn ptr to a native trampoline slot. Declared here
 * rather than in a header because the descriptor layout is private to that file
 * and typeconv.cc; the two codes we need are CBA_PTR=2 and CBR_PTR=2. */
#define PSX_CB_MAX_ARGS 16
typedef struct {
   uint32_t nargs;
   uint32_t ret_kind;
   uint8_t  arg_kinds[PSX_CB_MAX_ARGS];
} psx_cb_sig;
extern uint64_t x64_cb_wrap(uint32_t fn32, const psx_cb_sig *sig);

/* void *(*start_routine)(void *) -- one pointer in, pointer out. */
static const psx_cb_sig psx_start_sig = { 1, 2, { 2 } };

#define I386_ATTR_OPAQUE 36            /* i386 pthread_attr_t = 4 + 36 */

/* int pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *)
 * and pthread_create_suspended_np (same signature; the thread waits for
 * thread_resume on its Mach port, which IS the token). The generated bridge for
 * the suspended variant deep-converted *pthread_t as a struct and wrote 8 KB
 * past its frame (Portal 2 libmilesx86 sound init, SIGSEGV past the stack top). */
static int32_t psx_create(uint32_t *a, int suspended) {
   uint32_t *slot   = (uint32_t *)(uintptr_t)a[0];
   void     *i386at = a[1] ? (void *)(uintptr_t)a[1] : NULL;
   uint32_t  fn32   = a[2];
   uint32_t  arg32  = a[3];

   if (!fn32) { return EINVAL; }
   uint64_t tramp = x64_cb_wrap(fn32, &psx_start_sig);
   if (!tramp) { return EAGAIN; }

   /* Rebuild a native attr from the i386 one exactly as abigen does (native
    * pthread already accepts that reconstruction). A NULL attr stays NULL. */
   char nattr[8 + 56];
   pthread_attr_t *pat = NULL;
   if (i386at) {
      psx_build_attr(nattr, i386at, I386_ATTR_OPAQUE, 56);
      pat = (pthread_attr_t *)nattr;
   }

   pthread_t nat = NULL;
   int r = (suspended ? pthread_create_suspended_np : pthread_create)(
      &nat, pat, (void *(*)(void *))(uintptr_t)tramp, (void *)(uintptr_t)arg32);
   if (r != 0) { return (int32_t)r; }

   /* ★THE WRITE-BACK. This is the whole point of the shim: without it the
    * caller's handle stays 0 and every later use of it silently no-ops.
    * M64_NO_PTHREAD_TOKEN=1 reproduces the old (broken) behaviour exactly, so the
    * guard has a real OFF arm and a regression can be bisected with one env var
    * instead of a rebuild. */
   static int no_token = -1;
   if (no_token < 0) { no_token = getenv("M64_NO_PTHREAD_TOKEN") ? 1 : 0; }
   uint32_t tok = psx_thread_token(nat);
   if (slot && !no_token) { *slot = tok; }
   return 0;
}
int32_t shim_pthread_create(uint32_t *a) { return psx_create(a, 0); }
int32_t shim_pthread_create_suspended_np(uint32_t *a) { return psx_create(a, 1); }

/* pthread_t pthread_self(void) — returns the TOKEN, so it compares equal to
 * whatever pthread_create handed the creator. */
int32_t shim_pthread_self(uint32_t *a) {
   (void)a;
   return (int32_t)psx_thread_token(pthread_self());
}

int32_t shim_pthread_join(uint32_t *a) {
   pthread_t t = psx_thread_lookup(a[0]);
   if (!t) { return ESRCH; }
   void *ret = NULL;
   int r = pthread_join(t, &ret);
   if (r == 0) {
      psx_thread_forget(a[0]);
      /* void **value_ptr is optional; the returned value is an i386-width
       * pointer coming back from the translated start routine. */
      if (a[1]) { *(uint32_t *)(uintptr_t)a[1] = (uint32_t)(uintptr_t)ret; }
   }
   return (int32_t)r;
}

int32_t shim_pthread_detach(uint32_t *a) {
   pthread_t t = psx_thread_lookup(a[0]);
   if (!t) { return ESRCH; }
   int r = pthread_detach(t);
   if (r == 0) { psx_thread_forget(a[0]); }
   return (int32_t)r;
}

/* Comparing TOKENS is the whole benefit of using the mach port: identity works
 * without resolving either side back to a pthread_t. */
int32_t shim_pthread_equal(uint32_t *a) {
   return (int32_t)(a[0] != 0 && a[0] == a[1]);
}

int32_t shim_pthread_kill(uint32_t *a) {
   pthread_t t = psx_thread_lookup(a[0]);
   if (!t) { return ESRCH; }
   return (int32_t)pthread_kill(t, (int)a[1]);
}

int32_t shim_pthread_cancel(uint32_t *a) {
   pthread_t t = psx_thread_lookup(a[0]);
   if (!t) { return ESRCH; }
   return (int32_t)pthread_cancel(t);
}

/* int pthread_setschedparam(pthread_t, int policy, const struct sched_param *)
 * struct sched_param is { int sched_priority; char __opaque[4] } on both ABIs,
 * so the i386 layout is directly usable. */
int32_t shim_pthread_setschedparam(uint32_t *a) {
   pthread_t t = psx_thread_lookup(a[0]);
   if (!t || !a[2]) { return ESRCH; }
   struct sched_param sp;
   memset(&sp, 0, sizeof sp);
   sp.sched_priority = (int)*(int32_t *)(uintptr_t)a[2];
   return (int32_t)pthread_setschedparam(t, (int)a[1], &sp);
}

/* ---- thread-introspection _np calls, same token identity ----------------
 * abigen deep-copied the pthread_t as a struct pointer, i.e. dereferenced our
 * token (Numbers SFCompatibility: pthread_get_stackaddr_np(0x103) -> SIGSEGV).
 * The STACK answers describe the stack the i386 code actually runs on -- a
 * low-4GB region, not the native thread stack -- read from the VM region that
 * holds this call's own i386 args. Only the calling thread can be described
 * that way; for another thread the native answer is returned when it fits 32
 * bits, else 0 (ponytail: record each translated thread's low stack at
 * create if a target ever asks about a foreign thread). */
#include <mach/mach_vm.h>
static int psx_low_stack(uint32_t *a, uint32_t tok, uint64_t *top, uint64_t *size) {
   if (tok != (uint32_t)pthread_mach_thread_np(pthread_self())) { return 0; }
   mach_vm_address_t addr = (mach_vm_address_t)(uintptr_t)a;
   mach_vm_size_t rsz = 0;
   vm_region_basic_info_data_64_t info;
   mach_msg_type_number_t cnt = VM_REGION_BASIC_INFO_COUNT_64;
   mach_port_t obj = MACH_PORT_NULL;
   if (mach_vm_region(mach_task_self(), &addr, &rsz, VM_REGION_BASIC_INFO_64,
                      (vm_region_info_t)&info, &cnt, &obj) != KERN_SUCCESS) { return 0; }
   *top = addr + rsz; *size = rsz;
   return 1;
}

/* void *pthread_get_stackaddr_np(pthread_t) -- the stack TOP (highest address). */
int32_t shim_pthread_get_stackaddr_np(uint32_t *a) {
   uint64_t top, size;
   if (psx_low_stack(a, a[0], &top, &size)) { return (int32_t)(uint32_t)top; }
   pthread_t t = psx_thread_lookup(a[0]);
   uint64_t v = t ? (uint64_t)(uintptr_t)pthread_get_stackaddr_np(t) : 0;
   return v >> 32 ? 0 : (int32_t)(uint32_t)v;
}

/* size_t pthread_get_stacksize_np(pthread_t) */
int32_t shim_pthread_get_stacksize_np(uint32_t *a) {
   uint64_t top, size;
   if (psx_low_stack(a, a[0], &top, &size)) { return (int32_t)(uint32_t)size; }
   pthread_t t = psx_thread_lookup(a[0]);
   return t ? (int32_t)(uint32_t)pthread_get_stacksize_np(t) : 0;
}

/* mach_port_t pthread_mach_thread_np(pthread_t) -- the token IS the port. */
int32_t shim_pthread_mach_thread_np(uint32_t *a) {
   return psx_thread_lookup(a[0]) ? (int32_t)a[0] : 0;
}

/* int pthread_threadid_np(pthread_t, uint64_t *) -- NULL thread = self. */
int32_t shim_pthread_threadid_np(uint32_t *a) {
   uint64_t *out = (uint64_t *)(uintptr_t)a[1];
   if (!out) { return EINVAL; }
   pthread_t t = a[0] ? psx_thread_lookup(a[0]) : pthread_self();
   if (!t) { *out = 0; return ESRCH; }
   return pthread_threadid_np(t, out);
}

int32_t shim_pthread_getschedparam(uint32_t *a) {
   pthread_t t = psx_thread_lookup(a[0]);
   if (!t) { return ESRCH; }
   int policy = 0;
   struct sched_param sp;
   memset(&sp, 0, sizeof sp);
   int r = pthread_getschedparam(t, &policy, &sp);
   if (r == 0) {
      if (a[1]) { *(int32_t *)(uintptr_t)a[1] = (int32_t)policy; }
      if (a[2]) { *(int32_t *)(uintptr_t)a[2] = (int32_t)sp.sched_priority; }
   }
   return (int32_t)r;
}
