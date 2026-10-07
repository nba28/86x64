/*
 * Hand-written i386->x86_64 shims for CFRunLoop APIs that take C-function
 * CALLBACKS (iPhoto 22nd blocker: observers/timers registered during window
 * bring-up).
 *
 * abigen cannot marshal function-pointer parameters: its generated shim
 * treats `callout` like a pointer-to-struct deep copy and hands the native
 * API the address of an UNINITIALIZED (and immediately dead) stack temp.
 * When CF later fires the observer/timer it jumps into stale stack bytes —
 * the varying window-path crashes (wild jmp to joined 4-byte fn ptrs,
 * bad-isa msgSend).
 *
 * Bridge design:
 *  - Register a NATIVE callout whose context.info points at our slot; the
 *    slot stores the legacy i386 callout + info32 + a dedicated low-4GB
 *    stack. The native callout re-enters the translated world via
 *    _86x64_call_i386 (objc_reverse.asm): i386 cdecl frame on the slot's
 *    low stack.
 *  - The native CFRunLoopObserver/Timer refs live in the HIGH CF heap
 *    (0x6000........), so every ref crossing into the i386 world is wrapped
 *    into a proxy-arena handle (x64_objc_wrap) and every ref coming back is
 *    unwrapped — the same convention as the objc bridge, so CFRelease and
 *    the abigen passthrough shims compose. Only Create (callout + context)
 *    and Invalidate (slot teardown) are hand-shimmed, plus Add/Remove whose
 *    rl/ref/MODE arguments may be handles (CFRunLoopGetCurrent returns and
 *    the kCFRunLoopDefaultMode data-symbol shadows, see data_shadows.syms).
 *  - context.retain/release are dropped: info passes back to the legacy
 *    callout raw; its lifetime is owned by the legacy caller.
 *
 * All reached via MTSHIM trampolines (maptable_tramp.asm); abigen excluded
 * via custom.syms. The export names match the abigen ones, so already-
 * interposed binaries pick these up without re-running static-interpose.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <os/lock.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

uint32_t _86x64_call_i386(uint64_t fn, uint64_t nwords,
                          const uint32_t *words, uint64_t lowstack_top);
uint64_t x64_objc_unwrap(uint32_t h);   /* objc_shim.c: handle -> real ptr */
uint32_t x64_objc_wrap(uint64_t real);  /* objc_shim.c: real ptr -> handle */
void     x64_cb_enter(void);            /* objc_shim.c: cross-copy depth */
void     x64_cb_leave(void);

#define CB_MAX_SLOTS 64
#define CB_STACK_SZ  (256u * 1024u)

typedef struct {
   int       in_use;
   uint32_t  callout32;   /* legacy i386 callout */
   uint32_t  info32;      /* legacy context.info, passed back raw */
   CFTypeRef ref;         /* the native observer/timer (high CF heap) */
} cb_slot_t;

static cb_slot_t      g_cb[CB_MAX_SLOTS];
static os_unfair_lock g_cb_lock = OS_UNFAIR_LOCK_INIT;

static cb_slot_t *cb_slot_alloc(void) {
   os_unfair_lock_lock(&g_cb_lock);
   for (int i = 0; i < CB_MAX_SLOTS; ++i) {
      if (!g_cb[i].in_use) {
         g_cb[i].in_use = 1;
         os_unfair_lock_unlock(&g_cb_lock);
         return &g_cb[i];
      }
   }
   os_unfair_lock_unlock(&g_cb_lock);
   return NULL;
}

static void cb_slot_release_by_ref(CFTypeRef ref) {
   if (!ref) { return; }
   os_unfair_lock_lock(&g_cb_lock);
   for (int i = 0; i < CB_MAX_SLOTS; ++i) {
      if (g_cb[i].in_use && g_cb[i].ref == ref) {
         g_cb[i].ref = NULL;
         g_cb[i].callout32 = 0;
         g_cb[i].in_use = 0;          /* keep lowstack for reuse */
         break;
      }
   }
   os_unfair_lock_unlock(&g_cb_lock);
}

/* Zero-I/O breadcrumb ring for CF run-loop callouts (observers/timers). This
 * path enters translated code via _86x64_call_i386 just like cb_bridge, but is
 * NOT counted in cb_bridge's depth — instrument it separately so a crash inside
 * a CF callout is visible. */
struct cf_crumb { uint32_t callout32; uint32_t kind; uint64_t stk_lo;
                  uint64_t stk_hi; uint32_t tid; uint32_t depth; uint32_t done; };
#define CF_RING 64
static struct cf_crumb g_cf_ring[CF_RING];
static uint32_t        g_cf_pos;
static int             g_cf_depth;

void x64_cf_dump_ring(void);
void x64_cf_dump_ring(void) {
   uint32_t pos = __atomic_load_n(&g_cf_pos, __ATOMIC_SEQ_CST);
   fprintf(stderr, "[cf-ring] last CF callouts (newest first), depth=%d:\n",
           __atomic_load_n(&g_cf_depth, __ATOMIC_SEQ_CST));
   for (int i = 0; i < CF_RING; ++i) {
      uint32_t idx = (pos - 1 - i) & (CF_RING - 1);
      struct cf_crumb *c = &g_cf_ring[idx];
      if (!c->callout32 && !c->stk_lo) continue;
      fprintf(stderr, "  [%2d] callout=0x%-8x kind=%s d=%u %s t=%x stk=[0x%llx..0x%llx]\n",
              i, c->callout32, c->kind ? "timer" : "observer", c->depth,
              c->done ? "ret" : "IN ", c->tid,
              (unsigned long long)c->stk_lo, (unsigned long long)c->stk_hi);
   }
   fflush(stderr);
}

/* A fresh low-4GB stack PER INVOCATION: a repeating timer/observer callout
 * that pumps the run loop re-enters the same slot's callout before the outer
 * call returns — a shared per-slot stack would be smashed (the reverse ObjC
 * IMP mallocs per call for the same reason). */
static uint32_t cb_call(cb_slot_t *s, uint32_t nwords, const uint32_t *words) {
   void *stk = malloc(CB_STACK_SZ);
   if (!stk) { return 0; }
   uint64_t top = ((uint64_t)(uintptr_t)stk + CB_STACK_SZ) & ~0xfULL;

   uint32_t depth = (uint32_t)__atomic_add_fetch(&g_cf_depth, 1, __ATOMIC_SEQ_CST);
   uint32_t ri = __atomic_fetch_add(&g_cf_pos, 1, __ATOMIC_SEQ_CST) & (CF_RING - 1);
   struct cf_crumb *c = &g_cf_ring[ri];
   c->callout32 = s->callout32; c->kind = (nwords == 2);
   c->stk_lo = (uint64_t)(uintptr_t)stk; c->stk_hi = top;
   c->tid = pthread_mach_thread_np(pthread_self()); c->depth = depth; c->done = 0;

   x64_cb_enter();
   uint32_t r = _86x64_call_i386(s->callout32, nwords, words, top);
   x64_cb_leave();

   c->done = 1;
   __atomic_sub_fetch(&g_cf_depth, 1, __ATOMIC_SEQ_CST);
   free(stk);
   return r;
}

/* Run-loop MODE argument: legacy code passes the value of its low-4GB
 * shadow for kCFRunLoopDefaultMode/CommonModes — an arena HANDLE for the
 * (high, dyld-shared-cache) real constant. Unwrap; a raw low value (an
 * NSString the legacy code built) passes through; 0 falls back to common. */
static CFStringRef conv_mode(uint32_t m) {
   uint64_t v = x64_objc_unwrap(m);
   return v ? (CFStringRef)(uintptr_t)v : kCFRunLoopCommonModes;
}

/* ---- CFRunLoopObserver ---- */

static void cb_observer_callout(CFRunLoopObserverRef observer,
                                CFRunLoopActivity activity, void *info)
{
   cb_slot_t *s = (cb_slot_t *)info;
   if (!s || !s->in_use || !s->callout32) { return; }
   uint32_t words[3] = { x64_objc_wrap((uint64_t)(uintptr_t)observer),
                         (uint32_t)activity, s->info32 };
   cb_call(s, 3, words);
}

/* CFRunLoopObserverRef CFRunLoopObserverCreate(allocator, activities,
 *      repeats, order, callout, context);
 * i386 frame: allocator[0] activities[1] repeats[2] order[3] callout[4]
 *             context[5] -> {version,info,retain,release,copyDesc} (4B each). */
uint32_t shim_CFRunLoopObserverCreate(uint32_t *a)
{
   if (!a[4]) { return 0; }
   cb_slot_t *s = cb_slot_alloc();
   if (!s) { return 0; }
   const uint32_t *ctx32 = (const uint32_t *)(uintptr_t)a[5];
   s->callout32 = a[4];
   s->info32    = ctx32 ? ctx32[1] : 0;

   CFRunLoopObserverContext nctx;
   memset(&nctx, 0, sizeof nctx);
   nctx.info = s;
   CFRunLoopObserverRef o =
      CFRunLoopObserverCreate(kCFAllocatorDefault, (CFOptionFlags)a[1],
                              (Boolean)a[2], (CFIndex)(int32_t)a[3],
                              cb_observer_callout, &nctx);
   s->ref = o;
   if (!o) { s->in_use = 0; }
   /* the real ref is high-heap; truncating it loses the object — hand the
    * i386 caller a proxy-arena handle instead (CFRelease/Add/Remove unwrap) */
   return x64_objc_wrap((uint64_t)(uintptr_t)o);
}

/* void CFRunLoopAddObserver(rl, observer, mode); i386: rl[0] obs[1] mode[2]. */
void shim_CFRunLoopAddObserver(uint32_t *a)
{
   CFRunLoopAddObserver((CFRunLoopRef)(uintptr_t)x64_objc_unwrap(a[0]),
                        (CFRunLoopObserverRef)(uintptr_t)x64_objc_unwrap(a[1]),
                        conv_mode(a[2]));
}

void shim_CFRunLoopRemoveObserver(uint32_t *a)
{
   CFRunLoopRemoveObserver((CFRunLoopRef)(uintptr_t)x64_objc_unwrap(a[0]),
                           (CFRunLoopObserverRef)(uintptr_t)x64_objc_unwrap(a[1]),
                           conv_mode(a[2]));
}

void shim_CFRunLoopObserverInvalidate(uint32_t *a)
{
   CFRunLoopObserverRef o =
      (CFRunLoopObserverRef)(uintptr_t)x64_objc_unwrap(a[0]);
   if (!o) { return; }
   CFRunLoopObserverInvalidate(o);
   cb_slot_release_by_ref(o);
}

/* ---- CFRunLoopTimer ---- */

static void cb_timer_callout(CFRunLoopTimerRef timer, void *info)
{
   cb_slot_t *s = (cb_slot_t *)info;
   if (!s || !s->in_use || !s->callout32) { return; }
   uint32_t words[2] = { x64_objc_wrap((uint64_t)(uintptr_t)timer),
                         s->info32 };
   cb_call(s, 2, words);
}

/* CFRunLoopTimerRef CFRunLoopTimerCreate(allocator, fireDate, interval,
 *      flags, order, callout, context);
 * i386 frame: allocator[0] fireDate[1,2](double) interval[3,4](double)
 *             flags[5] order[6] callout[7] context[8]. */
uint32_t shim_CFRunLoopTimerCreate(uint32_t *a)
{
   if (!a[7]) { return 0; }
   cb_slot_t *s = cb_slot_alloc();
   if (!s) { return 0; }
   const uint32_t *ctx32 = (const uint32_t *)(uintptr_t)a[8];
   s->callout32 = a[7];
   s->info32    = ctx32 ? ctx32[1] : 0;

   CFAbsoluteTime fireDate;
   CFTimeInterval interval;
   memcpy(&fireDate, &a[1], 8);
   memcpy(&interval, &a[3], 8);

   CFRunLoopTimerContext nctx;
   memset(&nctx, 0, sizeof nctx);
   nctx.info = s;
   CFRunLoopTimerRef t =
      CFRunLoopTimerCreate(kCFAllocatorDefault, fireDate, interval,
                           (CFOptionFlags)a[5], (CFIndex)(int32_t)a[6],
                           cb_timer_callout, &nctx);
   s->ref = t;
   if (!t) { s->in_use = 0; }
   return x64_objc_wrap((uint64_t)(uintptr_t)t);
}

/* void CFRunLoopAddTimer(rl, timer, mode); i386: rl[0] timer[1] mode[2]. */
void shim_CFRunLoopAddTimer(uint32_t *a)
{
   CFRunLoopAddTimer((CFRunLoopRef)(uintptr_t)x64_objc_unwrap(a[0]),
                     (CFRunLoopTimerRef)(uintptr_t)x64_objc_unwrap(a[1]),
                     conv_mode(a[2]));
}

void shim_CFRunLoopRemoveTimer(uint32_t *a)
{
   CFRunLoopRemoveTimer((CFRunLoopRef)(uintptr_t)x64_objc_unwrap(a[0]),
                        (CFRunLoopTimerRef)(uintptr_t)x64_objc_unwrap(a[1]),
                        conv_mode(a[2]));
}

void shim_CFRunLoopTimerInvalidate(uint32_t *a)
{
   CFRunLoopTimerRef t =
      (CFRunLoopTimerRef)(uintptr_t)x64_objc_unwrap(a[0]);
   if (!t) { return; }
   CFRunLoopTimerInvalidate(t);
   cb_slot_release_by_ref(t);
}
