/*
 * objc_shim.c — legacy-i386-ObjC → modern-x86_64-ObjC bridge runtime.
 *
 * The translated i386 program calls `objc_msgSend` with the i386 cdecl ABI
 * (self, _cmd, args all as 4-byte stack slots) and holds every `id` as a
 * 32-bit value. The real x86_64 objc_msgSend wants self/_cmd/args in
 * registers, and real objc objects live at 64-bit addresses that do not fit
 * in the program's 32-bit slots.
 *
 * This file provides the C side of the bridge:
 *   - a proxy table that maps real 64-bit object pointers <-> 32-bit handles
 *     (handles are addresses of cells in a low-4GB arena, so the translated
 *     code can store them in its 32-bit slots);
 *   - objc_bridge_prep(), which the __objc_msgSend asm trampoline calls to
 *     resolve self/_cmd and lay out the x86_64 register arguments.
 *
 * The asm trampoline (objc_msgSend.asm) handles the i386 stack ABI and the
 * actual register marshalling / tail call into the real objc_msgSend.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <objc/runtime.h>
#include <objc/message.h>
extern id objc_retain(id);   /* libobjc ARC entrypoint; not in runtime.h */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <sys/mman.h>
#include <pthread.h>
#include <os/lock.h>

/* Low-4GB search window — same range the wrapper/malloc shim use. */
#define LOW_REGION_BASE 0x080000000UL
#define LOW_REGION_END  0x0F0000000UL

/* env-gated (ABICONV_CALLRING) per-thread reverse-bridge call-ring diagnostic
 * (callring_shim.c) — inert unless ABICONV_CALLRING is set. _send captures the
 * pending send at prep; _ret completes it with the native return value at the
 * wrap/finish return site. */
int  _86x64_callring_enabled(void);
void _86x64_callring_arm(void);
void _86x64_callring_send(uint64_t self, uint64_t sel, uint32_t caller_ra);
void _86x64_callring_ret(uint64_t ret);

/* Debug-trace env flags, resolved ONCE. These gate diagnostic fprintf paths on
 * the ObjC/CFString bridge HOT PATH (i386_cfstr_to_real's reject branch, the
 * wrap/class-lookup tracers). getenv() does a LINEAR SCAN of the environment;
 * calling it per-conversion turned a rejected-CFString candidate in a tight
 * loop into a multi-minute 100%-CPU spin (Halo startup: millions of
 * non-CFString pointers hit the reject branch -> BRIDGE_TRACE()
 * each time). Cache the answer — the environment never changes mid-run. */
static int obj_trace_flag(const char *name, int *cache) {
   int v = *cache;
   if (__builtin_expect(v < 0, 0)) { v = getenv(name) != NULL; *cache = v; }
   return v;
}
static int g_bridge_trace_cache = -1;
static int g_wrap_trace_cache   = -1;
static int g_argstr_trace_cache = -1;
#define BRIDGE_TRACE() obj_trace_flag("OBJC_BRIDGE_TRACE", &g_bridge_trace_cache)
#define WRAP_TRACE()   obj_trace_flag("OBJC_WRAP_TRACE",   &g_wrap_trace_cache)
/* Narrow, low-noise diagnostic (NOT the OBJC_BRIDGE_TRACE firehose): dump the
 * RESOLVED content of every object arg unwrapped for a native call, with the
 * caller's return address. Used to answer "what string does Civ's
 * CreateStandardAlert actually pass, and does it round-trip non-empty" without
 * drowning in Halo/Civ's millions of non-CFString unwrap probes. */
#define ARGSTR_TRACE() obj_trace_flag("ABICONV_ARGSTR_TRACE", &g_argstr_trace_cache)
/* Name the TRANSLATED CALL SITE behind an object/CF arg that a native framework
 * will certainly fault on. Neither of the obvious channels can do this: the
 * crashlog unwinder keeps only the frames ABOVE our shim (it cannot walk the
 * i386 frame below it), and ARGSTR_TRACE's caller= is _86x64_unwrap_obj_arg's
 * own return address — the same value on every line. See the recovery trick at
 * _86x64_unwrap_obj_arg. CALLSITE reports a non-nil SUB-PAGE result, which is
 * never legitimate; CALLSITE_NIL additionally reports nil, which IS legitimate
 * for e.g. a NULL CFAllocatorRef and is therefore opt-in and noisy. */
static int g_callsite_trace_cache = -1;
static int g_callsite_nil_cache   = -1;
static int g_callsite_all_cache   = -1;
#define CALLSITE_TRACE()     obj_trace_flag("ABICONV_CALLSITE_TRACE", &g_callsite_trace_cache)
#define CALLSITE_NIL_TRACE() obj_trace_flag("ABICONV_CALLSITE_NIL",   &g_callsite_nil_cache)
/* Firehose: EVERY bridged object/CF arg with its call site. Use when the bad
 * value never reaches the selective traces above — then the LAST line before the
 * crash names the call that died, regardless of what the value looked like. */
#define CALLSITE_ALL_TRACE() obj_trace_flag("ABICONV_CALLSITE_ALL", &g_callsite_all_cache)

/*
 * Proxy arena: a flat low-4GB array of 64-bit reals. A "handle" is the
 * 32-bit address of one slot. unwrap(handle) = *slot. The arena must live
 * below 4GB so handles fit in the translated program's 32-bit id slots.
 */
#define ARENA_SLOTS (1u << 20)            /* 1M proxied objects */
#define MAP_CAP     (1u << 21)
struct map_ent { uint64_t real; uint32_t handle; };

/*
 * Cross-copy shared state. The translated bundle co-locates one
 * libabiconv.dylib per framework, and dyld loads each PATH as a SEPARATE image
 * with its own __DATA. Plain module statics would therefore give every copy
 * its OWN proxy arena + dedup map, so a 32-bit handle minted by one copy would
 * be meaningless to another — and handles cross framework boundaries on nearly
 * every message. Worse, each copy's arena_init would re-run, and the map
 * allocation routes through libabiconv's own malloc shim (it defines calloc),
 * which a later copy's shim region can't satisfy ("proxy map alloc failed").
 *
 * Fix: keep ONE arena/map per process. The first copy to need them creates the
 * arena (low-4GB, so handles fit in 32 bits), the dedup map, and this control
 * block, then publishes the block's address via the process environment (one
 * `environ` shared by all copies). Every other copy reads it back and adopts
 * the same pointers — all copies share a single address space, so the pointers
 * are directly usable. The map is allocated with mach_vm_allocate (not the
 * shim's calloc) so it never depends on a per-copy heap.
 */
/* Shared page-readability memo (see page_readable_len). Genuinely-readable
 * byte extent from a 4K page base, determined once with real reads and reused
 * by every libabiconv copy — readability is a process-global fact, so without
 * sharing each of the ~12 active copies re-probes the same metadata pages with
 * a cold cache (the iWeb startup hang: 14 images × 12 copies of syscalls). */
#define PGMEMO_CAP 262144u   /* power of two; holds mapped pages AND cached
                              * negative (unmapped garbage-page) verdicts, so
                              * size well above both to avoid probe-degrading
                              * saturation */
/* rb = readable byte extent from page base (0..4096). rb>0 is permanent (a
 * mapped page's readability is stable for the life of its mapping). rb==0
 * (unmapped) is valid only for image generation `gen` = _dyld_image_count at
 * probe time, since a later dlopen could map the page — without caching the
 * negative, the same garbage metadata pointer is re-probed millions of times
 * (the iWeb startup storm: 3M+ repeated unmapped syscalls). */
struct pgmemo_ent { uintptr_t pg; uint32_t gen; uint16_t rb; uint8_t used; };

struct objc_shared_ctrl {
   uint64_t        magic;        /* OBJC_CTRL_MAGIC once fully initialized */
   uint64_t        pgmemo;       /* struct pgmemo_ent *[PGMEMO_CAP], shared */
   uint64_t        arena;        /* low-4GB slot array base */
   uint64_t        arena_base;
   uint64_t        arena_end;
   uint32_t        arena_used;   /* the one shared allocation cursor */
   struct map_ent *map;          /* real->handle dedup table */
   /* i386 instance-shadow arena (legacy class registration). Shared across the
    * N libabiconv copies so resolve_self in ANY copy recognizes a shadow that
    * another copy created (single address space). shadow_cur is the bump
    * cursor; the registering copy is the only one that allocates. */
   uint64_t        shadow_base;
   uint64_t        shadow_cur;
   uint64_t        shadow_end;
   /* Per-thread super-dispatch hints (legacy [super sel] -> reverse bridge).
    * MUST live in the shared ctrl, not a per-copy __thread var: the forward
    * super bridge (objc_bridge_prep_super) runs in the caller's libabiconv
    * copy, but reverse_prep always runs in the REGISTERING copy — different
    * copies, different statics. Keyed by thread id; (recv,sel) matched on
    * consume so a hash collision from another thread is a safe miss. */
   struct { uint64_t tid; uint64_t recv; uint64_t sel; uint64_t lookup;
            int valid; } super_hints[64];
   /* legacy i386 object -> real modern object pair table (14th iPhoto blocker).
    * Shared so every libabiconv copy agrees on which modern R' stands for a
    * given raw i386 instance. Allocated by the first copy (see arena_init). */
   uint64_t        lpair;        /* struct lpair_ent * (open-addressed) */
   /* reverse-registered class -> i386 instance size, shared so any copy's
    * get_or_create_shadow sizes the shadow buffer right (see struct rcls_ent). */
   uint64_t        rcls;         /* struct rcls_ent * (open-addressed) */
   /* Legacy ObjC1 setjmp-exception chain (NS_DURING): per-thread top of the
    * _objc_exception_data list. MUST be cross-copy: try_enter runs in one
    * image's adjacent libabiconv copy, the matching objc_exception_throw can
    * run in another's. Slot claimed by tid (open-addressed, CAS on tid);
    * top==0 marks the slot reclaimable. */
   struct { uint64_t tid; uint32_t top; uint32_t _pad; } exc_chain[64];
   /* Process-global "claim once" flag for the locale objectForKey: swizzle.
    * MUST be cross-copy: every libabiconv copy runs legacy_locale_compat_install,
    * and if more than one swizzles -[NSUserDefaults objectForKey:] they chain
    * mt_compat onto the previous copy's mt_compat — a broken link in that chain
    * collapses EVERY defaults read to nil (iPhoto then can't read RootDirectory
    * -> "library missing"). Only the winner of this CAS swizzles, so it captures
    * the REAL Foundation IMP. */
   uint32_t        locale_ofk_done;
   /* Guards the open-addressed real->handle dedup table `map`. The arena slot
    * cursor (`arena_used`) bumps atomically, but the map probe+insert is a
    * read-modify-write that two threads (RedRock spins up DB threads while the
    * main thread inits) can interleave: a hash collision between two concurrent
    * inserts tears an entry (one thread's `real` paired with the other's
    * `handle`), so a later wrap returns a handle that unwraps to the WRONG
    * object — intermittently faulting deep in native code. MUST be cross-copy
    * (lives in the shared ctrl): threads in different libabiconv copies share
    * the one `map`. os_unfair_lock is valid in shared memory (mach-port owner).
    * Zero-initialized by the calloc of the ctrl == OS_UNFAIR_LOCK_INIT. */
   os_unfair_lock  map_lock;
   /* Cross-copy count of native->translated callbacks currently executing
    * (cb_bridge x64_cb_dispatch, cf_callback cb_call, reverse-IMP). Per-copy
    * counters miss a callback running through a DIFFERENT libabiconv copy than
    * the one a crash handler dumps. */
   int             cb_active_depth;
   /* Cross-copy registry of VARIADIC legacy methods (Class,SEL)->present. The
    * forward bridge runs in the CALLER's libabiconv copy but legacy methods are
    * registered (and their per-copy g_rmeth populated) only in the REGISTERING
    * copy, so the forward side can't tell a legacy nil-terminated id-list method
    * (e.g. -[ATAnimationGroup addAnimations:a,b,nil]) from a 1-arg one. The
    * registering copy publishes variadic legacy methods here (Class/SEL are
    * process-global), so any copy's forward bridge spills the full id list.
    * Open-addressed struct rvar_ent *, allocated by the first copy. */
   uint64_t        rvariadic;
   /* Cross-copy registry of every copy's _86x64_reverse_imp(+_stret) address.
    * method_is_legacy() decides marshalling CONVENTIONS (CONV_I386 vs
    * CONV_NATIVE) by "is this Method's IMP a reverse trampoline" — but each
    * libabiconv copy has its OWN trampoline address, so a per-copy equality
    * test misses legacy methods a DIFFERENT copy registered (e.g. Quinn's
    * bundled-framework ATViewAnimation vs Quinn.dylib's copy). The forward
    * bridge then marshalled with NATIVE conventions ('f' as 4-byte float,
    * 'q' as one slot) while the registering copy's reverse_prep rebuilt the
    * i386 frame with LEGACY conventions ('f' widened double) -> setProgress:
    * received a denormal ~0 -> Quinn splash rendered fully transparent.
    * Each copy publishes its two trampolines at arena attach. */
   uint64_t        rev_imps[16];
   /* Cross-copy set of mach_headers ANY libabiconv copy has already claimed for
    * slide_objc processing (the slide/ObjC-register/run-inits per-image work).
    * MUST be cross-copy: dyld runs each co-located copy's own
    * _dyld_register_func_for_add_image(slide_objc), so a SECOND copy's
    * registration re-fires slide_objc over every already-mapped image with its
    * OWN (empty) per-copy processed-set and re-walks metadata the FIRST copy
    * already processed / is mid-processing — reading a not-yet-ready pointer
    * field back as NULL and dereferencing NULL+offset (the intermittent
    * slide_objc `[rbx+0x88]`, rbx=0 crash; Civ IV multicopy, ~40% of launches).
    * The per-image work is once-per-IMAGE, never once-per-copy, so a single
    * claim across all copies is exactly correct. Open-addressed on the 64-bit
    * header pointer; claimed via CAS under proc_img_lock. A 0 slot is free
    * (a mapped mach_header is never NULL). Generalizes a501466's zerofill
    * partial cure to every slide_objc pass. */
   os_unfair_lock  proc_img_lock;
   uint64_t        proc_img;     /* uint64_t[PROC_IMG_CAP], allocated by copy 0 */
};
#define PROC_IMG_CAP 16384u
#define SUPER_HINT_SLOTS 64
#define OBJC_CTRL_MAGIC 0x3836583634415243ULL  /* "86X64ARC" */
#define OBJC_CTRL_ENV   "ABICONV_OBJC_CTRL"

/* legacy i386 object pointer -> real modern id (paired proxy). */
struct lpair_ent { uint32_t p; uint64_t real; };
#define LPAIR_CAP (1u << 16)

/* (Class,SEL) of a variadic legacy method, shared cross-copy (see ctrl
 * ->rvariadic). Open-addressed; an entry's presence == "this legacy method
 * takes a nil-terminated id list past its declared arg". */
struct rvar_ent { Class cls; SEL sel; };
#define RVAR_CAP 8192u

/* reverse-registered legacy class -> (modern Class, legacy isa addr, i386
 * instance size). Lives in the shared g_ctrl so get_or_create_shadow in ANY
 * libabiconv copy sizes the i386 shadow buffer correctly: a per-copy table
 * misses for a class another copy registered, defaulting isz to 64 and
 * UNDER-allocating the shadow, so the i386 init overruns into the next
 * shadow's real-object header (-> object_getClass on a garbage receiver). */
struct rcls_ent { Class cls; uint32_t legacy_addr; uint32_t instance_size; };
#define RCLS_CAP 8192u
/* Per-shadow trailing slack: absorbs i386 -init writes beyond a class's
 * (under-reported) instance_size so they can't reach the next shadow's
 * real-object header. Cheap — the shadow arena is 64MB. */
#define SHADOW_SLACK 1024u

static struct objc_shared_ctrl *g_ctrl = NULL;
/* hot-path caches; identical across copies once attached to the shared ctrl */
static uint64_t       *g_arena      = NULL;
static struct map_ent *g_map        = NULL;
static uintptr_t       g_arena_base = 0;
static uintptr_t       g_arena_end  = 0;

extern void _86x64_reverse_imp(void);        /* objc_reverse.asm */
extern void _86x64_reverse_imp_stret(void);

/* Publish THIS copy's reverse trampolines in the shared registry (see the
 * rev_imps field comment). Idempotent; CAS-safe against racing copies. */
static void rev_imp_publish(struct objc_shared_ctrl *c) {
   uint64_t mine[2] = { (uint64_t)(uintptr_t)&_86x64_reverse_imp,
                        (uint64_t)(uintptr_t)&_86x64_reverse_imp_stret };
   for (int k = 0; k < 2; k++) {
      for (unsigned i = 0; i < sizeof c->rev_imps / sizeof c->rev_imps[0]; i++) {
         uint64_t cur = __atomic_load_n(&c->rev_imps[i], __ATOMIC_SEQ_CST);
         if (cur == mine[k]) { break; }               /* already published */
         if (cur == 0) {
            uint64_t expect = 0;
            if (__atomic_compare_exchange_n(&c->rev_imps[i], &expect, mine[k],
                                            0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
               break;
            }
            if (__atomic_load_n(&c->rev_imps[i], __ATOMIC_SEQ_CST) == mine[k]) {
               break;                                  /* lost race to ourselves */
            }
         }
      }
   }
}

static void arena_attach(struct objc_shared_ctrl *c) {
   g_ctrl       = c;
   g_arena      = (uint64_t *)(uintptr_t)c->arena;
   g_arena_base = (uintptr_t)c->arena_base;
   g_arena_end  = (uintptr_t)c->arena_end;
   g_map        = c->map;
   rev_imp_publish(c);
}

/* rc=139 fix: the ONE shared ctrl is published through a FIXED low-4GB
 * rendezvous word + an atomic compare-exchange, replacing the old setenv/getenv
 * publish. That env publish was a getenv-before-setenv TOCTOU: two co-located
 * libabiconv copies whose arena_inits raced each "won first" -> two ctrls ->
 * two proc_img claim tables -> the SAME image's (Civ.dylib) static initializers
 * ran TWICE -> FConsole's intrusive-list registry NULL-spliced at +0x88 (the
 * residual ~1/5 rc=139 startup crash). All copies share one address space, so a
 * fixed address needs no discovery channel, and mach_vm_allocate(FIXED) + a CAS
 * are globally atomic -> exactly one copy creates, every other adopts. */
#define ARENA_RENDEZVOUS_ADDR      LOW_REGION_BASE   /* one dedicated low-4GB page */
#define ARENA_RENDEZVOUS_CREATING  1ULL              /* sentinel: a creator is in flight */

/* Map (once, kernel-atomically across the copies) the fixed rendezvous page and
 * return its 8-byte atomic word (= the shared ctrl pointer, 0 until published).
 * Exactly one copy's FIXED allocate maps the fresh zeroed page; the rest get an
 * error because the range is already taken — either way the page is present at
 * the fixed address afterward. The arena scan below tolerates this page being
 * occupied (it simply picks the next free slot). */
static uint64_t *arena_rendezvous_word(void) {
   static uint64_t *slot;                 /* per-copy memo */
   if (slot) { return slot; }
   mach_vm_address_t r = ARENA_RENDEZVOUS_ADDR;
   (void)mach_vm_allocate(mach_task_self(), &r, 0x1000, VM_FLAGS_FIXED);
   slot = (uint64_t *)(uintptr_t)ARENA_RENDEZVOUS_ADDR;
   return slot;
}

static void arena_init(void) {
   if (g_ctrl) { return; }

   /* Test-only pre-fix demonstrator: with ABICONV_ARENA_PUBLISH_GAP set we skip
    * the rendezvous entirely so every copy creates its own ctrl (>=2 `[arena]
    * create` lines), reproducing the OLD race for the tests-i386 guard. Never
    * set in production. */
   const int gap = getenv("ABICONV_ARENA_PUBLISH_GAP") != NULL;
   uint64_t *slot = gap ? NULL : arena_rendezvous_word();

   if (slot) {
      for (;;) {
         uint64_t cur = __atomic_load_n(slot, __ATOMIC_ACQUIRE);
         if (cur == 0) {
            /* Try to claim creation rights atomically. */
            uint64_t expect = 0;
            if (__atomic_compare_exchange_n(slot, &expect,
                                            ARENA_RENDEZVOUS_CREATING, false,
                                            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
               break;                      /* we won -> create below */
            }
            continue;                      /* another copy claimed; re-read */
         }
         if (cur == ARENA_RENDEZVOUS_CREATING) {
            /* A sibling is mid-create; wait for it to publish the real ctrl. */
            for (int i = 0; i < 1000000 &&
                 __atomic_load_n(slot, __ATOMIC_ACQUIRE) == ARENA_RENDEZVOUS_CREATING;
                 ++i) { }
            continue;
         }
         /* A real ctrl pointer is published: adopt it (spin briefly for magic,
          * which the creator sets just before the release store below). */
         struct objc_shared_ctrl *c = (struct objc_shared_ctrl *)(uintptr_t)cur;
         volatile uint64_t *magicp = &c->magic;
         for (int i = 0; i < 1000000 && *magicp != OBJC_CTRL_MAGIC; ++i) { }
         if (getenv("ABICONV_ARENA_TRACE")) {
            fprintf(stderr, "[arena] attach 0x%llx\n",
                    (unsigned long long)(uintptr_t)c); fflush(stderr);
         }
         arena_attach(c);
         return;
      }
   }

   /* ---- We hold creation rights (or gap mode): build the ONE arena. ---- */
   const size_t bytes = (size_t)ARENA_SLOTS * sizeof(uint64_t);
   const size_t len   = (bytes + 0xFFF) & ~(size_t)0xFFF;
   void *region = NULL;
   for (uintptr_t a = LOW_REGION_BASE; a + len <= LOW_REGION_END;
        a += len + 0x1000) {
      mach_vm_address_t addr = a;
      if (mach_vm_allocate(mach_task_self(), &addr, len,
                           VM_FLAGS_FIXED) == KERN_SUCCESS) {
         region = (void *)(uintptr_t)addr;
         break;
      }
   }
   if (!region) {
      fprintf(stderr, "objc_shim: no low-4GB region for proxy arena\n");
      abort();
   }

   /* Only the FIRST copy ever reaches here, so its malloc shim (libabiconv
    * defines calloc) allocates these exactly once per process — the original
    * crash came from EVERY copy re-allocating. mach_vm_allocate is no good
    * here: interpose.dylib constrains it to the low-4GB window, which can't
    * fit a 32MB ANYWHERE request (KERN_NO_SPACE). */
   struct map_ent *map =
      (struct map_ent *)calloc(MAP_CAP, sizeof(struct map_ent));
   if (!map) {
      fprintf(stderr, "objc_shim: proxy map alloc failed\n");
      abort();
   }

   struct objc_shared_ctrl *c =
      (struct objc_shared_ctrl *)calloc(1, sizeof(*c));
   if (!c) {
      fprintf(stderr, "objc_shim: ctrl alloc failed\n");
      abort();
   }
   c->arena      = (uint64_t)(uintptr_t)region;
   c->arena_base = (uint64_t)(uintptr_t)region;
   c->arena_end  = (uint64_t)(uintptr_t)region + bytes;
   c->arena_used = 0;
   c->map        = map;
   c->lpair      = (uint64_t)(uintptr_t)
      calloc(LPAIR_CAP, sizeof(struct lpair_ent));
   c->rcls       = (uint64_t)(uintptr_t)
      calloc(RCLS_CAP, sizeof(struct rcls_ent));
   c->rvariadic  = (uint64_t)(uintptr_t)
      calloc(RVAR_CAP, sizeof(struct rvar_ent));
   c->pgmemo     = (uint64_t)(uintptr_t)
      calloc(PGMEMO_CAP, sizeof(struct pgmemo_ent));
   c->proc_img   = (uint64_t)(uintptr_t)
      calloc(PROC_IMG_CAP, sizeof(uint64_t));
   __sync_synchronize();
   c->magic      = OBJC_CTRL_MAGIC;

   /* Publish the real ctrl, releasing any spinning adopters (this release store
    * pairs with their acquire load of the rendezvous word). In gap mode
    * (slot==NULL) we deliberately DON'T publish, so a second copy also creates —
    * the pre-fix >=2-ctrl behavior the tests-i386 guard's PRE-FIX arm asserts. */
   if (slot) {
      __atomic_store_n(slot, (uint64_t)(uintptr_t)c, __ATOMIC_RELEASE);
   }
   if (getenv("ABICONV_ARENA_TRACE")) {
      fprintf(stderr, "[arena] create 0x%llx\n",
              (unsigned long long)(uintptr_t)c); fflush(stderr);
   }
   arena_attach(c);
}

/* Process-global claim for the locale objectForKey: swizzle (maptable_shim.m).
 * Returns non-zero to EXACTLY ONE caller across all libabiconv copies; that
 * winner swizzles -[NSUserDefaults objectForKey:] and so captures the real
 * Foundation IMP instead of a sibling copy's mt_compat trampoline. Race-free
 * via an atomic exchange on the shared ctrl (the env-var pattern used elsewhere
 * has a TOCTOU window we can't afford here — a lost race breaks ALL defaults). */
int objc_shared_claim_locale_ofk(void);
int objc_shared_claim_locale_ofk(void) {
   if (!g_ctrl) { arena_init(); }
   if (!g_ctrl) { return 1; }   /* no shared ctrl: act as the sole installer */
   return __atomic_exchange_n(&g_ctrl->locale_ofk_done, 1u,
                              __ATOMIC_SEQ_CST) == 0u;
}

static uint32_t hash64(uint64_t x) {
   x ^= x >> 33;
   x *= 0xff51afd7ed558ccdULL;
   x ^= x >> 33;
   return (uint32_t)x;
}

/* Cross-copy claim for slide_objc per-image processing (objc_slide.c).
 * Returns 1 to EXACTLY ONE libabiconv copy for a given mach_header (the copy
 * that should slide + register + run the image), 0 to every later caller — so a
 * second co-located copy's add-image re-scan never re-walks an image another
 * copy already claimed (the multicopy re-scan race; NULL-base +0x88 deref).
 * Open-addressed CAS insert on the header pointer under proc_img_lock; a full
 * table (never expected: PROC_IMG_CAP >> any process's image count) fails safe
 * by returning 1 (process it locally, exactly the pre-fix per-copy behavior).
 * If there is no shared ctrl (single copy, or ctrl init failed) it always
 * returns 1 — the per-copy g_processed set in objc_slide.c is then the sole,
 * and sufficient, guard. */
int _86x64_objc_shared_claim_image(const void *mh);
int _86x64_objc_shared_claim_image(const void *mh) {
   if (!mh) { return 1; }
   if (!g_ctrl) { arena_init(); }
   if (!g_ctrl || !g_ctrl->proc_img) { return 1; }
   /* Test-only escape hatch (dyld_multicopy_reprocess_test.sh): disable the
    * cross-copy dedup so EVERY copy processes, reproducing the PRE-FIX per-copy
    * re-scan A/B arm. Inert in production (env var never set). Memoized once. */
   static int no_xcopy = -1;
   if (no_xcopy < 0) { no_xcopy = getenv("ABICONV_NO_XCOPY_CLAIM") ? 1 : 0; }
   uint64_t key = (uint64_t)(uintptr_t)mh;
   uint64_t *tab = (uint64_t *)(uintptr_t)g_ctrl->proc_img;
   os_unfair_lock_lock(&g_ctrl->proc_img_lock);
   int claimed = 1;
   uint32_t h = hash64(key) & (PROC_IMG_CAP - 1);
   for (uint32_t probe = 0; probe < PROC_IMG_CAP; ++probe) {
      uint32_t idx = (h + probe) & (PROC_IMG_CAP - 1);
      if (tab[idx] == key) { claimed = 0; break; }   /* already claimed */
      if (tab[idx] == 0)   { tab[idx] = key; break; } /* claim it (we win) */
   }
   os_unfair_lock_unlock(&g_ctrl->proc_img_lock);
   /* no_xcopy models the PRE-FIX per-copy behavior for the test A/B: every copy
    * "wins" (processes) — so the second copy re-scans, the very hazard the fix
    * removes. The claim table is still populated so production dedup is exact. */
   return no_xcopy ? 1 : claimed;
}

/* ---- per-thread rsp/rbp stash for the native->i386 trampolines ----
 * Translated i386 code preserves only the LOW 32 BITS of the registers the
 * trampoline would like to trust across the call (its 4-byte push/pop frame
 * slots zero-extend on the way back). On a low (<4GB) native stack that
 * truncation is the identity, so the main thread never noticed — but a
 * thread with a >4GB native stack (FileCoordination queues) got rsp/rbp
 * back with the top bits gone: the ASCII-stack / wild-pc crash family.
 * The trampolines park rsp+rbp here before `jmp`ing into translated code
 * and recover the pair after its i386 `ret`. LIFO per thread (reverse
 * calls nest). */
struct rsp_stash_pair { uint64_t rsp, rbp; };
#define RSTASH_MAX 1024
static __thread struct rsp_stash_pair g_rstash[RSTASH_MAX];
static __thread uint32_t g_rstash_n;

void _86x64_rsp_stash(uint64_t rsp, uint64_t rbp) {
   if (g_rstash_n >= RSTASH_MAX) {
      fprintf(stderr, "objc_shim: reverse rsp stash overflow\n");
      abort();
   }
   g_rstash[g_rstash_n].rsp = rsp;
   g_rstash[g_rstash_n].rbp = rbp;
   ++g_rstash_n;
}

struct rsp_stash_pair _86x64_rsp_unstash(void) {
   return g_rstash[--g_rstash_n];     /* 16-byte POD: returned in rax:rdx */
}

/* Reverse-stash depth capture/restore for the ObjC1 setjmp/longjmp bridge.
 * A longjmp (objc_exception_throw) jumps straight back to the setjmp point,
 * abandoning every reverse-IMP frame in between WITHOUT running their %%back
 * epilogues — so the rsp/rbp pairs those frames pushed onto g_rstash are never
 * popped. The next reverse-IMP to return then unstashes a STALE pair and lands
 * on a wrong rsp -> a translated `ret` into a 0 slot (rip=0). The exception
 * machinery captures this depth at try_enter and rewinds it at throw, exactly
 * like restoring a stack pointer. (Per libabiconv copy: the legacy class whose
 * reverse-IMPs nest here registers + dispatches through one copy, the same one
 * that interposes try_enter/throw for the faulting image.) */
uint32_t x64_rstash_depth(void) { return g_rstash_n; }
void x64_rstash_set_depth(uint32_t n) {
   if (n <= g_rstash_n) { g_rstash_n = n; }   /* only ever rewind, never grow */
}

/* real 64-bit object -> 32-bit handle the translated code can store. */
uint32_t x64_objc_wrap(uint64_t real) {
   if (real == 0) { return 0; }
   if (real == ~0ULL) {
      /* -1 sentinel (kAlertDefault*Text = (CFStringRef)-1, kCFNotFound
       * handles): keep it a SENTINEL on the i386 side too, symmetric with
       * unwrap_obj_arg's 0xffffffff -> ~0ULL widening. Minting an arena
       * handle would hide the value from the app's own `== -1` compares
       * (GetStandardAlertDefaultParams fills defaultText = -1 that apps
       * legitimately test against kAlertDefaultOKText). */
      return 0xffffffffu;
   }
   arena_init();

   /* The probe+insert is a read-modify-write on the shared `map`: serialize it
    * (see map_lock). Without this, two concurrent inserts that hash-collide
    * tear an entry and a later wrap returns a handle bound to the wrong real. */
   os_unfair_lock_lock(&g_ctrl->map_lock);
   uint32_t i = hash64(real) & (MAP_CAP - 1);
   while (g_map[i].handle != 0) {
      if (g_map[i].real == real) {
         uint32_t h = g_map[i].handle;
         os_unfair_lock_unlock(&g_ctrl->map_lock);
         return h;
      }
      i = (i + 1) & (MAP_CAP - 1);
   }

   /* Atomic bump: iPhoto creates reverse-bridge instances on multiple threads
    * concurrently (e.g. IP_IPHostReachabilityMgr on a background thread while
    * PhotoCDManager inits on main), so a plain `arena_used++` loses updates and
    * hands two callers the same slot. */
   uint32_t slot = __atomic_fetch_add(&g_ctrl->arena_used, 1, __ATOMIC_SEQ_CST);
   if (slot >= ARENA_SLOTS) {
      fprintf(stderr, "objc_shim: proxy arena exhausted\n");
      abort();
   }
   g_arena[slot] = real;
   uint32_t handle = (uint32_t)(uintptr_t)&g_arena[slot];

   g_map[i].real   = real;
   g_map[i].handle = handle;
   os_unfair_lock_unlock(&g_ctrl->map_lock);
   /* Diagnostic: a "real" that is neither a tagged pointer (high bit / low
    * bit per platform) nor a plausible mapped address (0x6000…/0x7ff8…/low)
    * is almost certainly a clobbered register being wrapped — the source of
    * garbage-backed handles that later crash as msgSend receivers. */
   if (WRAP_TRACE()) {
      /* Log EVERY fresh mint: a garbage real can look tagged (odd low byte),
       * so a validity heuristic can't catch it — instead we grep the log for
       * the crashing handle afterwards. ra discriminates the libabiconv call
       * site (which bridge path minted it), symbolizable via copy slide. */
      fprintf(stderr, "[wrap] slot=%u handle=0x%x real=0x%llx t=%x ra=%p\n",
              slot, handle, (unsigned long long)real,
              pthread_mach_thread_np(pthread_self()),
              __builtin_return_address(0));
      fflush(stderr);
   }
   return handle;
}

/* C-function ObjC-object returns are wrapped via x64_objc_wrap (same as the
 * objc_msgSend bridge). NB the value may be an obfuscated tagged pointer (short
 * NSString/NSNumber), which is NOT a dereferenceable address and whose high bit
 * is randomized per process — wrap/unwrap store and return the 64-bit value
 * verbatim, so tagged pointers round-trip to Foundation correctly. Do NOT add a
 * "looks like a real object" validity gate here: it would wrongly drop valid
 * tagged pointers. See abigen.cc emit_body's return-wrap. */

/*
 * Bounce a 64-bit C-string return (UTF8String etc.) into a low-4GB buffer so
 * the translated i386 caller gets a 32-bit pointer it can dereference. Real
 * char* returns from Foundation point into 64-bit-mapped CFString storage,
 * which truncates to garbage in a 32-bit slot. We round-robin a small ring of
 * shim-malloc'd (low-4GB) buffers; UTF8String results are short-lived (used
 * immediately by the caller), so a buffer is safe to recycle long before it
 * wraps around. Returns 0 for NULL; passes an already-low pointer through.
 */
#define CSTR_RING 64
static char    *g_cstr_buf[CSTR_RING];
static size_t   g_cstr_cap[CSTR_RING];
static unsigned g_cstr_idx;

uint32_t x64_objc_bounce_cstr(const char *s) {
   if (!s) { return 0; }
   uintptr_t p = (uintptr_t)s;
   if (p < 0x100000000UL) { return (uint32_t)p; }   /* already low; usable */

   size_t n = strlen(s) + 1;
   unsigned i = g_cstr_idx;
   g_cstr_idx = (g_cstr_idx + 1) % CSTR_RING;
   if (g_cstr_cap[i] < n) {
      char *nb = (char *)realloc(g_cstr_buf[i], n);  /* shim malloc -> low-4GB */
      if (!nb) { return 0; }
      g_cstr_buf[i] = nb;
      g_cstr_cap[i] = n;
   }
   memcpy(g_cstr_buf[i], s, n);
   uintptr_t bp = (uintptr_t)g_cstr_buf[i];
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[bp]   bounce_cstr in=%p -> 0x%lx \"%.32s\"\n",
              (void*)s, (unsigned long)bp, s);
      fflush(stderr);
   }
   return bp < 0x100000000UL ? (uint32_t)bp : 0;
}

/* Forward decl: the i386-CFConstantString-constant resolver (defined far below
 * with the other legacy bridges). x64_objc_unwrap needs it — see below. */
static id i386_cfstr_to_real(uint32_t p);

/* 32-bit handle / i386 value -> real 64-bit object for a native call.
 *   - an arena PROXY HANDLE resolves to its real object (the common case);
 *   - an i386/x86_64 CFConstantString @"..."/CFSTR("...") constant whose isa is
 *     a wrapped proxy handle (the data-shadow of ___CFConstantStringClassReference)
 *     is converted to a real immortal NSString: passing it RAW to native CF
 *     makes CF read that handle as the object's Class -> "unknown class
 *     0x800xxxxx" / __CF_IS_OBJC __builtin_trap. Detection (i386_cfstr_to_real)
 *     keys on the constant's flags+exact-strlen layout, NEVER the isa and never
 *     the app, so it cannot false-match a genuine void-ptr / `^v` data buffer
 *     routed here (CFDictionary key/value, CFHash, the iWeb/iPhoto map families);
 *   - anything else passes through zero-extended (nil / already-low raw value).
 * This keeps the conservative void-ptr / `^v` C-bridge unwrap consistent with the full
 * object resolver (unwrap_obj_arg) the objc_msgSend bridge and the CF-ref/objc
 * typed-arg shims use. */
uint64_t x64_objc_unwrap(uint32_t h) {
   if (h == 0) { return 0; }
   uintptr_t p = h;
   if (p >= g_arena_base && p < g_arena_end) {
      return *(uint64_t *)p;
   }
   id cf = i386_cfstr_to_real(h);
   if (cf) { return (uint64_t)(uintptr_t)cf; }
   return (uint64_t)h;
}

/*
 * Register layout handed back to the asm trampoline. reg[0..5] map to
 * rdi,rsi,rdx,rcx,r8,r9. Offsets are relied on by objc_msgSend.asm:
 *   reg      @ 0
 *   nreg     @ 48
 *   ret_is_obj @ 52
 *   super    @ 56  — temp objc_super for the Super* variants
 *   legacy_imp @ 72  — see below
 *
 * legacy_imp: if nonzero, the receiver is one of the translated binary's OWN
 * legacy ObjC-1.0 classes (defined in __OBJC, never registered with the modern
 * runtime), and this is the runtime address of the translated i386 IMP for the
 * sent class method. The asm trampoline then does NOT call objc_msgSend at all:
 * it restores the original i386 cdecl frame (which is still intact on the stack,
 * since the i386 caller pushed self/_cmd/args as 4-byte slots) and jmps straight
 * into the IMP, resuming i386 dispatch. No reverse ABI marshalling is needed
 * because we never left i386-cdecl land — every [self ...] inside the IMP just
 * re-enters this same forward bridge. See legacy_class_method_imp() below.
 */
struct objc_super_x64 { id receiver; Class super_class; };
/* Overflow args (7th SysV integer arg onward) live on the stack. objc varargs
 * (arrayWithObjects:, dictionaryWithObjectsAndKeys:, stringWithFormat:) can
 * exceed the 6 GP registers AND must carry a nil terminator past the last
 * object — when the explicit objects fill rdi..r9 the terminator itself lands
 * in stack[0]. The asm trampoline copies plan.stack[0..nstack) onto the real
 * stack before the call (see objc_msgSend.asm).
 *
 * The spill capacity bounds how long a nil-terminated id-list variadic call can
 * be (arrayWithObjects:/dictionaryWithObjectsAndKeys: literals): the list needs
 * PLAN_STACK_MAX + 6 - 2 slots for its objects plus the nil terminator. Sized
 * generously so real hand-written literals (Quinn/Sparkle build ~18-pair
 * dictionaries) never overflow; at 32 they did, dropping the nil terminator and
 * sending Foundation walking off the end of the args into uninitialized stack.
 * The cost is purely a larger per-call trampoline frame (sub rsp), freed on
 * return — no memory is touched unless written. Keep PLAN_STACK_Q in
 * objc_msgSend.asm in sync with this value. */
#define PLAN_STACK_MAX 128
#define PLAN_STRET_MAX 184       /* native bounce buffer for stret narrows */
struct objc_call_plan {
   uint64_t reg[6];               /* +0   rdi,rsi,rdx,rcx,r8,r9            */
   int32_t  nreg;                 /* +48                                  */
   int32_t  ret_is_obj;           /* +52  return KIND, see fill_args_and_return */
   struct objc_super_x64 super;   /* +56  (16 bytes)                      */
   uint64_t legacy_imp;           /* +72                                  */
   uint32_t nstack;               /* +80  number of valid stack[] entries */
   uint32_t _pad;                 /* +84                                  */
   uint64_t stack[PLAN_STACK_MAX];/* +88  overflow args (7th onward)      */
   /* ---- FP / struct-by-value support (28th blocker). The offsets below are
    * baked into objc_msgSend.asm as OFF_XMM/OFF_NXMM/OFF_TARGET/OFF_FP_OUT,
    * all computed from PLAN_STACK_Q — keep that constant == PLAN_STACK_MAX.
    * (Offsets shown for PLAN_STACK_MAX=128.) ---- */
   uint64_t xmm[8];               /* +1112 xmm0..7 (doubles or float bits) */
   uint32_t nxmm;                 /* +1176 count of valid xmm slots (-> al) */
   uint32_t sret_conv;            /* +1180 encoding convention of sret_enc */
   uint32_t sret_dst32;           /* +1184 i386 caller's struct-return buf */
   uint32_t _pad3;                /* +1188                                */
   const char *sret_enc;          /* +1192 return-type encoding           */
   uint64_t target;               /* +1200 override for the real msgSend
                                   *       variant (kind-6 reg-return form) */
   uint64_t fp_out[2];            /* +1208 asm scratch: fld src / xmm0,1 out */
   uint8_t  stret_buf[PLAN_STRET_MAX]; /* +1224 native struct bounce buffer */
};                                /* sizeof == 1408; asm reserves PLAN_SIZE */

/* Write SysV integer-arg position `pos` (0=rdi..5=r9, 6+=stack) into the plan. */
static inline void plan_put(struct objc_call_plan *plan, unsigned pos, uint64_t v) {
   if (pos < 6) {
      plan->reg[pos] = v;
   } else if (pos - 6 < PLAN_STACK_MAX) {
      plan->stack[pos - 6] = v;
   }
}

/* Defined after the legacy __OBJC struct decls at the bottom of this file.
 * Given an i386 class-name-string pointer (the value the translated __cls_refs
 * slot holds) and the sent selector name, returns the runtime address of the
 * translated class-method IMP, or 0 if the name is not one of our own legacy
 * classes / the selector is not a legacy class method. */
static uint64_t legacy_class_method_imp(uint32_t self32, const char *sel_name);
static uint64_t legacy_class_method_imp_byname(const char *clsname,
                                               const char *sel_name);
/* Reverse bridge (defined near end of file): if self32 is an i386 shadow of a
 * registered legacy-class instance, return the real modern object it stands
 * for, else 0. Lets a legacy IMP's `[self ...]` re-dispatch on the real obj. */
static id shadow_real(uint32_t s);
/* legacy i386 object/class bridging (defined after the legacy __OBJC structs). */
static id       legacy_obj_to_real(uint32_t p);
static id       i386_cfstr_to_real(uint32_t p);
static id       lpair_lookup(uint32_t p);
static uint64_t unwrap_obj_arg(uint32_t a);
/* Cross-copy registry of variadic legacy methods (defined with the rmeth map):
 * the forward bridge consults it to know when a legacy method takes a nil-
 * terminated id list past its single declared arg (e.g. addAnimations:a,b,nil). */
static int rvar_contains(Class start, SEL s);
/* ObjC block bridge (defined after unwrap_obj_arg). A translated i386 block
 * crossing into the native runtime needs structural marshalling: see the big
 * comment at i386_block_kind. */
static int      i386_block_kind(uint32_t p);     /* 0 / 1 stack / 2 global / 3 malloc */
static uint64_t i386_block_to_native(uint32_t p);/* synth native Block_layout, or 0 */
static uint32_t i386_block_copy(uint32_t p);     /* i386-layout heap copy (low-4GB) */
/* native->i386 C call primitive (objc_reverse.asm), shared with cb_bridge. */
extern uint32_t _86x64_call_i386(uint64_t fn, uint64_t nwords,
                                 const uint32_t *words, uint64_t lowstack_top);
/* cross-copy native->translated nesting depth (defined later in this file). */
void x64_cb_enter(void);
void x64_cb_leave(void);
/* The i386 IMP of a legacy instance method on `c` (walking superclasses), or 0
 * if the legacy-method map has none / it is implausible. Lets the forward bridge
 * route [self legacyOnlyMethod] back to the i386 code when the modern class
 * never registered it (defined after rmeth_lookup). */
static uint64_t legacy_instance_method_imp(Class c, SEL s);

/* i386 layout of struct objc_super: two 4-byte pointers. The translated
 * code passes a pointer to this struct as the first arg of msgSendSuper. */
struct objc_super_i386 { uint32_t receiver; uint32_t super_class; };

/* Is [p, p+len) readable in this task? Uses mach_vm_read_overwrite so we can
 * safely test a translated-program pointer before treating it as a C-string
 * and handing it to objc_getClass / sel_registerName. Without this, a self32
 * or cmd32 that is an unmapped value (truncated real pointer or garbage)
 * crashes deep inside libobjc with KERN_INVALID_ADDRESS. Only ever hit on
 * the non-arena (class-message / selector) path, so the cost is off the
 * common instance-message hot path. */
#define PAGE_SZ_4K 0x1000u

/* genuine readability probe: actually touch [p, p+len) so a file-backed page
 * mapped READ but past its file's EOF (faults KERN_MEMORY_ERROR) is rejected.
 * Region protection bits alone do NOT catch that. */
static int genuine_probe(uintptr_t p, size_t len) {
   char buf[32];
   while (len) {
      size_t chunk = len < sizeof buf ? len : sizeof buf;
      mach_vm_size_t got = 0;
      kern_return_t kr = mach_vm_read_overwrite(
         mach_task_self(), (mach_vm_address_t)p, chunk,
         (mach_vm_address_t)(uintptr_t)buf, &got);
      if (kr != KERN_SUCCESS || got != chunk) { return 0; }
      p += chunk;
      len -= chunk;
   }
   return 1;
}

/* mem_readable() is called O(classes×methods) times per image, once per
 * add-image callback, once per co-located libabiconv copy (~27 in iWeb), and
 * the class-registration fixpoint re-probes every class name up to 64 times.
 * A genuine_probe syscall per call turns startup into a multi-minute hang once
 * the iWork SF* frameworks load. So memoize, per 4K page, the genuinely-
 * readable byte extent from the page base (0..4096) — determined once with
 * real reads (so a file-backed page that faults past EOF is handled), then
 * every later probe in that page is a syscall-free table lookup. A mapped
 * page's readable extent is stable for the life of the mapping (never torn
 * down during startup), so the memo never goes stale. Fully-unreadable pages
 * (rb==0, i.e. unmapped garbage pointers) are NOT memoized — they are already
 * filtered out by ptr_ok before reaching the hot fixpoint loop, and skipping
 * them avoids caching a verdict that a later image-load could invalidate.
 * The memo table lives in the shared objc_shared_ctrl (pgmemo) so a page is
 * probed once per PROCESS, not once per libabiconv copy — see arena_init. A
 * tiny per-copy table backs the window before the shared ctrl is attached. */
static struct pgmemo_ent g_pgmemo_boot[256];

static struct pgmemo_ent *pgmemo_table(uint32_t *cap_out) {
   if (!g_ctrl) { arena_init(); }
   if (g_ctrl && g_ctrl->pgmemo) {
      *cap_out = PGMEMO_CAP;
      return (struct pgmemo_ent *)(uintptr_t)g_ctrl->pgmemo;
   }
   *cap_out = (uint32_t)(sizeof g_pgmemo_boot / sizeof g_pgmemo_boot[0]);
   return g_pgmemo_boot;
}

/* Genuinely-readable byte count from page base pg (page-aligned, nonzero). */
static uint32_t page_readable_len(uintptr_t pg) {
   uint32_t cap;
   struct pgmemo_ent *memo = pgmemo_table(&cap);
   uint32_t mask = cap - 1;
   uint32_t cur_gen = _dyld_image_count();
   uint32_t h0 = (uint32_t)((pg >> 12) * 2654435761u) & mask;
   uint32_t h = h0, slot = h0;
   int have_slot = 0;
   for (uint32_t n = 0; n < cap; ++n) {
      if (!memo[h].used) { slot = h; have_slot = 1; break; }
      if (memo[h].pg == pg) {
         if (memo[h].rb > 0) { return memo[h].rb; }       /* permanent positive */
         if (memo[h].gen == cur_gen) { return 0; }        /* fresh negative */
         slot = h; have_slot = 1; break;                  /* stale negative: reprobe */
      }
      h = (h + 1) & mask;
   }
   uint32_t rb;
   if (genuine_probe(pg, PAGE_SZ_4K)) {
      rb = PAGE_SZ_4K;                       /* common case: fully mapped */
   } else if (!genuine_probe(pg, 1)) {
      rb = 0;                                /* unmapped */
   } else {
      uint32_t lo = 1, hi = PAGE_SZ_4K;      /* partial: bisect EOF boundary */
      while (lo + 1 < hi) {
         uint32_t mid = (lo + hi) / 2;
         if (genuine_probe(pg, mid)) { lo = mid; } else { hi = mid; }
      }
      rb = lo;
   }
   if (have_slot) {
      memo[slot].pg = pg; memo[slot].gen = cur_gen; memo[slot].rb = (uint16_t)rb;
      __sync_synchronize();                  /* publish fields before used flag */
      memo[slot].used = 1;
   }
   return rb;
}

static int mem_readable(uintptr_t p, size_t len) {
   if (p == 0) { return 0; }
   if (len == 0) { return 1; }
   uintptr_t end = p + len;
   if (end < p) { return 0; }                            /* wrap */
   for (uintptr_t pg = p & ~(uintptr_t)(PAGE_SZ_4K - 1); pg < end;
        pg += PAGE_SZ_4K) {
      uintptr_t readable_end = pg + page_readable_len(pg);
      uintptr_t need_end = end < pg + PAGE_SZ_4K ? end : pg + PAGE_SZ_4K;
      if (need_end > readable_end) { return 0; }
   }
   return 1;
}

/* Validate a legacy metadata C-string: readable AND NUL-terminated without
 * running off the mapping. ptr_ok/mem_readable(p,1) only prove the FIRST
 * byte; a name string ending flush against the end of a mapped segment makes
 * strlen (inside objc_getClass / sel_registerName) fault on the next page —
 * the intermittent registration-time SIGBUS (reverse_register_one+202,
 * KERN_MEMORY_ERROR at 0x...003). Resolve each page's readable extent once
 * (memoized) and scan it with a tight in-bounds loop rather than a per-byte
 * mem_readable call. Capped: real class/selector/type-encoding strings are
 * short, and a garbage pointer that lands in a large readable region must not
 * cost a multi-KB scan per class per registration pass (the iWeb startup
 * hang once syscalls were eliminated). */
#define LEGACY_CSTR_MAX 1024u
static int legacy_cstr_ok(uint32_t p32) {
   uintptr_t p = p32;
   if (!p) { return 0; }
   uintptr_t limit = p + LEGACY_CSTR_MAX;
   while (p < limit) {
      uintptr_t pg = p & ~(uintptr_t)(PAGE_SZ_4K - 1);
      uintptr_t readable_end = pg + page_readable_len(pg);
      if (p >= readable_end) { return 0; }          /* p itself unreadable */
      uintptr_t scan_end = readable_end < limit ? readable_end : limit;
      for (; p < scan_end; ++p) {
         if (*(const char *)p == '\0') { return 1; }
      }
      if (readable_end < pg + PAGE_SZ_4K) { return 0; }  /* EOF before a NUL */
      /* else fall through: continues into the next (contiguous) page */
   }
   return 0;
}

/* A real x86_64 object the i386 code references by its raw LOW-4GB address —
 * e.g. a __DATA,__cfstring constant @"..." stored/loaded via a relocated
 * absolute immediate (movl $&cfstring, ivar). Such an object's isa is a real
 * (high, >4GB) Class pointer, unlike a legacy i386 object whose isa is a
 * low-4GB legacy_objc_class. legacy_obj_to_real reads only the low 4 bytes of
 * the isa and would false-match the constant as a legacy object, so recognise
 * it first and use it directly. Heuristic but safe: a legacy i386 object's
 * 8-byte "isa" (isa_low | next_ivar<<32) is essentially never a readable
 * high pointer whose own isa (metaclass) is also a readable high pointer. */
static int is_real_x86_object(uint32_t p) {
   if (p == 0 || !mem_readable(p, 8)) { return 0; }
   uint64_t isa = *(const uint64_t *)(uintptr_t)p;
   if (isa < 0x100000000ULL || (isa & 0x7) || !mem_readable(isa, 8)) { return 0; }
   uint64_t meta = *(const uint64_t *)(uintptr_t)isa;   /* class's isa = metaclass */
   if (meta < 0x100000000ULL || (meta & 0x7) || !mem_readable(meta, 8)) { return 0; }
   /* Distinguish "p IS an object" from "p is a slot merely HOLDING a pointer to
    * an object". The latter is the SIGBUS bug: a 32-bit slot holding a native
    * NSString/CFString pointer false-matched here (its *p and **p are both high
    * readable pointers), so resolve_self returned the slot address, objc_msgSend
    * read *slot = the held string and treated IT as the receiver's Class, and
    * libobjc's realizeClass wrote _objc_empty_cache into a READ-ONLY class page
    * (CoreFoundation's __DATA_CONST/__DATA_DIRTY) -> SIGBUS.
    * Invariant that separates the two: for a REAL object, isa is a Class whose
    * own isa (`meta`) is a METACLASS; for a slot holding an object pointer, `isa`
    * is an INSTANCE whose isa (`meta`) is a regular CLASS, not a metaclass. So a
    * genuine object requires `meta` to be a metaclass. Universal (any held
    * object pointer, not just CF strings; symbol-free). (An earlier guard
    * compared meta to &___CFConstantStringClassReference — WRONG: CF's empty
    * constant string's isa is __NSCFConstantString, not that symbol.) */
   if (!mem_readable(meta, 0x30)) { return 0; }
   if (!class_isMetaClass((Class)(uintptr_t)meta)) { return 0; }
   return 1;
}

/* Resolve an i386 self32 to a real x86_64 id. Handles the three forms a
 * translated binary can pass: arena proxy handle, class-name cstring
 * pointer (for class messages), or nil. An unmapped value is none of these:
 * log it and fall back to nil rather than crashing objc_getClass. */
static id resolve_self_raw(uint32_t self32) {
   uintptr_t sp = self32;
   /* A legacy instance method runs with self32 = its i386 shadow; a `[self ...]`
    * inside it must re-dispatch on the real modern object (so inherited/AppKit
    * methods hit real impls and our registered legacy methods route back
    * through the reverse bridge). */
   id sreal = shadow_real(self32);
   if (sreal) { return sreal; }
   if (sp >= g_arena_base && sp < g_arena_end) {
      return (id)(uintptr_t)*(uint64_t *)sp;
   }
   /* A raw constant x86_64 object (e.g. a __cfstring @"") referenced by its
    * low-4GB address — use it directly before the legacy mapper mis-claims it. */
   if (is_real_x86_object(self32)) { return (id)(uintptr_t)self32; }
   /* A raw legacy i386 object/class used as a receiver (e.g. a static class
    * object, or an instance handed back to us): map to its real modern peer.
    * For a class-name cstring (the usual class-message receiver) this returns
    * 0 — its first 4 bytes don't form a valid legacy isa — so we fall through
    * to the objc_getClass(name) path below. */
   {
      id lr = legacy_obj_to_real(self32);
      if (lr) { return lr; }
   }
   {
      id cf = i386_cfstr_to_real(self32);   /* i386 CFSTR("...") constant receiver */
      if (cf) { return cf; }
   }
   if (self32 != 0) {
      if (!mem_readable(sp, 1)) {
         if (BRIDGE_TRACE()) {
            fprintf(stderr, "[bp] resolve_self: unmapped self32=0x%08x -> nil\n",
                    self32);
            fflush(stderr);
         }
         return (id)0;
      }
      id cls = (id)objc_getClass((const char *)(uintptr_t)self32);
      if (!cls && BRIDGE_TRACE()) {
         fprintf(stderr, "[bp] resolve_self: no class named \"%s\" (self32=0x%08x)\n",
                 (const char *)(uintptr_t)self32, self32);
         fflush(stderr);
      }
      return cls;
   }
   return (id)0;
}

/* Reject a MALFORMED receiver whose isa is not a real Class — the realize-SIGBUS
 * guard, applied to EVERY branch's result (not just is_real_x86_object, which
 * the crashing object bypasses). A well-formed receiver `obj` (instance OR class)
 * has obj->isa = a Class, and a Class's own isa is a METACLASS; so
 * class_isMetaClass((obj->isa)->isa) must hold. A receiver whose isa is an
 * INSTANCE (e.g. a native CFStringRef stored where a receiver was expected) fails
 * this: messaging it makes libobjc treat the held string as the Class and
 * realizeClass writes _objc_empty_cache into a READ-ONLY class page -> SIGBUS.
 * Drop such a receiver to nil (a safe no-op message) instead of crashing.
 * Tagged/low/odd isas are left untouched (can't validate, and they're not the
 * faulting shape). Universal + structural. */
static id resolve_self(uint32_t self32) {
   id obj = resolve_self_raw(self32);
   if (!obj) { return obj; }
   uintptr_t o = (uintptr_t)obj;
   if (!mem_readable(o, 8)) { return obj; }
   uintptr_t isa = *(const uint64_t *)o;                 /* obj->isa (candidate Class) */
   if (isa < 0x100000000ULL || (isa & 0x7) || !mem_readable(isa, 8)) {
      return obj;                                        /* tagged/low/odd: not the bug shape */
   }
   uintptr_t meta = *(const uint64_t *)isa;              /* (obj->isa)->isa: must be a metaclass */
   if (meta < 0x100000000ULL || (meta & 0x7) || !mem_readable(meta, 0x30) ||
       !class_isMetaClass((Class)(uintptr_t)meta)) {
      if (BRIDGE_TRACE()) {
         fprintf(stderr, "[bp] resolve_self: MALFORMED receiver self32=0x%08x obj=%p "
                 "isa=0x%lx -> nil\n", self32, obj, (unsigned long)isa);
         fflush(stderr);
      }
      return (id)0;
   }
   return obj;
}

/* Resolve an i386 cmd32 selector-name pointer to a real SEL, guarding against
 * an unmapped pointer the same way resolve_self does. */
static SEL resolve_sel(uint32_t cmd32) {
   if (cmd32 != 0 && !mem_readable((uintptr_t)cmd32, 1)) {
      if (BRIDGE_TRACE()) {
         fprintf(stderr, "[bp] resolve_sel: unmapped cmd32=0x%08x -> NULL\n",
                 cmd32);
         fflush(stderr);
      }
      return (SEL)0;
   }
   return sel_registerName((const char *)(uintptr_t)cmd32);
}

/* Convert an i386 `:`-typed SEL argument (a selector-name pointer) to a real
 * x86_64 SEL. Mirrors resolve_sel but for an explicit method argument. An
 * already-registered SEL from elsewhere in the bridge round-trips fine
 * (sel_registerName is idempotent on its own name). */
static SEL conv_sel_arg(uint32_t a) {
   if (!a) { return (SEL)0; }
   if (!mem_readable((uintptr_t)a, 1)) {
      if (BRIDGE_TRACE()) {
         fprintf(stderr, "[bp] conv_sel_arg: unmapped SEL 0x%08x -> NULL\n", a);
         fflush(stderr);
      }
      return (SEL)0;
   }
   return sel_registerName((const char *)(uintptr_t)a);
}

/* C-function shim path (typeconv convert_objc_sel): an i386 SEL argument to a
 * shimmed C function (NSStringFromSelector etc.) -> real x86_64 SEL. */
uint64_t x64_objc_sel_unwrap(uint32_t a) {
   return (uint64_t)(uintptr_t)conv_sel_arg(a);
}

/* Real SEL -> stable low-4GB selector-name pointer the i386 caller can store
 * indefinitely and later message with (the forward bridge re-registers it by
 * name). Interned: SELs are few and runtime-interned, so a small dedupe table
 * with strdup'd (shim-malloc, low-4GB) names is bounded. */
#define SEL_INTERN_CAP 512
static struct { uint64_t sel; uint32_t low; } g_sel_intern[SEL_INTERN_CAP];
static unsigned g_sel_intern_n;

uint32_t x64_objc_sel_wrap(uint64_t s) {
   if (!s) { return 0; }
   if (s < 0x100000000ULL) { return (uint32_t)s; }  /* already a low name ptr */
   for (unsigned i = 0; i < g_sel_intern_n; ++i) {
      if (g_sel_intern[i].sel == s) { return g_sel_intern[i].low; }
   }
   /* explicit malloc, not strdup: strdup allocates via libc's internal
    * (high) heap, bypassing the low-4GB shim malloc */
   const char *name = sel_getName((SEL)(uintptr_t)s);
   if (!name) { name = ""; }
   const size_t n = strlen(name) + 1;
   char *low = (char *)malloc(n);
   if (!low || (uintptr_t)low >= 0x100000000UL) { return 0; }
   memcpy(low, name, n);
   if (g_sel_intern_n < SEL_INTERN_CAP) {
      g_sel_intern[g_sel_intern_n].sel = s;
      g_sel_intern[g_sel_intern_n].low = (uint32_t)(uintptr_t)low;
      ++g_sel_intern_n;
   }
   return (uint32_t)(uintptr_t)low;
}

/* A C function returning a C string (a pointer to char or unsigned char, e.g.
 * glGetString's const GLubyte*, gai_strerror, ...) may return a pointer to
 * NATIVE static data living above 4GB. The i386 caller reads only eax (the low
 * 4 bytes), truncating it to a wild low pointer that faults the moment the
 * bytes are read (strlen / UTF8String). Bounce a high return into a low-4GB
 * copy the i386 caller can hold and read; a low return (the common strchr or
 * strstr case, a pointer into a buffer the caller passed in) is already
 * representable and passes straight through. No cache: a fresh copy is correct
 * whether the caller owns-and-frees it (it frees our shim-malloc'd low copy) or
 * treats it as borrowed (one small leak per high return; such functions are
 * called a handful of times). */
const char *x64_cstr_ret_low(const char *p);
const char *x64_cstr_ret_low(const char *p) {
   if (!p || (uintptr_t)p < 0x100000000ULL) { return p; }
   const size_t n = strlen(p) + 1;
   char *low = (char *)malloc(n);                 /* low-4GB shim heap */
   if (!low || (uintptr_t)low >= 0x100000000UL) { return p; }
   memcpy(low, p, n);
   return low;
}

/* Per-method arg unwrap loop: iterates from method-arg index `arg_start`
 * (==2 for self+cmd already-placed methods) up through `cap_regs`,
 * pulling 4-byte slots out of args32 and writing 8-byte slots into plan.
 * Slot-i of args32 maps to plan->reg[reg_base + (i - arg_start)]. */
/* ObjC type-encoding qualifier/modifier prefix chars that precede the real
 * type letter. Legacy fragile-ABI set (r const, n in, N inout, o out, O bycopy,
 * R byref, V oneway) PLUS the modern modifiers the native runtime can emit on
 * the CONV_NATIVE path: j (complex), A (atomic), + (GNU register). Verified
 * against references/objc4/runtime/runtime.h (_C_CONST.._C_GNUREGISTER). An
 * unhandled prefix here is not benign: enc_skip_type would consume the prefix
 * but leave the real type unread, desyncing the entire remaining arg list and
 * mis-marshalling the call. (NOTE: `j` doubles the underlying type's size for a
 * true complex value; complex never occurs in this AppKit/Foundation surface,
 * so it is skipped as a plain prefix — robust against desync, size-approximate
 * in the impossible case it appears.) */
#define ENC_QUALS "rnNoORVAj+"

/* Strip ObjC type-encoding qualifier prefixes (const/in/out/byref/...) so we
 * see the underlying type letter. */
static char encoding_base_type(const char *t) {
   if (!t) { return 0; }
   while (*t && strchr(ENC_QUALS, *t)) { ++t; }
   return *t;
}

/* Old GCC (i386 ObjC-1 era, and the 64-bit gcc that built RedRock) encodes
 * class-typed parameters/returns — `NSObject *foo` — as a pointer-to-struct
 * `^{NSObject=#...}` instead of `@`. The `#` (isa) first member marks it
 * unambiguously as an ObjC object reference; it must be marshalled like `@`
 * everywhere, or a raw i386 pointer reaches native code which retains it and
 * crashes on the 4-byte isa (iPhoto 18th blocker:
 * -[RKTerminateQueue addTerminationDelegate:] types "v24@0:8^{NSObject=#}16").
 * Deliberately requires the `=#` shape: opaque `^{__CFString=}` and genuine
 * struct pointers keep their raw-pointer behavior. */
static int enc_is_objptr_struct(const char *t) {
   if (!t) { return 0; }
   while (*t && strchr(ENC_QUALS, *t)) { ++t; }   /* qualifiers */
   if (*t != '^') { return 0; }
   ++t;
   while (*t && strchr(ENC_QUALS, *t)) { ++t; }
   if (*t != '{') { return 0; }
   ++t;
   while (*t && *t != '=' && *t != '}') { ++t; }  /* struct tag */
   return *t == '=' && t[1] == '#';
}

/* Opaque CF/CG pointer: `^{Name=}` with an EMPTY struct body (no members), e.g.
 * `^{CGColor=}` / `^{CGContext=}` / `^{__CFString=}`. i386 code treats these as
 * opaque tokens it never dereferences, so they must be HANDLE-bridged like '@'
 * (wrap real->handle on return, unwrap handle->real on arg) rather than
 * truncated to 32 bits. This MIRRORS abigen's C-function CF-ptr bridging
 * (typeconv.cc cf_opaque_ptr_type / convert_cf_ptr + abigen.cc return wrap), so
 * a CF ref round-trips consistently between ObjC-bridge and C-shim call sites.
 * A struct pointer WITH a body (`^{CGRect=...}`) or a typed buffer ('^v','^c',
 * '^i') is NOT matched and keeps raw-pointer behaviour (i386 derefs it). The
 * `=#` objc-object-struct shape is handled by enc_is_objptr_struct, which the
 * callers check first; its body is non-empty so it is not matched here. */
static int enc_is_cfptr(const char *t) {
   if (!t) { return 0; }
   while (*t && strchr(ENC_QUALS, *t)) { ++t; }
   if (*t != '^') { return 0; }
   ++t;
   while (*t && strchr(ENC_QUALS, *t)) { ++t; }
   if (*t != '{') { return 0; }
   ++t;
   while (*t && *t != '=' && *t != '}') { ++t; }  /* struct tag */
   if (*t == '}') { return 1; }                   /* `^{Name}` — no body */
   return *t == '=' && t[1] == '}';               /* `^{Name=}` — empty body */
}

/* A bare `^v` (`void *`, optionally const-qualified `r^v`) — an OPAQUE pointer
 * with no struct shape. A native method returning one (e.g.
 * -[NSGraphicsContext graphicsPort] -> CGContextRef) hands back a real 64-bit
 * pointer the i386 caller truncates to eax. It is the RETURN-side counterpart of
 * the `^v` ARG unwrap in fill_method_args, and is wrapped CONDITIONALLY (only a
 * >4GB value, like abigen's `.cfretlow`) so genuine low-4GB `void*` buffers pass
 * through untouched. A typed buffer (`^c`/`^i`/`^S`) or a struct pointer with a
 * body (`^{T=...}`) is NOT matched — those are real i386-side <4GB pointers. */
static int enc_is_opaque_voidptr(const char *t) {
   if (!t) { return 0; }
   while (*t && strchr(ENC_QUALS, *t)) { ++t; }
   if (*t != '^') { return 0; }
   ++t;
   while (*t && strchr(ENC_QUALS, *t)) { ++t; }
   return *t == 'v';
}

/* ---- struct layout walk over an ObjC type encoding ----
 * The legacy method's encoding carries i386 widths (notably CGFloat == 'f',
 * a 4-byte float). We need both the i386 layout (to read the struct the legacy
 * stret IMP wrote on the low stack) and the x86_64 layout (the native caller's
 * buffer). One walker computes both in lockstep and, when dst != NULL, copies
 * each scalar, widening i386 -> native: 'f' CGFloat -> double, 'l'/'L' long and
 * pointers 4 -> 8 bytes. (DOMAIN NOTE: 'f' is assumed CGFloat; struct returns
 * in this surface are NSRect/NSPoint/NSSize/NSRange, all CGFloat- or
 * NSInteger-based — a genuine C float member would be wrongly widened, but
 * none occurs among reverse-bridged AppKit override returns.) */
static const char *enc_walk(const char *t, const uint8_t *src, uint8_t *dst,
                            size_t *i_off, size_t *n_off);

static size_t align_up_sz(size_t off, size_t a) {
   return a ? ((off + a - 1) & ~(a - 1)) : off;
}

/* scalar widths/aligns: returns 0 if not a scalar this walker handles. */
static int enc_scalar(char c, size_t *isz, size_t *ial,
                      size_t *nsz, size_t *nal) {
   switch (c) {
   case 'c': case 'C': case 'B': *isz=*nsz=1; *ial=*nal=1; return 1;
   case 's': case 'S':           *isz=*nsz=2; *ial=*nal=2; return 1;
   case 'i': case 'I':           *isz=*nsz=4; *ial=*nal=4; return 1;
   case 'l': case 'L':           *isz=4; *ial=4; *nsz=8; *nal=8; return 1;
   case 'q': case 'Q':           *isz=8; *ial=4; *nsz=8; *nal=8; return 1;
   case 'f':                     *isz=4; *ial=4; *nsz=8; *nal=8; return 1; /* CGFloat */
   case 'd':                     *isz=8; *ial=4; *nsz=8; *nal=8; return 1;
   case '*': case '^': case '@': case '#': case ':':
                                 *isz=4; *ial=4; *nsz=8; *nal=8; return 1;
   default: return 0;
   }
}

static void enc_copy_scalar(char c, const uint8_t *s, uint8_t *d) {
   switch (c) {
   case 'f': { float f; memcpy(&f, s, 4); double dv = f; memcpy(d, &dv, 8); break; }
   case 'l': { int32_t v; memcpy(&v,s,4); int64_t w=v; memcpy(d,&w,8); break; }
   case 'L': case '*': case '^': case '@': case '#': case ':': {
      uint32_t v; memcpy(&v,s,4); uint64_t w=v; memcpy(d,&w,8); break; }
   case 'c': case 'C': case 'B': *d=*s; break;
   case 's': case 'S': memcpy(d,s,2); break;
   case 'i': case 'I': memcpy(d,s,4); break;
   case 'q': case 'Q': case 'd': memcpy(d,s,8); break;
   default: break;
   }
}

/* Walk one type. Advances both offset cursors past it; copies if dst != NULL.
 * Returns the encoding cursor just past the type (not trailing digits). */
static const char *enc_walk(const char *t, const uint8_t *src, uint8_t *dst,
                            size_t *i_off, size_t *n_off) {
   while (*t && strchr(ENC_QUALS, *t)) { ++t; }   /* qualifiers */
   char c = *t;
   size_t isz, ial, nsz, nal;
   if (enc_scalar(c, &isz, &ial, &nsz, &nal)) {
      *i_off = align_up_sz(*i_off, ial);
      *n_off = align_up_sz(*n_off, nal);
      if (dst) { enc_copy_scalar(c, src + *i_off, dst + *n_off); }
      *i_off += isz; *n_off += nsz;
      return t + 1;
   }
   if (c == '{' || c == '(') {                    /* struct / union */
      char close = (c == '{') ? '}' : ')';
      const char *p = t + 1;
      while (*p && *p != '=' && *p != close) { ++p; }   /* skip tag */
      size_t imax_al = 1, nmax_al = 1;
      size_t istart = *i_off, nstart = *n_off;     /* union: all from base */
      if (*p == '=') {
         ++p;
         while (*p && *p != close) {
            size_t ib = (c == '(') ? istart : *i_off;
            size_t nb = (c == '(') ? nstart : *n_off;
            *i_off = ib; *n_off = nb;
            /* track member alignment via a dry sub-walk start */
            size_t before_i = *i_off, before_n = *n_off;
            p = enc_walk(p, src, dst, i_off, n_off);
            while (*p >= '0' && *p <= '9') { ++p; }  /* bitfield/array digits */
            (void)before_i; (void)before_n;
            if (c == '(') {  /* union size = max member size */
               if (*i_off - istart > imax_al) { imax_al = *i_off - istart; }
            }
         }
      }
      if (*p == close) { ++p; }
      /* pad struct to its own alignment is approximate; callers only need the
       * scalar copies to land at correct native offsets, which they do. */
      return p;
   }
   if (c == '[') {                                /* array [N type] */
      const char *p = t + 1;
      unsigned n = 0;
      while (*p >= '0' && *p <= '9') { n = n*10 + (unsigned)(*p-'0'); ++p; }
      const char *elem = p;
      for (unsigned k = 0; k < n; ++k) { p = enc_walk(elem, src, dst, i_off, n_off); }
      if (*p == ']') { ++p; }
      return p;
   }
   if (c == 'b') {                                /* bitfield: skip width */
      ++t; while (*t >= '0' && *t <= '9') { ++t; }
      return t;
   }
   return t + (*t ? 1 : 0);
}

/* Native size of a (struct) return type, for the stret decision. */
static size_t enc_native_size(const char *t) {
   size_t i_off = 0, n_off = 0;
   enc_walk(t, NULL, NULL, &i_off, &n_off);
   return n_off;
}

/* Does this return encoding use the x86_64 stret (hidden-pointer) convention?
 * Memory-class aggregates (> 16 bytes) do; <=16-byte structs are returned in
 * registers (INTEGER or SSE) and are a separate, currently-unhandled path. */
static int enc_ret_is_stret(const char *types) {
   if (!types) { return 0; }
   const char *t = types;
   while (*t && strchr(ENC_QUALS, *t)) { ++t; }
   if (*t != '{' && *t != '(' && *t != '[') { return 0; }
   return enc_native_size(t) > 16;
}

/* =========================================================================
 * Conv-aware dual-layout encoding walker (28th blocker: FP/struct ABI).
 *
 * Two encoding CONVENTIONS reach the bridge:
 *   CONV_I386:   strings from the translated binary's legacy __OBJC metadata
 *                (and from methods we registered with them): 'f' is CGFloat
 *                (i386 4-byte float -> native 8-byte double), 'l'/pointers are
 *                4 -> 8, 'q' is 8 -> 8. This is what enc_scalar above assumes.
 *   CONV_NATIVE: strings from the modern runtime (method_copyArgumentType on
 *                a real AppKit/Foundation method): 'd' is double OR CGFloat,
 *                'q'/'Q' is long long OR NSInteger — the i386 source width is
 *                ambiguous and resolved by STRUCT-TAG knowledge (CGRect et al
 *                are CGFloat-based; _NSRange/CFRange are index-based) plus,
 *                for scalars, by the legacy seltypes registry when the app's
 *                own metadata declares the selector. 'f' is a genuine float
 *                (native single precision, NOT widened).
 *
 * One recursive walker computes the i386 and native layouts in lockstep,
 * optionally copying scalars (widening i386->native or narrowing back), and
 * records the SysV class (SSE vs INTEGER) of every native eightbyte so
 * aggregates can be classified per the x86_64 ABI.
 * ========================================================================= */
#define CONV_I386   0
#define CONV_NATIVE 1
/* struct-tag context for CONV_NATIVE ambiguity resolution */
#define CTX_NONE    0
#define CTX_CGFLOAT 1   /* 'd' members are CGFloat: i386 4-byte float */
#define CTX_INDEX   2   /* 'q'/'Q' members are NS/CFIndex: i386 4-byte */

/* defined with the other encoding scanners near reverse_prep */
static const char *enc_skip_quals(const char *t);
static const char *enc_skip_type(const char *t);
static const char *enc_skip_digits(const char *t);

static int enc_tag_ctx(const char *tag, size_t n) {
   static const char *cg[] = { "CGPoint", "CGSize", "CGRect", "NSPoint",
      "NSSize", "NSRect", "_NSPoint", "_NSSize", "_NSRect",
      "CGAffineTransform", "NSAffineTransformStruct", "CGVector",
      "CATransform3D", "NSEdgeInsets" };
   static const char *ix[] = { "_NSRange", "NSRange", "CFRange" };
   for (unsigned i = 0; i < sizeof cg / sizeof *cg; ++i) {
      if (strlen(cg[i]) == n && !strncmp(tag, cg[i], n)) { return CTX_CGFLOAT; }
   }
   for (unsigned i = 0; i < sizeof ix / sizeof *ix; ++i) {
      if (strlen(ix[i]) == n && !strncmp(tag, ix[i], n)) { return CTX_INDEX; }
   }
   return CTX_NONE;
}

/* True if an aggregate body (the chars AFTER '=' up to `close`) is a non-empty
 * run of ONLY floating-point scalars — 'd'/'f', with interleaved field-offset
 * digits tolerated. Used to recognise an ANONYMOUS-tag CGFloat aggregate
 * ({?=dddddd}, e.g. NSAffineTransformStruct, which is typedef'd from an unnamed
 * struct so its tag is '?' and never matches enc_tag_ctx's named list): the
 * native runtime spells each CGFloat field 'd' (double) but the legacy i386 ABI
 * spells it 'f' (4-byte float), so the field widths must be reconciled exactly
 * like the named CG/NS geometry structs. A nested aggregate or any non-FP
 * member makes this return 0 (conservative: only a flat all-FP unnamed struct
 * is treated as CGFloat). */
static int enc_body_all_fp(const char *p, char close) {
   int n = 0;
   while (*p && *p != close) {
      if (*p == 'd' || *p == 'f') { ++n; ++p; continue; }
      if (*p >= '0' && *p <= '9') { ++p; continue; }   /* field-offset digit */
      return 0;                                         /* non-FP / nested */
   }
   return n > 0;
}

/* scalar widths under a convention+context; *fp set for SSE-class scalars.
 * Returns 0 if c is not a scalar. */
static int enc_scalar3(char c, int conv, int ctx, size_t *isz, size_t *ial,
                       size_t *nsz, size_t *nal, int *fp) {
   *fp = 0;
   switch (c) {
   case 'c': case 'C': case 'B': *isz=*nsz=1; *ial=*nal=1; return 1;
   case 's': case 'S':           *isz=*nsz=2; *ial=*nal=2; return 1;
   case 'i': case 'I':           *isz=*nsz=4; *ial=*nal=4; return 1;
   case 'l': case 'L':
      if (conv == CONV_NATIVE) { *isz=8; *ial=4; *nsz=8; *nal=8; }
      else                     { *isz=4; *ial=4; *nsz=8; *nal=8; }
      return 1;
   case 'q': case 'Q':
      if (conv == CONV_NATIVE && ctx == CTX_INDEX) { *isz=4; *ial=4; }
      else                                         { *isz=8; *ial=4; }
      *nsz=8; *nal=8; return 1;
   case 'f':
      *fp = 1;
      /* A bare 'f' under a NATIVE encoding is a genuine 4-byte C float — UNLESS it
       * sits inside a CGFloat-typed struct (NSRect/NSPoint/NSSize/CG*), where the
       * legacy i386 encoding spells CGFloat as 'f'. There it is a CGFloat and must
       * widen to a 64-bit double, exactly like the 'd' case. Without this, a
       * reverse-registered legacy class whose geometry method carries the i386
       * `{_NSRect={_NSPoint=ff}{_NSSize=ff}}` encoding got its 32-byte MEMORY-class
       * NSRect misclassified as a 16-byte register aggregate (2 SSE eightbytes ->
       * xmm0/xmm1); native AppKit then read the rect from the stack and saw 0 ->
       * a 0x0 window (iPhoto/iWork IWWindow setFrame:display:/initWithContentRect:). */
      if (conv == CONV_NATIVE && ctx != CTX_CGFLOAT) {
         *isz=4; *ial=4; *nsz=4; *nal=4;        /* genuine C float */
      } else {
         *isz=4; *ial=4; *nsz=8; *nal=8;        /* CGFloat -> double */
      }
      return 1;
   case 'd':
      *fp = 1;
      if (conv == CONV_NATIVE && ctx == CTX_CGFLOAT) { *isz=4; *ial=4; }
      else                                           { *isz=8; *ial=4; }
      *nsz=8; *nal=8; return 1;
   case '*': case '^': case '@': case '#': case ':':
      *isz=4; *ial=4; *nsz=8; *nal=8; return 1;
   default: return 0;
   }
}

/* copy one scalar between layouts. dir: 1 = widen (i386 src -> native dst),
 * 2 = narrow (native src -> i386 dst). */
static void enc_copy3(char c, size_t isz, size_t nsz, int fp, int dir,
                      const uint8_t *s, uint8_t *d) {
   if (dir == 1) {                            /* widen */
      if (fp && isz == 4 && nsz == 8) {       /* CGFloat float -> double */
         float f; memcpy(&f, s, 4); double dv = f; memcpy(d, &dv, 8);
      } else if (isz == 4 && nsz == 8) {
         if (c == 'l' || c == 'q') {          /* signed extend */
            int32_t v; memcpy(&v, s, 4); int64_t w = v; memcpy(d, &w, 8);
         } else {
            uint32_t v; memcpy(&v, s, 4); uint64_t w = v; memcpy(d, &w, 8);
         }
      } else {
         memcpy(d, s, isz < nsz ? isz : nsz);
      }
   } else {                                   /* narrow */
      if (fp && isz == 4 && nsz == 8) {       /* double -> CGFloat float */
         double dv; memcpy(&dv, s, 8); float f = (float)dv; memcpy(d, &f, 4);
      } else if (isz == 4 && nsz == 8) {
         memcpy(d, s, 4);                     /* take the low half */
      } else {
         memcpy(d, s, isz < nsz ? isz : nsz);
      }
   }
}

/* i386 callback -> native trampoline bridge (cb_bridge.c). A `^?` (function
 * pointer) argument or struct MEMBER is an i386 code address the native API
 * will later CALL with the x86_64 ABI; passed raw it crashes. We bind it to a
 * trampoline whose dispatcher marshals the native args down to an i386 cdecl
 * word frame. The type encoding `^?` carries NO inner signature, so the
 * descriptor below is fixed by what `^?` MEANS on the ObjC message-send
 * surface: the only AppKit/Foundation methods that take a bare C function
 * pointer are the sort-comparator family —
 *   -[NS(Mutable)Array sort(ed)UsingFunction:context:(hint:)]
 * and any third-party method of the same shape — whose callback is universally
 *   NSInteger (*)(id obj1, id obj2, void *context).
 * (A `^?` only reaches this forward bridge when it is being handed to a NATIVE
 * method; native methods with a C-fn-ptr arg are exactly the comparators.
 * C-API callbacks — qsort, CFArray* — never come through here; abigen emits a
 * per-prototype descriptor for those.)  So we bind it with the comparator
 * shape: two OBJECT args (kind 3, handle-wrapped so a >4GB / tagged native
 * object pointer survives into the i386 callee, where it unwraps on the next
 * message send — NOT truncated like a raw pointer), a pointer context (kind 2),
 * and a SIGN-EXTENDED NSInteger return (kind 4) so -1/0/1 (NSOrderedAscending/
 * Same/Descending) reaches CoreFoundation as a signed 64-bit long rather than a
 * zero-extended huge positive. (Quinn 3.5.7 crashed in CoreFoundation's
 * -[NSMutableArray sortUsingFunction:context:] -> sortRange:options:
 * usingComparator: because the old all-POINTER descriptor truncated the two id
 * args -> the i386 comparator dereferenced a chopped object pointer.) */
typedef struct { uint32_t nargs; uint32_t ret_kind; uint8_t arg_kinds[16]; }
   x64_cb_sig_t;
extern uint64_t x64_cb_wrap(uint32_t fn32, const x64_cb_sig_t *sig);
/* arg kinds: OBJ=3, OBJ=3, PTR=2 ; ret kind I32SX=4 (mirror cb_bridge.c) */
static const x64_cb_sig_t g_objc_cmp_sig = { 3, 4, { 3, 3, 2 } };

/* walk state: eightbyte SysV class accumulators over the NATIVE layout */
struct enc_ew {
   int conv;
   int dir;            /* 0 dry, 1 widen i386->native, 2 narrow native->i386 */
   int wrap_fnptr;     /* dir1: bind `^?` members to cb trampolines */
   const uint8_t *src; /* dir1: i386 base; dir2: native base */
   uint8_t *dst;       /* dir1: native base; dir2: i386 base */
   uint8_t eb_fp[8];   /* native eightbyte k contains FP scalar(s) */
   uint8_t eb_int[8];  /* native eightbyte k contains integer scalar(s) */
};

static const char *enc_walk3(struct enc_ew *w, const char *t, int ctx,
                             size_t *i_off, size_t *n_off) {
   t = enc_skip_quals(t);
   char c = *t;
   size_t isz, ial, nsz, nal; int fp;
   if (enc_scalar3(c, w->conv, ctx, &isz, &ial, &nsz, &nal, &fp)) {
      *i_off = align_up_sz(*i_off, ial);
      *n_off = align_up_sz(*n_off, nal);
      for (size_t k = *n_off / 8; k <= (*n_off + nsz - 1) / 8 && k < 8; ++k) {
         if (fp) { w->eb_fp[k] = 1; } else { w->eb_int[k] = 1; }
      }
      if (w->dir == 1 && w->src && w->dst) {
         enc_copy3(c, isz, nsz, fp, 1, w->src + *i_off, w->dst + *n_off);
         /* A `^?` member is a callback: bind the i386 fn-ptr to a native
          * trampoline (overwriting the zero-extended copy) so native code can
          * call it with the x86_64 ABI. */
         if (w->wrap_fnptr && c == '^' && t[1] == '?') {
            uint32_t fn32; memcpy(&fn32, w->src + *i_off, 4);
            uint64_t tr = x64_cb_wrap(fn32, &g_objc_cmp_sig);
            memcpy(w->dst + *n_off, &tr, 8);
         }
      } else if (w->dir == 2 && w->src && w->dst) {
         enc_copy3(c, isz, nsz, fp, 2, w->src + *n_off, w->dst + *i_off);
      }
      *i_off += isz; *n_off += nsz;
      if (c == '^') { return enc_skip_type(t); }   /* skip the pointee type */
      return t + 1;
   }
   if (c == '{' || c == '(') {
      char close = (c == '{') ? '}' : ')';
      const char *p = t + 1;
      const char *tag = p;
      while (*p && *p != '=' && *p != close) { ++p; }
      int sub_ctx = (w->conv == CONV_NATIVE)
         ? enc_tag_ctx(tag, (size_t)(p - tag)) : CTX_NONE;
      /* Anonymous-tag CGFloat aggregate: {?=dddddd} (NSAffineTransformStruct and
       * other CGFloat-only structs typedef'd from an unnamed struct) never match
       * the named-tag table, yet their 'd' fields are i386 4-byte CGFloats. A
       * '?'-tagged all-floating-point body is treated as CGFloat context so each
       * field widens i386(4B float)->native(8B double) and narrows back. */
      if (sub_ctx == CTX_NONE && w->conv == CONV_NATIVE
          && (size_t)(p - tag) == 1 && tag[0] == '?'
          && *p == '=' && enc_body_all_fp(p + 1, close)) {
         sub_ctx = CTX_CGFLOAT;
      }
      if (sub_ctx == CTX_NONE && ctx != CTX_NONE) { sub_ctx = ctx; }
      size_t istart = *i_off, nstart = *n_off;
      size_t imax = istart, nmax = nstart;
      if (*p == '=') {
         ++p;
         while (*p && *p != close) {
            if (c == '(') { *i_off = istart; *n_off = nstart; }   /* union */
            p = enc_walk3(w, p, sub_ctx, i_off, n_off);
            p = enc_skip_digits(p);
            if (*i_off > imax) { imax = *i_off; }
            if (*n_off > nmax) { nmax = *n_off; }
         }
         *i_off = imax; *n_off = nmax;
      }
      if (*p == close) { ++p; }
      return p;
   }
   if (c == '[') {
      const char *p = t + 1;
      unsigned n = 0;
      while (*p >= '0' && *p <= '9') { n = n*10 + (unsigned)(*p - '0'); ++p; }
      const char *elem = p;
      for (unsigned k = 0; k < n; ++k) {
         p = enc_walk3(w, elem, ctx, i_off, n_off);
      }
      if (*p == ']') { ++p; }
      return p;
   }
   if (c == 'b') {
      ++t; while (*t >= '0' && *t <= '9') { ++t; }
      return t;
   }
   return t + (*t ? 1 : 0);
}

/* dry-classify one type: i386 size, native size, per-native-eightbyte SSE
 * class. Returns the number of native eightbytes (0 for empty/unknown). */
static unsigned enc_classify(const char *t, int conv, size_t *isz_out,
                             size_t *nsz_out, uint8_t sse[8]) {
   struct enc_ew w; memset(&w, 0, sizeof w);
   w.conv = conv; w.dir = 0;
   size_t i_off = 0, n_off = 0;
   enc_walk3(&w, t, CTX_NONE, &i_off, &n_off);
   *isz_out = i_off; *nsz_out = n_off;
   unsigned nebs = (unsigned)((n_off + 7) / 8);
   if (nebs > 8) { nebs = 8; }
   for (unsigned k = 0; k < nebs; ++k) {
      sse[k] = (w.eb_fp[k] && !w.eb_int[k]) ? 1 : 0;
   }
   return nebs;
}

static void enc_widen(const char *t, int conv, const uint8_t *i386_src,
                      uint8_t *native_dst) {
   struct enc_ew w; memset(&w, 0, sizeof w);
   w.conv = conv; w.dir = 1; w.wrap_fnptr = 1;
   w.src = i386_src; w.dst = native_dst;
   size_t i_off = 0, n_off = 0;
   enc_walk3(&w, t, CTX_NONE, &i_off, &n_off);
}

static void enc_narrow(const char *t, int conv, const uint8_t *native_src,
                       uint8_t *i386_dst) {
   struct enc_ew w; memset(&w, 0, sizeof w);
   w.conv = conv; w.dir = 2; w.src = native_src; w.dst = i386_dst;
   size_t i_off = 0, n_off = 0;
   enc_walk3(&w, t, CTX_NONE, &i_off, &n_off);
}

/* ---- selector -> legacy i386 types registry ----
 * Filled during legacy class registration (reverse_add_methods): the app's
 * own __OBJC metadata records the TRUE i386 encoding of every selector it
 * declares or overrides, which disambiguates CGFloat-vs-double and
 * NSInteger-vs-long-long when the native runtime encoding is all we'd have.
 * Keyed by real SEL (runtime-interned, so pointer compare is exact). */
/* power of two; must exceed the total DISTINCT selectors across every legacy
 * framework loaded — iWork's SF* set alone registers tens of thousands. An
 * undersized table saturates: open-addressed inserts degrade to full-table
 * O(n) scans (the post-syscall iWeb startup hang) and, worse, silently drop
 * entries so the forward bridge later can't recover an arg encoding. */
#define SELTYPES_CAP 131072u
static struct { SEL sel; const char *types; } g_seltypes[SELTYPES_CAP];
static void seltypes_insert(SEL s, const char *types) {
   if (!s || !types) { return; }
   uint32_t i = (uint32_t)(((uintptr_t)s >> 3) * 2654435761u) & (SELTYPES_CAP - 1);
   for (uint32_t n = 0; n < SELTYPES_CAP; ++n) {
      if (g_seltypes[i].sel == NULL || g_seltypes[i].sel == s) {
         g_seltypes[i].sel = s; g_seltypes[i].types = types; return;
      }
      i = (i + 1) & (SELTYPES_CAP - 1);
   }
}
static const char *seltypes_lookup(SEL s) {
   if (!s) { return NULL; }
   uint32_t i = (uint32_t)(((uintptr_t)s >> 3) * 2654435761u) & (SELTYPES_CAP - 1);
   for (uint32_t n = 0; n < SELTYPES_CAP; ++n) {
      if (g_seltypes[i].sel == NULL) { return NULL; }
      if (g_seltypes[i].sel == s) { return g_seltypes[i].types; }
      i = (i + 1) & (SELTYPES_CAP - 1);
   }
   return NULL;
}

/* ---- selector -> CGFloat explicit-arg bitmask registry ----
 * A `CGFloat` is a 4-byte float on i386 but an 8-byte double on x86_64, and the
 * modern runtime encodes BOTH a CGFloat and a true `double` parameter as 'd' —
 * the native encoding alone cannot tell them apart. When the translated app
 * only CALLS a system method (it doesn't declare/override it) the seltypes
 * registry never learns the true i386 width, so the forward bridge falls back to
 * the native 'd' and reads 8 bytes (two i386 slots) for a value the i386 caller
 * pushed as ONE 4-byte float -> a fused garbage double AND a one-slot
 * misalignment of every following argument. This registry records, per
 * selector, WHICH explicit args are CGFloat (bit k => explicit arg k, 0-based
 * after self/_cmd) so the bridge reads them as a single 4-byte i386 float and
 * cvtss2sd-widens to the double the native method expects — at any arg position,
 * any arity. Keyed by real (interned) SEL: pointer compare is exact. */
#define CGFLOAT_MASK_CAP 256u   /* power of two; only a curated handful seeded */
static struct { SEL sel; uint32_t mask; } g_cgfloat_mask[CGFLOAT_MASK_CAP];
static void cgfloat_mask_insert(SEL s, uint32_t mask) {
   if (!s) { return; }
   uint32_t i = (uint32_t)(((uintptr_t)s >> 3) * 2654435761u) & (CGFLOAT_MASK_CAP - 1);
   for (uint32_t n = 0; n < CGFLOAT_MASK_CAP; ++n) {
      if (g_cgfloat_mask[i].sel == NULL || g_cgfloat_mask[i].sel == s) {
         g_cgfloat_mask[i].sel = s; g_cgfloat_mask[i].mask = mask; return;
      }
      i = (i + 1) & (CGFLOAT_MASK_CAP - 1);
   }
}
static uint32_t cgfloat_mask_lookup(SEL s) {
   if (!s) { return 0; }
   uint32_t i = (uint32_t)(((uintptr_t)s >> 3) * 2654435761u) & (CGFLOAT_MASK_CAP - 1);
   for (uint32_t n = 0; n < CGFLOAT_MASK_CAP; ++n) {
      if (g_cgfloat_mask[i].sel == NULL) { return 0; }
      if (g_cgfloat_mask[i].sel == s) { return g_cgfloat_mask[i].mask; }
      i = (i + 1) & (CGFLOAT_MASK_CAP - 1);
   }
   return 0;
}

/* Extract explicit-arg encoding #k (method-arg index k+2) from a full legacy
 * types string ("v24@0:8{_NSRect=...}16"). Returns NULL past the end. */
static const char *enc_nth_arg(const char *types, unsigned k) {
   const char *t = types;
   t = enc_skip_digits(enc_skip_type(t));   /* return */
   t = enc_skip_digits(enc_skip_type(t));   /* self  */
   t = enc_skip_digits(enc_skip_type(t));   /* _cmd  */
   for (unsigned i = 0; i < k; ++i) {
      if (!*t) { return NULL; }
      t = enc_skip_digits(enc_skip_type(t));
   }
   return *t ? t : NULL;
}

/* ---- forward-marshal cursors: SysV positions over the plan ---- */
struct mcur {
   unsigned gp;      /* next GP slot: reg[gp] while < 6                  */
   unsigned xmm;     /* next XMM slot while < 8                          */
   size_t   stk;     /* bytes used in the plan->stack overflow area      */
};

static int mcur_put_gp(struct objc_call_plan *plan, struct mcur *c, uint64_t v) {
   if (c->gp < 6) { plan->reg[c->gp++] = v; return 1; }
   if (c->stk + 8 <= sizeof plan->stack) {
      memcpy((uint8_t *)plan->stack + c->stk, &v, 8); c->stk += 8; return 1;
   }
   return 0;
}
static int mcur_put_xmm(struct objc_call_plan *plan, struct mcur *c, uint64_t bits) {
   if (c->xmm < 8) { plan->xmm[c->xmm++] = bits; return 1; }
   if (c->stk + 8 <= sizeof plan->stack) {
      memcpy((uint8_t *)plan->stack + c->stk, &bits, 8); c->stk += 8; return 1;
   }
   return 0;
}

/* reverse-bridge trampolines (objc_reverse.asm): a Method whose IMP is one of
 * these carries a LEGACY (CONV_I386) encoding string. */
extern void _86x64_reverse_imp(void);
extern void _86x64_reverse_imp_stret(void);
static int method_is_legacy(Method m) {
   if (!m) { return 0; }
   IMP imp = method_getImplementation(m);
   if (imp == (IMP)_86x64_reverse_imp || imp == (IMP)_86x64_reverse_imp_stret) {
      return 1;
   }
   /* A legacy method registered by a DIFFERENT libabiconv copy carries THAT
    * copy's trampoline address — consult the shared registry (see rev_imps).
    * Without this, forward marshalling used NATIVE conventions against a
    * reverse side that rebuilds the frame with LEGACY ones ('f' float-vs-
    * double, 'q' one-slot-vs-two) -> silently corrupted scalar args/returns
    * (Quinn: ATViewAnimation setProgress:1.0 arrived as ~0 -> black splash). */
   if (g_ctrl) {
      for (unsigned i = 0; i < sizeof g_ctrl->rev_imps / sizeof g_ctrl->rev_imps[0];
           i++) {
         uint64_t v = g_ctrl->rev_imps[i];
         if (!v) { break; }
         if (v == (uint64_t)(uintptr_t)imp) { return 1; }
      }
   }
   return 0;
}

/* Marshal ONE explicit argument from the i386 frame into the plan.
 * enc/conv describe it; *ai is the args32 slot cursor (advanced by the i386
 * width in 4-byte slots). Returns 0 only on overflow (arg dropped). */
static int marshal_arg_fwd(struct objc_call_plan *plan, struct mcur *c,
                           const char *enc, int conv,
                           const uint32_t *args32, unsigned *ai) {
   const int trace = BRIDGE_TRACE();
   const char *t = enc_skip_quals(enc);
   char b = *t;
   if (b == '@' || b == '#' || enc_is_objptr_struct(enc) || enc_is_cfptr(enc)) {
      return mcur_put_gp(plan, c, unwrap_obj_arg(args32[(*ai)++]));
   }
   if (b == ':') {
      return mcur_put_gp(plan, c,
                         (uint64_t)(uintptr_t)conv_sel_arg(args32[(*ai)++]));
   }
   if (b == 'f') {
      uint32_t raw = args32[(*ai)++];
      if (conv == CONV_NATIVE) {
         /* genuine float: pass single-precision bits */
         return mcur_put_xmm(plan, c, (uint64_t)raw);
      }
      float f; memcpy(&f, &raw, 4);           /* CGFloat: widen to double */
      double dv = f; uint64_t bits; memcpy(&bits, &dv, 8);
      return mcur_put_xmm(plan, c, bits);
   }
   if (b == 'd') {
      /* CONV_NATIVE scalar 'd' is ambiguous (double vs CGFloat); without a
       * registry hit we assume a true 8-byte double — NSTimeInterval and the
       * NSNumber family dominate this shape. A CGFloat-typed scalar (e.g.
       * setAlphaValue:) would need its legacy encoding to be in seltypes. */
      uint64_t bits = (uint64_t)args32[*ai] | ((uint64_t)args32[*ai + 1] << 32);
      *ai += 2;
      if (trace && conv == CONV_NATIVE) {
         fprintf(stderr, "[fma] 'd' scalar: assuming 8-byte double source\n");
      }
      return mcur_put_xmm(plan, c, bits);
   }
   if (b == 'q' || b == 'Q') {
      if (conv == CONV_NATIVE) {
         /* native NSInteger/NSUInteger (the common case): ONE i386 slot */
         uint32_t raw = args32[(*ai)++];
         uint64_t v = (b == 'q') ? (uint64_t)(int64_t)(int32_t)raw
                                 : (uint64_t)raw;
         return mcur_put_gp(plan, c, v);
      }
      uint64_t v = (uint64_t)args32[*ai] | ((uint64_t)args32[*ai + 1] << 32);
      *ai += 2;
      return mcur_put_gp(plan, c, v);
   }
   if (b == '{' || b == '(' || b == '[') {
      size_t isz = 0, nsz = 0;
      uint8_t sse[8] = {0};
      unsigned nebs = enc_classify(t, conv, &isz, &nsz, sse);
      unsigned islots = (unsigned)((isz + 3) / 4);
      if (nsz == 0 || nebs == 0) { *ai += islots ? islots : 1; return 0; }
      if (nsz <= 16) {
         /* register-class aggregate: SSE eightbytes -> xmm, INTEGER -> GP.
          * SysV: if EITHER eightbyte lacks a register, the whole aggregate
          * goes to memory. */
         unsigned need_gp = 0, need_xmm = 0;
         for (unsigned k = 0; k < nebs; ++k) { if (sse[k]) ++need_xmm; else ++need_gp; }
         uint8_t tmp[16] = {0};
         enc_widen(t, conv, (const uint8_t *)&args32[*ai], tmp);
         *ai += islots;
         if (6 - c->gp >= need_gp && 8 - c->xmm >= need_xmm) {
            for (unsigned k = 0; k < nebs; ++k) {
               uint64_t eb; memcpy(&eb, tmp + 8*k, 8);
               if (sse[k]) { mcur_put_xmm(plan, c, eb); }
               else        { mcur_put_gp(plan, c, eb); }
            }
            return 1;
         }
         if (c->stk + 8*nebs <= sizeof plan->stack) {
            memcpy((uint8_t *)plan->stack + c->stk, tmp, 8*nebs);
            c->stk += 8*nebs;
            return 1;
         }
         return 0;
      }
      /* MEMORY-class aggregate: widen straight into the stack area */
      size_t need = align_up_sz(nsz, 8);
      if (c->stk + need <= sizeof plan->stack) {
         memset((uint8_t *)plan->stack + c->stk, 0, need);
         enc_widen(t, conv, (const uint8_t *)&args32[*ai],
                   (uint8_t *)plan->stack + c->stk);
         c->stk += need;
         *ai += islots;
         return 1;
      }
      *ai += islots;
      if (trace) {
         fprintf(stderr, "[fma] MEMORY struct arg overflows plan stack (%zu)\n",
                 nsz);
      }
      return 0;
   }
   if (b == 'D') {                            /* long double: unsupported */
      *ai += 3;                               /* i386 x87 ext = 12 bytes */
      if (trace) { fprintf(stderr, "[fma] long double arg unsupported\n"); }
      return mcur_put_xmm(plan, c, 0);
   }
   if (b == '^' && t[1] == '?') {             /* function pointer (callback) */
      uint32_t fn32 = args32[(*ai)++];
      return mcur_put_gp(plan, c, x64_cb_wrap(fn32, &g_objc_cmp_sig));
   }
   if (b == '^' && t[1] == 'v') {
      /* Opaque `void*` arg. When the i386 app holds a value the bridge earlier
       * WRAPPED (a 64-bit native pointer that does NOT fit a 32-bit slot — e.g. a
       * CGContextRef obtained via `-[NSGraphicsContext CGContext]`
       * (`^{CGContext=}`, return KIND 1 -> arena handle) and handed back as the
       * `^v` graphicsPort of `+[NSGraphicsContext graphicsContextWithGraphicsPort:
       * flipped:]`), that value is a low-4GB ARENA HANDLE that must be unwrapped
       * to the real 64-bit pointer. Passing the raw handle makes native code
       * (CGContextRetain) dereference the arena-slot ADDRESS as the object ->
       * SIGSEGV (Quinn: fault 0x343545854, the handle's slot read as a CGContext).
       * x64_objc_unwrap is a pure arena read: an arena handle -> its real 64-bit
       * pointer; EVERY other value (a genuine low-4GB i386 `void*` buffer the
       * native side reads/writes, or NULL) passes through zero-extended
       * unchanged, so real i386 buffers are never disturbed. This is the
       * symmetric inverse of the opaque-pointer RETURN wrap (and mirrors
       * bp_track_tag, which unwraps a wrapped 64-bit token on the remove* arg).
       * Scoped to `^v` only: typed buffers (`^i`/`^c`/`^S`), by-ref out-params
       * (`^@`), and struct pointers (`^{T=body}`) are always genuine app-side
       * <4GB pointers, never wrapped handles, so they keep raw passthrough.
       * (`^{Name=}` opaque CF/CG tokens and `^{Name=#}` object struct-ptrs are
       * already unwrapped by enc_is_cfptr / enc_is_objptr_struct at the top.)
       * UNIVERSAL: triggers on the `^v` encoding crossing into a native method,
       * not on the selector. */
      return mcur_put_gp(plan, c, x64_objc_unwrap(args32[(*ai)++]));
   }
   /* everything else: int/char/short/BOOL/enum, ^i/^c/^@/^{T=body}/^* pointers —
    * one 4-byte slot, GP (raw; genuine i386 pointers already fit <4GB) */
   return mcur_put_gp(plan, c, (uint64_t)args32[(*ai)++]);
}

/* Marshal every explicit method arg from the i386 frame into the plan.
 * `*ai` is the args32 slot cursor (in: first explicit-arg slot; out: one past
 * the last consumed slot). `cur` carries the SysV gp/xmm/stack cursors (gp
 * pre-advanced past self/_cmd/retbuf by the caller). Encodings come from the
 * resolved Method; when that is a NATIVE method but the legacy seltypes
 * registry knows the selector, the legacy i386 encoding wins (it
 * disambiguates CGFloat vs double and NSInteger vs long long). Returns the
 * method-arg count (incl. self+_cmd). */
/* Explicit-argument count of a selector = number of ':' in its name (keyword
 * selectors); a no-colon selector takes 0 explicit args. Used when the runtime
 * has NO Method for the selector (message-forwarding proxies, dynamically
 * resolved methods) so the bridge still knows how many i386 args to marshal. */
static unsigned sel_arg_count(SEL sel) {
   const char *s = sel ? sel_getName(sel) : NULL;
   unsigned n = 0;
   for (; s && *s; ++s) if (*s == ':') ++n;
   return n;
}

static unsigned fill_method_args(struct objc_call_plan *plan,
                                 const uint32_t *args32,
                                 unsigned *ai,
                                 Method m, SEL sel,
                                 struct mcur *cur) {
   /* No Method (m==NULL): the receiver forwards this selector (RKInvoker /
    * NSProxy / NSInvocation-style) or it was resolved dynamically. Without a
    * type encoding we previously marshalled ZERO explicit args, so every
    * forwarded message silently lost its arguments (e.g. RKInvoker forwarding
    * -[NSNotificationCenter postNotification:] delivered a NIL notification ->
    * NSInvalidArgumentException). Derive the count from the selector so the args
    * survive; each is marshalled as an object below (forwarded sends are object
    * messages — see the !enc path). Universal: any forwarding proxy in any app. */
   unsigned nargs = m ? method_getNumberOfArguments(m) : (2 + sel_arg_count(sel));
   const int trace = BRIDGE_TRACE();
   const int conv = method_is_legacy(m) ? CONV_I386 : CONV_NATIVE;
   /* legacy-registry override only matters for native methods */
   const char *lt = (conv == CONV_NATIVE && sel) ? seltypes_lookup(sel) : NULL;
   if (lt) {
      /* trust it only if the explicit-arg count matches the method's */
      if (enc_nth_arg(lt, nargs - 2) != NULL ||
          (nargs >= 3 && enc_nth_arg(lt, nargs - 3) == NULL)) {
         lt = NULL;   /* arg-count mismatch: same name, different signature */
      }
   }
   /* CGFloat-arg fallback for system methods the app only CALLS (no legacy
    * metadata): bit k marks explicit arg k as a CGFloat (i386 4-byte float). */
   const uint32_t cgf_mask = (conv == CONV_NATIVE && sel)
      ? cgfloat_mask_lookup(sel) : 0;
   if (trace) {
      fprintf(stderr, "[fma] m=%p nargs=%u conv=%s%s types=\"%s\"\n", (void *)m,
              nargs, conv == CONV_I386 ? "i386" : "native",
              lt ? "+registry" : "",
              m ? method_getTypeEncoding(m) : "(null)");
      fflush(stderr);
   }
   for (unsigned i = 2; i < nargs; ++i) {
      char *rt_alloc = m ? method_copyArgumentType(m, i) : NULL;
      const char *enc = rt_alloc;
      int aconv = conv;
      int overridden = 0;
      if (lt) {
         const char *le = enc_nth_arg(lt, i - 2);
         if (le) { enc = le; aconv = CONV_I386; overridden = 1; }
      }
      /* CGFloat override: a native 'd'/'f' arg the registry marks as CGFloat is
       * an i386 4-byte float in ONE slot (not an 8-byte double). Force the
       * i386-float reading so marshal_arg_fwd widens it (cvtss2sd) and consumes
       * a single slot, keeping every following arg aligned. */
      if (!overridden && cgf_mask && ((cgf_mask >> (i - 2)) & 1u) && enc) {
         const char *bb = enc_skip_quals(enc);
         if (*bb == 'd' || *bb == 'f') { enc = "f"; aconv = CONV_I386; }
      }
      if (!enc || !*enc) {
         if (!m) {
            /* Forwarded/dynamic selector with no type info: marshal as an
             * object so an i386 proxy handle resolves to the real object the
             * forwarding target expects (a non-object int/BOOL resolves to
             * itself). Without this the arg is dropped -> nil at the target. */
            marshal_arg_fwd(plan, cur, "@", CONV_I386, args32, ai);
         } else {                              /* no type info: raw GP slot */
            mcur_put_gp(plan, cur, (uint64_t)args32[(*ai)++]);
         }
         free(rt_alloc);
         continue;
      }
      unsigned ai_before = *ai;
      marshal_arg_fwd(plan, cur, enc, aconv, args32, ai);
      if (trace) {
         fprintf(stderr, "[fma]   arg%u slots[%u..%u) t=\"%s\" conv=%s "
                 "gp=%u xmm=%u stk=%zu\n",
                 i - 2, ai_before, *ai, enc,
                 aconv == CONV_I386 ? "i386" : "native",
                 cur->gp, cur->xmm, cur->stk);
         fflush(stderr);
      }
      free(rt_alloc);
   }
   return nargs;
}

/* Trace dump: i386 stack arg slots [0..7] in hex. */
static void trace_args(const char *prefix, const char *cls_name,
                       SEL sel, const uint32_t *args32) {
   fprintf(stderr, "[bp] %s class=%s sel=%s args=[",
           prefix, cls_name, (const char *)sel);
   for (unsigned i = 0; i < 8; ++i) {
      fprintf(stderr, "%s%08x", i ? "," : "", args32[i]);
   }
   fprintf(stderr, "]\n");
   fflush(stderr);
}

/*
 * Variadic ObjC method detection.
 *
 * The ObjC runtime doesn't model varargs in the Method type encoding —
 * method_getNumberOfArguments returns only the fixed-arg count. For a
 * method like -[NSArray initWithObjects:] that signature is `(self,_cmd,
 * firstObj)` = 3, even though the real call passes a nil-terminated list.
 *
 * Without varargs handling, bridge_prep zero-fills plan->reg past the
 * fixed count. Foundation does `va_arg(ap, id)` on rcx (4th arg, first
 * past the fixed obj), reads 0, treats it as the nil terminator, and
 * builds an array with just `firstObj` instead of the caller's list.
 * Tessera's `__GLOBAL__I_TESTellus*` static init hits this for
 * `[NSArray arrayWithObjects:n1, n2, nil]` — the truncated array
 * cascades into wild-pointer dereferences a few calls downstream.
 *
 * Fix: maintain a known-varargs sel-name table; for those, walk past
 * the fixed args unwrapping arena handles as `id`-typed until we see
 * a nil sentinel or run out of register slots. Capacity is the same
 * 6-slot plan as fixed args — covers the typical 2-4 explicit varargs
 * we see in real binaries. Longer lists would need plan extension +
 * trampoline stack spill (sysv ABI > 6 GP args land above rsp).
 */
static int sel_has_suffix(const char *s, const char *suf) {
   size_t ls = strlen(s), lf = strlen(suf);
   return ls >= lf && strcmp(s + (ls - lf), suf) == 0;
}

static int is_varargs_sel(const char *sel_name) {
   if (!sel_name) { return 0; }

   /* Suffix families that are reliably variadic across Foundation/AppKit,
    * so we don't have to enumerate every concrete selector. These are
    * conservative (low false-positive): only the trailing keyword of a
    * nil-/format-terminated method matches. */
   static const char *const SUFFIXES[] = {
      "WithObjects:",          /* arrayWithObjects:, setWithObjects:, ... */
      "ObjectsAndKeys:",       /* dictionaryWithObjectsAndKeys:, init... */
      "WithFormat:",           /* stringWithFormat:, initWithFormat:, predicateWithFormat:, ... */
      "AppendingFormat:",      /* stringByAppendingFormat: */
      "appendFormat:",
      ":format:",              /* +[NSException raise:format:] etc. */
      NULL,
   };
   for (const char *const *p = SUFFIXES; *p; ++p) {
      if (sel_has_suffix(sel_name, *p)) { return 1; }
   }

   /* Allow a target-specific extension list without recompiling:
    * ABICONV_VARARGS_SELS="selA:,selB:" (comma/space separated, exact match). */
   const char *env = getenv("ABICONV_VARARGS_SELS");
   if (env && *env) {
      size_t ln = strlen(sel_name);
      const char *p = env;
      while (*p) {
         while (*p == ',' || *p == ' ') { ++p; }
         const char *start = p;
         while (*p && *p != ',' && *p != ' ') { ++p; }
         size_t len = (size_t)(p - start);
         if (len == ln && strncmp(start, sel_name, len) == 0) { return 1; }
      }
   }
   return 0;
}

/* Format-string-taking variadic selectors: stringWithFormat:, initWithFormat:,
 * predicateWithFormat:, stringByAppendingFormat:, appendFormat:, raise:format:,
 * etc. Distinguished from the nil-terminated id-list varargs (WithObjects:,
 * ObjectsAndKeys:) because their args are TYPED by the format string, not an
 * id list — each conversion tells us whether the arg is an object (resolve the
 * proxy handle to the real id), an integer (pass through), or a float. */
static int is_format_sel(const char *s) {
   return s && (sel_has_suffix(s, "WithFormat:") || sel_has_suffix(s, "AppendingFormat:")
                || sel_has_suffix(s, "appendFormat:") || sel_has_suffix(s, ":format:"));
}

/* ObjC TAGGED-POINTER object: a VALUE, not an address (bit 0 on x86_64, bit
 * 63 on arm64 — this runtime is x86_64-under-Rosetta but check both). Short
 * (<=7 ASCII char) NSString/CFString contents are realized as tagged pointers
 * by Foundation — including the real CFStrings i386_cfstr_to_real mints for
 * short i386 @"..." constants. mem_readable() (a genuine address probe)
 * always REJECTS them, so any readability gate must exempt them; CF/objc
 * themselves handle tagged pointers natively (CFGetTypeID /
 * CFStringGetCString are tagged-safe). */
static int objc_tagged_ptr(uint64_t obj) {
   return (obj & 1ULL) != 0 || (obj >> 63) != 0;
}

/* Extract the C-string of a real Foundation format object (an NSString, which
 * may be a translated low-address constant @"...", a real 64-bit NSString, or
 * a TAGGED-POINTER short string). Returns 1 and fills buf on success; 0 if
 * obj is not a usable CFString.
 * ★The tagged exemption is load-bearing: a short format like @"Msg%s" (Halo's
 * alert-title key, <=7 chars) unwraps to a tagged pointer; gating it through
 * mem_readable called it "unreadable", so shim_CFStringCreateWithFormat took
 * the no-directive fast path and returned the format LITERALLY — the visible
 * unsubstituted "Msg%s" alert title. */
static int format_cstr(uint64_t obj, char *buf, size_t buflen) {
   if (obj == 0) { return 0; }
   if (!objc_tagged_ptr(obj)
       && !mem_readable((uintptr_t)obj, sizeof(void *))) { return 0; }
   CFTypeRef cf = (CFTypeRef)(uintptr_t)obj;
   if (CFGetTypeID(cf) != CFStringGetTypeID()) { return 0; }
   return CFStringGetCString((CFStringRef)cf, buf, (CFIndex)buflen,
                             kCFStringEncodingUTF8) ? 1 : 0;
}

/*
 * Place a format method's variadic args into the plan's GP registers, typed by
 * the format string. %@ args are resolved handle->real id (the whole point);
 * integers/strings/pointers pass through (NO unwrap — an int value that happens
 * to land in the arena range must not be "resolved"); floats would need XMM
 * (not modelled by the plan/asm) so they are skipped with a warning. Returns
 * the next free plan->reg index (so the caller can compute plan->nreg).
 *
 * gp/gp_cap index plan->reg directly; ai indexes args32. In x86_64 SysV
 * varargs, GP and SSE arg sequences are INDEPENDENT, so skipping a float does
 * not perturb the GP slot of a following %@.
 */
static unsigned fill_format_varargs(struct objc_call_plan *plan,
                                    const uint32_t *args32,
                                    unsigned ai, unsigned gp, unsigned gp_cap,
                                    const char *fmt, struct mcur *cur) {
   const int trace = BRIDGE_TRACE();
   /* base slot/arg indices, for POSITIONAL specifiers (%N$conv): position N
    * (1-based) maps to the Nth vararg, independent of textual order. Localized
    * NSLocalizedString format strings use these heavily so translators can
    * reorder args (e.g. @"%1$@'s Library"). */
   const unsigned gp0 = gp, ai0 = ai;
   unsigned max_gp = gp;            /* highest reg slot written + 1 */
   for (const char *p = fmt; *p; ++p) {
      if (*p != '%') { continue; }
      ++p;
      if (*p == '%' || *p == '\0') { continue; }     /* literal %% */
      /* positional argument specifier "N$" (must come right after %). */
      int pos = -1;
      {
         const char *q = p;
         unsigned v = 0;
         while (*q >= '0' && *q <= '9') { v = v * 10u + (unsigned)(*q - '0'); ++q; }
         if (q != p && *q == '$') { pos = (int)v; p = q + 1; }
      }
      /* flags */
      while (*p && strchr("-+ #0'", *p)) { ++p; }
      /* width: digits or '*' (a non-positional '*' consumes an int arg) */
      if (*p == '*') {
         if (pos < 0) {
            if (gp < gp_cap) { plan_put(plan, gp, (uint64_t)args32[ai]);
                               if (gp + 1 > max_gp) { max_gp = gp + 1; } ++gp; }
            ai++;
         }
         ++p;
      } else { while (*p >= '0' && *p <= '9') { ++p; } }
      /* precision */
      if (*p == '.') {
         ++p;
         if (*p == '*') {
            if (pos < 0) {
               if (gp < gp_cap) { plan_put(plan, gp, (uint64_t)args32[ai]);
                                  if (gp + 1 > max_gp) { max_gp = gp + 1; } ++gp; }
               ai++;
            }
            ++p;
         } else { while (*p >= '0' && *p <= '9') { ++p; } }
      }
      /* length modifiers; 64-bit only for ll/q/j (i386 long/size_t are 4 bytes) */
      int len64 = 0;
      while (*p && strchr("hlLqjzt", *p)) {
         if ((*p == 'l' && p[1] == 'l') || *p == 'q' || *p == 'j') { len64 = 1; }
         ++p;
      }
      char c = *p;
      if (c == '\0') { break; }
      /* Destination reg slot + source arg index: positional uses N-1 from the
       * base; sequential uses the running cursors. (i386 positional args are
       * 4-byte slots — objects/ints/strings — so position N == slot N-1; 64-bit
       * and FP positionals are rare in localized strings and not handled.) */
      const unsigned tg = (pos > 0) ? (gp0 + (unsigned)(pos - 1)) : gp;
      const unsigned ta = (pos > 0) ? (ai0 + (unsigned)(pos - 1)) : ai;
      switch (c) {
      case '@':
         if (tg < gp_cap) { plan_put(plan, tg, unwrap_obj_arg(args32[ta]));
                            if (tg + 1 > max_gp) { max_gp = tg + 1; } }
         break;
      case 'd': case 'i': case 'u': case 'x': case 'X': case 'o': case 'c': case 'C':
         if (len64 && pos < 0) {
            if (tg < gp_cap) {
               plan_put(plan, tg, (uint64_t)args32[ta] | ((uint64_t)args32[ta + 1] << 32));
               if (tg + 1 > max_gp) { max_gp = tg + 1; }
            }
         } else {
            if (tg < gp_cap) { plan_put(plan, tg, (uint64_t)args32[ta]);
                               if (tg + 1 > max_gp) { max_gp = tg + 1; } }
         }
         break;
      case 's': case 'S': case 'p':
         if (tg < gp_cap) { plan_put(plan, tg, (uint64_t)args32[ta]);
                            if (tg + 1 > max_gp) { max_gp = tg + 1; } }
         break;
      case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A':
         /* default-promoted to double: 8 bytes on the i386 stack -> next XMM
          * slot (SysV varargs: GP and SSE sequences are independent; al
          * carries the SSE count via plan->nxmm). */
         if (pos >= 0) { break; }              /* positional FP: unsupported */
         if (cur) {
            uint64_t bits = (uint64_t)args32[ai] | ((uint64_t)args32[ai + 1] << 32);
            mcur_put_xmm(plan, cur, bits);
            plan->nxmm = cur->xmm;
         } else if (trace) {
            fprintf(stderr, "[bp] WARN: format %%%c (float/double) not bridged to "
                    "XMM here; that conversion will print garbage\n", c);
            fflush(stderr);
         }
         ai += 2;
         continue;                  /* no GP slot consumed */
      default:
         if (tg < gp_cap) { plan_put(plan, tg, (uint64_t)args32[ta]);
                            if (tg + 1 > max_gp) { max_gp = tg + 1; } }
         break;
      }
      /* advance the running cursors only for sequential (non-positional) args */
      if (pos < 0) {
         ai += (len64 && c != '@' && c != 's' && c != 'S' && c != 'p') ? 2u : 1u;
         ++gp;
      }
   }
   return max_gp;
}

/* ---- Legacy AppKit alert panels (iPhoto 25th blocker) ----
 * NSRunAlertPanel & friends are VARIADIC C functions, so abigen skips them
 * and the translated binds went straight into native AppKit with arena
 * handles in the NSString slots — native formatting then ran
 * objc_msgSend(handle, ...) with the handle as receiver (the slot's 64-bit
 * backing read as an isa). Hand shims: unwrap the five fixed NSString args,
 * expand format + i386 varargs into a real NSString HERE (fill_format_varargs
 * resolves %@/%N$@ handles), then call the native panel with an inert "%@".
 * The natives are dlsym'd so libabiconv keeps not linking AppKit.
 * i386 arg block: a[0]=title a[1]=msgFormat a[2]=defaultBtn a[3]=altBtn
 * a[4]=otherBtn a[5...]=varargs (4-byte slots). */

static id alert_format_message(uint32_t fmt32, const uint32_t *va32) {
   uint64_t fmt = unwrap_obj_arg(fmt32);
   if (!fmt) { return (id)CFSTR(""); }
   char fmtbuf[2048];
   if (!format_cstr(fmt, fmtbuf, sizeof fmtbuf) || !strchr(fmtbuf, '%')) {
      return (id)(uintptr_t)fmt;          /* literal or unreadable: as-is */
   }
   struct objc_call_plan plan;
   memset(&plan, 0, sizeof plan);
   /* GP slots 0..2 = cls/sel/format of the stringWithFormat: call below */
   fill_format_varargs(&plan, va32, /*ai=*/0, /*gp=*/3,
                       /*gp_cap=*/6 + PLAN_STACK_MAX, fmtbuf, /*cur=*/NULL);
   /* a true variadic prototype so the compiler zeroes al (no XMM varargs —
    * fill_format_varargs drops float conversions, a known gap) */
   id msg = ((id (*)(id, SEL, id, ...))objc_msgSend)(
      (id)objc_getClass("NSString"), sel_registerName("stringWithFormat:"),
      (id)(uintptr_t)fmt,
      plan.reg[3], plan.reg[4], plan.reg[5],
      plan.stack[0], plan.stack[1], plan.stack[2], plan.stack[3],
      plan.stack[4], plan.stack[5], plan.stack[6], plan.stack[7]);
   return msg ? msg : (id)(uintptr_t)fmt;
}

static uint32_t alert_panel_common(const char *fn_name, const uint32_t *a,
                                   int returns_obj) {
   void *fn = dlsym(RTLD_DEFAULT, fn_name);
   id title = (id)(uintptr_t)unwrap_obj_arg(a[0]);
   id msg   = alert_format_message(a[1], a + 5);
   id def   = (id)(uintptr_t)unwrap_obj_arg(a[2]);
   id alt   = (id)(uintptr_t)unwrap_obj_arg(a[3]);
   id other = (id)(uintptr_t)unwrap_obj_arg(a[4]);
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[alert] %s%s title=%p def=%p alt=%p other=%p msg=%p\n",
              fn_name, fn ? "" : " (MISSING)", (void *)title, (void *)def,
              (void *)alt, (void *)other, (void *)msg);
      fflush(stderr);
   }
   if (!fn) { return 0; }
   if (returns_obj) {
      id p = ((id (*)(id, id, id, id, id, ...))fn)(
         title, (id)CFSTR("%@"), def, alt, other, msg);
      return x64_objc_wrap((uint64_t)(uintptr_t)p);
   }
   long r = ((long (*)(id, id, id, id, id, ...))fn)(
      title, (id)CFSTR("%@"), def, alt, other, msg);
   return (uint32_t)r;
}

uint32_t shim_NSRunAlertPanel(const uint32_t *a) {
   return alert_panel_common("NSRunAlertPanel", a, 0);
}
uint32_t shim_NSRunCriticalAlertPanel(const uint32_t *a) {
   return alert_panel_common("NSRunCriticalAlertPanel", a, 0);
}
uint32_t shim_NSRunInformationalAlertPanel(const uint32_t *a) {
   return alert_panel_common("NSRunInformationalAlertPanel", a, 0);
}
uint32_t shim_NSGetAlertPanel(const uint32_t *a) {
   return alert_panel_common("NSGetAlertPanel", a, 1);
}

/* ---- Variadic CF formatting (Halo wall) ----
 * CFStringCreateWithFormat is VARIADIC, so abigen skips it and the
 * translated bind went straight into native CoreFoundation with i386 cdecl
 * STACK varargs — native read the SysV registers (garbage) and crashed
 * inside CFStringCreateWithFormatAndArguments. Same cure as the alert
 * panels: unwrap the fixed args, expand the CF-style format + i386 varargs
 * through fill_format_varargs (positional %N$ + %@ handle resolution), and
 * make the native variadic call from the converted plan slots.
 * i386 arg block: a[0]=alloc a[1]=formatOptions a[2]=format a[3...]=varargs */
/* Common CF-format core. `va32` is a flat array of 4-byte i386 vararg slots —
 * either the inline stack varargs (CFStringCreateWithFormat) OR the pointee of an
 * i386 va_list (CFStringCreateWithFormatAndArguments): an i386 va_list is a plain
 * char* pointing straight at the first vararg, so both cases hand
 * fill_format_varargs the same flat slot array. Reusing the VARIADIC
 * CFStringCreateWithFormat with the expanded plan is equivalent to the
 * AndArguments form (the variadic one calls it internally) and needs no x86_64
 * va_list synthesized. Returns a wrapped CFStringRef handle (0 on failure). */
static uint32_t cfstring_with_format_core(uint32_t alloc32, uint32_t opts32,
                                          uint32_t fmt32, const uint32_t *va32) {
   CFAllocatorRef  alloc = (CFAllocatorRef)(uintptr_t)unwrap_obj_arg(alloc32);
   CFDictionaryRef opts  = (CFDictionaryRef)(uintptr_t)unwrap_obj_arg(opts32);
   uint64_t fmt = unwrap_obj_arg(fmt32);
   if (!fmt) { return 0; }
   char fmtbuf[2048];
   if (!format_cstr(fmt, fmtbuf, sizeof fmtbuf) || !strchr(fmtbuf, '%')) {
      /* no directives (or unreadable format): plain copy semantics */
      CFStringRef s = CFStringCreateCopy(alloc, (CFStringRef)(uintptr_t)fmt);
      return s ? x64_objc_wrap((uint64_t)(uintptr_t)s) : 0;
   }
   struct objc_call_plan plan;
   memset(&plan, 0, sizeof plan);
   /* GP slots 0..2 = alloc/options/format of the native call below */
   fill_format_varargs(&plan, va32, /*ai=*/0, /*gp=*/3,
                       /*gp_cap=*/6 + PLAN_STACK_MAX, fmtbuf, /*cur=*/NULL);
   /* a true variadic prototype so the compiler zeroes al (no XMM varargs —
    * fill_format_varargs drops float conversions, the known alert-panel gap) */
   CFStringRef s = ((CFStringRef (*)(CFAllocatorRef, CFDictionaryRef,
                                     CFStringRef, ...))CFStringCreateWithFormat)(
      alloc, opts, (CFStringRef)(uintptr_t)fmt,
      plan.reg[3], plan.reg[4], plan.reg[5],
      plan.stack[0], plan.stack[1], plan.stack[2], plan.stack[3],
      plan.stack[4], plan.stack[5], plan.stack[6], plan.stack[7]);
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[cfformat] CFStringCreateWithFormat fmt=\"%s\" -> %p\n",
              fmtbuf, (void *)s);
      fflush(stderr);
   }
   return s ? x64_objc_wrap((uint64_t)(uintptr_t)s) : 0;
}

uint32_t shim_CFStringCreateWithFormat(const uint32_t *a) {
   /* i386 arg block: a[0]=alloc a[1]=formatOptions a[2]=format a[3...]=varargs */
   return cfstring_with_format_core(a[0], a[1], a[2], a + 3);
}

/* CFStringCreateWithFormatAndArguments(alloc, options, format, va_list) — the
 * va_list sibling of CFStringCreateWithFormat (the Msg%s / Halo wall family).
 * abigen can't marshal a va_list arg (the i386 va_list is a char* to the stack
 * varargs, NOTHING like the x86_64 __va_list_tag register-save struct), so its
 * auto-shim deep-copied garbage. i386 arg block: a[0]=alloc a[1]=options
 * a[2]=format a[3]=va_list (a pointer to the first 4-byte vararg slot). */
uint32_t shim_CFStringCreateWithFormatAndArguments(const uint32_t *a) {
   return cfstring_with_format_core(a[0], a[1], a[2],
                                    (const uint32_t *)(uintptr_t)a[3]);
}

/* CFStringAppendFormatAndArguments(theString, options, format, va_list): appends
 * the formatted result to a mutable string (void). Same va_list translation, but
 * the native call is the variadic CFStringAppendFormat and there is no return.
 * i386 arg block: a[0]=theString a[1]=options a[2]=format a[3]=va_list. */
uint32_t shim_CFStringAppendFormatAndArguments(const uint32_t *a) {
   CFMutableStringRef str = (CFMutableStringRef)(uintptr_t)unwrap_obj_arg(a[0]);
   CFDictionaryRef    opts = (CFDictionaryRef)(uintptr_t)unwrap_obj_arg(a[1]);
   uint64_t fmt = unwrap_obj_arg(a[2]);
   const uint32_t *va32 = (const uint32_t *)(uintptr_t)a[3];
   if (!str || !fmt) { return 0; }
   char fmtbuf[2048];
   if (!format_cstr(fmt, fmtbuf, sizeof fmtbuf) || !strchr(fmtbuf, '%')) {
      CFStringAppend(str, (CFStringRef)(uintptr_t)fmt);
      return 0;
   }
   struct objc_call_plan plan;
   memset(&plan, 0, sizeof plan);
   fill_format_varargs(&plan, va32, /*ai=*/0, /*gp=*/3,
                       /*gp_cap=*/6 + PLAN_STACK_MAX, fmtbuf, /*cur=*/NULL);
   ((void (*)(CFMutableStringRef, CFDictionaryRef, CFStringRef, ...))
    CFStringAppendFormat)(
      str, opts, (CFStringRef)(uintptr_t)fmt,
      plan.reg[3], plan.reg[4], plan.reg[5],
      plan.stack[0], plan.stack[1], plan.stack[2], plan.stack[3],
      plan.stack[4], plan.stack[5], plan.stack[6], plan.stack[7]);
   return 0;
}

/*
 * Common core for all msgSend variants. The shape:
 *   - real_self is already resolved (by variant-specific entry below);
 *   - sel is already registered;
 *   - args32 + arg_base_idx points at the first explicit-arg i386 slot;
 *   - reg_base is the first plan->reg slot to write args into.
 *
 *  variants:
 *    msgSend:        reg_base=2 (self@0, _cmd@1)
 *    msgSendSuper:   reg_base=2 (super@0, _cmd@1)
 *    msgSend_stret:  reg_base=3 (retbuf@0, self@1, _cmd@2)
 *    msgSendSuper_stret: reg_base=3 (retbuf@0, super@1, _cmd@2)
 *
 *  arg_base_idx is the args32 index of the first explicit arg
 *  (corresponds to method-arg index 2).
 */
/* `lookup` is the class whose method table already holds the selector at the
 * right level: object_getClass(receiver) for normal sends (instance → class,
 * class → metaclass), the super class itself for super sends (prep_super has
 * already metaclass-converted it for class-method super calls — passing it
 * through object_getClass here would over-step to the metaclass and miss
 * every instance method, leaving m=NULL → args unmarshalled, ret untyped). */
static Method fill_args_and_return(struct objc_call_plan *plan,
                                   const uint32_t *args32,
                                   unsigned arg_base_idx,
                                   unsigned reg_base,
                                   Class lookup, SEL sel) {
   Method m = NULL;
   if (lookup) {
      m = class_getInstanceMethod(lookup, sel);
   }

   struct mcur cur = { reg_base, 0, 0 };
   unsigned ai = arg_base_idx;
   unsigned nargs = fill_method_args(plan, args32, &ai, m, sel, &cur);

   /* Varargs continuation. Foundation methods like initWithObjects: take
    * a nil-terminated id list past their fixed-arg count. The signature
    * doesn't model varargs so method_getNumberOfArguments stops at
    * firstObj; the next register would stay zero (== nil) and Foundation
    * would terminate the list early. Walk forward through args32 until
    * nil sentinel or plan capacity. */
   unsigned final_pos = (cur.gp < 6 && cur.stk == 0)
      ? cur.gp : 6 + (unsigned)(cur.stk / 8);
   /* A LEGACY (reverse-bridge) method may itself be variadic (its i386 IMP does
    * va_start past the single declared arg, e.g. -[ATAnimationGroup
    * addAnimations:a,b,nil]). The encoding doesn't say so, so detect it from the
    * IMP prologue and spill the same nil-terminated id list; the reverse bridge
    * re-lays it into the IMP's i386 frame. (Quinn 0x00080000 garbage-receiver.) */
   /* The forward bridge runs in the CALLER's libabiconv copy, whose g_rmeth is
    * empty for a class some OTHER copy reverse-registered (and whose local
    * _86x64_reverse_imp address won't match the registered IMP) — so consult the
    * SHARED rvariadic registry the registering copy published. lookup/sel are
    * process-global Class/SEL, valid in any copy. */
   int legacy_va = (m && !is_varargs_sel((const char *)sel))
                      ? rvar_contains(lookup, sel) : 0;
   if (m && (is_varargs_sel((const char *)sel) || legacy_va)) {
      char fmtbuf[2048];
      /* the format NSString is the last fixed arg == last GP slot filled */
      uint64_t fmt_slot = (cur.gp > reg_base && cur.gp <= 6)
         ? plan->reg[cur.gp - 1] : 0;
      if (is_format_sel((const char *)sel) && nargs >= 3 && fmt_slot
          && format_cstr(fmt_slot, fmtbuf, sizeof fmtbuf)) {
         /* Format-typed varargs: place the following args by conversion type,
          * resolving %@ handles, spilling past the 6 GP regs onto the plan
          * stack. FP conversions go to the xmm block (see fill_format_varargs). */
         unsigned gp_end = fill_format_varargs(plan, args32, ai, final_pos,
                                               /*gp_cap=*/6 + PLAN_STACK_MAX,
                                               fmtbuf, &cur);
         final_pos = gp_end;
      } else {
         /* nil-terminated id list (arrayWithObjects:, dictionaryWith...Keys:).
          * Walk forward placing each object (registers then stack) up to and
          * INCLUDING the nil terminator — when the explicit objects fill all 6
          * GP registers the terminator itself must still go out, in stack[0],
          * or Foundation reads uninitialized stack and retains garbage.
          *
          * If the list is longer than the plan can hold, force the nil
          * terminator into the LAST available slot rather than breaking with a
          * real object there: a missing terminator makes Foundation read past
          * the marshalled args into uninitialized stack -> garbage key/value ->
          * SIGSEGV or an uncaught NSException (the Quinn/Sparkle crash). The cap
          * is sized so real literals never reach here; this is the safety net,
          * which truncates gracefully (terminated) instead of corrupting. */
         const unsigned pos_cap = 6 + PLAN_STACK_MAX;
         unsigned i386_i = ai, pos = final_pos;
         for (;;) {
            if (pos >= pos_cap) {
               plan_put(plan, pos - 1, 0);   /* force-terminate at capacity */
               break;
            }
            uint32_t a = args32[i386_i++];
            plan_put(plan, pos++, unwrap_obj_arg(a));
            if (a == 0) { break; }        /* placed the nil terminator */
         }
         final_pos = pos;
      }
   }

   plan->nreg   = (int32_t)(final_pos > 6 ? 6 : final_pos);
   {
      unsigned stk_q = (unsigned)(cur.stk / 8);
      unsigned va_q  = final_pos > 6 ? final_pos - 6 : 0;
      plan->nstack = stk_q > va_q ? stk_q : va_q;
   }
   plan->nxmm = cur.xmm;

   /* ret_is_obj is really a return-KIND for the asm trampoline:
    *   0 = scalar (pass rax straight back, truncated to eax by caller)
    *   1 = object/Class  (wrap the 64-bit id into a 32-bit arena handle)
    *   2 = C-string      (bounce the 64-bit char* into a low-4GB buffer)
    *   3 = stret narrow  (native MEMORY-class struct in plan->stret_buf ->
    *                      narrowed into the i386 caller's buffer; set by the
    *                      stret preps, never here)
    *   4 = fp double     (native xmm0 double -> x87 st0 for the i386 caller)
    *   5 = small struct  (native rax/rdx/xmm0/xmm1 -> narrowed 8-byte i386
    *                      struct returned in eax:edx)
    *   6 = reg-return struct into i386 stret buffer (set by stret preps)
    *   7 = fp float      (native xmm0 single -> x87 st0)
    *   8 = 64-bit int    (remap the NSNotFound sentinel: native NSIntegerMax
    *                      0x7fffffffffffffff -> i386 NSIntegerMax 0x7fffffff so
    *                      32-bit `cmp eax,0x7fffffff` NSNotFound termination tests
    *                      fire; all other values pass through rax/rdx unchanged)
    *   9 = token wrap    (plain >4GB token -> 32-bit arena handle, no obj deref)
    *  10 = opaque void*  (`^v` return: conditionally wrap a >4GB native pointer
    *                      into a 32-bit arena handle, low pointers pass through) */
   char *rt = m ? method_copyReturnType(m) : NULL;
   const int rconv = method_is_legacy(m) ? CONV_I386 : CONV_NATIVE;
   char rb = rt ? *enc_skip_quals(rt) : 0;
   if (rt && (rb == '@' || rb == '#')) {
      plan->ret_is_obj = 1;
   } else if (rt && rb == '*') {
      plan->ret_is_obj = 2;
   } else if (enc_is_objptr_struct(rt) || enc_is_cfptr(rt)) {
      plan->ret_is_obj = 1;          /* ^{Class=#...} obj / ^{CF=} ref -> wrap */
   } else if (enc_is_opaque_voidptr(rt)) {
      /* `^v` (void*) opaque-pointer return, e.g. -[NSGraphicsContext graphicsPort]
       * -> CGContextRef. A >4GB native pointer truncates in the i386 caller's eax;
       * conditionally wrap it (>4GB only, like abigen's `.cfretlow`) into a low-4GB
       * arena handle so the opaque-pointer-consuming C shims recover the real
       * pointer (CGContext* via convert_cf_ptr's __86x64_unwrap_obj_arg). The
       * symmetric inverse of the `^v` ARG unwrap in fill_method_args. Quinn
       * 3.5.7's _QuinnGeneralFastDrawCells fetches its draw context this way; the
       * pre-fix truncated CGContextRef made every CGContextDrawImage/FillRect
       * silently draw into an invalid context -> invisible Tetris blocks. */
      plan->ret_is_obj = 10;
   } else if (rb == 'd' || (rb == 'f' && rconv == CONV_I386)) {
      /* CGFloat/double: the i386 caller dispatched via _fpret and reads st0.
       * A legacy 'f' (CGFloat) return arrives as a double too (the reverse
       * bridge widens st0 -> xmm0 double). */
      plan->ret_is_obj = 4;
   } else if (rb == 'f') {
      plan->ret_is_obj = 7;          /* genuine float: xmm0 single -> st0 */
   } else if (rb == '{' || rb == '(' || rb == '[') {
      /* aggregate return through the NON-stret entry: i386 size <= 8 (else
       * the caller would have used _stret), native <= 16 (register classes) */
      size_t isz = 0, nsz = 0; uint8_t sse[8];
      enc_classify(rt ? enc_skip_quals(rt) : "", rconv, &isz, &nsz, sse);
      if (nsz > 0 && nsz <= 16 && isz <= 8) {
         plan->ret_is_obj = 5;
         plan->sret_enc  = method_getTypeEncoding(m);  /* stable runtime str */
         plan->sret_conv = (uint32_t)rconv;
      } else {
         plan->ret_is_obj = 0;
      }
   } else if (rb == 'q' || rb == 'Q' || rb == 'l' || rb == 'L') {
      /* 64-bit integer return (NSInteger/NSUInteger/long/long long). The native
       * value is truncated to eax by the i386 caller; the ONLY value that breaks
       * is the NSNotFound sentinel (NSIntegerMax = 0x7fffffffffffffff on x86_64
       * vs 0x7fffffff on i386), whose truncation (0xffffffff) never matches the
       * i386's `cmp eax,0x7fffffff` -> infinite loops in NSIndexSet /
       * NSArray indexOfObject: enumeration. Kind 8 remaps just that sentinel in
       * the asm; every other value (incl. genuine int64 edx:eax) passes through. */
      plan->ret_is_obj = 8;
   } else {
      plan->ret_is_obj = 0;
   }
   if (BRIDGE_TRACE() && sel) {
      fprintf(stderr, "[bp]   ret_kind=%d rt=\"%s\" sel=%s m=%p nxmm=%u nstk=%u\n",
              plan->ret_is_obj, rt ? rt : "(null)", sel_getName(sel), (void*)m,
              plan->nxmm, plan->nstack);
      fflush(stderr);
   }
   free(rt);
   return m;
}

/* ===========================================================================
 * Legacy-AppKit method compat.
 *
 * Old apps call (often via [super ...] from their own overrides) methods that
 * were deprecated in 10.4/10.5 and have since been DELETED from modern AppKit,
 * so both the forward bridge and the super-dispatch path resolve them to
 * m=0x0 and the call silently no-ops. Reinstall them on the REAL system class
 * with class_addMethod, re-implemented per the documented 10.x behavior and
 * dispatching every step through objc_msgSend so subclass overrides still win.
 *
 * First instance (iPhoto 20th blocker): -[ArchiveDocController
 * openUntitledDocumentOfType:display:] does [super openUntitledDocumentOfType:
 * display:]; NSDocumentController no longer implements it, so no document is
 * ever created, attemptedOpenUntitledDoc stays NO, and applicationDidFinish-
 * Launching: re-arms itself via performSelector:afterDelay: forever — the app
 * idles windowless.
 *
 * Installed lazily from the bridge entry points (the legacy app's first
 * message send happens long after AppKit is loaded). Idempotent across the
 * several co-located libabiconv copies: class_addMethod refuses duplicates.
 * =========================================================================== */

static id msg_id0(id r, SEL s) {
   return ((id (*)(id, SEL))objc_msgSend)(r, s);
}
static id msg_id1(id r, SEL s, id a) {
   return ((id (*)(id, SEL, id))objc_msgSend)(r, s, a);
}

/* 10.x: - (id)makeUntitledDocumentOfType:(NSString *)type */
static id compat_makeUntitledDocumentOfType(id self, SEL _cmd, id type) {
   (void)_cmd;
   /* Prefer the modern replacement (still present, subclass override wins). */
   SEL s_modern = sel_registerName("makeUntitledDocumentOfType:error:");
   if (class_getInstanceMethod(object_getClass(self), s_modern)) {
      id err = nil;
      return ((id (*)(id, SEL, id, id *))objc_msgSend)(self, s_modern, type, &err);
   }
   /* Fallback: the documented 10.x implementation. */
   Class cls = (Class)msg_id1(self, sel_registerName("documentClassForType:"), type);
   if (!cls) return nil;
   id doc = msg_id0(msg_id0((id)cls, sel_registerName("alloc")),
                    sel_registerName("init"));
   if (doc) msg_id1(doc, sel_registerName("setFileType:"), type);
   return doc;
}

/* 10.x: - (id)openUntitledDocumentOfType:(NSString *)type display:(BOOL)flag */
static id compat_openUntitledDocumentOfType_display(id self, SEL _cmd,
                                                    id type, signed char display) {
   (void)_cmd;
   id doc = msg_id1(self, sel_registerName("makeUntitledDocumentOfType:"), type);
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[compat] openUntitledDocumentOfType:%p display:%d -> doc=%p\n",
              (void *)type, (int)display, (void *)doc);
      fflush(stderr);
   }
   if (!doc) return nil;
   msg_id1(self, sel_registerName("addDocument:"), doc);
   if (display) {
      msg_id0(doc, sel_registerName("makeWindowControllers"));
      msg_id0(doc, sel_registerName("showWindows"));
   }
   return doc;
}

/* ---- iLife ProKit font construction (NSProFont) ----
 * +[NSProFont _proSystemFontWithFontName:pointSize:fontAppearance:
 * useSystemHelveticaAdjustments:] drives a private CoreText/UIFoundation typeface
 * init path that is gone/incompatible on modern macOS: iLife'11 ProKit sends
 * -[IPKFont initWithInstanceInfo:renderingMode:] (unrecognized selector on the
 * NSProFont subclass), and later ProKit builds send -[NSFont initWithTypefaceInfo:
 * key:renderingMode:] -> CTFontGetClientObject access fault. This is the SAME
 * crash Retroactive hits running Aperture/iPhoto, and the SAME fix: bypass the
 * pro-font construction and return a plain system font of the requested size.
 * boldSystemFontOfSize:withAppearance: and proSetFont: both funnel through here,
 * so this one swizzle covers the family. Cosmetic only (loses the custom
 * Helvetica-adjusted pro font). */
static id compat_proSystemFont(id self, SEL _cmd, id name, double size,
                               id appearance, signed char adj) {
   (void)self; (void)_cmd; (void)name; (void)appearance; (void)adj;
   Class nsfont = objc_getClass("NSFont");
   if (!nsfont) { return nil; }
   if (!(size > 0.0)) { size = 13.0; }
   return ((id (*)(id, SEL, double))objc_msgSend)(
      (id)nsfont, sel_registerName("systemFontOfSize:"), size);
}

/* Replace the broken class method's IMP once ProKit is loaded. Once-per-process
 * (env guard, like NSColor) — but only marked done after the swizzle lands, so
 * it retries on later message sends until ProKit appears. */
static void prokit_font_compat_install(void) {
   if (getenv("ABICONV_PROFONT_COMPAT")) { return; }
   Class c = objc_getClass("NSProFont");
   if (!c) { return; }                  /* ProKit not loaded yet: retry */
   SEL s = sel_registerName("_proSystemFontWithFontName:pointSize:"
                            "fontAppearance:useSystemHelveticaAdjustments:");
   Method m = class_getClassMethod(c, s);   /* class method -> metaclass */
   if (!m) { return; }
   method_setImplementation(m, (IMP)compat_proSystemFont);
   setenv("ABICONV_PROFONT_COMPAT", "1", 1);
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[compat] swizzled +[NSProFont _proSystemFontWithFontName:"
              "...] -> systemFontOfSize:\n");
      fflush(stderr);
   }
}

/* ---- NSColor component getters (iPhoto 24th blocker) ----
 * Modern AppKit RAISES NSInvalidArgumentException from getRed:green:blue:
 * alpha: (and family) when the receiver is an extended-sRGB / HDR / catalog
 * system color; 10.6 answered for anything RGB-convertible. Swizzle the
 * originals to pre-convert the receiver via colorUsingColorSpace: into the
 * method's natural space, then run the original IMP on the converted color.
 *
 * Same call, second gap: i386 CGFloat is a 4-byte FLOAT. A legacy caller's
 * out-pointers are 4-byte slots — the native double writes would both feed
 * the caller garbage and overrun its stack. Legacy pointers are always
 * < 4GB (i386 stacks/heaps live there by construction; native stack/heap
 * out-params never do), so low out-pointers select a narrow-to-float path
 * through a local double buffer. */

static id compat_color_convert(id c, const char *space_sel) {
   Class nscs = objc_getClass("NSColorSpace");
   if (!nscs) { return c; }
   id space = ((id (*)(Class, SEL))objc_msgSend)(
      nscs, sel_registerName(space_sel));
   if (!space) { return c; }
   id conv = ((id (*)(id, SEL, id))objc_msgSend)(
      c, sel_registerName("colorUsingColorSpace:"), space);
   return conv ? conv : c;
}

typedef void (*color_get2_t)(id, SEL, double *, double *);
typedef void (*color_get4_t)(id, SEL, double *, double *, double *, double *);
typedef void (*color_get5_t)(id, SEL, double *, double *, double *, double *,
                             double *);

/* The raising implementations live on CONCRETE NSColor subclasses
 * (NSColorSpaceColor for the extended-sRGB/HDR system colors,
 * _NSTaggedPointerColor, NSCalibratedRGBColor) — a base-class swizzle never
 * runs for them. Swizzle every NSColor-family class that implements the
 * getter, keeping each class's original IMP in a small table searched up the
 * superclass chain at call time. */
#define COLOR_SWZ_SELS 4
#define COLOR_SWZ_MAX  24
struct color_orig_ent { Class cls; IMP imp; };
static struct color_orig_ent g_color_orig[COLOR_SWZ_SELS][COLOR_SWZ_MAX];

static IMP color_orig_lookup(unsigned sidx, Class cls) {
   for (Class c = cls; c; c = class_getSuperclass(c)) {
      for (unsigned i = 0;
           i < COLOR_SWZ_MAX && g_color_orig[sidx][i].cls; ++i) {
         if (g_color_orig[sidx][i].cls == c) {
            return g_color_orig[sidx][i].imp;
         }
      }
   }
   return NULL;
}

/* Re-dispatch depth guard: colorUsingColorSpace: normally returns self once
 * the receiver is already in the target space, but never trust that for
 * termination — past depth 2 call the original directly. */
static __thread int g_color_depth;

/* Returns 1 if any out-pointer is a legacy (<4GB) slot needing float width. */
static int color_outs_legacy(double *const *outs, unsigned n) {
   for (unsigned i = 0; i < n; ++i) {
      if (outs[i] && (uintptr_t)outs[i] < 0x100000000UL) { return 1; }
   }
   return 0;
}
static void color_narrow_outs(double *const *outs, const double *buf,
                              unsigned n) {
   for (unsigned i = 0; i < n; ++i) {
      if (outs[i]) { *(float *)outs[i] = (float)buf[i]; }
   }
}

/* Common driver for the 4-out-pointer getters (RGBA / HSBA). */
static void color_get4_common(unsigned sidx, const char *space_sel, id self,
                              SEL _cmd, double *o0, double *o1, double *o2,
                              double *o3) {
   id conv = compat_color_convert(self, space_sel);
   if (conv != self && g_color_depth < 2) {
      ++g_color_depth;
      ((color_get4_t)objc_msgSend)(conv, _cmd, o0, o1, o2, o3);
      --g_color_depth;
      return;
   }
   id tgt = (conv != self) ? conv : self;
   IMP orig = color_orig_lookup(sidx, object_getClass(tgt));
   double *outs[4] = { o0, o1, o2, o3 };
   if (!orig) {                      /* no original anywhere: benign zeros */
      double zeros[4] = { 0, 0, 0, 1 };
      color_narrow_outs(outs, zeros, 4);
      return;
   }
   if (!color_outs_legacy(outs, 4)) {
      ((color_get4_t)orig)(tgt, _cmd, o0, o1, o2, o3);
      return;
   }
   double buf[4] = { 0, 0, 0, 1 };
   ((color_get4_t)orig)(tgt, _cmd, &buf[0], &buf[1], &buf[2], &buf[3]);
   color_narrow_outs(outs, buf, 4);
}

/* Extract RGBA straight from the color's CGColor. This never raises, unlike
 * -getRed:green:blue:alpha:, which modern AppKit REFUSES for an extended-sRGB /
 * HDR system color even when its components are perfectly in-gamut — the exact
 * s14 crash: an accent color in "sRGB IEC61966-2.1 (extended)" (components
 * 0 0.478431 1 1) thrown deep inside a native NSView Auto Layout constraint
 * pass, where converting to plain sRGBColorSpace first still left the original
 * raising getter to run. Trusted only for an RGB-model CGColor with >=3
 * components; pattern / catalog / gray colors (NULL or non-RGB CGColor) return
 * 0 so the caller falls back to the convert+orig path. CGColor's RGB components
 * are exactly what getRed: would report for any RGB-family color. */
static int color_rgba_via_cgcolor(id color, double out[4]) {
   if (!color) { return 0; }
   CGColorRef cg = ((CGColorRef (*)(id, SEL))objc_msgSend)(
      color, sel_registerName("CGColor"));
   if (!cg) { return 0; }
   CGColorSpaceRef cs = CGColorGetColorSpace(cg);
   if (!cs || CGColorSpaceGetModel(cs) != kCGColorSpaceModelRGB) { return 0; }
   size_t nc = CGColorGetNumberOfComponents(cg);
   const CGFloat *comp = CGColorGetComponents(cg);
   if (!comp || nc < 3) { return 0; }
   out[0] = comp[0]; out[1] = comp[1]; out[2] = comp[2];
   out[3] = (nc >= 4) ? comp[3] : 1.0;
   return 1;
}

static void compat_getRGBA(id self, SEL _cmd, double *r, double *g,
                           double *b, double *a) {
   double rgba[4];
   if (color_rgba_via_cgcolor(self, rgba)) {
      double *outs[4] = { r, g, b, a };
      if (color_outs_legacy(outs, 4)) {
         color_narrow_outs(outs, rgba, 4);
      } else {
         if (r) { *r = rgba[0]; } if (g) { *g = rgba[1]; }
         if (b) { *b = rgba[2]; } if (a) { *a = rgba[3]; }
      }
      return;
   }
   color_get4_common(0, "sRGBColorSpace", self, _cmd, r, g, b, a);
}

static void compat_getHSBA(id self, SEL _cmd, double *h, double *s,
                           double *b, double *a) {
   color_get4_common(1, "sRGBColorSpace", self, _cmd, h, s, b, a);
}

static void compat_getWA(id self, SEL _cmd, double *w, double *a) {
   id conv = compat_color_convert(self, "genericGamma22GrayColorSpace");
   if (conv != self && g_color_depth < 2) {
      ++g_color_depth;
      ((color_get2_t)objc_msgSend)(conv, _cmd, w, a);
      --g_color_depth;
      return;
   }
   id tgt = (conv != self) ? conv : self;
   IMP orig = color_orig_lookup(2, object_getClass(tgt));
   double *outs[2] = { w, a };
   if (!orig) {
      double zeros[2] = { 0, 1 };
      color_narrow_outs(outs, zeros, 2);
      return;
   }
   if (!color_outs_legacy(outs, 2)) {
      ((color_get2_t)orig)(tgt, _cmd, w, a);
      return;
   }
   double buf[2] = { 0, 1 };
   ((color_get2_t)orig)(tgt, _cmd, &buf[0], &buf[1]);
   color_narrow_outs(outs, buf, 2);
}

static void compat_getCMYKA(id self, SEL _cmd, double *c, double *m,
                            double *y, double *k, double *a) {
   id conv = compat_color_convert(self, "genericCMYKColorSpace");
   if (conv != self && g_color_depth < 2) {
      ++g_color_depth;
      ((color_get5_t)objc_msgSend)(conv, _cmd, c, m, y, k, a);
      --g_color_depth;
      return;
   }
   id tgt = (conv != self) ? conv : self;
   IMP orig = color_orig_lookup(3, object_getClass(tgt));
   double *outs[5] = { c, m, y, k, a };
   if (!orig) {
      double zeros[5] = { 0, 0, 0, 0, 1 };
      color_narrow_outs(outs, zeros, 5);
      return;
   }
   if (!color_outs_legacy(outs, 5)) {
      ((color_get5_t)orig)(tgt, _cmd, c, m, y, k, a);
      return;
   }
   double buf[5] = { 0, 0, 0, 0, 1 };
   ((color_get5_t)orig)(tgt, _cmd, &buf[0], &buf[1], &buf[2], &buf[3],
                        &buf[4]);
   color_narrow_outs(outs, buf, 5);
}

/* ProKit also swizzles the SINGLE-component NSColor getters (-redComponent,
 * -greenComponent, -blueComponent, -whiteComponent, -hueComponent,
 * -saturationComponent, -brightnessComponent) with its own *ComponentImp
 * functions, each of which raises the SAME color-space exception for an
 * extended/HDR system color ("whiteComponent is not implemented for Generic
 * Gray Gamma 2.2 Profile (extended) colorspace ..."). Modern AppKit calls these
 * while drawing text (NSHighContrastForegroundColorModifier asks a dynamic
 * color for its whiteComponent), so the s14 getRGBAImp patch alone isn't enough.
 * Compute each component straight from the CGColor — never raises. which:
 * 0=red 1=green 2=blue 3=white(luminance) 4=hue 5=saturation 6=brightness. */
static double color_component_via_cgcolor(id self, int which) {
   double r = 0, g = 0, b = 0;
   double rgba[4];
   int have = color_rgba_via_cgcolor(self, rgba);   /* RGB-model CGColor */
   if (have) {
      r = rgba[0]; g = rgba[1]; b = rgba[2];
   } else if (self) {                               /* gray / monochrome */
      CGColorRef cg = ((CGColorRef (*)(id, SEL))objc_msgSend)(
         self, sel_registerName("CGColor"));
      if (cg) {
         size_t nc = CGColorGetNumberOfComponents(cg);
         const CGFloat *comp = CGColorGetComponents(cg);
         if (comp && nc >= 1) { r = g = b = comp[0]; have = 1; }
      }
   }
   if (!have) { return 0.0; }
   switch (which) {
   case 0: return r;
   case 1: return g;
   case 2: return b;
   case 3: return 0.299 * r + 0.587 * g + 0.114 * b;   /* luminance */
   default: break;
   }
   double mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
   double mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
   double d = mx - mn, h = 0;
   if (d > 0) {
      if (mx == r)      { h = (g - b) / d + (g < b ? 6 : 0); }
      else if (mx == g) { h = (b - r) / d + 2; }
      else              { h = (r - g) / d + 4; }
      h /= 6;
   }
   switch (which) {
   case 4: return h;                          /* hue */
   case 5: return mx <= 0 ? 0 : d / mx;        /* saturation */
   case 6: return mx;                          /* brightness */
   default: return 0.0;
   }
}
static double compat_redComponent(id s, SEL c)        { (void)c; return color_component_via_cgcolor(s, 0); }
static double compat_greenComponent(id s, SEL c)      { (void)c; return color_component_via_cgcolor(s, 1); }
static double compat_blueComponent(id s, SEL c)       { (void)c; return color_component_via_cgcolor(s, 2); }
static double compat_whiteComponent(id s, SEL c)      { (void)c; return color_component_via_cgcolor(s, 3); }
static double compat_hueComponent(id s, SEL c)        { (void)c; return color_component_via_cgcolor(s, 4); }
static double compat_saturationComponent(id s, SEL c) { (void)c; return color_component_via_cgcolor(s, 5); }
static double compat_brightnessComponent(id s, SEL c) { (void)c; return color_component_via_cgcolor(s, 6); }

static const struct { const char *sel; IMP imp; } g_color_swz[COLOR_SWZ_SELS] = {
   { "getRed:green:blue:alpha:",            (IMP)compat_getRGBA  },
   { "getHue:saturation:brightness:alpha:", (IMP)compat_getHSBA  },
   { "getWhite:alpha:",                     (IMP)compat_getWA    },
   { "getCyan:magenta:yellow:black:alpha:", (IMP)compat_getCMYKA },
};
static int g_color_installed;

/* Locate symbol `sym` (mangled, i.e. with the leading '_') in the FIRST loaded
 * image whose path contains `image_substr`, returning its runtime address or
 * NULL. Walks LC_SYMTAB including LOCAL symbols — getRGBAImp is a non-exported
 * ProKit function, so dlsym can't see it. */
static void *find_image_symbol(const char *image_substr, const char *sym) {
   uint32_t nimg = _dyld_image_count();
   for (uint32_t i = 0; i < nimg; ++i) {
      const char *path = _dyld_get_image_name(i);
      if (!path || !strstr(path, image_substr)) { continue; }
      const struct mach_header_64 *mh =
         (const struct mach_header_64 *)_dyld_get_image_header(i);
      if (!mh || mh->magic != MH_MAGIC_64) { return NULL; }
      intptr_t slide = _dyld_get_image_vmaddr_slide(i);
      const struct load_command *lc =
         (const struct load_command *)((const uint8_t *)mh + sizeof *mh);
      const struct symtab_command *st = NULL;
      uintptr_t linkedit_base = 0;
      for (uint32_t c = 0; c < mh->ncmds; ++c) {
         if (lc->cmd == LC_SYMTAB) {
            st = (const struct symtab_command *)lc;
         } else if (lc->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *sg =
               (const struct segment_command_64 *)lc;
            if (strcmp(sg->segname, "__LINKEDIT") == 0) {
               linkedit_base = (uintptr_t)(sg->vmaddr + slide - sg->fileoff);
            }
         }
         lc = (const struct load_command *)((const uint8_t *)lc + lc->cmdsize);
      }
      if (!st || !linkedit_base) { return NULL; }
      const struct nlist_64 *syms =
         (const struct nlist_64 *)(linkedit_base + st->symoff);
      const char *strs = (const char *)(linkedit_base + st->stroff);
      for (uint32_t s = 0; s < st->nsyms; ++s) {
         uint32_t strx = syms[s].n_un.n_strx;
         if (!strx || !syms[s].n_value) { continue; }
         if (strcmp(strs + strx, sym) == 0) {
            return (void *)(uintptr_t)(syms[s].n_value + slide);
         }
      }
      return NULL;
   }
   return NULL;
}

/* Overwrite the first 12 bytes of native function `fn` with an absolute
 * tail-jump to `dest` (movabs rax,dest ; jmp rax). vm_protect with VM_PROT_COPY
 * forces a private COW copy so the code-signed page is writable. */
static int patch_tailjmp(void *fn, void *dest) {
   if (!fn || !dest) { return 0; }
   uint8_t code[12];
   code[0] = 0x48; code[1] = 0xB8;            /* movabs rax, imm64 */
   memcpy(code + 2, &dest, 8);
   code[10] = 0xFF; code[11] = 0xE0;          /* jmp rax */
   uintptr_t pg = (uintptr_t)fn & ~(uintptr_t)0xFFF;
   size_t len = (((uintptr_t)fn + sizeof code) - pg + 0xFFF) & ~(size_t)0xFFF;
   if (mach_vm_protect(mach_task_self(), (mach_vm_address_t)pg, len, FALSE,
                       VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY)
       != KERN_SUCCESS &&
       mprotect((void *)pg, len, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
      return 0;
   }
   memcpy(fn, code, sizeof code);
   mach_vm_protect(mach_task_self(), (mach_vm_address_t)pg, len, FALSE,
                   VM_PROT_READ | VM_PROT_EXECUTE);
   __builtin___clear_cache((char *)fn, (char *)fn + sizeof code);
   return 1;
}

/* ProKit (bundled 2010 pro-apps framework, runs NATIVE) replaces the NSColor
 * -getRed:green:blue:alpha: IMP with its own getRGBAImp during native window
 * theme init, whose __raiseColorSpaceException throws an UNCAUGHT
 * NSInvalidArgumentException for any post-2010 (extended-sRGB / HDR) colorspace
 * -> +[NSApplication _crashOnException:]. That steal happens MID -becomeActive:,
 * after our last reassert and with no reverse-bridge re-entry before the raising
 * getter runs (deep in a native NSView Auto Layout constraint pass), so neither
 * the per-class reassert nor an own-entry sweep can win the race. Instead
 * neutralize getRGBAImp at the SOURCE: patch the function (resolvable in ProKit's
 * symbol table as soon as ProKit loads, BEFORE it is ever installed as an IMP)
 * to tail-jump into our compat_getRGBA, which returns RGBA via CGColor and never
 * raises. Once-per-process; retries until ProKit is loaded. getRGBAImp is the
 * getRed: IMP, so it shares compat_getRGBA's (self,_cmd,r,g,b,a) signature. */
static void prokit_color_neutralize(void) {
   static int done;
   if (done) { return; }
   /* Process-wide guard: the patch edits shared ProKit code, so once ANY copy
    * has done it the others must not re-walk the symtab every reverse entry. */
   if (getenv("ABICONV_GETRGBA_PATCHED")) { done = 1; return; }
   /* getRGBAImp + each single-component getter ProKit overrides; all raise the
    * same color-space exception for extended/HDR colors. Patch every one whose
    * symbol resolves; require at least getRGBAImp before declaring success. */
   static const struct { const char *sym; void *dest; } patches[] = {
      { "_getRGBAImp",             (void *)compat_getRGBA            },
      { "_redComponentImp",        (void *)compat_redComponent       },
      { "_greenComponentImp",      (void *)compat_greenComponent     },
      { "_blueComponentImp",       (void *)compat_blueComponent      },
      { "_whiteComponentImp",      (void *)compat_whiteComponent     },
      { "_hueComponentImp",        (void *)compat_hueComponent       },
      { "_saturationComponentImp", (void *)compat_saturationComponent},
      { "_brightnessComponentImp", (void *)compat_brightnessComponent},
   };
   void *fn = find_image_symbol("/ProKit", patches[0].sym);
   if (!fn) {
      static int traced;
      if (!traced && BRIDGE_TRACE()) {
         int pk = 0;
         for (uint32_t i = 0, n = _dyld_image_count(); i < n; ++i) {
            const char *p = _dyld_get_image_name(i);
            if (p && strstr(p, "/ProKit")) { pk = 1; break; }
         }
         if (pk) {       /* ProKit IS loaded but symbol not found: trace once */
            traced = 1;
            fprintf(stderr, "[compat] getRGBAImp NOT found though ProKit loaded\n");
            fflush(stderr);
         }
      }
      return;                              /* ProKit not loaded yet: retry */
   }
   int rgba_ok = 0;
   for (unsigned i = 0; i < sizeof patches / sizeof patches[0]; ++i) {
      void *p = (i == 0) ? fn : find_image_symbol("/ProKit", patches[i].sym);
      int ok = p ? patch_tailjmp(p, patches[i].dest) : 0;
      if (i == 0) { rgba_ok = ok; }
      if (BRIDGE_TRACE()) {
         fprintf(stderr, "[compat] %s @%p patch %s -> compat\n",
                 patches[i].sym, p, ok ? "OK" : (p ? "FAILED" : "absent"));
         fflush(stderr);
      }
   }
   if (rgba_ok) { setenv("ABICONV_GETRGBA_PATCHED", "1", 1); done = 1; }
}

/* ProKit (bundled 2010 pro-apps framework, runs NATIVE) installs its OWN
 * NSColor getter replacement (getRGBAImp) during theme init — AFTER our
 * install — whose __raiseColorSpaceException fires for every post-2010
 * colorspace, killing modern AppKit/SwiftUI menu rendering on the extended-
 * sRGB accent color. Re-take any slot somebody replaced; the first-captured
 * AppKit originals stay our fallback. Called from the reverse-bridge prep
 * (every UI event passes through the legacy sendEvent: override, so a
 * re-swizzle never survives to the next draw). */
static void color_sweep(void);

static void appkit_color_compat_reassert(void) {
   /* BEFORE the g_color_installed gate: that flag is per-copy (set only in the
    * copy that ran install), but the reverse bridge — and thus this reassert —
    * runs in whatever copy handles the call, and the getRGBAImp patch is a
    * process-global code edit any copy can perform. */
   prokit_color_neutralize();   /* patch getRGBAImp once ProKit has loaded */
   if (!g_color_installed) { return; }
   for (unsigned si = 0; si < COLOR_SWZ_SELS; ++si) {
      SEL s = sel_registerName(g_color_swz[si].sel);
      for (unsigned i = 0;
           i < COLOR_SWZ_MAX && g_color_orig[si][i].cls; ++i) {
         Method m = class_getInstanceMethod(g_color_orig[si][i].cls, s);
         if (m && method_getImplementation(m) != g_color_swz[si].imp) {
            method_setImplementation(m, g_color_swz[si].imp);
         }
      }
   }
   /* New images can register NSColor subclasses that inherit a stealable slot
    * (ProKit). AppKit also realizes some concrete color classes lazily — the
    * extended-sRGB/HDR NSColorSpaceColor for a system accent color may not
    * exist until first painted, with NO new image load (s14, raised mid Auto
    * Layout). So re-sweep when EITHER the image count OR the total registered
    * class count moves. objc_getClassList(NULL,0) is a cheap count (no copy);
    * the heavy sweep (objc_copyClassList) only runs on a real change, and the
    * count stabilizes after startup so steady-state cost is just the count. */
   static uint32_t s_last_imgcount;
   static int      s_last_clscount;
   uint32_t ic = _dyld_image_count();
   int      cc = objc_getClassList(NULL, 0);
   if (ic != s_last_imgcount || cc != s_last_clscount) {
      s_last_imgcount = ic;
      s_last_clscount = cc;
      color_sweep();
   }
}

/* One pass over every NSColor-family class, idempotent so it can re-run when
 * new images load (ProKit registers its own color classes long after our
 * install):
 *   - a class that DECLARES a getter gets swizzled (original captured once);
 *   - a class that INHERITS it gets its OWN entry via class_addMethod. This
 *     is the ProKit-race fix: ProKit steals the BASE NSColor Method during
 *     native theme init, mid-event, where no reverse-bridge entry (and thus
 *     no reassert) can run before the next draw — but a stolen base slot is
 *     unreachable when every subclass resolves to its own entry. */
static void color_sweep(void) {
   Class base = objc_getClass("NSColor");
   if (!base) { return; }                 /* AppKit not loaded yet: retry */

   unsigned n = 0;
   Class *all = objc_copyClassList(&n);
   unsigned swizzled = 0, added = 0;
   for (unsigned ci = 0; ci < n + 1; ++ci) {
      Class c = (ci == n) ? base : all[ci];   /* subclasses + NSColor itself */
      if (c != base) {
         Class sup = c;
         int is_color = 0;
         while ((sup = class_getSuperclass(sup)) != NULL) {
            if (sup == base) { is_color = 1; break; }
         }
         if (!is_color) { continue; }
      }
      for (unsigned si = 0; si < COLOR_SWZ_SELS; ++si) {
         SEL s = sel_registerName(g_color_swz[si].sel);
         Method m = class_getInstanceMethod(c, s);
         if (!m) { continue; }
         /* same method_t up the chain == inherited, not declared here */
         Method sm = (c == base) ? NULL
                     : class_getInstanceMethod(class_getSuperclass(c), s);
         if (sm == m) {
            if (class_addMethod(c, s, g_color_swz[si].imp,
                                method_getTypeEncoding(m))) { ++added; }
            continue;
         }
         IMP cur = method_getImplementation(m);
         if (cur == g_color_swz[si].imp) { continue; }      /* already ours */
         unsigned slot = 0;
         int known = 0;
         while (slot < COLOR_SWZ_MAX && g_color_orig[si][slot].cls) {
            if (g_color_orig[si][slot].cls == c) { known = 1; break; }
            ++slot;
         }
         if (!known) {
            if (slot >= COLOR_SWZ_MAX) { continue; }
            g_color_orig[si][slot].cls = c;
            g_color_orig[si][slot].imp = cur;
         }
         method_setImplementation(m, g_color_swz[si].imp);
         ++swizzled;
      }
   }
   free(all);
   if ((swizzled || added) && BRIDGE_TRACE()) {
      fprintf(stderr, "[compat] NSColor getters: %u swizzled, %u own-entry "
              "added\n", swizzled, added);
      fflush(stderr);
   }
}

static void appkit_color_compat_install(void) {
   /* Once per PROCESS, not per libabiconv copy: a second copy would capture
    * the first copy's compat IMP as "original" and chain through it (correct
    * but wasteful). Same env-flag pattern as OBJC_CTRL_ENV. */
   if (getenv("ABICONV_NSCOLOR_COMPAT")) { return; }
   if (!objc_getClass("NSColor")) { return; }   /* AppKit not loaded: retry */
   setenv("ABICONV_NSCOLOR_COMPAT", "1", 1);
   color_sweep();
   g_color_installed = 1;
}

static const struct {
   const char *cls;
   const char *sel;
   IMP         imp;
   const char *types;          /* x86_64 encoding */
} g_appkit_compat[] = {
   { "NSDocumentController", "makeUntitledDocumentOfType:",
     (IMP)compat_makeUntitledDocumentOfType, "@24@0:8@16" },
   { "NSDocumentController", "openUntitledDocumentOfType:display:",
     (IMP)compat_openUntitledDocumentOfType_display, "@28@0:8@16c24" },
};

extern void legacy_locale_compat_install(void);   /* maptable_shim.m */

/* ---- legacy NSOpenGLView 1x-surface compat --------------------------------
 * Modern layer-backed AppKit gives every NSOpenGLView a Retina-scaled (2x)
 * drawable via _NSOpenGLViewBackingLayer, but LEGACY (translated 10.6-era)
 * subclasses size their glViewport/glOrtho in POINTS — the 10.7+ opt-in
 * `wantsBestResolutionOpenGLSurface` didn't exist for them, whose default
 * contract was a points-sized (1x) drawable. With a 2x drawable their GL
 * output lands in the bottom-left quarter of the surface (Quinn splash/board).
 * Restore the 10.6 contract for exactly those instances: any view whose class
 * chain below NSOpenGLView contains a reverse-registered legacy class gets
 * (a) wantsBestResolutionOpenGLSurface == NO and (b) its GL backing layer's
 * contentsScale clamped to 1.0. Native GL views are untouched. */
static struct rcls_ent *rcls_lookup(Class c);          /* fwd (defined below) */
static int glview_chain_is_legacy(Class c) {
   Class oglv = objc_getClass("NSOpenGLView");
   if (!oglv) { return 0; }
   int below_oglv = 0;
   for (Class k = c; k; k = class_getSuperclass(k)) {
      if (k == oglv) { below_oglv = 1; break; }
   }
   if (!below_oglv) { return 0; }
   for (Class k = c; k && k != oglv; k = class_getSuperclass(k)) {
      if (rcls_lookup(k)) { return 1; }
   }
   return 0;
}
static IMP g_oglv_wbros;                 /* original wantsBestResolution... */
static signed char oglv_wbros(id self, SEL _cmd) {
   if (glview_chain_is_legacy(object_getClass(self))) { return 0; }
   return g_oglv_wbros ? ((signed char(*)(id,SEL))g_oglv_wbros)(self, _cmd) : 0;
}
static IMP g_ogll_setScale;              /* super/prev setContentsScale: */
static void ogll_setScale(id self, SEL _cmd, double s) {
   id dlg = ((id(*)(id,SEL))objc_msgSend)(self, sel_registerName("delegate"));
   if (dlg && glview_chain_is_legacy(object_getClass(dlg))) { s = 1.0; }
   ((void(*)(id,SEL,double))g_ogll_setScale)(self, _cmd, s);
}
static void legacy_glview_1x_install(void) {
   static int done_prop, done_layer;
   if (!done_prop) {
      Class oglv = objc_getClass("NSOpenGLView");
      if (oglv) {
         Method m = class_getInstanceMethod(
            oglv, sel_registerName("wantsBestResolutionOpenGLSurface"));
         if (m) {
            g_oglv_wbros = method_getImplementation(m);
            method_setImplementation(m, (IMP)oglv_wbros);
         }
         done_prop = 1;    /* class present: installed (or no such method) */
      }
   }
   if (!done_layer) {
      Class gll = objc_getClass("_NSOpenGLViewBackingLayer");
      if (gll) {
         SEL s = sel_registerName("setContentsScale:");
         Method m = class_getInstanceMethod(gll, s);   /* may be CALayer's */
         if (m) {
            g_ogll_setScale = method_getImplementation(m);
            /* add an OVERRIDE on the private subclass (patching the found
             * Method directly could hit CALayer and affect every layer) */
            if (!class_addMethod(gll, s, (IMP)ogll_setScale,
                                 method_getTypeEncoding(m))) {
               /* subclass already had its own: patch that one */
               method_setImplementation(m, (IMP)ogll_setScale);
            }
            done_layer = 1;
         }
      }
   }
}

/* ---- legacy NSView-snapshot compat (initWithFocusedViewRect:) --------------
 * The pre-10.6 view-snapshot idiom — render a view into a (possibly never
 * shown) window, lockFocus, then READ BACK the pixels:
 *
 *    [offscreenWindow.contentView addSubview:view];
 *    [view display];
 *    [view lockFocus];
 *    rep = [[NSBitmapImageRep alloc] initWithFocusedViewRect:rect];
 *    [view unlockFocus];
 *
 * is DEAD on modern AppKit (proven natively, macOS 15): for a never-ordered-in
 * window `[view display]` doesn't even invoke drawRect:, and the deprecated
 * `initWithFocusedViewRect:` returns nil (no CPU-readable backing store).
 * Every legacy consumer of the captured image then collapses downstream —
 * `[NSImage TIFFRepresentation]` logs "CGImageDestinationFinalize failed for
 * output type 'public.tiff'", GL texture uploads get NULL bitmapData, view-
 * transition animations composite EMPTY content (Quinn: bitmapFromView /
 * imageFromView feed GLImage textures for the board flip + AnimatedContainerView
 * menu switches + QuinnHighscoreAnimationView — all rendered blank).
 *
 * Restore the 10.6 contract at its choke point: swizzle
 * -[NSBitmapImageRep initWithFocusedViewRect:] to synthesize the capture with
 * the MODERN cacheDisplayInRect:toBitmapImageRep: (which re-renders the view
 * subtree into the rep regardless of window visibility — verified to call
 * drawRect: and produce correct pixels for never-shown windows). [NSView
 * focusView] still tracks lockFocus on modern AppKit, so the focused view is
 * recoverable without touching lockFocus itself. The rep is hand-built 1x
 * (pixelsWide == points) to match what 10.6 returned on pre-Retina hardware —
 * legacy consumers feed rep.bitmapData/bytesPerRow straight into
 * glTexImage2D and size quads in points (companion to legacy_glview_1x).
 * A per-thread depth guard falls back to the original IMP if a capture ever
 * re-enters (cacheDisplay runs drawRect:, and a drawRect: that itself calls
 * initWithFocusedViewRect: on the view being drawn would recurse). */
static IMP g_snap_orig_initfvr;
static __thread int g_snap_depth;
static id snap_init_with_focused_view_rect(id self, SEL _cmd, CGRect rect) {
   Class nsview_cls = objc_getClass("NSView");
   id fv = nsview_cls
      ? ((id(*)(id, SEL))objc_msgSend)((id)nsview_cls,
                                       sel_registerName("focusView"))
      : nil;
   long pw = (long)(rect.size.width + 0.5);
   long ph = (long)(rect.size.height + 0.5);
   if (!fv || g_snap_depth > 0 || pw <= 0 || ph <= 0) {
      return g_snap_orig_initfvr
         ? ((id(*)(id, SEL, CGRect))g_snap_orig_initfvr)(self, _cmd, rect)
         : nil;
   }
   /* hand-built 1x rep: pixels == points (the 10.6 return contract) */
   Class rep_cls = objc_getClass("NSBitmapImageRep");
   id rep = ((id(*)(id, SEL))objc_msgSend)((id)rep_cls,
                                           sel_registerName("alloc"));
   rep = ((id(*)(id, SEL, void *, long, long, long, long, signed char,
                 signed char, id, long, long))objc_msgSend)(
      rep,
      sel_registerName("initWithBitmapDataPlanes:pixelsWide:pixelsHigh:"
                       "bitsPerSample:samplesPerPixel:hasAlpha:isPlanar:"
                       "colorSpaceName:bytesPerRow:bitsPerPixel:"),
      NULL, pw, ph, 8L, 4L, (signed char)1, (signed char)0,
      (id)CFSTR("NSCalibratedRGBColorSpace"), 0L, 0L);
   if (!rep) {
      return g_snap_orig_initfvr
         ? ((id(*)(id, SEL, CGRect))g_snap_orig_initfvr)(self, _cmd, rect)
         : nil;
   }
   ((void(*)(id, SEL, CGSize))objc_msgSend)(rep, sel_registerName("setSize:"),
                                            rect.size);
   ++g_snap_depth;
   ((void(*)(id, SEL, CGRect, id))objc_msgSend)(
      fv, sel_registerName("cacheDisplayInRect:toBitmapImageRep:"), rect, rep);
   --g_snap_depth;
   /* init contract: consume the alloc'd self, hand back the +1 capture rep */
   ((void(*)(id, SEL))objc_msgSend)(self, sel_registerName("release"));
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[compat] initWithFocusedViewRect: -> cacheDisplay "
              "capture of %s %ldx%ld\n", object_getClassName(fv), pw, ph);
      fflush(stderr);
   }
   return rep;
}
static void legacy_snapshot_compat_install(void) {
   static int done = 0;
   if (done) { return; }
   /* once per PROCESS (env flag), not per libabiconv copy — a second copy
    * would capture the first copy's compat IMP as "original" and chain. */
   if (getenv("ABICONV_SNAPSHOT_COMPAT")) { done = 1; return; }
   Class rep_cls = objc_getClass("NSBitmapImageRep");
   if (!rep_cls) { return; }                  /* AppKit not loaded yet: retry */
   Method m = class_getInstanceMethod(rep_cls,
                                      sel_registerName("initWithFocusedViewRect:"));
   if (!m) { return; }
   setenv("ABICONV_SNAPSHOT_COMPAT", "1", 1);
   g_snap_orig_initfvr = method_getImplementation(m);
   method_setImplementation(m, (IMP)snap_init_with_focused_view_rect);
   done = 1;
}

/* ---- legacy -[NSImage lockFocus] 1x-backing compat -------------------------
 * Modern AppKit backs an -[NSImage lockFocus] drawing context at the current
 * display's backingScaleFactor: on a Retina (2x) main screen a 10x10-POINT
 * image gets a 20x20-PIXEL rep. LEGACY 10.6 lockFocus had no backing-scale
 * concept and ALWAYS produced a 1x context (pixels == points). Every legacy
 * consumer of the lockFocus'd content assumes points == pixels: it reads back
 * the rep and indexes bitmapData/bytesPerRow at POINT coordinates, uploads it
 * to glTexImage2D sizing the textured quad in points, or measures pixelsWide to
 * lay out. A 2x rep gives them the wrong stride/extent -> garbled blits, GL
 * texcoord mismatch, off-by-2x layout. This is the OFFSCREEN sibling of
 * legacy_glview_1x (on-screen NSOpenGLView) and the initWithFocusedViewRect:
 * snapshot compat (focused-view readback) — the plain lockFocus path was the
 * one uncovered leg, and it shares their documented 1x contract.
 *
 * Restore the 10.6 contract for LEGACY-originated lockFocus only: swizzle
 * -[NSImage lockFocus]/lockFocusFlipped:/unlockFocus so a lockFocus issued FROM
 * translated i386 code draws into an explicit 1x NSBitmapImageRep-backed
 * NSGraphicsContext instead of the device-scaled backing. The existing image
 * content is pre-painted into the 1x rep first (legacy lockFocus draws ONTO
 * the current content, it does not clear), and on unlockFocus the 1x rep
 * becomes the image's sole representation so downstream readers see 1x.
 *
 * TRIGGER = STRUCTURE, never app/class name: the forward bridge
 * (objc_bridge_prep) sets g_lockfocus_legacy for exactly the lockFocus send it
 * is dispatching from i386 (receiver is an NSImage), consumed+cleared by the
 * swizzled IMP that runs synchronously as this send's native dispatch. A
 * lockFocus that native ProKit/AppKit issues for its OWN Retina rendering (not
 * from a legacy send) leaves the flag 0 and runs the original device-scaled IMP
 * untouched. Nesting is handled with a small per-thread stack. */
static IMP g_img_lockFocus, g_img_lockFocusFlipped, g_img_unlockFocus;
static __thread int g_lockfocus_legacy;          /* set by objc_bridge_prep */
/* test-only setter: a __thread var can't be poked via dlsym (dlsym returns the
 * TLV descriptor, not the per-thread slot), so the native guard drives the flag
 * through this in-library accessor which performs the real TLS write. */
void _86x64_test_set_lockfocus_legacy(int v) { g_lockfocus_legacy = v; }
#define LF1X_MAX 16
static __thread id  g_lf1x_img[LF1X_MAX];
static __thread id  g_lf1x_rep[LF1X_MAX];
static __thread int g_lf1x_depth;

static int lf1x_begin(id self) {
   CGSize pts = ((CGSize(*)(id, SEL))objc_msgSend)(self, sel_registerName("size"));
   if (pts.width <= 0 || pts.height <= 0 || g_lf1x_depth >= LF1X_MAX) { return 0; }
   long W = (long)(pts.width + 0.5), H = (long)(pts.height + 0.5);
   Class rep_cls = objc_getClass("NSBitmapImageRep");
   Class gc_cls  = objc_getClass("NSGraphicsContext");
   if (!rep_cls || !gc_cls) { return 0; }
   id rep = ((id(*)(id, SEL))objc_msgSend)((id)rep_cls, sel_registerName("alloc"));
   rep = ((id(*)(id, SEL, void *, long, long, long, long, signed char,
                 signed char, id, long, long))objc_msgSend)(
      rep,
      sel_registerName("initWithBitmapDataPlanes:pixelsWide:pixelsHigh:"
                       "bitsPerSample:samplesPerPixel:hasAlpha:isPlanar:"
                       "colorSpaceName:bytesPerRow:bitsPerPixel:"),
      NULL, W, H, 8L, 4L, (signed char)1, (signed char)0,
      (id)CFSTR("NSCalibratedRGBColorSpace"), 0L, 0L);
   if (!rep) { return 0; }
   ((void(*)(id, SEL, CGSize))objc_msgSend)(rep, sel_registerName("setSize:"), pts);
   id gc = ((id(*)(id, SEL, id))objc_msgSend)(
      (id)gc_cls, sel_registerName("graphicsContextWithBitmapImageRep:"), rep);
   if (!gc) { return 0; }
   ((void(*)(id, SEL))objc_msgSend)((id)gc_cls, sel_registerName("saveGraphicsState"));
   ((void(*)(id, SEL, id))objc_msgSend)(
      (id)gc_cls, sel_registerName("setCurrentContext:"), gc);
   /* pre-paint existing content: legacy lockFocus draws ONTO current pixels */
   long nreps = ((long(*)(id, SEL))objc_msgSend)(
      ((id(*)(id, SEL))objc_msgSend)(self, sel_registerName("representations")),
      sel_registerName("count"));
   if (nreps > 0) {
      ((void(*)(id, SEL, CGRect, CGRect, unsigned long, double))objc_msgSend)(
         self, sel_registerName("drawInRect:fromRect:operation:fraction:"),
         CGRectMake(0, 0, pts.width, pts.height), CGRectZero,
         1UL /* NSCompositeCopy */, 1.0);
   }
   g_lf1x_img[g_lf1x_depth] = self;
   g_lf1x_rep[g_lf1x_depth] = rep;
   g_lf1x_depth++;
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[compat] NSImage lockFocus -> 1x %ldx%ld (%s)\n", W, H,
              object_getClassName(self));
      fflush(stderr);
   }
   return 1;
}
static void img_lockFocus(id self, SEL _cmd) {
   if (g_lockfocus_legacy) { g_lockfocus_legacy = 0; if (lf1x_begin(self)) { return; } }
   ((void(*)(id, SEL))g_img_lockFocus)(self, _cmd);
}
static signed char img_lockFocusFlipped(id self, SEL _cmd, signed char flipped) {
   /* Legacy 1x path ignores the flip request's device scaling; the drawInRect:
    * pre-paint + subsequent draws use the context's own (unflipped-by-default)
    * coordinates, matching what a legacy 1x lockFocus produced. Only intercept
    * when we successfully begin a 1x context; else fall through. */
   if (g_lockfocus_legacy) {
      g_lockfocus_legacy = 0;
      if (lf1x_begin(self)) { return 1; }
   }
   return ((signed char(*)(id, SEL, signed char))g_img_lockFocusFlipped)(
      self, _cmd, flipped);
}
static void img_unlockFocus(id self, SEL _cmd) {
   if (g_lf1x_depth > 0 && g_lf1x_img[g_lf1x_depth - 1] == self) {
      g_lf1x_depth--;
      id rep = g_lf1x_rep[g_lf1x_depth];
      g_lf1x_rep[g_lf1x_depth] = nil;
      g_lf1x_img[g_lf1x_depth] = nil;
      Class gc_cls = objc_getClass("NSGraphicsContext");
      ((void(*)(id, SEL))objc_msgSend)((id)gc_cls,
                                       sel_registerName("restoreGraphicsState"));
      /* commit: our 1x rep becomes the image's sole representation */
      id reps = ((id(*)(id, SEL))objc_msgSend)(self,
                                               sel_registerName("representations"));
      id copy = ((id(*)(id, SEL))objc_msgSend)(reps, sel_registerName("copy"));
      long n = ((long(*)(id, SEL))objc_msgSend)(copy, sel_registerName("count"));
      for (long i = 0; i < n; ++i) {
         id r = ((id(*)(id, SEL, long))objc_msgSend)(
            copy, sel_registerName("objectAtIndex:"), i);
         ((void(*)(id, SEL, id))objc_msgSend)(
            self, sel_registerName("removeRepresentation:"), r);
      }
      ((void(*)(id, SEL))objc_msgSend)(copy, sel_registerName("release"));
      ((void(*)(id, SEL, id))objc_msgSend)(
         self, sel_registerName("addRepresentation:"), rep);
      return;
   }
   ((void(*)(id, SEL))g_img_unlockFocus)(self, _cmd);
}
static void legacy_lockfocus_1x_install(void) {
   static int done = 0;
   if (done) { return; }
   if (getenv("ABICONV_LOCKFOCUS1X_COMPAT")) { done = 1; return; }
   Class img = objc_getClass("NSImage");
   if (!img) { return; }                       /* AppKit not loaded yet: retry */
   Method mL = class_getInstanceMethod(img, sel_registerName("lockFocus"));
   Method mU = class_getInstanceMethod(img, sel_registerName("unlockFocus"));
   if (!mL || !mU) { return; }
   setenv("ABICONV_LOCKFOCUS1X_COMPAT", "1", 1);
   g_img_lockFocus = method_getImplementation(mL);
   method_setImplementation(mL, (IMP)img_lockFocus);
   g_img_unlockFocus = method_getImplementation(mU);
   method_setImplementation(mU, (IMP)img_unlockFocus);
   Method mLF = class_getInstanceMethod(img, sel_registerName("lockFocusFlipped:"));
   if (mLF) {
      g_img_lockFocusFlipped = method_getImplementation(mLF);
      method_setImplementation(mLF, (IMP)img_lockFocusFlipped);
   }
   done = 1;
}

/* ---- legacy -[NSView cacheDisplay] 1x-rep compat ---------------------------
 * The VIEW-side sibling of legacy_lockfocus_1x. The offscreen view-capture
 * idiom -[NSView bitmapImageRepForCachingDisplayInRect:] (vend a rep) +
 * cacheDisplayInRect:toBitmapImageRep: (render into it) vends a rep sized at the
 * display backingScaleFactor (2x on Retina: a 10-POINT view -> a 20-PIXEL rep,
 * and cacheDisplay fills the full 2x extent). LEGACY 10.6 vended a 1x rep
 * (pixels == points). A legacy consumer reads the rep's bitmapData/bytesPerRow
 * at POINT coordinates or uploads to glTexImage2D sizing the quad in points ->
 * a 2x rep gives it the wrong stride/extent (top-left-quarter blit, GL texcoord
 * mismatch). Same "legacy offscreen backing is 1x" contract as lockFocus /
 * initWithFocusedViewRect: / legacy_glview_1x.
 *
 * Restore it for LEGACY-originated captures only: swizzle
 * -[NSView bitmapImageRepForCachingDisplayInRect:] so a call issued FROM
 * translated i386 code returns a hand-built 1x NSBitmapImageRep (pixels ==
 * points); the app's own cacheDisplayInRect:toBitmapImageRep: then renders into
 * that 1x rep at 1x. Gate = the forward bridge sets g_cachedisplay_legacy for
 * exactly the bitmapImageRepForCachingDisplayInRect: send it dispatches from
 * i386 to an NSView, consumed+cleared by the swizzled IMP. Native AppKit/ProKit
 * captures (flag 0) get the original device-scaled rep untouched. */
static IMP g_view_bitmapRepForCaching;
static __thread int g_cachedisplay_legacy;       /* set by objc_bridge_prep */
void _86x64_test_set_cachedisplay_legacy(int v) { g_cachedisplay_legacy = v; }
static id view_bitmapRepForCaching(id self, SEL _cmd, CGRect rect) {
   if (!g_cachedisplay_legacy) {
      return ((id(*)(id, SEL, CGRect))g_view_bitmapRepForCaching)(self, _cmd, rect);
   }
   g_cachedisplay_legacy = 0;
   long W = (long)(rect.size.width + 0.5), H = (long)(rect.size.height + 0.5);
   if (W <= 0 || H <= 0) {
      return ((id(*)(id, SEL, CGRect))g_view_bitmapRepForCaching)(self, _cmd, rect);
   }
   Class rep_cls = objc_getClass("NSBitmapImageRep");
   if (!rep_cls) {
      return ((id(*)(id, SEL, CGRect))g_view_bitmapRepForCaching)(self, _cmd, rect);
   }
   id rep = ((id(*)(id, SEL))objc_msgSend)((id)rep_cls, sel_registerName("alloc"));
   rep = ((id(*)(id, SEL, void *, long, long, long, long, signed char,
                 signed char, id, long, long))objc_msgSend)(
      rep,
      sel_registerName("initWithBitmapDataPlanes:pixelsWide:pixelsHigh:"
                       "bitsPerSample:samplesPerPixel:hasAlpha:isPlanar:"
                       "colorSpaceName:bytesPerRow:bitsPerPixel:"),
      NULL, W, H, 8L, 4L, (signed char)1, (signed char)0,
      (id)CFSTR("NSCalibratedRGBColorSpace"), 0L, 0L);
   if (!rep) {
      return ((id(*)(id, SEL, CGRect))g_view_bitmapRepForCaching)(self, _cmd, rect);
   }
   ((void(*)(id, SEL, CGSize))objc_msgSend)(rep, sel_registerName("setSize:"),
                                            rect.size);
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[compat] NSView cacheDisplay rep -> 1x %ldx%ld (%s)\n",
              W, H, object_getClassName(self));
      fflush(stderr);
   }
   return rep;
}
static void legacy_cachedisplay_1x_install(void) {
   static int done = 0;
   if (done) { return; }
   if (getenv("ABICONV_CACHEDISPLAY1X_COMPAT")) { done = 1; return; }
   Class view = objc_getClass("NSView");
   if (!view) { return; }                      /* AppKit not loaded yet: retry */
   Method m = class_getInstanceMethod(
      view, sel_registerName("bitmapImageRepForCachingDisplayInRect:"));
   if (!m) { return; }
   setenv("ABICONV_CACHEDISPLAY1X_COMPAT", "1", 1);
   g_view_bitmapRepForCaching = method_getImplementation(m);
   method_setImplementation(m, (IMP)view_bitmapRepForCaching);
   done = 1;
}

/* ---- LEGACY immediate-displayRect compositor co-mark -------------------------
 * A legacy i386 app's animation framework drives per-frame redraw via an
 * IMMEDIATE synchronous -[NSView displayRect:] / displayRectIgnoringOpacity:
 * (Quinn's ATViewAnimation -> [board displayRect:(union of the piece's old+new
 * cell rects)] on each rotate/move frame). On 10.6 that drew straight to the
 * window backing store the WindowServer read. On modern macOS, once the view is
 * LAYER-BACKED (Quinn's board is, forced by the sibling NSOpenGLView), an
 * immediate displayRect: lands in the layer backing but the WindowServer does not
 * always recomposite that sub-rect -> stale old-piece pixels persist (the observed
 * ROTATE TRAIL) and the new piece paints only partially (lost squares); the next
 * frame's larger union eventually repaints over the mess.
 *
 * FIX: swizzle displayRect:/displayRectIgnoringOpacity: so a LEGACY-originated
 * call on a LAYER-BACKED view runs the ORIGINAL immediate draw FIRST (keeps
 * synchrony for any draw-then-readback caller) and THEN setNeedsDisplayInRect:
 * the same rect, scheduling AppKit's coalesced, compositor-facing display pass so
 * the WindowServer recomposites that sub-rect. STRUCTURAL gate: legacy-origin
 * (the __thread flag set in objc_bridge_prep for exactly this send) + layer!=nil.
 * Native AppKit displayRect: (flag 0) and non-layer-backed views are untouched.
 * Env ABICONV_QUINN_DISPLAYRECT_COMARK (default OFF pending the live pixel A/B;
 * flip default-on once the tester confirms the trail is gone). Trace under
 * ABICONV_QUINN_DISPLAYRECT_TRACE. */
static __thread int g_displayrect_legacy;         /* set by objc_bridge_prep */
void _86x64_test_set_displayrect_legacy(int v) { g_displayrect_legacy = v; }
static IMP g_view_displayRect;
static IMP g_view_displayRectIgnoringOpacity;
static int displayrect_comark_on(void) {
   static int v = -1;
   if (v < 0) { v = getenv("ABICONV_QUINN_DISPLAYRECT_COMARK") ? 1 : 0; }
   return v;
}
static int displayrect_trace_on(void) {
   static int v = -1;
   if (v < 0) { v = getenv("ABICONV_QUINN_DISPLAYRECT_TRACE") ? 1 : 0; }
   return v;
}
/* shared body: run the original IMP, then (legacy + layer-backed) co-mark. */
static void displayrect_comark_common(id self, SEL _cmd, CGRect rect, IMP orig) {
   int legacy = g_displayrect_legacy; g_displayrect_legacy = 0;
   ((void(*)(id, SEL, CGRect))orig)(self, _cmd, rect);   /* original immediate draw */
   if (!legacy) { return; }
   id lyr = ((id(*)(id, SEL))objc_msgSend)(self, sel_registerName("layer"));
   int layered = lyr != nil;
   if (displayrect_trace_on()) {
      fprintf(stderr, "[disprect] %s<%s> rect=[%.1f %.1f %.1f %.1f] layerBacked=%d comark=%d\n",
              sel_getName(_cmd), object_getClassName(self),
              rect.origin.x, rect.origin.y, rect.size.width, rect.size.height,
              layered, displayrect_comark_on() && layered);
      fflush(stderr);
   }
   if (displayrect_comark_on() && layered) {
      ((void(*)(id, SEL, CGRect))objc_msgSend)(
         self, sel_registerName("setNeedsDisplayInRect:"), rect);
   }
}
static void view_displayRect(id self, SEL _cmd, CGRect rect) {
   displayrect_comark_common(self, _cmd, rect, g_view_displayRect);
}
static void view_displayRectIgnoringOpacity(id self, SEL _cmd, CGRect rect) {
   displayrect_comark_common(self, _cmd, rect, g_view_displayRectIgnoringOpacity);
}
static void legacy_displayrect_comark_install(void) {
   static int done = 0;
   if (done) { return; }
   Class view = objc_getClass("NSView");
   if (!view) { return; }                      /* AppKit not loaded yet: retry */
   Method mR = class_getInstanceMethod(view, sel_registerName("displayRect:"));
   Method mI = class_getInstanceMethod(view,
                  sel_registerName("displayRectIgnoringOpacity:"));
   if (!mR || !mI) { return; }
   g_view_displayRect = method_getImplementation(mR);
   g_view_displayRectIgnoringOpacity = method_getImplementation(mI);
   method_setImplementation(mR, (IMP)view_displayRect);
   method_setImplementation(mI, (IMP)view_displayRectIgnoringOpacity);
   done = 1;
   if (getenv("ABICONV_QUINN_DISPLAYRECT_TRACE")) {
      fprintf(stderr, "[disprect] comark swizzle INSTALLED on NSView\n"); fflush(stderr);
   }
}

/* ---- legacy -[NSView gState] + NSCopyBits view-capture -----------------------
 * The pre-10.10 offscreen view-capture idiom copies a source view's pixels with
 * NSCopyBits([sourceView gState], srcRect, destPoint) into a lockFocus'd dest
 * (Quinn's -[NSView(QuinnExtensions) imageFromRect:], used to build the board
 * REFLECTION source image). On modern macOS -[NSView gState] returns 0 (the
 * window-server graphics-state IDs are gone) so NSCopyBits(0, ...) copies
 * NOTHING -> the reflected pieces are absent. We restore the contract in two
 * halves: (1) -[NSView gState] hands a LEGACY caller a non-zero TOKEN that maps
 * back to the view; (2) the NSCopyBits shim (nscopybits_shim.c) sees the token,
 * resolves the source view and renders its srcRect into the current focus
 * context at destPoint. A genuine (native) gState / non-token is passed straight
 * through. Universal: any legacy app using the gState+NSCopyBits view blit.
 *
 * Token space: [GSTATE_TOKEN_BASE, +GSTATE_TOKEN_CAP). Small ring keyed by the
 * view (a view's gState is stable for a capture); reused across captures. */
#define GSTATE_TOKEN_BASE 0x67530000L      /* "gS.." — clearly not a real gState */
#define GSTATE_TOKEN_CAP  256
static id  g_gstate_view[GSTATE_TOKEN_CAP];
static os_unfair_lock g_gstate_lock = OS_UNFAIR_LOCK_INIT;
static __thread int g_gstate_legacy;              /* set by objc_bridge_prep */
void _86x64_test_set_gstate_legacy(int v) { g_gstate_legacy = v; }
/* NSCopyBits shim (nscopybits_shim.c) resolves a token -> source view here. */
id _86x64_gstate_token_view(long tok) {
   long i = tok - GSTATE_TOKEN_BASE;
   if (i < 0 || i >= GSTATE_TOKEN_CAP) { return nil; }
   os_unfair_lock_lock(&g_gstate_lock);
   id v = g_gstate_view[i];
   os_unfair_lock_unlock(&g_gstate_lock);
   return v;
}
static long (*g_view_gState)(id, SEL);
static long view_gState(id self, SEL _cmd) {
   long real = g_view_gState ? g_view_gState(self, _cmd) : 0;
   int legacy = g_gstate_legacy; g_gstate_legacy = 0;
   if (!legacy || real != 0) { return real; }   /* native, or a usable gState */
   /* dead gState for a legacy caller: mint/reuse a token for this view */
   os_unfair_lock_lock(&g_gstate_lock);
   long slot = -1, freeslot = -1;
   for (long i = 0; i < GSTATE_TOKEN_CAP; ++i) {
      if (g_gstate_view[i] == self) { slot = i; break; }
      if (freeslot < 0 && g_gstate_view[i] == nil) { freeslot = i; }
   }
   if (slot < 0) {
      slot = (freeslot >= 0) ? freeslot
                             : (long)((uintptr_t)self >> 4) % GSTATE_TOKEN_CAP;
      g_gstate_view[slot] = self;
   }
   os_unfair_lock_unlock(&g_gstate_lock);
   return GSTATE_TOKEN_BASE + slot;
}
static void legacy_gstate_capture_install(void) {
   static int done = 0;
   if (done) { return; }
   Class view = objc_getClass("NSView");
   if (!view) { return; }                      /* AppKit not loaded yet: retry */
   Method m = class_getInstanceMethod(view, sel_registerName("gState"));
   if (!m) { return; }
   g_view_gState = (long(*)(id, SEL))method_getImplementation(m);
   method_setImplementation(m, (IMP)view_gState);
   done = 1;
}

/* ---- legacy private-frame-view window CHROME compat -----------------------
 * A pre-10.x Cocoa app that wanted a custom titlebar/border look drew it by
 * SWAPPING NSWindow's PRIVATE frame-view class: it subclassed the private
 * `NSFrameView` (looked up dynamically as NSClassFromString(@"NSFrameView"))
 * and overrode the frame view's chrome-draw entry points, wiring them through
 * NSWindow private hooks like `-borderViewClass` / a startup
 * `+hackBorderViewClass:` that method-swizzles the frame view's `-drawRect:`.
 * When the frame view then drew, it called back into the window's
 * `-drawWindowBorderInRect:` (paint the metal band) and `-drawWindowTitle`
 * (draw the title text) overrides.
 *
 * On modern macOS the private frame-view class is `NSThemeFrame`, NSFrameView
 * is GONE, and the titlebar is a SEPARATE layer-composited `NSTitlebarView`
 * subtree — so the app's NSClassFromString(@"NSFrameView") returns nil, the
 * chrome-draw swap no-ops, and the window's `-drawWindowBorderInRect:` /
 * `-drawWindowTitle` overrides are NEVER invoked. Result: the window falls back
 * to the stock NSThemeFrame chrome (a plain white/system titlebar) instead of
 * the app's own custom look. (Quinn's PolishedMetalWindow brushed-metal band;
 * the app even logs its own "ERROR: NSFrameView class does not exist!".)
 *
 * RESTORE the contract UNIVERSALLY, keyed on STRUCTURE not any class name:
 * a window whose class responds to the legacy private-frame chrome selector
 * `-drawWindowBorderInRect:` AND whose implementation of it is a LEGACY
 * (translated-i386, reverse-bridge) method is exactly an app that expected the
 * orphaned private-frame swap. For such a window we install a lightweight
 * native overlay view into its `NSTitlebarView` (above the stock background,
 * BELOW the traffic-light widgets + title so those stay visible = functionality
 * preserved) whose -drawRect: re-invokes the window's own
 * `-drawWindowBorderInRect:` + `-drawWindowTitle` in the full frame-view
 * coordinate space — so the app paints its OWN authentic chrome again. A native
 * (non-legacy) window never responds to that selector with a reverse-bridge IMP
 * and is never touched.
 *
 * The install is triggered by swizzling `-[NSThemeFrame drawRect:]` (fires on
 * every window display, gives us the frame view directly); the overlay itself
 * is a runtime-built NSView subclass. Env kill-switch ABICONV_WINCHROME_COMPAT
 * (set => skip). */

/* low-noise env-gated trace for the window-chrome compat (independent of the
 * OBJC_BRIDGE_TRACE firehose, which slows app startup to a crawl). */
static int wchrome_trace(void) {
   static int v = -1;
   if (v < 0) { v = getenv("ABICONV_WINCHROME_TRACE") ? 1 : 0; }
   return v;
}

/* forward decls used by the overlay IMP */
static Class g_chrome_overlay_cls;          /* our runtime NSView subclass     */
static SEL   g_sel_drawWindowBorderInRect;  /* cached                          */
static SEL   g_sel_drawWindowTitle;
static SEL   g_sel_window;
static SEL   g_sel_bounds;
/* -[NSView bounds]/-frame return a 32B CGRect (struct-return ABI on x86_64) —
 * must go through objc_msgSend_stret with a hidden retbuf pointer, NOT a plain
 * CGRect-returning cast of objc_msgSend. Canonical helper defined later. */
static CGRect abiconv_view_bounds(id v);
static CGRect abiconv_msg_rect(id v, SEL sel) {
   CGRect r = {{0, 0}, {0, 0}};
   if (!v) { return r; }
   ((void (*)(CGRect *, id, SEL))objc_msgSend_stret)(&r, v, sel);
   return r;
}

/* When set (native test guard only), the legacy-chrome gate accepts ANY window
 * that merely RESPONDS to -drawWindowBorderInRect: (a native guard can't forge a
 * reverse-bridge IMP). Zero in production => real gate is legacy-IMP only. */
static int g_chrome_test_accept_responds;
void _86x64_test_window_chrome_accept_responds(int v) { g_chrome_test_accept_responds = v; }

/* Is `win` a legacy window whose private-frame chrome swap was orphaned?
 * Structural: responds to -drawWindowBorderInRect: with a LEGACY (reverse-
 * bridge) IMP. Never triggers on a native window (which has no such method, and
 * if it did, its IMP would not be a reverse-bridge trampoline). */
static int window_has_legacy_chrome(id win) {
   if (!win) { return 0; }
   Class wc = object_getClass(win);
   Method m = class_getInstanceMethod(wc, g_sel_drawWindowBorderInRect);
   if (!m) { return 0; }
   if (method_is_legacy(m)) { return 1; }
   return g_chrome_test_accept_responds ? 1 : 0;
}

/* Does CLASS `c` (own method list only, not inherited) define ANY instance
 * method whose IMP is a LEGACY reverse-bridge trampoline? True exactly for a
 * class that was reverse-registered from a translated i386 image. */
static int class_has_own_legacy_imp(Class c) {
   if (!c) { return 0; }
   unsigned n = 0;
   Method *ml = class_copyMethodList(c, &n);
   int legacy = 0;
   for (unsigned i = 0; i < n && !legacy; ++i) {
      if (method_is_legacy(ml[i])) { legacy = 1; }
   }
   if (ml) { free(ml); }
   return legacy;
}

/* When set (native test guard only), the legacy-window gate accepts any window
 * whose class name is NOT a stock AppKit class (a native guard can't forge a
 * reverse-bridge IMP but subclasses NSWindow to stand in for a legacy one). */
static int g_win_test_accept_subclass;
void _86x64_test_window_accept_subclass(int v) { g_win_test_accept_subclass = v; }

/* Is `win` a LEGACY-ORIGIN window (translated i386 app)? Structural, keyed on NO
 * class name: TRUE if the window's class — or any class up to (not including)
 * NSWindow — carries a reverse-bridge (translated-i386) IMP, i.e. it is a
 * subclass materialized from the app's own translated code. A stock native
 * NSWindow / NSPanel has no such IMP anywhere in its chain and is never touched.
 * This is a SUPERSET of window_has_legacy_chrome (which only catches windows
 * that also override the private -drawWindowBorderInRect:). Used to gate the
 * generic title-on-show + toolbar-styling fixes, which apply to EVERY legacy
 * Cocoa window, not just the brushed-metal frame-swap archetype. */
/* strip the dynamic KVO subclass (isa-swizzled when the object is observed / has
 * a bound toolbar/delegate): NSKVONotifying_<Real> -> <Real>. Generic, no app
 * name. Returns the real class the app actually authored. */
static Class class_strip_kvo(Class c) {
   if (!c) { return c; }
   const char *cn = class_getName(c);
   if (cn && strncmp(cn, "NSKVONotifying_", 15) == 0) {
      Class sup = class_getSuperclass(c);
      if (sup) { return sup; }
   }
   return c;
}
static int window_is_legacy(id win) {
   if (!win) { return 0; }
   Class stop = objc_getClass("NSWindow");
   Class start = class_strip_kvo(object_getClass(win));
   for (Class c = start; c && c != stop; c = class_getSuperclass(c)) {
      if (class_has_own_legacy_imp(c)) { return 1; }
   }
   if (g_win_test_accept_subclass) {
      /* the class is a non-stock subclass of NSWindow/NSPanel */
      const char *cn = start ? class_getName(start) : "";
      if (cn && strncmp(cn, "NS", 2) != 0) { return 1; }
   }
   return 0;
}

/* find the first descendant view whose class-name contains `needle` */
static id chrome_find_subview(id root, const char *needle) {
   if (!root) { return nil; }
   const char *cn = object_getClassName(root);
   if (cn && strstr(cn, needle)) { return root; }
   id subs = ((id(*)(id, SEL))objc_msgSend)(root, sel_registerName("subviews"));
   if (!subs) { return nil; }
   long n = ((long(*)(id, SEL))objc_msgSend)(subs, sel_registerName("count"));
   for (long i = 0; i < n; ++i) {
      id s = ((id(*)(id, SEL, long))objc_msgSend)(
                subs, sel_registerName("objectAtIndex:"), i);
      id r = chrome_find_subview(s, needle);
      if (r) { return r; }
   }
   return nil;
}

/* -drawRect: for our overlay. self is the overlay view; it lives inside the
 * window's NSTitlebarView. Re-invoke the window's OWN legacy chrome-draw in the
 * full frame-view coordinate space: the app draws its band at the TOP of the
 * window frame, so translate the CTM DOWN so that frame-top lands over our band
 * (which sits at the top of the frame, i.e. the titlebar). */
static void chrome_overlay_drawRect(id self, SEL _cmd, CGRect dirty) {
   (void)_cmd; (void)dirty;
   id win = ((id(*)(id, SEL))objc_msgSend)(self, g_sel_window);
   if (!window_has_legacy_chrome(win)) { return; }

   /* our bounds (the titlebar band, unflipped: 0,0 .. bandW,bandH) */
   CGRect ob = abiconv_view_bounds(self);
   /* the window frame view + its height (band sits at the top of the frame) */
   id contentView = ((id(*)(id, SEL))objc_msgSend)(win, sel_registerName("contentView"));
   id frameView = contentView
      ? ((id(*)(id, SEL))objc_msgSend)(contentView, sel_registerName("superview"))
      : nil;
   CGRect fb = frameView ? abiconv_view_bounds(frameView) : ob;
   double bandH = ob.size.height;
   double frameH = fb.size.height > 0 ? fb.size.height : bandH;

   /* current CG context (the overlay's drawRect: focused it) */
   id nsctx = ((id(*)(id, SEL))objc_msgSend)(
                 (id)objc_getClass("NSGraphicsContext"),
                 sel_registerName("currentContext"));
   CGContextRef cg = nsctx
      ? (CGContextRef)((void*(*)(id, SEL))objc_msgSend)(nsctx, sel_registerName("CGContext"))
      : NULL;
   if (cg) { CGContextSaveGState(cg); }
   /* Map frame-view coords -> overlay coords: the app draws the band spanning
    * frame y=[frameH-bandH .. frameH]; our overlay spans y=[0..bandH]. Shift the
    * origin down by (frameH-bandH) so the app's top-of-frame band lands on us. */
   if (cg && frameH > bandH) {
      CGContextTranslateCTM(cg, 0.0, -(frameH - bandH));
   }

   /* re-invoke the app's own chrome draw in frame-view space */
   CGRect frameRect = fb;
   ((void(*)(id, SEL, CGRect))objc_msgSend)(win, g_sel_drawWindowBorderInRect, frameRect);
   if (((signed char(*)(id, SEL, SEL))objc_msgSend)(
          win, sel_registerName("respondsToSelector:"), g_sel_drawWindowTitle)) {
      ((void(*)(id, SEL))objc_msgSend)(win, g_sel_drawWindowTitle);
   }

   if (cg) { CGContextRestoreGState(cg); }
   if (BRIDGE_TRACE() || wchrome_trace()) {
      fprintf(stderr, "[compat] wchrome overlay draw: band=%.0fx%.0f frameH=%.0f cg=%p (%s)\n",
              ob.size.width, bandH, frameH, (void*)cg, object_getClassName(win));
      fflush(stderr);
   }

   /* ABICONV_WINCHROME_PROBE: verify (headlessly — screencapture is wedged in
    * agent sessions) WHAT the re-invoked app chrome actually painted. Once,
    * offscreen-render THIS overlay via cacheDisplayInRect: (re-runs our drawRect:
    * into a fresh bitmap, NOT the on-screen surface) and vertical-scan the band.
    * This is how we established Quinn's -drawWindowBorderInRect: yields a uniformly
    * BLACK band (=> PAINT off by default). Re-entry-guarded; inert unless set. */
   static __thread int probe_depth;
   if (getenv("ABICONV_WINCHROME_PROBE") && probe_depth == 0) {
      static int probed;
      if (!probed) {
         probed = 1;
         ++probe_depth;
         id rep = ((id(*)(id, SEL, CGRect))objc_msgSend)(
            self, sel_registerName("bitmapImageRepForCachingDisplayInRect:"), ob);
         if (rep) {
            ((void(*)(id, SEL, CGRect, id))objc_msgSend)(
               self, sel_registerName("cacheDisplayInRect:toBitmapImageRep:"), ob, rep);
            long W = ((long(*)(id, SEL))objc_msgSend)(rep, sel_registerName("pixelsWide"));
            long H = ((long(*)(id, SEL))objc_msgSend)(rep, sel_registerName("pixelsHigh"));
            fprintf(stderr, "[wchrome-probe] overlay rep %ldx%ld; vertical scan at x=W/2:\n", W, H);
            long xc = W/2;
            for (long y = 0; y < H; y += (H > 20 ? H/20 : 1)) {
               id col = ((id(*)(id, SEL, long, long))objc_msgSend)(
                  rep, sel_registerName("colorAtX:y:"), xc, y);
               if (!col) { continue; }
               double r = ((double(*)(id, SEL))objc_msgSend)(col, sel_registerName("redComponent"));
               double g = ((double(*)(id, SEL))objc_msgSend)(col, sel_registerName("greenComponent"));
               double b = ((double(*)(id, SEL))objc_msgSend)(col, sel_registerName("blueComponent"));
               const char *tag = (r>0.35&&r<0.85&&g>0.35&&g<0.85&&b>0.35&&b<0.85) ? "METAL?"
                               : (r<0.15&&g<0.15&&b<0.15) ? "black"
                               : (r>0.85&&g>0.85&&b>0.85) ? "white" : "other";
               fprintf(stderr, "[wchrome-probe]   y=%ld rgb=%.2f,%.2f,%.2f %s\n", y, r, g, b, tag);
            }
            fflush(stderr);
         }
         --probe_depth;
      }
   }
}
static signed char chrome_overlay_isFlipped(id self, SEL _cmd) {
   (void)self; (void)_cmd; return 0;   /* unflipped: origin bottom-left, like the frame view */
}
/* let clicks pass through to the traffic-lights / draggable titlebar */
static id chrome_overlay_hitTest(id self, SEL _cmd, CGPoint p) {
   (void)self; (void)_cmd; (void)p; return nil;
}

static Class chrome_overlay_class(void) {
   if (g_chrome_overlay_cls) { return g_chrome_overlay_cls; }
   Class super = objc_getClass("NSView");
   if (!super) { return NULL; }
   Class c = objc_allocateClassPair(super, "_86x64_ChromeOverlay", 0);
   if (!c) {   /* another copy already made it: adopt the existing one */
      g_chrome_overlay_cls = objc_getClass("_86x64_ChromeOverlay");
      return g_chrome_overlay_cls;
   }
   class_addMethod(c, sel_registerName("drawRect:"),
                   (IMP)chrome_overlay_drawRect, "v@:{CGRect={CGPoint=dd}{CGSize=dd}}");
   class_addMethod(c, sel_registerName("isFlipped"),
                   (IMP)chrome_overlay_isFlipped, "c@:");
   class_addMethod(c, sel_registerName("hitTest:"),
                   (IMP)chrome_overlay_hitTest, "@@:{CGPoint=dd}");
   objc_registerClassPair(c);
   g_chrome_overlay_cls = c;
   return c;
}

/* Install our overlay into `win`'s NSTitlebarView (idempotent). Self-gates on
 * the structural legacy-chrome check, so it's a no-op on any native window. */
static void chrome_install_overlay(id win) {
   if (!window_has_legacy_chrome(win)) { return; }
   /* PAINT is OFF BY DEFAULT. Re-invoking the app's own -drawWindowBorderInRect:
    * is only safe when the app's chrome draw actually RENDERS under translation.
    * For the archetype (Quinn's PolishedMetalWindow) it does NOT: the draw reads a
    * hardcoded 2007 NSWindow private ivar (the old _borderView) for its geometry
    * and sources the brushed metal from the classic TEXTURED-window / NSColor
    * metal-pattern substrate that modern macOS removed — so the re-invoke paints a
    * uniformly BLACK band, a REGRESSION vs the stock titled chrome (which already
    * shows the title + traffic-lights = the functionality-first fallback). Until a
    * classic textured-window / metal-pattern substrate shim exists (see todo_gaps
    * "legacy brushed-metal window substrate"), leave the stock chrome and DON'T
    * paint. The full structural detection + install funnel + overlay class + guard
    * stay as landed infrastructure: any future legacy app whose re-invoked chrome
    * DOES render correctly is served by flipping ABICONV_WINCHROME_PAINT on (then
    * the overlay installs + repaints the band from the app's own chrome draw).
    * The native window-chrome guard drives this path via a working synthetic draw. */
   if (!getenv("ABICONV_WINCHROME_PAINT") && !g_chrome_test_accept_responds) {
      if (wchrome_trace()) {
         fprintf(stderr, "[compat] wchrome: legacy-chrome window detected (%s); "
                 "PAINT off by default (stock titled chrome kept) — set "
                 "ABICONV_WINCHROME_PAINT to re-invoke the app chrome draw\n",
                 object_getClassName(win));
         fflush(stderr);
      }
      return;
   }
   id contentView = ((id(*)(id, SEL))objc_msgSend)(win, sel_registerName("contentView"));
   id frameView = contentView
      ? ((id(*)(id, SEL))objc_msgSend)(contentView, sel_registerName("superview"))
      : nil;
   if (!frameView) { return; }
   id tbv = chrome_find_subview(frameView, "NSTitlebarView");
   if (!tbv) { return; }                       /* titlebar not built yet: retry */

   /* already installed? scan the titlebar's subviews for our class */
   Class ov_cls = chrome_overlay_class();
   if (!ov_cls) { return; }
   id subs = ((id(*)(id, SEL))objc_msgSend)(tbv, sel_registerName("subviews"));
   long n = subs ? ((long(*)(id, SEL))objc_msgSend)(subs, sel_registerName("count")) : 0;
   id bg = nil;
   for (long i = 0; i < n; ++i) {
      id s = ((id(*)(id, SEL, long))objc_msgSend)(subs, sel_registerName("objectAtIndex:"), i);
      if (object_getClass(s) == ov_cls) { return; }   /* already present */
      const char *cn = object_getClassName(s);
      if (!bg && cn && strstr(cn, "NSTitlebarBackgroundView")) { bg = s; }
   }

   CGRect tb = abiconv_view_bounds(tbv);
   (void)abiconv_msg_rect; /* reserved helper for future non-bounds rects */
   id ov = ((id(*)(id, SEL))objc_msgSend)((id)ov_cls, sel_registerName("alloc"));
   ov = ((id(*)(id, SEL, CGRect))objc_msgSend)(ov, sel_registerName("initWithFrame:"), tb);
   /* fill the band + track resize (width & height sizable) */
   ((void(*)(id, SEL, unsigned long))objc_msgSend)(
      ov, sel_registerName("setAutoresizingMask:"), (unsigned long)(2 /*WidthSizable*/ | 16 /*HeightSizable*/));
   /* insert ABOVE the stock background (so our chrome hides the white system
    * titlebar) but BELOW the widgets/title (so traffic-lights + title stay). */
   if (bg) {
      ((void(*)(id, SEL, id, long, id))objc_msgSend)(
         tbv, sel_registerName("addSubview:positioned:relativeTo:"),
         ov, (long)1 /*NSWindowAbove*/, bg);
   } else {
      /* no background found: put us at the very bottom so we don't cover widgets */
      id first = (n > 0) ? ((id(*)(id, SEL, long))objc_msgSend)(subs, sel_registerName("objectAtIndex:"), 0) : nil;
      ((void(*)(id, SEL, id, long, id))objc_msgSend)(
         tbv, sel_registerName("addSubview:positioned:relativeTo:"),
         ov, (long)-1 /*NSWindowBelow*/, first);
   }
   ((void(*)(id, SEL, signed char))objc_msgSend)(ov, sel_registerName("setNeedsDisplay:"), 1);
   if (BRIDGE_TRACE() || wchrome_trace()) {
      fprintf(stderr, "[compat] wchrome overlay INSTALLED on %s (band %.0fx%.0f)\n",
              object_getClassName(win), tb.size.width, tb.size.height);
      fflush(stderr);
   }
}

/* Install trigger: swizzle -[NSWindow orderWindow:relativeTo:], the funnel every
 * window-show path runs through (orderFront:/makeKeyAndOrderFront:/...). Modern
 * NSThemeFrame draws through its LAYER (updateLayer), NOT -drawRect:, so a
 * drawRect: swizzle never fires; orderWindow: does, and by then the window's
 * NSTitlebarView subtree is built (verified). Idempotent + self-gated, so it is
 * a no-op on every native window and only paints an orphaned-chrome legacy one. */
/* ABICONV_TITLEBAR_PROBE (diagnostic, inert unless set): after a legacy window is
 * shown, offscreen-render its NSTitlebarView and report whether the STOCK chrome
 * actually composited the title text + the traffic-light widgets, and the band's
 * dominant colour. Answers "does the stock NSThemeFrame chrome show title+lights
 * under translation, or is the band blank?" without a display/screencapture. */
static void titlebar_probe(id win) {
   if (!getenv("ABICONV_TITLEBAR_PROBE")) { return; }
   static int done; if (done) { return; } done = 1;
   id contentView = ((id(*)(id, SEL))objc_msgSend)(win, sel_registerName("contentView"));
   id frameView = contentView
      ? ((id(*)(id, SEL))objc_msgSend)(contentView, sel_registerName("superview")) : nil;
   id tbv = frameView ? chrome_find_subview(frameView, "NSTitlebarView") : nil;
   if (!tbv) { fprintf(stderr, "[tbprobe] no NSTitlebarView\n"); fflush(stderr); return; }
   /* enumerate the titlebar subtree: note widgets + the title field */
   id subs = ((id(*)(id, SEL))objc_msgSend)(tbv, sel_registerName("subviews"));
   long n = subs ? ((long(*)(id, SEL))objc_msgSend)(subs, sel_registerName("count")) : 0;
   int lights = 0, titlefield = 0;
   for (long i = 0; i < n; ++i) {
      id s = ((id(*)(id, SEL, long))objc_msgSend)(subs, sel_registerName("objectAtIndex:"), i);
      const char *cn = object_getClassName(s);
      if (cn && strstr(cn, "Widget")) { lights++; }
      if (cn && strstr(cn, "TextField")) { titlefield++; }
   }
   /* window title string */
   id title = ((id(*)(id, SEL))objc_msgSend)(win, sel_registerName("title"));
   const char *tcstr = title
      ? ((const char*(*)(id, SEL))objc_msgSend)(title, sel_registerName("UTF8String")) : "(nil)";
   /* offscreen-render the titlebar band + sample its centre for a dominant colour */
   CGRect tb = abiconv_view_bounds(tbv);
   id rep = ((id(*)(id, SEL, CGRect))objc_msgSend)(
      tbv, sel_registerName("bitmapImageRepForCachingDisplayInRect:"), tb);
   double r = -1, g = -1, b = -1; long W = 0, H = 0;
   if (rep) {
      ((void(*)(id, SEL, CGRect, id))objc_msgSend)(
         tbv, sel_registerName("cacheDisplayInRect:toBitmapImageRep:"), tb, rep);
      W = ((long(*)(id, SEL))objc_msgSend)(rep, sel_registerName("pixelsWide"));
      H = ((long(*)(id, SEL))objc_msgSend)(rep, sel_registerName("pixelsHigh"));
      id col = ((id(*)(id, SEL, long, long))objc_msgSend)(
         rep, sel_registerName("colorAtX:y:"), W/2, H/2);
      if (col) {
         r = ((double(*)(id, SEL))objc_msgSend)(col, sel_registerName("redComponent"));
         g = ((double(*)(id, SEL))objc_msgSend)(col, sel_registerName("greenComponent"));
         b = ((double(*)(id, SEL))objc_msgSend)(col, sel_registerName("blueComponent"));
      }
      /* rep row 0 = TOP of the band (title/traffic-light strip); bottom rows = the
       * unified-toolbar strip. Sample both + the traffic-light x-columns. */
      long ys[3] = { H/8, H/2, (H*7)/8 };
      const char *yn[3] = { "top(title/lights)", "middle", "bottom(toolbar)" };
      for (int k = 0; k < 3; ++k) {
         id c2 = ((id(*)(id, SEL, long, long))objc_msgSend)(
            rep, sel_registerName("colorAtX:y:"), W/2, ys[k]);
         if (!c2) { continue; }
         double rr = ((double(*)(id, SEL))objc_msgSend)(c2, sel_registerName("redComponent"));
         double gg = ((double(*)(id, SEL))objc_msgSend)(c2, sel_registerName("greenComponent"));
         double bb = ((double(*)(id, SEL))objc_msgSend)(c2, sel_registerName("blueComponent"));
         fprintf(stderr, "[tbprobe]   y=%ld %-18s rgb=%.2f,%.2f,%.2f\n", ys[k], yn[k], rr, gg, bb);
      }
      /* traffic-light column at x~19pt*scale (close widget), y~top */
      double scale = (double)W / (tb.size.width > 0 ? tb.size.width : 1);
      long lx = (long)(19 * scale), ly = (long)(16 * scale);
      id cl = ((id(*)(id, SEL, long, long))objc_msgSend)(
         rep, sel_registerName("colorAtX:y:"), lx, ly);
      if (cl) {
         double rr = ((double(*)(id, SEL))objc_msgSend)(cl, sel_registerName("redComponent"));
         double gg = ((double(*)(id, SEL))objc_msgSend)(cl, sel_registerName("greenComponent"));
         double bb = ((double(*)(id, SEL))objc_msgSend)(cl, sel_registerName("blueComponent"));
         fprintf(stderr, "[tbprobe]   close-light px(%ld,%ld) rgb=%.2f,%.2f,%.2f (red-ish if drawn)\n",
                 lx, ly, rr, gg, bb);
      }
   }
   fprintf(stderr, "[tbprobe] title='%s' widgets(lights)=%d titleField=%d "
           "band=%ldx%ld centre_rgb=%.2f,%.2f,%.2f\n",
           tcstr, lights, titlefield, W, H, r, g, b);
   /* per-subview: class + opaque? + hidden? + wantsLayer? + layer bg colour, so we
    * can pinpoint WHICH titlebar view is painting the band black under translation */
   for (long i = 0; i < n; ++i) {
      id s = ((id(*)(id, SEL, long))objc_msgSend)(subs, sel_registerName("objectAtIndex:"), i);
      signed char opaque = ((signed char(*)(id, SEL))objc_msgSend)(s, sel_registerName("isOpaque"));
      signed char hidden = ((signed char(*)(id, SEL))objc_msgSend)(s, sel_registerName("isHidden"));
      signed char wl = ((signed char(*)(id, SEL))objc_msgSend)(s, sel_registerName("wantsLayer"));
      CGRect sf = abiconv_msg_rect(s, sel_registerName("frame"));
      fprintf(stderr, "[tbprobe]   [%ld] %-40s opaque=%d hidden=%d wantsLayer=%d frame=%.0f,%.0f %.0fx%.0f\n",
              i, object_getClassName(s), opaque, hidden, wl,
              sf.origin.x, sf.origin.y, sf.size.width, sf.size.height);
   }
   fflush(stderr);
}

/* ---- generic window-TITLE-on-show (Cocoa) --------------------------------
 * A pre-10.x Cocoa app that relied on the private-frame-view swap to draw its
 * OWN title (via -drawWindowTitle, orphaned on modern macOS — see the chrome
 * compat above) can come up with an EMPTY NSWindow.title: the app never called
 * -setTitle: because it painted the title itself. On modern macOS that path is
 * dead, so the stock NSThemeFrame titlebar has nothing to show and renders
 * blank (Quinn's window has no title text at all).
 *
 * Restore the contract UNIVERSALLY: when a LEGACY-origin window is shown with an
 * empty/whitespace title, fill NSWindow.title with the best available name —
 * the window's represented-URL/filename last path component if set, else the
 * running app's localized name (NSRunningApplication) / NSBundle display name /
 * process name — and force titleVisibility=visible so the modern titlebar shows
 * (and auto-centers) it. Never overwrites a title the app DID set; never touches
 * a native window. Env kill-switch ABICONV_WINTITLE_COMPAT (set => skip). */
static int wintitle_compat_off(void) {
   static int v = -1;
   if (v < 0) { v = getenv("ABICONV_WINTITLE_COMPAT") ? 1 : 0; }
   return v;
}
/* returns 1 if `s` is nil or all-whitespace */
static int nsstring_blank(id s) {
   if (!s) { return 1; }
   const char *c = ((const char*(*)(id, SEL))objc_msgSend)(s, sel_registerName("UTF8String"));
   if (!c) { return 1; }
   for (; *c; ++c) { if (*c != ' ' && *c != '\t' && *c != '\n' && *c != '\r') { return 0; } }
   return 1;
}
/* best-effort app display name -> autoreleased NSString (or nil) */
static id app_display_name(void) {
   /* NSRunningApplication.currentApplication.localizedName */
   Class rac = objc_getClass("NSRunningApplication");
   if (rac) {
      id ra = ((id(*)(id, SEL))objc_msgSend)((id)rac, sel_registerName("currentApplication"));
      if (ra) {
         id nm = ((id(*)(id, SEL))objc_msgSend)(ra, sel_registerName("localizedName"));
         if (!nsstring_blank(nm)) { return nm; }
      }
   }
   /* NSBundle.mainBundle CFBundleName / CFBundleDisplayName */
   Class bc = objc_getClass("NSBundle");
   if (bc) {
      id mb = ((id(*)(id, SEL))objc_msgSend)((id)bc, sel_registerName("mainBundle"));
      if (mb) {
         const char *keys[2] = { "CFBundleDisplayName", "CFBundleName" };
         for (int k = 0; k < 2; ++k) {
            id key = ((id(*)(id, SEL, const char*))objc_msgSend)(
               (id)objc_getClass("NSString"),
               sel_registerName("stringWithUTF8String:"), keys[k]);
            id v = ((id(*)(id, SEL, id))objc_msgSend)(
               mb, sel_registerName("objectForInfoDictionaryKey:"), key);
            if (!nsstring_blank(v)) { return v; }
         }
      }
   }
   /* NSProcessInfo.processName */
   Class pic = objc_getClass("NSProcessInfo");
   if (pic) {
      id pi = ((id(*)(id, SEL))objc_msgSend)((id)pic, sel_registerName("processInfo"));
      if (pi) {
         id nm = ((id(*)(id, SEL))objc_msgSend)(pi, sel_registerName("processName"));
         if (!nsstring_blank(nm)) { return nm; }
      }
   }
   return nil;
}
/* Guarantee a VISIBLE title label for a legacy window whose stock titlebar won't
 * render the title (legacy metal/toolbar window: the toolbar occupies the
 * titlebar row so the stock title has nowhere to draw). Install a titlebar
 * ACCESSORY view controller (NSTitlebarAccessoryViewController) whose view is a
 * centered NSTextField showing `title`. The accessory renders in the titlebar
 * band regardless of the stock title layout. Idempotent (tagged so a re-show
 * doesn't stack duplicates); app-agnostic; no-op if the accessory API or the
 * window's addTitlebarAccessoryViewController: is unavailable. */
#define ABICONV_TITLE_ACCESSORY_TAG 0x51544c42 /* 'QTLB' */
static void install_titlebar_label(id win, id title) {
   if (nsstring_blank(title)) { return; }
   SEL sel_add = sel_registerName("addTitlebarAccessoryViewController:");
   SEL sel_getlist = sel_registerName("titlebarAccessoryViewControllers");
   if (!((signed char(*)(id, SEL, SEL))objc_msgSend)(
          win, sel_registerName("respondsToSelector:"), sel_add)) {
      return;                      /* pre-10.10 AppKit: no accessory API */
   }
   Class avc_cls = objc_getClass("NSTitlebarAccessoryViewController");
   if (!avc_cls) { return; }

   /* already installed? scan existing accessories for our tagged text field */
   id list = ((id(*)(id, SEL))objc_msgSend)(win, sel_getlist);
   long ln = list ? ((long(*)(id, SEL))objc_msgSend)(list, sel_registerName("count")) : 0;
   for (long i = 0; i < ln; ++i) {
      id avc = ((id(*)(id, SEL, long))objc_msgSend)(list, sel_registerName("objectAtIndex:"), i);
      id v = ((id(*)(id, SEL))objc_msgSend)(avc, sel_registerName("view"));
      if (v && ((long(*)(id, SEL))objc_msgSend)(v, sel_registerName("tag")) == ABICONV_TITLE_ACCESSORY_TAG) {
         /* update the existing label's text (title may have changed) + return */
         ((void(*)(id, SEL, id))objc_msgSend)(v, sel_registerName("setStringValue:"), title);
         return;
      }
   }

   /* build a non-editable, borderless, transparent centered label */
   Class tfc = objc_getClass("NSTextField");
   if (!tfc) { return; }
   id label = ((id(*)(id, SEL))objc_msgSend)((id)tfc, sel_registerName("alloc"));
   /* frame ~ 240x22; the accessory centers itself in the titlebar band */
   CGRect lf = {{0, 0}, {240, 22}};
   label = ((id(*)(id, SEL, CGRect))objc_msgSend)(label, sel_registerName("initWithFrame:"), lf);
   ((void(*)(id, SEL, id))objc_msgSend)(label, sel_registerName("setStringValue:"), title);
   ((void(*)(id, SEL, signed char))objc_msgSend)(label, sel_registerName("setEditable:"), 0);
   ((void(*)(id, SEL, signed char))objc_msgSend)(label, sel_registerName("setSelectable:"), 0);
   ((void(*)(id, SEL, signed char))objc_msgSend)(label, sel_registerName("setBordered:"), 0);
   ((void(*)(id, SEL, signed char))objc_msgSend)(label, sel_registerName("setBezeled:"), 0);
   ((void(*)(id, SEL, signed char))objc_msgSend)(label, sel_registerName("setDrawsBackground:"), 0);
   /* NSTextAlignmentCenter == 2 */
   ((void(*)(id, SEL, long))objc_msgSend)(label, sel_registerName("setAlignment:"), 2);
   /* label control font ~13pt (the standard title look) */
   Class fc = objc_getClass("NSFont");
   if (fc) {
      id f = ((id(*)(id, SEL, double))objc_msgSend)(
         (id)fc, sel_registerName("systemFontOfSize:"), 13.0);
      if (f) { ((void(*)(id, SEL, id))objc_msgSend)(label, sel_registerName("setFont:"), f); }
   }
   /* tag it so we can find/update/dedupe it later */
   ((void(*)(id, SEL, long))objc_msgSend)(label, sel_registerName("setTag:"), ABICONV_TITLE_ACCESSORY_TAG);

   /* wrap in an accessory VC; layoutAttribute = NSLayoutAttributeBottom (10) so it
    * sits in the toolbar/title band; centered via the label's own alignment. */
   id avc = ((id(*)(id, SEL))objc_msgSend)((id)avc_cls, sel_registerName("alloc"));
   avc = ((id(*)(id, SEL))objc_msgSend)(avc, sel_registerName("init"));
   ((void(*)(id, SEL, id))objc_msgSend)(avc, sel_registerName("setView:"), label);
   if (((signed char(*)(id, SEL, SEL))objc_msgSend)(
          avc, sel_registerName("respondsToSelector:"), sel_registerName("setLayoutAttribute:"))) {
      /* A titlebar accessory REQUIRES Left/Right/Top/Bottom (NSLayoutAttribute:
       * Left=1 Right=2 Top=3 Bottom=4). Use Bottom(4) so the label sits in the
       * lower titlebar band (with a toolbar present); the label's own centered
       * alignment + 240pt width centers the text. */
      ((void(*)(id, SEL, long))objc_msgSend)(avc, sel_registerName("setLayoutAttribute:"), 4);
   }
   ((void(*)(id, SEL, id))objc_msgSend)(win, sel_add, avc);
   if (wchrome_trace()) {
      const char *tc = ((const char*(*)(id, SEL))objc_msgSend)(title, sel_registerName("UTF8String"));
      fprintf(stderr, "[compat] wintitle: installed titlebar-label accessory '%s'\n", tc ? tc : "?");
      fflush(stderr);
   }
}

/* Apply the generic title fix to a legacy window whose title is blank. */
static void window_title_on_show(id win) {
   if (wintitle_compat_off()) { return; }
   if (!(window_is_legacy(win) || window_has_legacy_chrome(win))) { return; }

   /* Ground truth (live Quinn diag): the title 'Quinn' is already SET and
    * titleVisibility is already Visible — but it does NOT render because the
    * window's styleMask has NSWindowStyleMaskUnifiedTitleAndToolbar (0x1000): the
    * title and the toolbar SHARE ONE row, and with a toolbar present the toolbar
    * fills that row so the title has nowhere to draw. The genuine 10.5/10.6 look
    * is TWO rows: the title centered ABOVE the toolbar.
    *
    * FIX: give the window the classic two-row (expanded) layout:
    *   (1) styleMask += Titled (bit 0), and CLEAR the unified bit (0x1000) so the
    *       title gets its own row above the toolbar;
    *   (2) toolbarStyle = NSWindowToolbarStyleExpanded (2) on macOS 11+ — the
    *       modern API for "title row above a separate toolbar row" (the reliable
    *       lever; clearing the unified bit alone can be ignored once a toolbar is
    *       attached, so we set both);
    *   (3) titlebar NOT transparent; titleVisibility = Visible (0).
    * We only ADD Titled + CLEAR the unified bit + set an expanded style; we never
    * clear an unrelated app bit. Idempotent on a window already in this state. */
   SEL sm_get = sel_registerName("styleMask");
   SEL sm_set = sel_registerName("setStyleMask:");
   if (((signed char(*)(id, SEL, SEL))objc_msgSend)(win, sel_registerName("respondsToSelector:"), sm_set)) {
      unsigned long mask = ((unsigned long(*)(id, SEL))objc_msgSend)(win, sm_get);
      unsigned long want = (mask | 1UL /*Titled*/) & ~0x1000UL /*clear UnifiedTitleAndToolbar*/;
      if (want != mask) {
         ((void(*)(id, SEL, unsigned long))objc_msgSend)(win, sm_set, want);
      }
   }
   /* NSWindowToolbarStyleExpanded == 1 (Automatic=0, Expanded=1): title row above
    * a separate toolbar row = the classic two-row look. */
   if (((signed char(*)(id, SEL, SEL))objc_msgSend)(
          win, sel_registerName("respondsToSelector:"),
          sel_registerName("setToolbarStyle:"))) {
      ((void(*)(id, SEL, long))objc_msgSend)(
         win, sel_registerName("setToolbarStyle:"), 1);
   }
   if (((signed char(*)(id, SEL, SEL))objc_msgSend)(
          win, sel_registerName("respondsToSelector:"),
          sel_registerName("setTitlebarAppearsTransparent:"))) {
      ((void(*)(id, SEL, signed char))objc_msgSend)(
         win, sel_registerName("setTitlebarAppearsTransparent:"), 0);
   }
   /* ensure the modern titlebar shows the title (0 == NSWindowTitleVisible) */
   if (((signed char(*)(id, SEL, SEL))objc_msgSend)(
          win, sel_registerName("respondsToSelector:"),
          sel_registerName("setTitleVisibility:"))) {
      ((void(*)(id, SEL, long))objc_msgSend)(
         win, sel_registerName("setTitleVisibility:"), 0);
   }

   /* Determine the final title string: keep the app's own if present (Quinn's nib
    * title "Quinn"), else derive one (represented filename's last component, else
    * the app/process name). */
   id cur = ((id(*)(id, SEL))objc_msgSend)(win, sel_registerName("title"));
   id name = cur;
   if (nsstring_blank(name)) {
      name = nil;
      id repf = ((id(*)(id, SEL))objc_msgSend)(win, sel_registerName("representedFilename"));
      if (!nsstring_blank(repf)) {
         name = ((id(*)(id, SEL))objc_msgSend)(repf, sel_registerName("lastPathComponent"));
         if (nsstring_blank(name)) { name = nil; }
      }
      if (!name) { name = app_display_name(); }
      if (nsstring_blank(name)) { return; }
      ((void(*)(id, SEL, id))objc_msgSend)(win, sel_registerName("setTitle:"), name);
   } else {
      /* re-assert setTitle: with the same value to force a titlebar relayout now
       * that the style is corrected (cheap). */
      ((void(*)(id, SEL, id))objc_msgSend)(win, sel_registerName("setTitle:"), name);
   }
   if (wchrome_trace()) {
      const char *nc = ((const char*(*)(id, SEL))objc_msgSend)(name, sel_registerName("UTF8String"));
      fprintf(stderr, "[compat] wintitle: legacy window (%s) title '%s' set/visible; "
              "installing titlebar label fallback\n", object_getClassName(win), nc ? nc : "?");
      fflush(stderr);
   }
   /* FALLBACK (the actual visible-title guarantee): on the LIVE Quinn diag the
    * title is correctly set + Visible + toolbarStyle=Expanded + unified bit
    * cleared, yet the legacy PolishedMetalWindow does NOT materialize a two-row
    * title row above the toolbar (the toolbar occupies the titlebar row → the
    * stock title has nowhere to draw → blank). AppKit's own title label can't be
    * forced to render there for this window class. So GUARANTEE a non-blank
    * visible title app-agnostically by installing a titlebar ACCESSORY view
    * controller carrying a centered NSTextField label with the title string — it
    * renders in the titlebar band regardless of the stock title layout. This is
    * generic (any legacy window), idempotent (install once), and independent of
    * whether the two-row layout materialized. */
   install_titlebar_label(win, name);
}

/* ---- generic TOOLBAR-BUTTON styling --------------------------------------
 * Ground truth (live Quinn diag): the toolbar items are STANDARD image
 * NSToolbarItems (view=nil), NOT custom-view buttons — QuinnPlayOrAbort,
 * QuinnPauseOrContinue, QuinnConnect ('Connect'), QuinnTournament, QuinnHighscores,
 * QuinnHelp, with NSToolbarFlexibleSpaceItems between the groups. displayMode is
 * already IconAndLabel (=1, the app set it) and sizeMode Regular (=1).
 *
 * On 10.5/10.6 a standard toolbar item on a brushed-metal toolbar rendered as a
 * FLAT icon with a text label beneath — no per-item background/bezel. On modern
 * macOS the SAME standard items render with the default modern gray bordered
 * pill background (NSToolbarItem gained a bordered look; 10.15+ exposes
 * -setBordered:). So the fix is NOT about custom views (there are none) — it is
 * to set each standard non-space item BORDERLESS via the standard NSToolbarItem
 * API: -[NSToolbarItem setBordered:NO] => flat icon on metal, exactly the
 * genuine look. The item's own label already renders under IconAndLabel once the
 * toolbar isn't the unified title row (fixed in window_title_on_show).
 *
 * Restore the classic look UNIVERSALLY, keyed on the toolbar being attached to a
 * LEGACY-origin window (structural, NO class name): keep displayMode=IconAndLabel
 * + sizeMode=Regular, and set every non-space item borderless. Space items
 * (NSToolbar{Flexible,}SpaceItem — identifier starts with "NSToolbar") are left
 * alone so the app's grouping is preserved. Native toolbars on native windows are
 * never touched. Env kill-switch ABICONV_TOOLBAR_COMPAT. */
static int toolbar_compat_off(void) {
   static int v = -1;
   if (v < 0) { v = getenv("ABICONV_TOOLBAR_COMPAT") ? 1 : 0; }
   return v;
}
/* Flatten one STANDARD toolbar item (setBordered:NO) so it's a flat icon on the
 * metal toolbar. No-op on a space item (grouping preserved) and on an item that
 * doesn't respond to -setBordered: (older SDK). If the item DOES carry a custom
 * view button (defensive — Quinn's are standard, but other apps may differ),
 * also flatten that button so it isn't a bordered blob. */
static void toolbar_style_item(id item) {
   /* skip the standard space/flexible-space items: identifier starts "NSToolbar" */
   id iid = ((id(*)(id, SEL))objc_msgSend)(item, sel_registerName("itemIdentifier"));
   if (iid) {
      const char *ic = ((const char*(*)(id, SEL))objc_msgSend)(iid, sel_registerName("UTF8String"));
      if (ic && strncmp(ic, "NSToolbar", 9) == 0) { return; }   /* space/flex-space */
   }
   /* STANDARD path: NSToolbarItem.setBordered: (macOS 10.15+) => flat icon */
   if (((signed char(*)(id, SEL, SEL))objc_msgSend)(
          item, sel_registerName("respondsToSelector:"), sel_registerName("setBordered:"))) {
      ((void(*)(id, SEL, signed char))objc_msgSend)(item, sel_registerName("setBordered:"), 0);
   }
   /* Defensive custom-view path: if this item HAS a button view, flatten it too. */
   id view = ((id(*)(id, SEL))objc_msgSend)(item, sel_registerName("view"));
   Class btn = objc_getClass("NSButton");
   if (view && btn && ((signed char(*)(id, SEL, Class))objc_msgSend)(
                          view, sel_registerName("isKindOfClass:"), btn)) {
      if (((signed char(*)(id, SEL, SEL))objc_msgSend)(
             view, sel_registerName("respondsToSelector:"), sel_registerName("setBordered:"))) {
         ((void(*)(id, SEL, signed char))objc_msgSend)(view, sel_registerName("setBordered:"), 0);
      }
      if (((signed char(*)(id, SEL, SEL))objc_msgSend)(
             view, sel_registerName("respondsToSelector:"), sel_registerName("setBezelStyle:"))) {
         ((void(*)(id, SEL, long))objc_msgSend)(view, sel_registerName("setBezelStyle:"), 0);
      }
   }
}
/* Apply classic display/size mode + item styling to a legacy window's toolbar. */
static void toolbar_style_on_show(id win) {
   if (toolbar_compat_off()) { return; }
   if (!(window_is_legacy(win) || window_has_legacy_chrome(win))) { return; }
   id tb = ((id(*)(id, SEL))objc_msgSend)(win, sel_registerName("toolbar"));
   if (!tb) {
      if (wchrome_trace()) {
         fprintf(stderr, "[compat] toolbar: legacy window (%s) has NO toolbar\n",
                 object_getClassName(win)); fflush(stderr);
      }
      return;
   }
   /* NSToolbarDisplayModeIconAndLabel == 1 ; NSToolbarSizeModeRegular == 1 */
   long dm = ((long(*)(id, SEL))objc_msgSend)(tb, sel_registerName("displayMode"));
   long sm = ((long(*)(id, SEL))objc_msgSend)(tb, sel_registerName("sizeMode"));
   /* only force IconAndLabel if the app left it at Default(0)/IconOnly(2) — do
    * not override an app that explicitly chose LabelOnly(3). */
   if (dm == 0 /*Default*/ || dm == 2 /*IconOnly*/) {
      ((void(*)(id, SEL, long))objc_msgSend)(tb, sel_registerName("setDisplayMode:"), 1);
   }
   if (sm == 0 /*Default*/) {
      ((void(*)(id, SEL, long))objc_msgSend)(tb, sel_registerName("setSizeMode:"), 1 /*Regular*/);
   }
   /* flatten each standard item (setBordered:NO) -> flat icon on metal */
   id items = ((id(*)(id, SEL))objc_msgSend)(tb, sel_registerName("items"));
   long n = items ? ((long(*)(id, SEL))objc_msgSend)(items, sel_registerName("count")) : 0;
   for (long i = 0; i < n; ++i) {
      id it = ((id(*)(id, SEL, long))objc_msgSend)(items, sel_registerName("objectAtIndex:"), i);
      toolbar_style_item(it);
   }
   if (wchrome_trace()) {
      fprintf(stderr, "[compat] toolbar: legacy window (%s) toolbar styled "
              "(displayMode %ld->%ld sizeMode %ld->%ld, %ld items)\n",
              object_getClassName(win), dm,
              ((long(*)(id, SEL))objc_msgSend)(tb, sel_registerName("displayMode")),
              sm,
              ((long(*)(id, SEL))objc_msgSend)(tb, sel_registerName("sizeMode")), n);
      fflush(stderr);
   }
}

/* test hooks for window_chrome_test.sh (title + toolbar styling) */
void _86x64_test_window_title_on_show(id win) { window_title_on_show(win); }
void _86x64_test_toolbar_style_on_show(id win) { toolbar_style_on_show(win); }
int  _86x64_test_window_is_legacy(id win)      { return window_is_legacy(win); }
/* returns the tagged titlebar-label accessory's string value (or nil) so the
 * guard can assert a NON-BLANK rendered title label was installed. */
id _86x64_test_titlebar_label_text(id win) {
   id list = ((id(*)(id, SEL))objc_msgSend)(win, sel_registerName("titlebarAccessoryViewControllers"));
   long ln = list ? ((long(*)(id, SEL))objc_msgSend)(list, sel_registerName("count")) : 0;
   for (long i = 0; i < ln; ++i) {
      id avc = ((id(*)(id, SEL, long))objc_msgSend)(list, sel_registerName("objectAtIndex:"), i);
      id v = ((id(*)(id, SEL))objc_msgSend)(avc, sel_registerName("view"));
      if (v && ((long(*)(id, SEL))objc_msgSend)(v, sel_registerName("tag")) == ABICONV_TITLE_ACCESSORY_TAG) {
         return ((id(*)(id, SEL))objc_msgSend)(v, sel_registerName("stringValue"));
      }
   }
   return nil;
}

/* ---- ABICONV_WINCHROME_DIAG (env-gated, default OFF) ----------------------
 * A pixel-oracle diagnostic: when a window is shown, dump to stderr its class
 * chain, styleMask, title, titleVisibility, isVisible, and — for its toolbar —
 * displayMode/sizeMode + each item's identifier/label/view-class. Runs AFTER the
 * title + toolbar fixes so the log shows the POST-fix state (which is what the tester
 * sees). Called for legacy windows only; inert unless ABICONV_WINCHROME_DIAG. */
static const char *nsstr_c(id s) {
   if (!s) { return "(nil)"; }
   const char *c = ((const char*(*)(id, SEL))objc_msgSend)(s, sel_registerName("UTF8String"));
   return c ? c : "(nil)";
}
static void winchrome_diag(id win) {
   if (!getenv("ABICONV_WINCHROME_DIAG")) { return; }
   if (!(window_is_legacy(win) || window_has_legacy_chrome(win))) { return; }
   /* class chain (real, KVO-stripped) */
   fprintf(stderr, "[wcdiag] === window %p ===\n", (void*)win);
   fprintf(stderr, "[wcdiag] class chain:");
   for (Class c = object_getClass(win); c; c = class_getSuperclass(c)) {
      fprintf(stderr, " %s", class_getName(c));
      if (c == objc_getClass("NSWindow")) { break; }
   }
   fprintf(stderr, "\n");
   unsigned long mask = ((unsigned long(*)(id, SEL))objc_msgSend)(win, sel_registerName("styleMask"));
   long tvis = ((long(*)(id, SEL))objc_msgSend)(win, sel_registerName("titleVisibility"));
   signed char vis = ((signed char(*)(id, SEL))objc_msgSend)(win, sel_registerName("isVisible"));
   signed char ttrans = 0;
   if (((signed char(*)(id, SEL, SEL))objc_msgSend)(win, sel_registerName("respondsToSelector:"),
          sel_registerName("titlebarAppearsTransparent"))) {
      ttrans = ((signed char(*)(id, SEL))objc_msgSend)(win, sel_registerName("titlebarAppearsTransparent"));
   }
   id title = ((id(*)(id, SEL))objc_msgSend)(win, sel_registerName("title"));
   long tbstyle = -1;
   if (((signed char(*)(id, SEL, SEL))objc_msgSend)(win, sel_registerName("respondsToSelector:"),
          sel_registerName("toolbarStyle"))) {
      tbstyle = ((long(*)(id, SEL))objc_msgSend)(win, sel_registerName("toolbarStyle"));
   }
   fprintf(stderr, "[wcdiag] styleMask=0x%lx (Titled=%d Textured=%d UnifiedToolbar=%d FullSizeContent=%d) "
           "title='%s' titleVisibility=%ld titlebarTransparent=%d toolbarStyle=%ld isVisible=%d\n",
           mask, (int)(mask & 1), (int)((mask >> 8) & 1), (int)((mask >> 12) & 1),
           (int)((mask >> 15) & 1), nsstr_c(title), tvis, ttrans, tbstyle, vis);
   /* titlebar-label accessory (our fallback visible-title guarantee) */
   {
      id acc = _86x64_test_titlebar_label_text(win);
      fprintf(stderr, "[wcdiag] titlebarLabelAccessory='%s'\n", nsstr_c(acc));
   }
   /* toolbar breakdown */
   id tb = ((id(*)(id, SEL))objc_msgSend)(win, sel_registerName("toolbar"));
   if (!tb) { fprintf(stderr, "[wcdiag] toolbar: (none)\n"); fflush(stderr); return; }
   long dm = ((long(*)(id, SEL))objc_msgSend)(tb, sel_registerName("displayMode"));
   long sm = ((long(*)(id, SEL))objc_msgSend)(tb, sel_registerName("sizeMode"));
   signed char tbvis = ((signed char(*)(id, SEL))objc_msgSend)(tb, sel_registerName("isVisible"));
   id ident = ((id(*)(id, SEL))objc_msgSend)(tb, sel_registerName("identifier"));
   fprintf(stderr, "[wcdiag] toolbar '%s' displayMode=%ld sizeMode=%ld isVisible=%d\n",
           nsstr_c(ident), dm, sm, tbvis);
   id items = ((id(*)(id, SEL))objc_msgSend)(tb, sel_registerName("items"));
   long n = items ? ((long(*)(id, SEL))objc_msgSend)(items, sel_registerName("count")) : 0;
   for (long i = 0; i < n; ++i) {
      id it = ((id(*)(id, SEL, long))objc_msgSend)(items, sel_registerName("objectAtIndex:"), i);
      id iid = ((id(*)(id, SEL))objc_msgSend)(it, sel_registerName("itemIdentifier"));
      id lbl = ((id(*)(id, SEL))objc_msgSend)(it, sel_registerName("label"));
      id img = ((id(*)(id, SEL))objc_msgSend)(it, sel_registerName("image"));
      id view = ((id(*)(id, SEL))objc_msgSend)(it, sel_registerName("view"));
      const char *vcls = view ? object_getClassName(view) : "(nil)";
      /* the STANDARD NSToolbarItem.isBordered (10.15+) — the property v3 flips */
      int itembordered = -1;
      if (((signed char(*)(id, SEL, SEL))objc_msgSend)(it, sel_registerName("respondsToSelector:"),
             sel_registerName("isBordered"))) {
         itembordered = (int)((signed char(*)(id, SEL))objc_msgSend)(it, sel_registerName("isBordered"));
      }
      const char *btitle = "";
      int bordered = -1; long bezel = -1, imgpos = -1;
      if (view && objc_getClass("NSButton") &&
          ((signed char(*)(id, SEL, Class))objc_msgSend)(view, sel_registerName("isKindOfClass:"),
             objc_getClass("NSButton"))) {
         btitle = nsstr_c(((id(*)(id, SEL))objc_msgSend)(view, sel_registerName("title")));
         bordered = (int)((signed char(*)(id, SEL))objc_msgSend)(view, sel_registerName("isBordered"));
         bezel = ((long(*)(id, SEL))objc_msgSend)(view, sel_registerName("bezelStyle"));
         imgpos = ((long(*)(id, SEL))objc_msgSend)(view, sel_registerName("imagePosition"));
      }
      fprintf(stderr, "[wcdiag]   item[%ld] id='%s' label='%s' hasImage=%d itemBordered=%d view=%s "
              "btnTitle='%s' viewBordered=%d bezel=%ld imgPos=%ld\n",
              i, nsstr_c(iid), nsstr_c(lbl), img ? 1 : 0, itembordered, vcls, btitle, bordered, bezel, imgpos);
   }
   fflush(stderr);
}

static IMP g_window_orig_orderWindow;
static void window_orderWindow(id self, SEL _cmd, long place, long relativeTo) {
   if (g_window_orig_orderWindow) {
      ((void(*)(id, SEL, long, long))g_window_orig_orderWindow)(self, _cmd, place, relativeTo);
   }
   /* place==0 (NSWindowOut) is an ORDER-OUT (hide): nothing to install. */
   if (place != 0) {
      /* generic (every legacy Cocoa window): title-on-show + toolbar styling */
      window_title_on_show(self);
      toolbar_style_on_show(self);
      winchrome_diag(self);   /* env-gated pixel-oracle dump (post-fix state) */
      /* archetype-only (frame-view chrome swap): the brushed-metal overlay */
      if (window_has_legacy_chrome(self)) {
         chrome_install_overlay(self);
         titlebar_probe(self);
      }
   }
}

static void legacy_window_chrome_install(void) {
   static int done = 0;
   if (done) { return; }
   /* This swizzle drives THREE generic on-show fixes (title, toolbar styling,
    * chrome overlay), each with its OWN kill-switch. Only skip installing the
    * swizzle entirely if ALL THREE are disabled. (ABICONV_WINCHROME_COMPAT kills
    * just the chrome overlay; it must NOT also disable title/toolbar.) */
   if (getenv("ABICONV_WINCHROME_COMPAT") &&
       wintitle_compat_off() && toolbar_compat_off()) { done = 1; return; }
   Class winc = objc_getClass("NSWindow");
   if (!winc) { return; }                       /* AppKit not loaded yet: retry */
   Method m = class_getInstanceMethod(winc, sel_registerName("orderWindow:relativeTo:"));
   if (!m) { return; }
   /* cache selectors */
   g_sel_drawWindowBorderInRect = sel_registerName("drawWindowBorderInRect:");
   g_sel_drawWindowTitle        = sel_registerName("drawWindowTitle");
   g_sel_window                 = sel_registerName("window");
   g_sel_bounds                 = sel_registerName("bounds");
   /* once per PROCESS (env flag) so a second libabiconv copy doesn't capture the
    * first copy's swizzled IMP as "original" and chain. */
   if (getenv("ABICONV_WINCHROME_INSTALLED")) { done = 1; return; }
   setenv("ABICONV_WINCHROME_INSTALLED", "1", 1);
   g_window_orig_orderWindow = method_getImplementation(m);
   method_setImplementation(m, (IMP)window_orderWindow);
   done = 1;
}

/* test hook: drive the chrome install + overlay-fire from a native guard
 * (tests-i386/window_chrome_test.sh). Returns the overlay Class (or NULL). */
Class _86x64_test_window_chrome_overlay_class(void) {
   g_sel_drawWindowBorderInRect = sel_registerName("drawWindowBorderInRect:");
   g_sel_drawWindowTitle        = sel_registerName("drawWindowTitle");
   g_sel_window                 = sel_registerName("window");
   g_sel_bounds                 = sel_registerName("bounds");
   return chrome_overlay_class();
}
void _86x64_test_window_chrome_install_overlay(id win) { chrome_install_overlay(win); }
int  _86x64_test_window_has_legacy_chrome(id win) { return window_has_legacy_chrome(win); }

static void appkit_compat_install(void);
/* test hook: force the AppKit legacy-compat installs from a NATIVE harness
 * (tests-i386/snapshot_compat_test.sh) without a bridge entry — the installs
 * normally run lazily from objc_bridge_prep/reverse-prep, which a native
 * guard process never reaches. */
void _86x64_test_appkit_compat_install(void) { appkit_compat_install(); }

static void appkit_compat_install(void) {
   legacy_glview_1x_install();
   legacy_snapshot_compat_install();
   legacy_lockfocus_1x_install();
   legacy_cachedisplay_1x_install();
   legacy_displayrect_comark_install();
   legacy_gstate_capture_install();
   legacy_window_chrome_install();
   /* These self-guard (own env vars) and must keep retrying until THEIR
    * framework loads — NSColor/AppKit early, NSProFont/ProKit much later — so
    * run them BEFORE the `done` short-circuit (which only tracks the AppKit
    * document methods below). */
   appkit_color_compat_install();
   prokit_font_compat_install();
   legacy_locale_compat_install();   /* re-seed removed old-style locale keys */

   static int done = 0;
   if (done) return;
   const unsigned n = sizeof(g_appkit_compat) / sizeof(g_appkit_compat[0]);
   unsigned installed = 0;
   for (unsigned i = 0; i < n; ++i) {
      Class c = objc_getClass(g_appkit_compat[i].cls);
      if (!c) continue;                    /* framework not loaded yet: retry */
      SEL s = sel_registerName(g_appkit_compat[i].sel);
      if (class_getInstanceMethod(c, s) ||  /* still exists / already added */
          class_addMethod(c, s, g_appkit_compat[i].imp, g_appkit_compat[i].types)) {
         ++installed;
      }
   }
   if (installed == n) done = 1;
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[compat] appkit legacy methods installed %u/%u\n",
              installed, n);
      fflush(stderr);
   }
}

/*
 * ============================ REMAINING ABI GAP ============================
 * The arg marshaller (fill_method_args) currently classifies every explicit
 * argument as a single 4-byte integer/pointer slot placed in a GP register.
 * That is correct for int/long/pointer/BOOL/char/short/SEL/id/Class args —
 * the overwhelming majority of startup traffic — but NOT for:
 *
 *   - float / double / long double: SysV x86_64 passes these in XMM0-7, not
 *     GP regs. The plan struct + objc_msgSend.asm would need an xmm[8] block
 *     and `movsd`/`mov al,nxmm` before the call.
 *   - CGFloat: i386 CGFloat is a 4-byte float; x86_64 CGFloat is an 8-byte
 *     double. The x86_64 method encoding reports 'd' for BOTH a real double
 *     and a CGFloat, so the true source width must come from the translated
 *     binary's *legacy i386* __OBJC method `types` (see legacy metadata walk
 *     in _86x64_objc_register_classes), not method_copyArgumentType.
 *   - struct/union by value (NSPoint/NSSize/NSRect, etc.): need full SysV
 *     field classification (the same logic abigen/typeconv.cc already does
 *     for C functions) plus i386-float→x86_64-double element conversion.
 *   - 64-bit scalars (long long): occupy two i386 slots; only one is read.
 *   - >4 explicit args: spill to the stack above rsp (asm change).
 *
 * fill_method_args emits an OBJC_BRIDGE_TRACE warning when it sees an arg it
 * can't faithfully marshal, so these cases are observable rather than silent.
 * Closing this gap is the main remaining work for fully universal ObjC.
 * ===========================================================================
 *
 * objc_msgSend (and objc_msgSend_fpret):
 *   args32[0] = self (handle / class-name ptr / 0)
 *   args32[1] = _cmd (cstring ptr)
 *   args32[2..] = explicit args
 */
void x64_refresh_data_shadows(void);
/* Populate THIS copy's data-shadow table if its constructor never ran (see the
 * long comment at x64_populate_data_shadows). Idempotent and near-free. */
static void x64_ensure_data_shadows(void);

/* Forward objc_msgSend breadcrumb ring (dump fn defined later): records each
 * translated->native send with the i386 caller's return address (args32[-1] =
 * [rbp+8]) and the current stack pointer, plus a global low-water SP mark. The
 * return-to-0 crash is a frame whose pushed return address is 0; if recent
 * forward sends already show caller_ra==0 the corruption precedes them; a
 * marching-down min_sp means the shared low-4GB stack is overflowing. */
struct fwd_crumb { uint32_t self32; uint64_t sel; uint32_t caller_ra;
                   uint64_t sp; uint32_t tid; uint32_t valid; };
#define FWD_RING 128
static struct fwd_crumb g_fwd_ring[FWD_RING];
static uint32_t         g_fwd_pos;
static uint64_t         g_fwd_min_sp = ~0ULL;

/* ── NSFastEnumeration bridging ──────────────────────────────────────────────
 * `-[X countByEnumeratingWithState:objects:count:]` (the `for (x in coll)`
 * primitive) fills a caller-provided NSFastEnumerationState THROUGH A POINTER.
 * The i386 layout {ulong state; id*itemsPtr; ulong*mutationsPtr; ulong extra[5]}
 * has 4-byte fields; native Foundation writes the x86_64 layout (8-byte fields)
 * into it, so the i386 caller reads itemsPtr/mutationsPtr at the wrong offsets
 * AND truncates the 64-bit pointers -> wild deref (iPhoto library load:
 * recursivelyInitChildrenOfFolder: -> arrangedChildren -> countByEnumerating).
 * objc_msgSend is untyped, so the generic bridge can't catch this by type.
 * FIX: drive the real enumeration through a per-(thread,state-ptr) shadow x86_64
 * state, then publish results the i386 caller CAN read — wrap each returned
 * object into a 4-byte proxy handle inside the caller's `objects` buffer, point
 * i386 state.itemsPtr at that buffer, and give state.mutationsPtr a STABLE
 * low-4GB sentinel parked in the caller's own state.extra[] (a __thread shadow
 * address would be >4GB and re-truncate). Only for NATIVE collections — a legacy
 * reverse-registered collection uses i386 layout end-to-end, so the normal
 * passthrough already works for it. Universal: any i386 binary using fast enum. */
struct ns_fast_enum_state_x64 {     /* the real x86_64 NSFastEnumerationState */
   unsigned long  state;
   id            *itemsPtr;
   unsigned long *mutationsPtr;
   unsigned long  extra[5];
};
struct fe_shadow {
   uint32_t state32;        /* i386 state ptr this shadow serves (key)        */
   uint32_t inuse;
   struct ns_fast_enum_state_x64 ns;   /* the real x86_64 state native fills   */
   id       buf[64];        /* native object scratch (the objects: argument)   */
};
#define FE_SHADOW_MAX 16
static __thread struct fe_shadow g_fe_shadow[FE_SHADOW_MAX];

uint64_t x64_ret_identity(uint64_t v);
uint64_t x64_ret_identity(uint64_t v) { return v; }  /* plan.target: yield the precomputed count */

static struct fe_shadow *fe_shadow_for(uint32_t state32) {
   struct fe_shadow *freeslot = NULL;
   for (int i = 0; i < FE_SHADOW_MAX; ++i) {
      if (g_fe_shadow[i].inuse && g_fe_shadow[i].state32 == state32)
         return &g_fe_shadow[i];
      if (!g_fe_shadow[i].inuse && !freeslot) freeslot = &g_fe_shadow[i];
   }
   if (freeslot) { memset(freeslot, 0, sizeof(*freeslot));
                   freeslot->state32 = state32; freeslot->inuse = 1; }
   return freeslot;         /* NULL if >16 nested enumerations -> caller falls through */
}

/* Returns 1 if handled (plan set to return the count via x64_ret_identity); 0 to
 * fall through to the normal dispatch. */
static int bp_fast_enum(struct objc_call_plan *plan, const uint32_t *args32,
                        id real_self, SEL sel) {
   static SEL fe_sel;
   if (!fe_sel) fe_sel = sel_registerName("countByEnumeratingWithState:objects:count:");
   if (sel != fe_sel || !real_self) return 0;
   /* legacy reverse-registered collection: i386 layout end-to-end, normal path. */
   if (method_is_legacy(class_getInstanceMethod(object_getClass(real_self), sel)))
      return 0;

   uint32_t state32 = args32[2];        /* NSFastEnumerationState* (low-4GB)   */
   uint32_t buf32   = args32[3];        /* id objects[] buffer    (low-4GB)    */
   uint32_t cap     = args32[4];        /* count: capacity                     */
   if (!state32 || !buf32 || !cap) return 0;

   struct fe_shadow *sh = fe_shadow_for(state32);
   if (!sh) return 0;
   if (cap > 64) cap = 64;              /* clamp to the native scratch buffer  */

   typedef unsigned long (*fe_fn)(id, SEL, struct ns_fast_enum_state_x64 *,
                                  id *, unsigned long);
   unsigned long n = ((fe_fn)objc_msgSend)(real_self, sel, &sh->ns, sh->buf, cap);

   if (n == 0) {
      sh->inuse = 0;                    /* enumeration finished -> recycle slot */
   } else {
      uint32_t *dst   = (uint32_t *)(uintptr_t)buf32;        /* i386 objects[] */
      uint32_t *st    = (uint32_t *)(uintptr_t)state32;      /* i386 state     */
      id       *items = sh->ns.itemsPtr;
      for (unsigned long i = 0; i < n; ++i)
         dst[i] = items ? x64_objc_wrap((uint64_t)(uintptr_t)items[i]) : 0;
      /* Park the mutation sentinel in the caller's own state.extra[0] (low-4GB)
       * and point mutationsPtr at it — stable across batches so the compiler's
       * `*mutationsPtr != saved` guard never false-trips. */
      st[3] = sh->ns.mutationsPtr ? (uint32_t)*sh->ns.mutationsPtr : 0; /* extra[0] */
      st[0] = (uint32_t)sh->ns.state;                  /* state.state          */
      st[1] = buf32;                                   /* state.itemsPtr       */
      st[2] = state32 + 12;                            /* state.mutationsPtr -> &extra[0] */
   }

   plan->reg[0] = (uint64_t)n;
   plan->reg[1] = plan->reg[2] = plan->reg[3] = plan->reg[4] = plan->reg[5] = 0;
   plan->nreg = 1;
   plan->ret_is_obj = 0;                /* scalar: count travels in eax        */
   plan->target = (uint64_t)(uintptr_t)x64_ret_identity;
   return 1;
}

/* Defined in maptable_shim.m (an ObjC unit — objc_shim.c is plain C, so the
 * @try/@catch that swallows modern-AppKit's invalid-tag NSException cannot live
 * here). Does `[self sel:tag]` inside @try/@catch. */
extern void x64_safe_remove_rect(id self, SEL sel, long tag);

/* NSTrackingRectTag / NSToolTipTag round-trip (the remove side). These tags are
 * 64-bit NSIntegers on x86_64 but 32-bit on i386: -[NSView addTrackingRect:owner:
 * userData:assumeInside:] / -[NSView addToolTipRect:owner:userData:] RETURN a tag
 * the i386 caller stores in 32 bits and later hands back to -[NSView
 * removeTrackingRect:] / -[NSView removeToolTipRect:]. Truncating the 64-bit tag
 * to the caller's eax loses the high bits, so AppKit aborts ("0x0 is an invalid
 * NSTrackingRectTag ... Truncated the NSTrackingRectTag to 32bit"). objc_bridge_prep
 * therefore WRAPS the tag return into a low-4GB arena handle (return KIND 9, plain
 * x64_objc_wrap) — exactly as a 64-bit object/pointer is wrapped for i386. Here we
 * UNWRAP that handle back to the real 64-bit tag for the remove* call. This is
 * keyed on the selectors because the ObjC type encoding ERASES the tag typedef
 * (it is plain 'q', indistinguishable from a numeric NSInteger), so only the API
 * contract identifies a token — the same reason bp_fast_enum keys on
 * countByEnumeratingWithState:. Universal: any app using NSView tracking/tooltip
 * rects. */
static int bp_track_tag(struct objc_call_plan *plan, const uint32_t *args32,
                        id real_self, SEL sel) {
   static SEL rm_track, rm_tip;
   if (!rm_track) {
      rm_track = sel_registerName("removeTrackingRect:");
      rm_tip   = sel_registerName("removeToolTipRect:");
   }
   if (!real_self || (sel != rm_track && sel != rm_tip)) return 0;
   /* A legacy reverse-registered view OVERRIDING remove* keeps i386 tags
    * end-to-end -> normal (legacy) path; don't intercept. */
   if (method_is_legacy(class_getInstanceMethod(object_getClass(real_self), sel)))
      return 0;
   long tag = (long)x64_objc_unwrap(args32[2]);   /* 32-bit handle -> real 64-bit tag */
   if (getenv("ABICONV_TAG_TRACE")) {
      fprintf(stderr, "[tag] remove sel=%s handle=0x%x -> tag=0x%lx self=%p\n",
              sel_getName(sel), args32[2], (unsigned long)tag, (void *)real_self);
      fflush(stderr);
   }
   /* Old macOS (the i386 era) silently IGNORED removeTrackingRect:/removeToolTipRect:
    * with an invalid tag; tag 0 = "none", the uninitialized-ivar "remove the old
    * one before adding a new one" idiom legacy views use. Modern AppKit instead
    * THROWS NSInternalInconsistencyException ("0x0 is an invalid NSTrackingRectTag"),
    * which aborts the app before it ever reaches the matching addTrackingRect:.
    * No-op the 0 tag to restore the leniency the legacy code depends on. Nonzero
    * tags round-trip through the arena (wrapped on the add* return, kind 9). */
   if (tag != 0) {
      /* Old macOS silently ignored removeTrackingRect:/removeToolTipRect: for a
       * STALE/already-removed tag too (not only tag 0): the "remove the old one
       * before adding a new one" idiom keeps a tag whose tracking rect AppKit may
       * have already discarded on a window change / view teardown / re-layout.
       * Modern AppKit instead THROWS NSInternalInconsistencyException
       * ("0x... is an invalid NSTrackingRectTag"), aborting the whole app
       * (observed: iPhoto startup, tag 0x600003e95040 = a gone _NSTrackingArea-
       * AKViewHelper). x64_safe_remove_rect (maptable_shim.m — this file is plain
       * C, the @try/@catch must live in an ObjC unit) swallows that ONE exception
       * to restore the legacy leniency — a no-op remove of an already-gone tag is
       * exactly what the i386 code expects. Dead/invalid surface must DEGRADE, not
       * crash. The throw unwinds only NATIVE frames (AppKit -> here), so no
       * reverse-IMP frame / rsp-stash entry is abandoned. Universal: any i386 app
       * removing tracking/tooltip rects across a view/window lifecycle. */
      x64_safe_remove_rect(real_self, sel, (long)tag);
   }
   plan->reg[0] = plan->reg[1] = plan->reg[2] = 0;
   plan->reg[3] = plan->reg[4] = plan->reg[5] = 0;
   plan->nreg = 1;
   plan->ret_is_obj = 0;                            /* void return */
   plan->target = (uint64_t)(uintptr_t)x64_ret_identity;
   return 1;
}

/* Forward decl: answer conformsToProtocol: from a legacy class's OWN i386
 * __OBJC protocol metadata (defined after the legacy registry below). */
static int legacy_class_conforms(const char *clsname, const char *qname, int depth);

/* conformsToProtocol: with a LEGACY i386 Protocol argument. The app's own
 * @protocol(...) objects live in its i386 __OBJC with the fragile-ABI layout
 * { Class isa; const char *name; struct protocol_list*; method_desc_list*
 *   instance_methods, *class_methods } — name at +4 — NOT the modern opaque
 * Protocol object. Passing one straight to native -[NSObject conformsToProtocol:]
 * makes libobjc strcmp a garbage name pointer -> crash (iPhoto AlbumView/AVSection
 * data-source wiring). Resolve it to the modern Protocol by NAME
 * (objc_getProtocol) and ask natively.
 *
 * A reverse-registered legacy class adopts its protocols (interface- AND
 * category-declared) only in its OWN i386 __OBJC metadata — reverse_register_one
 * never tells libobjc about them — so the native conformsToProtocol: returns NO
 * for any app-declared protocol. When the native answer is NO we therefore fall
 * back to legacy_class_conforms(), which walks the receiver's legacy class +
 * its categories + legacy-super chain protocol lists BY NAME. This makes
 * conformance to the app's own protocols (the common case) report YES while
 * still preferring the authoritative native answer (e.g. NSObject protocol,
 * a genuinely native-adopted protocol) when it says YES. A protocol passed as
 * an arena handle (a wrapped native Protocol) unwraps directly. Universal: any
 * i386 app calling conformsToProtocol:. Keyed on the selector. */
static int bp_conforms_protocol(struct objc_call_plan *plan, const uint32_t *args32,
                                id real_self, SEL sel) {
   static SEL s_conforms;
   if (!s_conforms) s_conforms = sel_registerName("conformsToProtocol:");
   if (sel != s_conforms) return 0;
   /* A legacy class overriding conformsToProtocol: keeps the i386 path. */
   if (real_self &&
       method_is_legacy(class_getInstanceMethod(object_getClass(real_self), sel)))
      return 0;

   uint32_t prot32 = args32[2];
   Protocol *p = NULL;
   const char *qname = NULL;                         /* queried protocol NAME */
   uint64_t r = x64_objc_unwrap(prot32);            /* arena handle -> native ptr */
   if (r > 0xFFFFFFFFULL) {
      p = (Protocol *)(uintptr_t)r;                 /* already a modern Protocol */
      qname = protocol_getName(p);
   } else if (prot32 && mem_readable((uintptr_t)prot32 + 4, 4)) {
      uint32_t name_ptr = *(const uint32_t *)(uintptr_t)(prot32 + 4);
      if (name_ptr && mem_readable(name_ptr, 1)) {
         qname = (const char *)(uintptr_t)name_ptr;
         p = objc_getProtocol(qname);
      }
   }

   int conforms = 0;
   if (p && real_self)
      conforms = ((BOOL (*)(id, SEL, Protocol *))objc_msgSend)(real_self, sel, p) ? 1 : 0;

   /* libobjc said NO (or never heard of the protocol) — consult the receiver's
    * own legacy __OBJC adoption list by name. */
   if (!conforms && qname && real_self) {
      Class c = object_getClass(real_self);
      const char *cn = c ? class_getName(c) : NULL;
      if (cn && legacy_class_conforms(cn, qname, 0))
         conforms = 1;
   }

   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[bp] conformsToProtocol: prot32=0x%x -> %p (%s) = %d\n",
              prot32, (void *)p, qname ? qname : "(unresolved)", conforms);
      fflush(stderr);
   }
   plan->nreg = 1;
   plan->reg[0] = (uint64_t)conforms;
   plan->reg[1] = plan->reg[2] = plan->reg[3] = plan->reg[4] = plan->reg[5] = 0;
   plan->ret_is_obj = 0;
   plan->target = (uint64_t)(uintptr_t)x64_ret_identity;
   return 1;
}

/* -[NSInvocation setArgument:atIndex:] / getArgument:atIndex: /
 * setReturnValue: / getReturnValue: carry an OPAQUE `void*` buffer whose
 * interpretation comes from the invocation's method SIGNATURE, so the generic
 * arg marshaller sees only `^v` and passes the raw i386 buffer through. For a
 * POINTER-typed slot ('@','#',':','*','^') that is wrong in both directions:
 *   - set: the i386 buffer holds a 4-byte i386 value (arena handle / legacy
 *     object / i386 SEL / i386 pointer); native NSInvocation memcpys the
 *     slot's native size (8 bytes — also over-reading 4 bytes past the
 *     caller's slot) into its frame and later USES the value natively:
 *     -retainArguments retains it, -invoke passes it to the target IMP. A raw
 *     arena handle left in an '@' slot makes libobjc read the arena SLOT as
 *     the object's isa -> garbage "Class" -> crash. (Quinn's reflection
 *     invocation, -[QuinnBoardView didDrawInRect:]: the snapshot NSImage's
 *     handle 0x8000b560 was retained by -retainArguments; isa = arena slot =
 *     the real NSImage ptr; the fake Class's cache pointer = the image's
 *     _size.width = 208.0 -> SIGSEGV at 0x406a000000000000, the IEEE-754
 *     bits of 208.0.)
 *   - get: the 8-byte native value would be written over / truncated into
 *     the caller's 4-byte i386 slot.
 * Translate the value through the SAME converters the typed arg marshaller
 * uses (unwrap_obj_arg / conv_sel_arg / x64_objc_unwrap on set; wrap-if-high
 * on get). Scalar and struct slots pass through untouched: NSInvocation
 * copies by the signature's own size, and a legacy-registered method's
 * signature keeps i386 widths (e.g. a float NSRect stays 16 bytes), so the
 * raw buffer already matches. Triggers on the NSInvocation selector shape +
 * receiver class, never the app. */
static int bp_invocation_arg(struct objc_call_plan *plan, const uint32_t *args32,
                             id real_self, SEL sel) {
   static SEL s_seta, s_geta, s_setr, s_getr;
   if (!s_seta) {
      s_seta = sel_registerName("setArgument:atIndex:");
      s_geta = sel_registerName("getArgument:atIndex:");
      s_setr = sel_registerName("setReturnValue:");
      s_getr = sel_registerName("getReturnValue:");
   }
   const int is_set  = (sel == s_seta), is_get  = (sel == s_geta);
   const int is_sret = (sel == s_setr), is_gret = (sel == s_getr);
   if (!is_set && !is_get && !is_sret && !is_gret) { return 0; }
   if (!real_self) { return 0; }
   /* receiver must be an NSInvocation (walk the chain; no msgSend needed) */
   Class inv_cls = objc_getClass("NSInvocation");
   int is_inv = 0;
   for (Class c = object_getClass(real_self); c; c = class_getSuperclass(c)) {
      if (c == inv_cls) { is_inv = 1; break; }
   }
   if (!is_inv) { return 0; }
   /* a legacy override keeps the i386 path end-to-end */
   if (method_is_legacy(class_getInstanceMethod(object_getClass(real_self), sel)))
      return 0;

   const uint32_t buf32 = args32[2];
   if (!buf32) { return 0; }                 /* let native raise, as before */
   const uint32_t idx = (is_set || is_get) ? args32[3] : 0;

   /* the slot's type comes from the invocation's own signature */
   id sig = ((id (*)(id, SEL))objc_msgSend)(
      real_self, sel_registerName("methodSignature"));
   if (!sig) { return 0; }
   const char *enc = NULL;
   if (is_set || is_get) {
      unsigned long nargs = ((unsigned long (*)(id, SEL))objc_msgSend)(
         sig, sel_registerName("numberOfArguments"));
      if ((unsigned long)idx >= nargs) { return 0; }   /* native throws */
      enc = ((const char *(*)(id, SEL, unsigned long))objc_msgSend)(
         sig, sel_registerName("getArgumentTypeAtIndex:"), (unsigned long)idx);
   } else {
      enc = ((const char *(*)(id, SEL))objc_msgSend)(
         sig, sel_registerName("methodReturnType"));
   }
   if (!enc) { return 0; }
   const char b = *enc_skip_quals(enc);
   /* pointer-shaped slots only; every other type's bytes already match the
    * signature-declared width on both sides */
   if (!(b == '@' || b == '#' || b == ':' || b == '*' || b == '^')) { return 0; }

   const int trace = BRIDGE_TRACE();
   uint64_t tmp = 0;
   if (is_set || is_sret) {
      const uint32_t v32 = *(const uint32_t *)(uintptr_t)buf32;
      if (b == '@' || b == '#') { tmp = unwrap_obj_arg(v32); }
      else if (b == ':')        { tmp = (uint64_t)(uintptr_t)conv_sel_arg(v32); }
      else                      { tmp = x64_objc_unwrap(v32); }  /* '*'/'^':
                                    identity for genuine i386 pointers,
                                    unwraps a ^v-wrapped 64-bit token */
      if (is_set) {
         ((void (*)(id, SEL, void *, unsigned long))objc_msgSend)(
            real_self, sel, &tmp, (unsigned long)idx);
      } else {
         ((void (*)(id, SEL, void *))objc_msgSend)(real_self, sel, &tmp);
      }
      if (trace) {
         fprintf(stderr, "[bp] invocation %s idx=%u enc=\"%s\" "
                 "0x%08x -> 0x%llx\n", sel_getName(sel), idx, enc, v32,
                 (unsigned long long)tmp);
         fflush(stderr);
      }
   } else {
      if (is_get) {
         ((void (*)(id, SEL, void *, unsigned long))objc_msgSend)(
            real_self, sel, &tmp, (unsigned long)idx);
      } else {
         ((void (*)(id, SEL, void *))objc_msgSend)(real_self, sel, &tmp);
      }
      uint32_t out;
      if (tmp == 0) {
         out = 0;
      } else if (b == ':') {
         /* a real SEL is a C-string pointer; bounce a >4GB one to a low copy
          * of its name (legacy sel handling matches by name) */
         out = (tmp < 0x100000000ULL)
                  ? (uint32_t)tmp
                  : x64_objc_bounce_cstr(sel_getName((SEL)(uintptr_t)tmp));
      } else if (tmp < 0x100000000ULL) {
         out = (uint32_t)tmp;
      } else {
         out = x64_objc_wrap(tmp);
      }
      *(uint32_t *)(uintptr_t)buf32 = out;
      if (trace) {
         fprintf(stderr, "[bp] invocation %s idx=%u enc=\"%s\" "
                 "0x%llx -> 0x%08x\n", sel_getName(sel), idx, enc,
                 (unsigned long long)tmp, out);
         fflush(stderr);
      }
   }
   plan->reg[0] = plan->reg[1] = plan->reg[2] = 0;
   plan->reg[3] = plan->reg[4] = plan->reg[5] = 0;
   plan->nreg = 1;
   plan->ret_is_obj = 0;                     /* void return */
   plan->target = (uint64_t)(uintptr_t)x64_ret_identity;
   return 1;
}

/* plan.target helper: returns its first argument verbatim. bp_block_copy puts
 * the precomputed i386 result in reg[0] and points plan.target here so the
 * msgSend asm returns it (ret_kind 0 = scalar) without calling objc_msgSend. */
static uint64_t x64_blk_ret_identity(uint64_t v) { return v; }

/* [block copy]/copyWithZone:/mutableCopy/retain/self/autorelease/release/dealloc
 * sent to an i386 block. Native -[NSBlock copy] (= _Block_copy) would misread
 * the i386 Block_layout (invoke@12/descriptor@16, 4-byte fields) and crash; and
 * the result MUST stay an i386-layout block so the translated code can keep
 * invoking it via the i386 layout. So handle these inline: copy -> i386 heap
 * copy (low-4GB); retain/self/autorelease -> same pointer; release/dealloc ->
 * no-op. Returns 1 (handled, result placed in the plan) or 0 (fall through).
 * Triggers on the block's structural isa + the selector name, never the app. */
static int bp_block_copy(struct objc_call_plan *plan, const uint32_t *args32,
                         SEL sel) {
   if (!sel) { return 0; }
   uint32_t p = args32[0];
   if (!i386_block_kind(p)) { return 0; }
   const char *s = sel_getName(sel);
   uint32_t result;
   if (!strcmp(s, "copy") || !strcmp(s, "copyWithZone:") ||
       !strcmp(s, "mutableCopy") || !strcmp(s, "mutableCopyWithZone:")) {
      result = i386_block_copy(p);
   } else if (!strcmp(s, "retain") || !strcmp(s, "self") ||
              !strcmp(s, "autorelease")) {
      result = p;
   } else if (!strcmp(s, "release") || !strcmp(s, "dealloc")) {
      result = 0;                          /* void; i386 caller ignores eax */
   } else {
      return 0;                            /* other selectors: normal dispatch */
   }
   plan->reg[0]     = result;
   plan->target     = (uint64_t)(uintptr_t)&x64_blk_ret_identity;
   plan->ret_is_obj = 0;                   /* scalar passthrough: eax = result */
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[bp] block %s on 0x%08x -> 0x%08x\n", s, p, result);
      fflush(stderr);
   }
   return 1;
}

/* -[NSData bytes] returns `const void *` (encoding `r^v`) into the data's buffer.
 * For a NATIVE NSData (e.g. from [[NSUserDefaults standardUserDefaults]
 * dataForKey:]) that buffer lives at a >4GB native address the i386 caller
 * cannot hold: the bare-`^v` return wrap (commit 8aab6e4, needed for the
 * graphicsPort/CGContextRef opaque-token family) turns it into a low-4GB ARENA
 * HANDLE — correct for an opaque token passed BACK to native, but WRONG for a
 * raw data buffer the app DEREFERENCES directly. Quinn's `ks_decrypt` reads the
 * encrypted-highscore bytes straight off `[data bytes]`; given the handle it
 * reads the arena slot as ciphertext -> garbage plaintext -> a corrupt
 * length-prefix -> malloc(huge)=NULL -> memcpy(NULL) -> SIGSEGV at 0x0 in
 * -[QuinnHighscoreDB read] on play. (Even WITHOUT the wrap the >4GB pointer
 * truncates to eax -> equally unreadable; the bug is fundamental to `bytes`.)
 *
 * Fix: intercept `-[NSData bytes]` and return a LOW-4GB COPY of the bytes.
 * libabiconv is native x86_64 so it can read the >4GB buffer directly; `malloc`
 * here is the low-4GB shim heap. The low copy is cached on the receiver via an
 * associated pointer (ASSIGN) so repeated `bytes` calls return the SAME low
 * pointer (the documented read-lifetime/identity contract). The copy itself is
 * never freed — one small leak per distinct NSData whose bytes are read, exactly
 * like x64_cstr_ret_low's high->low bounce (a freeWhenDone wrapper would hand the
 * shim-malloc'd buffer to native free on dealloc and abort). An NSData whose
 * buffer is ALREADY <4GB (an i386-side dataWithBytesNoCopy:) passes through
 * uncopied. Triggers on the NSData class + `bytes` selector, never the app.
 * (mutableBytes is intentionally NOT handled — it needs write-back; no current
 * target writes through it. See todo_gaps.) */
static const char nsdata_lowbytes_key;
static int bp_nsdata_bytes(struct objc_call_plan *plan, id real_self, SEL sel) {
   if (!sel || !real_self) { return 0; }
   /* The raw-buffer accessors that return a >4GB native pointer the app then
    * DEREFERENCES: -[NSData bytes] (raw C, Quinn ks_decrypt) and
    * -[NSBitmapImageRep bitmapData] (fed to glTexImage2D -> the Quinn splash GL
    * logo texture; encoding `^C`, so the 8aab6e4 `^v` wrap doesn't even catch it
    * and it truncates to eax -> a garbage/black texture). Same low-copy cure,
    * sized by the matching length accessor. */
   const char *s = sel_getName(sel);
   const char *clsname, *sizesel;
   if      (!strcmp(s, "bytes"))      { clsname = "NSData";           sizesel = "length"; }
   else if (!strcmp(s, "bitmapData")) { clsname = "NSBitmapImageRep"; sizesel = "bytesPerPlane"; }
   else { return 0; }
   if (getenv("ABICONV_GL_TEXLOG") && !strcmp(s, "bitmapData")) {
      fprintf(stderr, "[bmp] HOOK REACHED self=%p<%s>\n", (void*)real_self,
              real_self ? object_getClassName(real_self) : "(nil)");
      fflush(stderr);
   }
   Class cls = objc_getClass(clsname);
   if (!cls) { return 0; }
   typedef unsigned char (* msg_kind_t)(id, SEL, Class);
   typedef const void *  (*msg_ptr_t)(id, SEL);
   typedef unsigned long (*msg_len_t)(id, SEL);
   if (!((msg_kind_t)objc_msgSend)(real_self, sel_registerName("isKindOfClass:"),
                                   cls)) {
      return 0;                              /* receiver isn't that class: app sel */
   }
   /* `bytes` is cached (the app may hold the returned pointer; -[NSData bytes]
    * is immutable so the copy stays valid). `bitmapData` is NOT cached: it is
    * read once per glTexImage2D and the bitmap may be FILLED after an earlier
    * read, so a cached early (empty) copy would upload a blank texture; a fresh
    * copy reflects the current pixels (glTexImage2D copies immediately, so no
    * held-pointer contract). */
   int do_cache = !strcmp(s, "bytes");
   const void *low;
   id cached = do_cache ? objc_getAssociatedObject(real_self, &nsdata_lowbytes_key)
                        : (id)0;
   if (cached) {
      low = (const void *)cached;            /* cached low copy (ASSIGN: opaque ptr) */
   } else {
      const void *nb = ((msg_ptr_t)objc_msgSend)(real_self, sel);
      unsigned long len = ((msg_len_t)objc_msgSend)(real_self,
                                                    sel_registerName(sizesel));
      if (getenv("ABICONV_GL_TEXLOG") && !strcmp(s, "bitmapData")) {
         /* Diagnostic: native buffer, size accessor sanity, row distribution
          * of non-zero content in the NATIVE buffer (before any copy). */
         unsigned long bpr = ((msg_len_t)objc_msgSend)(real_self, sel_registerName("bytesPerRow"));
         unsigned long pw  = ((msg_len_t)objc_msgSend)(real_self, sel_registerName("pixelsWide"));
         unsigned long ph  = ((msg_len_t)objc_msgSend)(real_self, sel_registerName("pixelsHigh"));
         unsigned long bpp = ((msg_len_t)objc_msgSend)(real_self, sel_registerName("bitsPerPixel"));
         long r0=-1, r1=-1, nrows=0;
         if (nb && bpr && ph && len >= bpr) {
            const unsigned char *p = (const unsigned char*)nb;
            unsigned long hh = len / bpr; if (hh > ph) hh = ph;
            for (unsigned long y = 0; y < hh; y++) {
               const unsigned char *row = p + y*bpr;
               int rn = 0;
               for (unsigned long x = 0; x < bpr; x++) if (row[x]) { rn = 1; break; }
               if (rn) { if (r0 < 0) r0 = (long)y; r1 = (long)y; nrows++; }
            }
         }
         fprintf(stderr, "[bmp] bitmapData self=%p<%s> nb=%p len(bytesPerPlane)=%lu bpr=%lu %lux%lu bpp=%lu nzrows=[%ld..%ld]n=%ld\n",
                 (void*)real_self, object_getClassName(real_self), nb, len,
                 bpr, pw, ph, bpp, r0, r1, (long)nrows);
         fflush(stderr);
      }
      if ((uintptr_t)nb < 0x100000000ULL || !nb) {
         low = nb;                           /* already low-4GB (or nil): pass through */
      } else {
         void *lb = malloc(len ? len : 1);   /* low-4GB shim heap */
         if (!lb || (uintptr_t)lb >= 0x100000000ULL) { return 0; }  /* no low mem */
         memcpy(lb, nb, len);
         if (do_cache) {
            objc_setAssociatedObject(real_self, &nsdata_lowbytes_key, (id)lb,
                                     OBJC_ASSOCIATION_ASSIGN);
         }
         low = lb;
      }
   }
   plan->reg[0]     = (uint32_t)(uintptr_t)low;
   plan->target     = (uint64_t)(uintptr_t)&x64_blk_ret_identity;
   plan->ret_is_obj = 0;                     /* scalar passthrough: eax = low ptr */
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[bp] %s -> low 0x%08x\n", s, plan->reg[0]);
      fflush(stderr);
   }
   return 1;
}

/* -[NSString fileSystemRepresentation] / getFileSystemRepresentation:maxLength:
 * intermittently (~2-7%, heap/thread-timing dependent) SIGSEGVs at addr=0x8 in
 * modern Foundation: the fsrep path builds a VM-backed NSData
 * (initWithBytes:length:copy:freeWhenDone:bytesAreVM:) whose -[NSConcreteData
 * bytes] pthread_mutex_lock()s a mutex at a NULL base — iPhoto's account-config
 * background thread (createDir: path cleanup) at boot / opening Preferences.
 * The redirect of the deprecated removeFileAtPath:handler: (bp_deprecated_
 * removefile) did NOT cure it: the crashing fileSystemRepresentation is called
 * DIRECTLY by the app (and removeItemAtPath:error: hits the same path).
 *
 * Satisfy fileSystemRepresentation ourselves via getCString:...:NSUTF8String-
 * Encoding, which never enters the VM-NSData machinery, so the null-lock path is
 * never reached. For paths this is byte-equivalent to fileSystemRepresentation
 * (UTF-8; the only difference is HFS NFD decomposition of non-ASCII, immaterial
 * to the app's path use). Universal: any i386 caller. The returned C string
 * pointer must be low-4GB (i386-derefs it) and live until the autorelease pool
 * drains — a per-thread ring of grow-on-demand low-4GB buffers gives exactly
 * that lifetime (valid for the next FSREP_RING calls on the thread, matching the
 * "copy it if it must persist past the current pool" contract) with no per-call
 * leak. getFileSystemRepresentation:maxLength: fills the caller's OWN buffer, so
 * no ring slot is needed there. */
#define FSREP_RING 16
struct fsrep_slot { char *buf; size_t cap; };
static __thread struct fsrep_slot g_fsrep_ring[FSREP_RING];
static __thread unsigned g_fsrep_pos;
static char *fsrep_low_buf(size_t need) {
   if (need == 0) { need = 1; }
   struct fsrep_slot *sl = &g_fsrep_ring[g_fsrep_pos];
   g_fsrep_pos = (g_fsrep_pos + 1u) % FSREP_RING;
   if (sl->cap < need) {
      char *nb = (char *)malloc(need);                 /* low-4GB shim heap */
      if (!nb) { return NULL; }
      if ((uintptr_t)nb >= 0x100000000ULL) { free(nb); return NULL; }
      free(sl->buf);
      sl->buf = nb; sl->cap = need;
   }
   return sl->buf;
}
static int bp_fsrep(struct objc_call_plan *plan, const uint32_t *args32,
                    id real_self, SEL sel) {
   if (!sel || !real_self) { return 0; }
   const char *s = sel_getName(sel);
   int is_get = (strcmp(s, "getFileSystemRepresentation:maxLength:") == 0);
   int is_fsr = !is_get && (strcmp(s, "fileSystemRepresentation") == 0);
   if (!is_fsr && !is_get) { return 0; }
   Class nsstr = objc_getClass("NSString");
   typedef unsigned char (*kind_t)(id, SEL, Class);
   if (!nsstr || !((kind_t)objc_msgSend)(real_self,
                     sel_registerName("isKindOfClass:"), nsstr)) {
      return 0;                                   /* receiver isn't an NSString */
   }
   const unsigned long NSUTF8 = 4;                /* NSUTF8StringEncoding */
   SEL gc = sel_registerName("getCString:maxLength:encoding:");
   typedef unsigned char (*getc_t)(id, SEL, char *, unsigned long, unsigned long);

   if (is_get) {
      /* getFileSystemRepresentation:(char *)buffer maxLength:(NSUInteger) -> BOOL,
       * filling the caller's OWN i386 low-4GB buffer. */
      char *buf = (char *)(uintptr_t)args32[2];
      unsigned long maxLen = (unsigned long)args32[3];
      if (!buf || maxLen == 0) { return 0; }       /* let native handle the edge */
      unsigned char ok = ((getc_t)objc_msgSend)(real_self, gc, buf, maxLen, NSUTF8);
      plan->reg[0] = (uint64_t)(unsigned char)ok;
      plan->reg[1] = plan->reg[2] = plan->reg[3] = plan->reg[4] = plan->reg[5] = 0;
      plan->nreg = 1;
      plan->ret_is_obj = 0;
      plan->target = (uint64_t)(uintptr_t)x64_ret_identity;
      if (BRIDGE_TRACE()) {
         fprintf(stderr, "[bp] getFileSystemRepresentation:maxLength: -> getCString ok=%d\n",
                 (int)ok);
         fflush(stderr);
      }
      return 1;
   }

   /* fileSystemRepresentation -> const char *. Size to the exact UTF-8 max. */
   typedef unsigned long (*maxlen_t)(id, SEL, unsigned long);
   unsigned long cap = ((maxlen_t)objc_msgSend)(real_self,
                          sel_registerName("maximumLengthOfBytesUsingEncoding:"),
                          NSUTF8) + 1;               /* +1 for the NUL */
   char *buf = fsrep_low_buf((size_t)cap);
   if (!buf) { return 0; }                          /* no low mem: let native try */
   if (!((getc_t)objc_msgSend)(real_self, gc, buf, cap, NSUTF8)) {
      buf[0] = '\0';                                /* getCString shouldn't fail at max cap */
   }
   plan->reg[0]     = (uint32_t)(uintptr_t)buf;     /* low-4GB C-string ptr in eax */
   plan->target     = (uint64_t)(uintptr_t)&x64_blk_ret_identity;
   plan->ret_is_obj = 0;
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[bp] fileSystemRepresentation -> low 0x%08x \"%.64s\"\n",
              (uint32_t)(uintptr_t)buf, buf);
      fflush(stderr);
   }
   return 1;
}

/* -[NSView getRectsBeingDrawn:count:] hands the caller, via two out-parameters,
 * a pointer to an AppKit-owned array of NSRect plus a count (the dirty-rect
 * optimisation an app's drawRect: uses to redraw only invalidated cells). Two
 * things break the i386 caller on x86_64:
 *   1. the `const NSRect **` AppKit writes is a >4GB native address, but the
 *      i386 caller stores it into a 4-byte stack slot -> truncation to a low
 *      garbage address it then dereferences (Quinn: fault 0x760730 in
 *      -[QuinnLocalBoardView drawRect:], the low-32 of a 0x6000_0076_0730 native
 *      rect array).
 *   2. NSRect is 4xCGFloat: 16 bytes (float) on i386 but 32 bytes (double) on
 *      x86_64, so even an untruncated pointer would index a 32-byte-stride,
 *      double-field array as if it were 16-byte float rects.
 * Fix: call native with our OWN 64-bit out-buffers, then materialise a low-4GB
 * array of i386-layout (float) NSRects and write its 32-bit address + a 32-bit
 * count into the i386 out-param slots. The low buffer is cached (grow-only) on
 * the receiver: the returned array is valid only for the current drawRect:, so a
 * per-view reused buffer honours the lifetime contract and avoids a per-frame
 * leak. Universal: triggers on the standard AppKit selector, not the app. */
struct grect_i386 { float x, y, w, h; };
struct grect_buf  { uint64_t cap; struct grect_i386 rects[]; };
static const char nsview_getrects_key;
static int bp_getrects(struct objc_call_plan *plan, const uint32_t *args32,
                       id real_self, SEL sel) {
   if (!sel || !real_self) { return 0; }
   if (strcmp(sel_getName(sel), "getRectsBeingDrawn:count:")) { return 0; }
   Class nsview = objc_getClass("NSView");
   if (!nsview) { return 0; }
   typedef unsigned char (*msg_kind_t)(id, SEL, Class);
   if (!((msg_kind_t)objc_msgSend)(real_self, sel_registerName("isKindOfClass:"),
                                   nsview)) {
      return 0;                              /* not an NSView: app selector */
   }
   /* native call with our own 64-bit buffers (never the i386 4-byte slots) */
   const CGRect *nrects = NULL;
   long ncount = 0;
   typedef void (*grd_t)(id, SEL, const CGRect **, long *);
   ((grd_t)objc_msgSend)(real_self, sel, &nrects, &ncount);
   if (ncount < 0) { ncount = 0; }
   /* grow-only low-4GB buffer cached on the receiver */
   struct grect_buf *b =
      (struct grect_buf *)objc_getAssociatedObject(real_self, &nsview_getrects_key);
   if (!b || b->cap < (uint64_t)ncount) {
      size_t n = ncount ? (size_t)ncount : 1;
      struct grect_buf *nb =
         malloc(sizeof(struct grect_buf) + n * sizeof(struct grect_i386));
      if (!nb || (uintptr_t)nb >= 0x100000000ULL) { return 0; }  /* no low mem */
      nb->cap = n;
      objc_setAssociatedObject(real_self, &nsview_getrects_key, (id)nb,
                               OBJC_ASSOCIATION_ASSIGN);
      b = nb;
   }
   for (long i = 0; i < ncount; ++i) {
      b->rects[i].x = (float)nrects[i].origin.x;
      b->rects[i].y = (float)nrects[i].origin.y;
      b->rects[i].w = (float)nrects[i].size.width;
      b->rects[i].h = (float)nrects[i].size.height;
   }
   /* write the i386-layout results into the caller's out-param slots (each a
    * 4-byte i386 stack slot addressed by args32[2] / args32[3]) */
   if (args32[2]) { *(uint32_t *)(uintptr_t)args32[2] = (uint32_t)(uintptr_t)b->rects; }
   if (args32[3]) { *(uint32_t *)(uintptr_t)args32[3] = (uint32_t)ncount; }
   plan->reg[0]     = 0;                      /* void return */
   plan->target     = (uint64_t)(uintptr_t)&x64_blk_ret_identity;
   plan->ret_is_obj = 0;
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[bp] getRectsBeingDrawn:count: -> %ld rects @ low 0x%08x\n",
              ncount, (uint32_t)(uintptr_t)b->rects);
      fflush(stderr);
   }
   return 1;
}

/* +[NSData dataWithBytesNoCopy:length:(freeWhenDone:)] / -[NSData
 * initWithBytesNoCopy:length:(freeWhenDone:)] with freeWhenDone=YES hands the
 * NSData OWNERSHIP of the buffer: the NSData's -dealloc calls free() on it. When
 * i386 code passes a buffer it `malloc`'d (Quinn's decrypt_bytes returns a
 * malloc'd plaintext that -[QuinnHighscoreDB read] wraps via dataWithBytesNoCopy:
 * to feed unarchiveObjectWithData:), that buffer came from the low-4GB SHIM heap;
 * Foundation's native free() does not own it -> malloc_report ABORT at pool drain
 * (-[NSConcreteData dealloc], crashlog Quinn-2026-06-30-020609.ips).
 *
 * Fix: redirect the NoCopy creators to their COPYING form (dataWithBytes:length:
 * / initWithBytes:length:) so Foundation copies into its OWN native buffer that
 * native free() can release. The original shim buffer is left unfreed — the app
 * transferred ownership via freeWhenDone:YES so it will not free it either, and
 * native must never free shim memory (a bounded leak, like bp_nsdata_bytes /
 * x64_cstr_ret_low). freeWhenDone:NO is passed through untouched (the NSData
 * never frees the buffer, so there is no abort). Triggers on the NoCopy selector
 * + an NSData/NSMutableData receiver, never the app. */
static int bp_nsdata_nocopy(struct objc_call_plan *plan, const uint32_t *args32,
                            id real_self, SEL sel) {
   if (!sel || !real_self) { return 0; }
   const char *s = sel_getName(sel);
   const char *repl;
   int five;
   if      (!strcmp(s, "dataWithBytesNoCopy:length:"))              { repl = "dataWithBytes:length:"; five = 0; }
   else if (!strcmp(s, "dataWithBytesNoCopy:length:freeWhenDone:")) { repl = "dataWithBytes:length:"; five = 1; }
   else if (!strcmp(s, "initWithBytesNoCopy:length:"))             { repl = "initWithBytes:length:"; five = 0; }
   else if (!strcmp(s, "initWithBytesNoCopy:length:freeWhenDone:")){ repl = "initWithBytes:length:"; five = 1; }
   else { return 0; }
   uint32_t fwd = five ? args32[4] : 1;          /* 2-arg NoCopy defaults YES */
   if (!fwd) { return 0; }                       /* freeWhenDone:NO: never freed -> safe */
   SEL rsel = sel_registerName(repl);
   if (!class_respondsToSelector(object_getClass(real_self), rsel)) {
      return 0;                                  /* not NSData/NSMutableData */
   }
   const void  *buf = (const void *)(uintptr_t)args32[2];
   unsigned long len = (unsigned long)args32[3];
   typedef id (*msg_copy_t)(id, SEL, const void *, unsigned long);
   id result = ((msg_copy_t)objc_msgSend)(real_self, rsel, buf, len);
   plan->reg[0]     = result ? x64_objc_wrap((uint64_t)(uintptr_t)result) : 0;
   plan->target     = (uint64_t)(uintptr_t)&x64_blk_ret_identity;
   plan->ret_is_obj = 0;                         /* already a low-4GB handle */
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[bp] NSData %s -> native copy (len=%lu)\n", s, len);
      fflush(stderr);
   }
   return 1;
}

/* Deprecated -[NSFileManager removeFileAtPath:handler:] (Mac OS X 10.0-era,
 * still called by iPhoto's account-config path cleanup on a background thread)
 * crashes in modern Foundation: its compat impl (_removeFileAtPath:handler:
 * shouldDeleteFork:) runs -[NSString fileSystemRepresentation], which builds a
 * VM-backed NSData (initWithBytes:length:copy:freeWhenDone:bytesAreVM:) whose
 * -[NSConcreteData bytes] then pthread_mutex_lock()s a mutex at a NULL base ->
 * SIGSEGV addr=0x8 (repro'd ~8% on boot / opening Preferences; the path string
 * lengths are sane, so it is the deprecated compat path itself, not corrupt
 * data). The MODERN -[NSFileManager removeItemAtPath:error:] takes a different,
 * working internal path — iPhoto itself calls it elsewhere without crashing.
 * Redirect the deprecated call to the modern one (dropping the unused
 * error-callback delegate, passing error:NULL) and return the BOOL.
 *
 * A bare selector swap won't do: removeFileAtPath:handler:'s 2nd arg is an
 * object (the handler delegate) while removeItemAtPath:error:'s is an out
 * NSError** — not arg-compatible — so we resolve the path arg and re-dispatch.
 * Universal: any i386 app calling the deprecated handler method. Keyed on the
 * selector. */
static int bp_deprecated_removefile(struct objc_call_plan *plan,
                                    const uint32_t *args32,
                                    id real_self, SEL sel) {
   if (!sel || !real_self) { return 0; }
   if (strcmp(sel_getName(sel), "removeFileAtPath:handler:") != 0) { return 0; }
   SEL rsel = sel_registerName("removeItemAtPath:error:");
   if (!class_respondsToSelector(object_getClass(real_self), rsel)) { return 0; }
   id path = (id)(uintptr_t)unwrap_obj_arg(args32[2]);
   typedef signed char (*rm_t)(id, SEL, id, void *);
   signed char ok = ((rm_t)objc_msgSend)(real_self, rsel, path, (void *)0);
   plan->reg[0] = (uint64_t)(unsigned char)ok;              /* BOOL in eax */
   plan->reg[1] = plan->reg[2] = plan->reg[3] = plan->reg[4] = plan->reg[5] = 0;
   plan->nreg = 1;
   plan->ret_is_obj = 0;
   plan->target = (uint64_t)(uintptr_t)x64_ret_identity;
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[bp] removeFileAtPath:handler: -> removeItemAtPath:error: ok=%d\n",
              (int)ok);
      fflush(stderr);
   }
   return 1;
}
/* QUINN_PLAY_TRACE: one focused, low-volume line per event relevant to the
 * three remaining Quinn gameplay-rendering/input symptoms, so a SINGLE the tester
 * play-test disambiguates all three. Called from BOTH bridge directions —
 * forward (i386 -> native/self, objc_bridge_prep) and reverse (native ->
 * legacy IMP, reverse prep) — because the interesting sends split across them:
 *   INPUT (#3, dead board keys/mouse): keyDown:/keyUp:/flagsChanged:/
 *     mouseDown:/mouseDragged: arrive as REVERSE dispatches (AppKit -> the
 *     legacy QuinnController/QuinnMainWindow/board view). If these fire when
 *     The tester presses arrows, input reaches the handler (marshalling/keycode
 *     issue); if not, the responder chain never routes the event.
 *   BOARD (#1, black well + no landed cells): drawRect: is a REVERSE dispatch;
 *     the per-rect draw sub-methods drawBackgroundInRect: (white well,
 *     UNCONDITIONAL) / drawBoardInRect:...(landed cells, gated) /
 *     drawPieceInRect:...(the piece, gated) are FORWARD [self ...] sends from
 *     inside drawRect:. Seeing which fire per tick localizes it: bg fires but
 *     no white => the CGContext fill-color path; drawBoard absent while
 *     drawPiece present => the animation gate suppresses cells; drawBoard
 *     present but no cells => empty board matrix.
 *   SIDEBAR (#2, no NEXT/SCORE): drawRect: on QuinnPlayerInfoView / *InfoCell.
 * The forward getRectsBeingDrawn:count: count is already logged by bp_getrects.
 * Whitelisted by exact selector name (+ any *InfoView/*InfoCell drawRect:), so
 * the log stays tiny across a full session. Env-gated; zero cost when unset. */
static int quinn_play_trace_enabled(void) {
   static int v = -1;
   if (v < 0) { v = getenv("QUINN_PLAY_TRACE") ? 1 : 0; }
   return v;
}
static void quinn_play_trace(const char *dir, const char *cls, SEL sel) {
   if (!quinn_play_trace_enabled() || !sel) { return; }
   const char *s = sel_getName(sel);
   if (!s) { return; }
   static const char *const wl[] = {
      "keyDown:", "keyUp:", "flagsChanged:", "mouseDown:", "mouseDragged:",
      "mouseUp:", "becomeFirstResponder", "acceptsFirstResponder",
      "resignFirstResponder", "makeFirstResponder:",
      "drawRect:", "drawBackgroundInRect:",
      "drawBoardInRect:boardOpacity:cellDirtyRect:",
      "drawPieceInRect:boardOpacity:cellDirtyRect:",
      "getRectsBeingDrawn:count:", "setNeedsDisplay:", "setNeedsDisplayInRect:",
      "setNeedsDisplayInCellRect:", "setNeedsDisplayInCellRegion:", NULL };
   int hit = 0;
   for (int i = 0; wl[i]; ++i) { if (!strcmp(s, wl[i])) { hit = 1; break; } }
   /* also catch the sidebar's own drawRect: on the PlayerInfo view/cells */
   if (!hit && !strcmp(s, "drawRect:") && cls &&
       (strstr(cls, "PlayerInfo") || strstr(cls, "LCDCell"))) { hit = 1; }
   if (!hit) { return; }
   fprintf(stderr, "[qpt:%s] %s %s\n", dir, cls ? cls : "(nil)", s);
   fflush(stderr);
}

void objc_bridge_prep(struct objc_call_plan *plan, const uint32_t *args32) {
   arena_init();
   appkit_compat_install();
   x64_ensure_data_shadows();   /* this COPY's table, if its ctor never ran */
   x64_refresh_data_shadows();

   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[bp] entry: t=%x self32=0x%08x cmd32=0x%08x args[2..5]=0x%08x 0x%08x 0x%08x 0x%08x arena=[0x%lx..0x%lx)\n",
              pthread_mach_thread_np(pthread_self()),
              args32[0], args32[1], args32[2], args32[3], args32[4], args32[5],
              (unsigned long)g_arena_base, (unsigned long)g_arena_end);
      fflush(stderr);
   }

   id real_self = resolve_self(args32[0]);
   SEL sel = resolve_sel(args32[1]);

   /* Mark a lockFocus-family send as LEGACY-originated so the swizzled
    * -[NSImage lockFocus]/lockFocusFlipped: (legacy_lockfocus_1x_install) pins
    * this one to a 1x backing. Set here (forward bridge = i386 origin) and
    * consumed+cleared by the swizzled IMP that runs synchronously as this send's
    * native dispatch, so native ProKit/AppKit lockFocus is never affected. Scope
    * to an NSImage receiver (NSView also responds to lockFocus; that path has no
    * swizzle and must not leave the flag set to be mis-consumed by a later
    * NSImage send). Gate cheaply on the selector's first char before the strcmp. */
   if (sel) {
      const char *sn = sel_getName(sel);
      if (sn[0] == 'l' &&
          (!strcmp(sn, "lockFocus") || !strcmp(sn, "lockFocusFlipped:"))) {
         Class img = objc_getClass("NSImage");
         int is_img = 0;
         if (img && real_self) {
            for (Class c = object_getClass(real_self); c;
                 c = class_getSuperclass(c)) {
               if (c == img) { is_img = 1; break; }
            }
         }
         g_lockfocus_legacy = is_img;
      } else if (sn[0] == 'b' &&
                 !strcmp(sn, "bitmapImageRepForCachingDisplayInRect:")) {
         /* VIEW-side offscreen-1x sibling: mark a legacy NSView cache-rep vend
          * so legacy_cachedisplay_1x returns a 1x rep. Scope to an NSView
          * receiver (only NSView defines this selector; be defensive anyway). */
         Class view = objc_getClass("NSView");
         int is_view = 0;
         if (view && real_self) {
            for (Class c = object_getClass(real_self); c;
                 c = class_getSuperclass(c)) {
               if (c == view) { is_view = 1; break; }
            }
         }
         g_cachedisplay_legacy = is_view;
      }
   }

   /* LEGACY immediate-display-rect compat: mark this send legacy-originated so the
    * swizzled -[NSView displayRect:] / displayRectIgnoringOpacity:
    * (legacy_displayrect_comark_install) can run the ORIGINAL immediate draw and
    * THEN schedule a coalesced compositor pass. Set here (forward bridge = i386
    * origin) and consumed+cleared by the swizzled IMP that runs synchronously as
    * this send's native dispatch; native AppKit displayRect: is never affected.
    * Gate cheaply on the selector's first char before the strcmp. */
   if (sel && sel_getName(sel)[0] == 'd') {
      const char *sn = sel_getName(sel);
      if (!strcmp(sn, "displayRect:") ||
          !strcmp(sn, "displayRectIgnoringOpacity:") ||
          !strcmp(sn, "displayRectIgnoringOpacity:inContext:")) {
         g_displayrect_legacy = 1;
      }
   }
   /* mark a legacy-origin -[NSView gState] so the swizzle hands back a capture
    * token (dead native gState==0) for the NSCopyBits view-blit restore. */
   if (sel && sel_getName(sel)[0] == 'g' && !strcmp(sel_getName(sel), "gState")) {
      g_gstate_legacy = 1;
   }

   /* Quinn GL-upload diagnostic (env ABICONV_GL_TEXLOG): log the createTexture
    * selector sends BEFORE any gating, with raw handle + resolution result. */
   if (getenv("ABICONV_GL_TEXLOG") && sel) {
      const char *sn = sel_getName(sel);
      if (!strcmp(sn, "bitmapData") || !strcmp(sn, "bytesPerRow") ||
          !strcmp(sn, "bytesPerPlane") || !strcmp(sn, "bitsPerPixel")) {
         fprintf(stderr, "[bmp] prep sel=%s self32=0x%08x real=%p<%s>\n",
                 sn, args32[0], (void*)real_self,
                 real_self ? object_getClassName(real_self) : "(nil)");
         fflush(stderr);
      }
   }

   /* breadcrumb (zero-I/O): record the send + i386 caller RA + stack pointer */
   {
      uint64_t sp = (uint64_t)(uintptr_t)__builtin_frame_address(0);
      uint64_t prev = g_fwd_min_sp;
      while (sp < prev &&
             !__atomic_compare_exchange_n(&g_fwd_min_sp, &prev, sp, 0,
                                          __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {}
      uint32_t ri = __atomic_fetch_add(&g_fwd_pos, 1, __ATOMIC_SEQ_CST) & (FWD_RING - 1);
      struct fwd_crumb *c = &g_fwd_ring[ri];
      c->self32 = args32[0]; c->sel = (uint64_t)(uintptr_t)sel;
      c->caller_ra = args32[-1]; c->sp = sp;
      c->tid = pthread_mach_thread_np(pthread_self()); c->valid = 1;
   }

   /* env-gated (ABICONV_CALLRING) call-ring: stash this send's {self, sel, i386
    * caller RA} as pending; the matching return site (x64_objc_wrap_ret for the
    * common object return, or objc_bridge_ret_finish for struct returns)
    * completes it with the native return value. Inert unless the env var is set.
    * See callring_shim.c. */
   _86x64_callring_send((uint64_t)(uintptr_t)real_self,
                        (uint64_t)(uintptr_t)sel, args32[-1]);

   if (BRIDGE_TRACE() || quinn_play_trace_enabled()) {
      const char *cls_name = "(nil)";
      if (real_self) {
         Class c = object_getClass(real_self);
         if (c) cls_name = class_getName(c);
      }
      if (BRIDGE_TRACE()) { trace_args("send", cls_name, sel, args32); }
      quinn_play_trace("fwd", cls_name, sel);
      /* VIEW-HIERARCHY mutations (QUINN_PLAY_TRACE): translated code
       * reparenting / hiding views is invisible in the draw-selector traces,
       * but it is exactly what makes drawn content vanish (the legacy
       * offscreen-capture idiom reparents views through a hidden window;
       * animation-end handlers add/remove overlay views). One line per
       * addSubview: / removeFromSuperview[WithoutNeedingDisplay] /
       * setHidden: with the ARG's class + the receiver's current window. */
      if (quinn_play_trace_enabled() && sel) {
         const char *hsn = sel_getName(sel);
         int is_add = hsn && !strcmp(hsn, "addSubview:");
         int is_addpos = hsn && !strcmp(hsn, "addSubview:positioned:relativeTo:");
         int is_rm  = hsn && (!strcmp(hsn, "removeFromSuperview") ||
                              !strcmp(hsn, "removeFromSuperviewWithoutNeedingDisplay"));
         int is_hid = hsn && !strcmp(hsn, "setHidden:");
         if (is_add || is_addpos || is_rm || is_hid) {
            id arg0 = (is_add || is_addpos) ? resolve_self(args32[2]) : nil;
            id win = nil;
            if (real_self &&
                ((signed char(*)(id, SEL, SEL))objc_msgSend)(
                   real_self, sel_registerName("respondsToSelector:"),
                   sel_registerName("window"))) {
               win = ((id(*)(id, SEL))objc_msgSend)(real_self,
                                                    sel_registerName("window"));
            }
            fprintf(stderr, "[qpt:hier] %s %s%s%s win=%p%s%u\n",
                    cls_name, hsn,
                    arg0 ? " arg=" : "", arg0 ? object_getClassName(arg0) : "",
                    (void *)win,
                    is_hid ? " hidden=" : " #", is_hid ? args32[2] : 0u);
            fflush(stderr);
         }
      }
   }

   /* i386 block as the receiver of copy/retain/release etc.: handle inline so
    * the result stays an i386-layout block (native _Block_copy would misread
    * the layout). Detected structurally from args32[0]'s isa, independent of
    * how resolve_self mapped it. */
   if (bp_block_copy(plan, args32, sel))
      return;

   /* -[NSData bytes] on a >4GB native buffer -> return a low-4GB copy the i386
    * caller can dereference (raw-C consumers like Quinn's ks_decrypt). Must run
    * before the bare-`^v` return wrap would hand back an unreadable arena handle. */
   if (bp_nsdata_bytes(plan, real_self, sel))
      return;

   /* -[NSString fileSystemRepresentation]/getFileSystemRepresentation:maxLength:
    * -> satisfy via getCString into a low-4GB buffer, sidestepping the native
    * VM-backed-NSData null-lock (iPhoto account-config createDir:; see bp_fsrep). */
   if (bp_fsrep(plan, args32, real_self, sel))
      return;

   /* -[NSView getRectsBeingDrawn:count:]: AppKit writes a >4GB pointer to a
    * 32-byte-stride (double CGFloat) NSRect array + count into the caller's
    * out-params; convert to a low-4GB 16-byte-stride (float) i386 NSRect array
    * so the i386 drawRect: neither truncates the pointer nor misreads the
    * stride/layout (see bp_getrects). */
   if (bp_getrects(plan, args32, real_self, sel))
      return;

   /* NSData taking ownership of an i386 shim-malloc'd buffer via
    * dataWithBytesNoCopy:/initWithBytesNoCopy: freeWhenDone:YES -> redirect to the
    * COPYING form so native free() never touches shim memory (else -dealloc
    * aborts). */
   if (bp_nsdata_nocopy(plan, args32, real_self, sel))
      return;

   /* Deprecated -[NSFileManager removeFileAtPath:handler:] -> modern
    * removeItemAtPath:error: (the 10.0-era compat path null-locks a VM-backed
    * NSData in -[NSString fileSystemRepresentation]; iPhoto account-config
    * background thread, ~8% at boot/prefs). See bp_deprecated_removefile. */
   if (bp_deprecated_removefile(plan, args32, real_self, sel))
      return;

   /* NSFastEnumeration: convert the by-pointer state struct (x86_64 layout from
    * native Foundation) into the i386 layout the caller reads. */
   if (bp_fast_enum(plan, args32, real_self, sel))
      return;

   /* NSTrackingRectTag/NSToolTipTag removal: unwrap the 32-bit tag handle back to
    * the real 64-bit tag and call native (see bp_track_tag). The matching add*
    * side wraps the tag return below (kind 9). */
   if (bp_track_tag(plan, args32, real_self, sel))
      return;

   /* conformsToProtocol: with a legacy i386 Protocol argument — translate it to
    * the modern Protocol by name before asking natively (else libobjc strcmps a
    * garbage protocol-name pointer and crashes). */
   if (bp_conforms_protocol(plan, args32, real_self, sel))
      return;

   /* NSInvocation opaque-buffer argument/return accessors: translate
    * pointer-typed slots ('@' '#' ':' '*' '^') between the i386 4-byte value
    * and the real 8-byte native value per the invocation's signature (see
    * bp_invocation_arg — Quinn's reflection-invocation retainArguments
    * crash: a raw arena handle retained as an object). */
   if (bp_invocation_arg(plan, args32, real_self, sel))
      return;

   /* Legacy class-method dispatch fallback. Two distinct cases need it, both
    * resolved by jumping straight into the translated i386 IMP:
    *   (a) real_self == nil: a class message to one of our binary's OWN legacy
    *       classes, which the modern runtime never registered (e.g.
    *       +[UpgradeChecker checkOSVersion:]).
    *   (b) real_self is a real (system) class that our binary extended with a
    *       legacy CATEGORY, and the selector is one of those category class
    *       methods (e.g. +[NSObject isLogEnabled]). The real class does NOT
    *       respond, so sending to it would raise "unrecognized selector sent
    *       to class" and abort; dispatch to the legacy IMP instead.
    * Only attempt this for CLASS receivers (nil, or object_isClass): legacy
    * category dispatch walks the metaclass + category class-method lists.
    * legacy_class_method_imp() keys off the i386 class-name pointer (args32[0])
    * and returns 0 on a miss, in which case we fall through to the normal path
    * (no worse than before). */
   if (sel) {
      uint64_t imp = 0;
      if (!real_self) {
         /* Own legacy class: args32[0] is the __cls_refs class-name pointer. */
         imp = legacy_class_method_imp(args32[0], sel_getName(sel));
      } else if (object_isClass(real_self) &&
                 class_getClassMethod((Class)real_self, sel) == NULL) {
         /* Real (system) class that lacks this selector — a legacy category
          * class method (e.g. +[NSObject isLogEnabled]/+boolForDefaultsKey:).
          * args32[0] may be a __cls_refs name ptr OR a proxy handle (when the
          * Class came from `+class`), so look up by the real class name. */
         imp = legacy_class_method_imp_byname(class_getName((Class)real_self),
                                              sel_getName(sel));
      }
      if (imp) {
         if (BRIDGE_TRACE()) {
            fprintf(stderr, "[bp] legacy class-method dispatch \"%s\" -> imp 0x%llx\n",
                    sel_getName(sel), (unsigned long long)imp);
            fflush(stderr);
         }
         plan->legacy_imp = imp;
         return;
      }
   }

   /* Legacy INSTANCE-method dispatch fallback (the instance analogue of the
    * class-method case above). A reverse-bridged legacy instance can be sent a
    * selector that exists only in the app's i386 class metadata and was never
    * landed on the modern class by class_addMethod (a registration gap, or a
    * method list a later override re-add rejected) — the modern runtime then has
    * NO method, so the native dispatch below would raise "unrecognized selector"
    * and abort (iWeb -[BLCapabilitiesSingleton pSetOperationalLicenseState:],
    * sent from inside the class's own reverse-bridged -init). When the receiver's
    * class has no method for the selector on the modern runtime but the
    * legacy-method map does (walking the superclass chain, as reverse_prep does),
    * jump straight to the i386 IMP, reusing the i386 frame (args32[0] is already
    * the shadow the IMP expects). Precise: fires only where native dispatch would
    * otherwise crash; rmeth is keyed by our own legacy classes. Universal. */
   if (sel && real_self && !object_isClass(real_self) &&
       class_getInstanceMethod(object_getClass(real_self), sel) == NULL) {
      uint64_t imp = legacy_instance_method_imp(object_getClass(real_self), sel);
      if (imp) {
         if (BRIDGE_TRACE()) {
            fprintf(stderr, "[bp] legacy instance-method dispatch \"%s\" -> imp 0x%llx\n",
                    sel_getName(sel), (unsigned long long)imp);
            fflush(stderr);
         }
         plan->legacy_imp = imp;
         return;
      }
   }

   plan->reg[0] = (uint64_t)real_self;
   plan->reg[1] = (uint64_t)sel;
   plan->reg[2] = plan->reg[3] = plan->reg[4] = plan->reg[5] = 0;

   fill_args_and_return(plan, args32, /*arg_base_idx=*/2, /*reg_base=*/2,
                        real_self ? object_getClass(real_self) : NULL, sel);

   /* NSTrackingRectTag/NSToolTipTag creation: the 64-bit tag return must round-trip
    * through the i386 caller's 32-bit slot (it is later passed back to
    * removeTrackingRect:/removeToolTipRect:, see bp_track_tag). Truncating it to eax
    * (kind 8) loses the high bits -> AppKit's "invalid NSTrackingRectTag / truncated
    * to 32bit" abort. Wrap the tag into an arena handle instead (kind 9, plain
    * x64_objc_wrap). Keyed on the add* selectors: the 'q' return encoding erases the
    * tag typedef, so only the API names the token. The == 8 guard keeps this to the
    * native 64-bit-int return (a legacy i386 override returns via legacy_imp above). */
   {
      static SEL add_track, add_tip;
      if (!add_track) {
         add_track = sel_registerName("addTrackingRect:owner:userData:assumeInside:");
         add_tip   = sel_registerName("addToolTipRect:owner:userData:");
      }
      if (sel == add_track || sel == add_tip) {
         if (getenv("ABICONV_TAG_TRACE")) {
            fprintf(stderr, "[tag] add sel=%s ret_kind=%d (m=%p) self=%p\n",
                    sel_getName(sel), plan->ret_is_obj,
                    (void *)class_getInstanceMethod(
                        real_self ? object_getClass(real_self) : NULL, sel),
                    (void *)real_self);
            fflush(stderr);
         }
         if (plan->ret_is_obj == 8) plan->ret_is_obj = 9;
      }
   }
}

/*
 * objc_msgSend_stret: i386 ABI puts the struct-return buffer as an
 * implicit first arg (caller pushes its address before self). x86_64
 * SysV passes it in rdi with self/cmd shifted into rsi/rdx.
 *
 *   args32[0] = retbuf (pass-through low-4GB ptr)
 *   args32[1] = self
 *   args32[2] = _cmd
 *   args32[3..] = explicit args
 */
/* Shared stret-return planning: given the resolved method, decide how the
 * struct return travels and configure the plan. Returns the reg_base for the
 * explicit args (3 = stret form with hidden ptr in reg[0], 2 = native
 * register-return form), and writes reg[0] when it owns it.
 *   - layouts identical (isz==nsz) or unknown method: pass the i386 buffer
 *     raw (today's behavior), kind 0.
 *   - native MEMORY (>16B): hidden ptr = plan->stret_buf; kind 3 narrows it
 *     into the i386 buffer after the call.
 *   - native register-return (<=16B) while the i386 side used stret: the
 *     native callee takes NO hidden pointer — shift the layout down one and
 *     dispatch through the NON-stret msgSend variant (plan->target); kind 6
 *     materializes rax/rdx/xmm0/xmm1 into the i386 buffer. */
static unsigned plan_stret_return(struct objc_call_plan *plan, Method m,
                                  uint32_t retbuf32, uint64_t plain_target) {
   plan->target = 0;
   char *rt = m ? method_copyReturnType(m) : NULL;
   const int rconv = method_is_legacy(m) ? CONV_I386 : CONV_NATIVE;
   char rb = rt ? *enc_skip_quals(rt) : 0;
   size_t isz = 0, nsz = 0; uint8_t sse[8];
   int have = 0;
   if (rb == '{' || rb == '(' || rb == '[') {
      enc_classify(enc_skip_quals(rt), rconv, &isz, &nsz, sse);
      have = nsz > 0;
   }
   free(rt);
   if (!have || isz == nsz) {
      plan->reg[0] = (uint64_t)retbuf32;       /* raw passthrough */
      plan->ret_is_obj = 0;
      return 3;
   }
   if (nsz > 16) {
      if (nsz > PLAN_STRET_MAX) {
         if (BRIDGE_TRACE()) {
            fprintf(stderr, "[bp] WARN: stret %zuB exceeds bounce buffer; "
                    "passing i386 buffer raw\n", nsz);
         }
         plan->reg[0] = (uint64_t)retbuf32;
         plan->ret_is_obj = 0;
         return 3;
      }
      plan->reg[0]   = (uint64_t)(uintptr_t)plan->stret_buf;
      plan->ret_is_obj = 3;
      plan->sret_dst32 = retbuf32;
      plan->sret_enc   = m ? method_getTypeEncoding(m) : NULL;
      plan->sret_conv  = (uint32_t)rconv;
      return 3;
   }
   /* native register-return form */
   plan->target     = plain_target;
   plan->ret_is_obj = 6;
   plan->sret_dst32 = retbuf32;
   plan->sret_enc   = m ? method_getTypeEncoding(m) : NULL;
   plan->sret_conv  = (uint32_t)rconv;
   return 2;
}

void objc_bridge_prep_stret(struct objc_call_plan *plan, const uint32_t *args32) {
   arena_init();

   id real_self = resolve_self(args32[1]);
   SEL sel = resolve_sel(args32[2]);

   if (BRIDGE_TRACE()) {
      const char *cls_name = "(nil)";
      if (real_self) {
         Class c = object_getClass(real_self);
         if (c) cls_name = class_getName(c);
      }
      trace_args("stret", cls_name, sel, args32);
   }

   Class lookup = real_self ? object_getClass(real_self) : NULL;
   Method m = lookup ? class_getInstanceMethod(lookup, sel) : NULL;
   unsigned reg_base = plan_stret_return(plan, m, args32[0],
                                         (uint64_t)(uintptr_t)objc_msgSend);
   const int kind = plan->ret_is_obj;

   plan->reg[reg_base - 2] = (uint64_t)real_self;
   plan->reg[reg_base - 1] = (uint64_t)sel;
   for (unsigned k = reg_base; k < 6; ++k) { plan->reg[k] = 0; }

   fill_args_and_return(plan, args32, /*arg_base_idx=*/3, reg_base,
                        lookup, sel);
   /* fill computed a kind for the encoding as if this were a plain send;
    * the stret decision above owns the return. */
   plan->ret_is_obj = kind;
}

/* Defined with the reverse-bridge machinery below; lets a legacy [super sel]
 * tell reverse_prep to dispatch the SUPER's legacy method rather than the
 * receiver's derived override (prevents +initialize-style recursion). */
static void reverse_note_super(id recv, SEL sel, Class super_lookup);

/*
 * objc_msgSendSuper: args32[0] points at a struct objc_super_i386 in the
 * caller's stack frame. Resolve both halves, build an x86_64-layout
 * super struct inside the plan, and pass its address in reg[0].
 *
 *   args32[0] = &objc_super_i386 { receiver_handle, super_class_handle }
 *   args32[1] = _cmd
 *   args32[2..] = explicit args
 */
void objc_bridge_prep_super(struct objc_call_plan *plan, const uint32_t *args32) {
   arena_init();
   appkit_compat_install();

   const struct objc_super_i386 *super_i386 =
      (const struct objc_super_i386 *)(uintptr_t)args32[0];
   id real_receiver = resolve_self(super_i386->receiver);
   /* super_class slot can be either a Class proxy handle (rare) or a
    * class-name cstring pointer in __OBJC,__cls_refs. resolve_self
    * handles both. */
   id super_class_id = resolve_self(super_i386->super_class);
   SEL sel = resolve_sel(args32[1]);

   /*
    * Class-method super-calls (`[super foo]` inside a `+method`). When the
    * receiver is itself a class object, the message is a CLASS method, which
    * lives on the METACLASS, not the regular class. objc_msgSendSuper starts
    * its method lookup in super.super_class's own method list, so for a class
    * method that field must be the super's METACLASS — otherwise the runtime
    * searches the super class's INSTANCE methods, fails to find the class
    * method, and raises "+[Class sel]: unrecognized selector" (iPhoto 10th
    * blocker: +[IPPublishPluginManager initialize]'s `[super initialize]`).
    * The instance-method super-call (e.g. ExtendedApplication -init's
    * [super init]) keeps super_class as the regular class — receiver is an
    * instance there, so object_isClass is false and this is skipped.
    */
   if (super_class_id && real_receiver && object_isClass(real_receiver) &&
       !class_isMetaClass((Class)super_class_id)) {
      super_class_id = (id)object_getClass(super_class_id);
   }

   plan->super.receiver    = real_receiver;
   plan->super.super_class = (Class)super_class_id;

   /* Tell reverse_prep to look this sel up on the SUPER class, so a legacy
    * [super sel] runs the super's method, not the receiver's override. */
   reverse_note_super(real_receiver, sel, (Class)super_class_id);

   if (BRIDGE_TRACE()) {
      const char *cls_name = "(nil)";
      if (real_receiver) {
         Class c = object_getClass(real_receiver);
         if (c) cls_name = class_getName(c);
      }
      trace_args("super", cls_name, sel, args32);
   }

   plan->reg[0] = (uint64_t)(uintptr_t)&plan->super;
   plan->reg[1] = (uint64_t)sel;
   plan->reg[2] = plan->reg[3] = plan->reg[4] = plan->reg[5] = 0;

   /* For arg-type introspection: look up the method on the SUPER class,
    * not the receiver's class (the call goes to super's IMP). super_class is
    * already the right lookup table — metaclass-converted above for class
    * methods, the plain class for instance methods — so pass it AS the
    * lookup class (object_getClass here would land on the metaclass and
    * miss every instance method). */
   Class type_lookup = plan->super.super_class;
   if (!type_lookup && real_receiver) {
      type_lookup = object_getClass(real_receiver);
   }
   fill_args_and_return(plan, args32, /*arg_base_idx=*/2, /*reg_base=*/2,
                        type_lookup, sel);
}

/*
 * objc_msgSendSuper_stret: combination — retbuf is implicit-first, then
 * the super struct ptr, then _cmd.
 */
void objc_bridge_prep_super_stret(struct objc_call_plan *plan,
                                  const uint32_t *args32) {
   arena_init();

   const struct objc_super_i386 *super_i386 =
      (const struct objc_super_i386 *)(uintptr_t)args32[1];
   id real_receiver = resolve_self(super_i386->receiver);
   id super_class_id = resolve_self(super_i386->super_class);
   SEL sel = resolve_sel(args32[2]);

   /* Class-method super-call: search the super's metaclass (see
    * objc_bridge_prep_super for the full rationale). */
   if (super_class_id && real_receiver && object_isClass(real_receiver) &&
       !class_isMetaClass((Class)super_class_id)) {
      super_class_id = (id)object_getClass(super_class_id);
   }

   plan->super.receiver    = real_receiver;
   plan->super.super_class = (Class)super_class_id;

   reverse_note_super(real_receiver, sel, (Class)super_class_id);

   if (BRIDGE_TRACE()) {
      const char *cls_name = "(nil)";
      if (real_receiver) {
         Class c = object_getClass(real_receiver);
         if (c) cls_name = class_getName(c);
      }
      trace_args("super_stret", cls_name, sel, args32);
   }

   /* See objc_bridge_prep_super: super_class IS the lookup class. */
   Class type_lookup = plan->super.super_class;
   if (!type_lookup && real_receiver) {
      type_lookup = object_getClass(real_receiver);
   }
   Method m = type_lookup ? class_getInstanceMethod(type_lookup, sel) : NULL;
   unsigned reg_base = plan_stret_return(plan, m, args32[0],
                                         (uint64_t)(uintptr_t)objc_msgSendSuper);
   const int kind = plan->ret_is_obj;

   plan->reg[reg_base - 2] = (uint64_t)(uintptr_t)&plan->super;
   plan->reg[reg_base - 1] = (uint64_t)sel;
   for (unsigned k = reg_base; k < 6; ++k) { plan->reg[k] = 0; }

   fill_args_and_return(plan, args32, /*arg_base_idx=*/3, reg_base,
                        type_lookup, sel);
   plan->ret_is_obj = kind;
}

/* Post-call return finisher for the struct-return kinds (3/5/6). Called from
 * objc_msgSend.asm with the call's rax/rdx in the integer params and xmm0/xmm1
 * (still live from the callee) bound to the double params. Returns the i386
 * eax:edx pair as a __int128 (lo -> rax, hi -> rdx). */
unsigned __int128 objc_bridge_ret_finish(struct objc_call_plan *plan,
                                         uint64_t vrax, uint64_t vrdx,
                                         double x0, double x1) {
   /* env-gated (ABICONV_CALLRING) call-ring: complete this send's pending entry
    * with the native struct/scalar return (kinds 3/5/6). Object returns (kind 1)
    * complete in x64_objc_wrap_ret instead. Diagnostic-only; inert unless the env
    * var is set. See callring_shim.c. */
   _86x64_callring_ret(vrax);

   const int kind = plan->ret_is_obj;
   if (kind == 3) {
      /* native MEMORY struct in stret_buf -> narrow into the i386 buffer;
       * i386 stret returns the buffer address in eax */
      if (plan->sret_dst32 && plan->sret_enc) {
         enc_narrow(enc_skip_quals(plan->sret_enc), (int)plan->sret_conv,
                    plan->stret_buf, (uint8_t *)(uintptr_t)plan->sret_dst32);
      }
      return (unsigned __int128)plan->sret_dst32;
   }
   if (kind == 5 || kind == 6) {
      /* materialize the register-returned native aggregate, then narrow */
      const char *enc = plan->sret_enc ? enc_skip_quals(plan->sret_enc) : NULL;
      if (!enc) { return (unsigned __int128)vrax; }
      uint8_t nat[16] = {0};
      size_t isz = 0, nsz = 0; uint8_t sse[8] = {0};
      unsigned nebs = enc_classify(enc, (int)plan->sret_conv, &isz, &nsz, sse);
      unsigned ii = 0, fi = 0;
      for (unsigned k = 0; k < nebs && k < 2; ++k) {
         uint64_t eb;
         if (sse[k]) { double d = fi++ ? x1 : x0; memcpy(&eb, &d, 8); }
         else        { eb = ii++ ? vrdx : vrax; }
         memcpy(nat + 8*k, &eb, 8);
      }
      if (kind == 6) {
         if (plan->sret_dst32) {
            enc_narrow(enc, (int)plan->sret_conv, nat,
                       (uint8_t *)(uintptr_t)plan->sret_dst32);
         }
         return (unsigned __int128)plan->sret_dst32;
      }
      uint8_t out[8] = {0};                    /* i386 <=8B struct: eax:edx */
      enc_narrow(enc, (int)plan->sret_conv, nat, out);
      uint32_t lo, hi;
      memcpy(&lo, out, 4); memcpy(&hi, out + 4, 4);
      return ((unsigned __int128)hi << 64) | lo;
   }
   return (unsigned __int128)vrax;
}

/* ---------------------------------------------------------------------
 * Legacy ObjC 1.0 ABI class registration (skeleton)
 * ---------------------------------------------------------------------
 *
 * iPhoto (and any pre-modern i386 Cocoa app) defines its OWN classes in
 * the legacy __OBJC segment. The modern x86_64 libobjc completely ignores
 * legacy metadata — it only processes the ObjC2 __objc_classlist /
 * __objc_data sections. So unless we walk the legacy metadata and
 * register each class with the modern runtime via
 * objc_allocateClassPair/class_addMethod/class_addIvar/objc_registerClassPair,
 * none of iPhoto's own classes exist at runtime — every `[MyController ...]`
 * fails because MyController is unknown to the runtime.
 *
 * What's here NOW: structure definitions + a metadata walker that just
 * counts and reports what would be registered. This is the foundation
 * for the real registration code, which is gated on:
 *   - a working IMP bridge (x86_64 objc_msgSend ABI -> i386 IMP ABI per
 *     method's type encoding — the reverse of the existing __objc_msgSend
 *     forward marshaller);
 *   - ivar size/alignment derivation from ObjC type encodings;
 *   - two-pass / topological ordering for in-binary superclass deps.
 *
 * Diagnostic walk is invoked via _86x64_objc_register_classes(mh, slide)
 * by the wrapper at load time (after slide-fixup so __OBJC pointers are
 * correct). Outputs to stderr; safe to call multiple times.
 */

#include <mach-o/loader.h>

struct legacy_objc_module {
   uint32_t version;       /* expected 7 */
   uint32_t size;          /* expected 16 (sizeof this struct) */
   uint32_t name;          /* cstring ptr (module name) */
   uint32_t symtab;        /* legacy_objc_symtab ptr */
};

struct legacy_objc_symtab {
   uint32_t sel_ref_cnt;
   uint32_t refs;          /* SEL ptr (selector refs) */
   uint16_t cls_def_cnt;
   uint16_t cat_def_cnt;
   /* defs[cls_def_cnt + cat_def_cnt]: classes then categories */
};

struct legacy_objc_class {
   uint32_t isa;              /* metaclass ptr (CLS_META variant of self) */
   uint32_t super_class;      /* before runtime fixup: cstring ptr to super name */
   uint32_t name;             /* cstring ptr */
   uint32_t version;
   uint32_t info;             /* CLS_CLASS (0x1) / CLS_META (0x2) etc. */
   uint32_t instance_size;
   uint32_t ivars;            /* legacy_objc_ivar_list ptr (or 0) */
   uint32_t methodLists;      /* ptr to NULL-terminated array of method_list ptrs */
   uint32_t cache;
   uint32_t protocols;
};

struct legacy_objc_method {
   uint32_t name;          /* cstring (selector name pre-fixup) */
   uint32_t types;         /* cstring (ObjC type encoding) */
   uint32_t imp;           /* function ptr (the translated x86_64 IMP) */
};

struct legacy_objc_method_list {
   uint32_t obsolete;      /* expected 0 */
   int32_t  method_count;
   /* methods[method_count] */
};

struct legacy_objc_ivar {
   uint32_t name;
   uint32_t type;
   int32_t  offset;
};

struct legacy_objc_ivar_list {
   int32_t ivar_count;
   /* ivars[ivar_count] */
};

struct legacy_objc_category {
   uint32_t category_name;
   uint32_t class_name;
   uint32_t instance_methods;
   uint32_t class_methods;
   uint32_t protocols;
};

/* Locate __OBJC,__module_info in the given Mach-O image. Returns the
 * runtime base address of the section (vmaddr + slide) and writes size. */
static const struct legacy_objc_module *
find_module_info(const struct mach_header_64 *mh, intptr_t slide, size_t *size_out) {
   const uint8_t *cmd_ptr = (const uint8_t *)(mh + 1);
   for (uint32_t c = 0; c < mh->ncmds; ++c) {
      const struct load_command *lc = (const struct load_command *)cmd_ptr;
      cmd_ptr += lc->cmdsize;
      if (lc->cmd != LC_SEGMENT_64) continue;
      const struct segment_command_64 *seg = (const struct segment_command_64 *)lc;
      if (strcmp(seg->segname, "__OBJC") != 0) continue;
      const struct section_64 *sects = (const struct section_64 *)(seg + 1);
      for (uint32_t s = 0; s < seg->nsects; ++s) {
         if (strcmp(sects[s].sectname, "__module_info") == 0) {
            *size_out = sects[s].size;
            return (const struct legacy_objc_module *)
               ((uintptr_t)sects[s].addr + slide);
         }
      }
   }
   return NULL;
}

/* Locate an arbitrary __OBJC section (e.g. __category, __class) by name;
 * returns its slid runtime base + byte size, or NULL. */
static const void *find_objc_section(const struct mach_header_64 *mh,
                                     intptr_t slide, const char *sectname,
                                     size_t *size_out) {
   *size_out = 0;
   const uint8_t *cmd_ptr = (const uint8_t *)(mh + 1);
   for (uint32_t c = 0; c < mh->ncmds; ++c) {
      const struct load_command *lc = (const struct load_command *)cmd_ptr;
      cmd_ptr += lc->cmdsize;
      if (lc->cmd != LC_SEGMENT_64) continue;
      const struct segment_command_64 *seg = (const struct segment_command_64 *)lc;
      if (strcmp(seg->segname, "__OBJC") != 0) continue;
      const struct section_64 *sects = (const struct section_64 *)(seg + 1);
      for (uint32_t s = 0; s < seg->nsects; ++s) {
         if (strcmp(sects[s].sectname, sectname) == 0) {
            *size_out = sects[s].size;
            return (const void *)((uintptr_t)sects[s].addr + slide);
         }
      }
   }
   return NULL;
}

/* Public entry: register this image's legacy __OBJC classes with the modern
 * runtime so AppKit/Foundation can message them (principal class, delegates,
 * NSClassFromString, …). The real work lives in reverse_register_image() at
 * the end of this file, where the registry/encoding/reverse-bridge helpers
 * are all in scope. */
static void reverse_register_image(const struct mach_header_64 *mh,
                                   intptr_t slide);
void _86x64_objc_register_classes(const struct mach_header_64 *mh,
                                  intptr_t slide) {
   reverse_register_image(mh, slide);
}

/* ---------------------------------------------------------------------
 * Legacy class-method direct dispatch (the actually-wired path)
 * ---------------------------------------------------------------------
 *
 * Rather than register our binary's legacy classes with the modern runtime
 * (which would force the unsolvable 4-byte-isa/ivar-layout impedance and a
 * reverse IMP marshaller — see the skeleton above), we keep dispatch entirely
 * inside the i386-cdecl world. We index every legacy class by name at image
 * load, and when objc_bridge_prep sees a class message whose receiver name is
 * one of ours, it finds the translated class-method IMP here and hands its
 * address back to the asm trampoline, which resumes the i386 call into it.
 *
 * This only handles CLASS methods (the receiver is a class-name string, so
 * `self` is a class — no instance ivars are touched). Instance dispatch on
 * legacy objects is a separate, harder problem (ivar layout) and is untouched.
 */

#define LEGACY_REG_CAP 8192u   /* power of two */
struct legacy_reg_ent {
   const char *name;                       /* class name cstring (runtime ptr) */
   const struct legacy_objc_class *cls;    /* runtime class struct ptr */
};
static struct legacy_reg_ent g_legacy_reg[LEGACY_REG_CAP];
static uint32_t g_legacy_reg_cnt = 0;   /* distinct classes indexed (for tracing) */

/* Must exceed the TOTAL legacy category count across all images a single
 * libabiconv copy indexes. Direct __category enumeration (see the indexer)
 * surfaces ALL categories, not just the ~8% reachable via garbled symtabs
 * (iWeb.dylib alone has 581), so this is far larger than the old symtab-only
 * count needed. */
#define LEGACY_CAT_CAP 32768u
static const struct legacy_objc_category *g_legacy_cats[LEGACY_CAT_CAP];
static uint32_t g_legacy_cat_cnt = 0;

/* A legacy metadata pointer is trustworthy only if it is plausibly a real
 * address and actually mapped. The metadata in a 15-year-old i386 binary
 * carries sentinels (0, 1, -1) and the indexer runs inside dyld's add-image
 * callback, so a stray deref would crash the whole process before main(). */
static int ptr_ok(uint32_t p, size_t len) {
   return p >= 0x1000u && mem_readable((uintptr_t)p, len);
}

static uint32_t str_hash(const char *s) {
   uint32_t h = 2166136261u;
   for (; *s; ++s) { h = (h ^ (uint8_t)*s) * 16777619u; }
   return h;
}

static const struct legacy_objc_class *legacy_registry_lookup(const char *name) {
   if (!name) { return NULL; }
   uint32_t i = str_hash(name) & (LEGACY_REG_CAP - 1);
   for (uint32_t n = 0; n < LEGACY_REG_CAP; ++n) {
      const struct legacy_reg_ent *e = &g_legacy_reg[i];
      if (!e->name) { return NULL; }
      if (strcmp(e->name, name) == 0) { return e->cls; }
      i = (i + 1) & (LEGACY_REG_CAP - 1);
   }
   return NULL;
}

static void legacy_registry_insert(const struct legacy_objc_class *cls) {
   /* str_hash/strcmp below walk the whole name to its NUL, so 1 byte of
    * validation is not enough — a name flush against a mapping's EOF makes
    * the hash run off into an unmapped page (SIGBUS KERN_MEMORY_ERROR). */
   if (!legacy_cstr_ok(cls->name)) { return; }
   const char *name = (const char *)(uintptr_t)cls->name;
   uint32_t i = str_hash(name) & (LEGACY_REG_CAP - 1);
   for (uint32_t n = 0; n < LEGACY_REG_CAP; ++n) {
      struct legacy_reg_ent *e = &g_legacy_reg[i];
      if (!e->name) { e->name = name; e->cls = cls; ++g_legacy_reg_cnt; return; }
      if (strcmp(e->name, name) == 0) { e->cls = cls; return; }  /* idempotent */
      i = (i + 1) & (LEGACY_REG_CAP - 1);
   }
   /* table full — silently drop (only happens with >8k legacy classes) */
}

/* Scan a single legacy objc_method_list {obsolete, count, methods[count]} for
 * a method whose selector name matches sel_name; return its (slid) IMP or 0. */
static uint64_t scan_one_list(uint32_t list, const char *sel_name) {
   if (!ptr_ok(list, sizeof(struct legacy_objc_method_list))) { return 0; }
   const struct legacy_objc_method_list *ml =
      (const struct legacy_objc_method_list *)(uintptr_t)list;
   if (ml->method_count <= 0 || ml->method_count >= 100000) { return 0; }
   const struct legacy_objc_method *methods =
      (const struct legacy_objc_method *)(ml + 1);
   if (!ptr_ok(list + (uint32_t)sizeof(*ml),
               (size_t)ml->method_count * sizeof(*methods))) { return 0; }
   for (int m = 0; m < ml->method_count; ++m) {
      if (!ptr_ok(methods[m].name, 1)) { continue; }
      const char *mname = (const char *)(uintptr_t)methods[m].name;
      if (getenv("OBJC_BRIDGE_TRACE_METH")) {
         fprintf(stderr, "[bp]     meth \"%s\" imp=0x%x\n", mname, methods[m].imp);
         fflush(stderr);
      }
      if (strcmp(mname, sel_name) == 0) {
         return (uint64_t)(uintptr_t)methods[m].imp;
      }
   }
   return 0;
}

/* The class `methodLists` field has two legacy layouts: the GCC i386 static
 * compiler emits a pointer to ONE objc_method_list directly (obsolete word is
 * 0); after runtime fixup (CLS_NO_METHOD_ARRAY clear) it can instead point to
 * a NULL/(-1)-terminated array of objc_method_list pointers. We never ran the
 * legacy runtime, so it's almost always the direct form, but handle both:
 * if the first word is 0 (an obsolete field, not a list pointer) and the
 * second is a plausible method count, treat it as a single direct list;
 * otherwise treat it as an array of list pointers. */
static uint64_t find_method_in_lists(uint32_t methodLists, const char *sel_name) {
   if (!ptr_ok(methodLists, 2 * sizeof(uint32_t))) { return 0; }
   const uint32_t *w = (const uint32_t *)(uintptr_t)methodLists;
   const int32_t  cnt = (int32_t)w[1];
   if (w[0] == 0 && cnt > 0 && cnt < 100000) {
      return scan_one_list(methodLists, sel_name);   /* direct single list */
   }
   /* per-element guard: only the first 8 bytes were range-checked above,
    * and the (unterminated-junk) array can run off its mapping */
   for (size_t k = 0;
        k < 4096 && ptr_ok((uint32_t)(methodLists + 4 * k), 4)
           && w[k] != 0 && w[k] != 0xFFFFFFFFu;
        ++k) {
      uint64_t imp = scan_one_list(w[k], sel_name);  /* array of list ptrs */
      if (imp) { return imp; }
   }
   return 0;
}

/* Class methods added to a class via a legacy category. */
static uint64_t find_category_class_method(const char *clsname, const char *sel_name) {
   if (!clsname) { return 0; }
   for (uint32_t i = 0; i < g_legacy_cat_cnt; ++i) {
      const struct legacy_objc_category *cat = g_legacy_cats[i];
      if (!ptr_ok(cat->class_name, 1)) { continue; }
      const char *cn = (const char *)(uintptr_t)cat->class_name;
      if (strcmp(cn, clsname) == 0) {
         uint64_t imp = find_method_in_lists(cat->class_methods, sel_name);
         if (imp) { return imp; }
      }
   }
   return 0;
}

/* Walk a legacy (fragile-ABI) objc_protocol_list — { protocol_list *next;
 * long count; Protocol *list[count]; }, all 4-byte i386 fields — returning 1
 * if any protocol it holds (or, transitively, any protocol THOSE incorporate)
 * is named qname. Each protocol is { Class isa; char *name; protocol_list
 * *incorporated; ... } with name at +4 and incorporated list at +8. The
 * structures live in the app's own i386 __OBJC; every deref is ptr_ok-guarded
 * because a 15-year-old binary's metadata carries junk. Recursion is bounded
 * (incorporated-protocol / next-chain cycles). */
static int legacy_plist_has(uint32_t plist, const char *qname, int depth) {
   if (!qname || depth > 8) { return 0; }
   if (!ptr_ok(plist, 8)) { return 0; }
   const uint32_t *pl = (const uint32_t *)(uintptr_t)plist;
   int32_t count = (int32_t)pl[1];
   if (count <= 0 || count > 4096) {
      /* still follow a chained next list even on a junk count */
      uint32_t nx = pl[0];
      return (nx && nx != plist) ? legacy_plist_has(nx, qname, depth + 1) : 0;
   }
   if (!ptr_ok(plist + 8, (size_t)count * 4)) { return 0; }
   const uint32_t *list = pl + 2;
   for (int i = 0; i < count; ++i) {
      uint32_t proto = list[i];
      if (!ptr_ok(proto, 12)) { continue; }
      const uint32_t *pr = (const uint32_t *)(uintptr_t)proto;
      uint32_t nameptr = pr[1];                 /* +4 protocol_name */
      if (ptr_ok(nameptr, 1) &&
          strcmp((const char *)(uintptr_t)nameptr, qname) == 0) { return 1; }
      uint32_t incorporated = pr[2];            /* +8 adopted-protocol list */
      if (incorporated && legacy_plist_has(incorporated, qname, depth + 1)) { return 1; }
   }
   uint32_t next = pl[0];
   if (next && next != plist && legacy_plist_has(next, qname, depth + 1)) { return 1; }
   return 0;
}

/* Does the legacy class `clsname` (or one of its categories, or a legacy
 * superclass) adopt a protocol named `qname`? Mirrors the legacy super-chain
 * walk of legacy_class_method_imp_byname: super_class holds the super-NAME
 * cstring pre-fixup, and the chain stops at the first framework class (registry
 * miss) — protocols a native superclass adopts are already answered by the
 * native conformsToProtocol: check in bp_conforms_protocol. */
static int legacy_class_conforms(const char *clsname, const char *qname, int depth) {
   if (!clsname || !qname || depth > 64) { return 0; }
   const struct legacy_objc_class *c = legacy_registry_lookup(clsname);
   if (c && ptr_ok((uintptr_t)c, sizeof(*c)) && c->protocols &&
       legacy_plist_has(c->protocols, qname, 0)) { return 1; }
   /* categories on this class declare conformance in their own protocols field */
   for (uint32_t i = 0; i < g_legacy_cat_cnt; ++i) {
      const struct legacy_objc_category *cat = g_legacy_cats[i];
      if (!cat || !ptr_ok(cat->class_name, 1)) { continue; }
      if (strcmp((const char *)(uintptr_t)cat->class_name, clsname) != 0) { continue; }
      if (cat->protocols && legacy_plist_has(cat->protocols, qname, 0)) { return 1; }
   }
   /* legacy superclass chain */
   if (c && ptr_ok(c->super_class, 1)) {
      const char *sup = (const char *)(uintptr_t)c->super_class;
      if (strcmp(sup, clsname) != 0) {
         return legacy_class_conforms(sup, qname, depth + 1);
      }
   }
   return 0;
}

static uint64_t legacy_class_method_imp_byname(const char *clsname,
                                               const char *sel_name) {
   if (!clsname || !sel_name || clsname[0] == '\0') { return 0; }

   /* Walk the class and its legacy-superclass chain, checking each level's
    * metaclass method lists (class methods live in the metaclass) and any
    * categories that add class methods. Stop when the superclass is a
    * framework class (registry miss) — those class methods, if inherited,
    * are a known gap (we can't dispatch +alloc/+class etc. yet). */
   const struct legacy_objc_class *c = legacy_registry_lookup(clsname);
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[bp]   legacy_lookup(\"%s\",\"%s\"): class=%p isa_ok=%d\n",
              clsname, sel_name, (void*)c,
              c ? ptr_ok(c->isa, sizeof(struct legacy_objc_class)) : 0);
      fflush(stderr);
   }
   const char *cur = clsname;
   for (int depth = 0; depth < 256; ++depth) {
      if (c && ptr_ok(c->isa, sizeof(struct legacy_objc_class))) {
         const struct legacy_objc_class *meta =
            (const struct legacy_objc_class *)(uintptr_t)c->isa;
         if (BRIDGE_TRACE()) {
            const uint32_t *w = ptr_ok(meta->methodLists, 32)
               ? (const uint32_t*)(uintptr_t)meta->methodLists : NULL;
            fprintf(stderr, "[bp]   level %d cur=\"%s\" meta=%p mL=0x%x info=0x%x words=[%x %x %x %x %x %x]\n",
                    depth, cur, (void*)meta, meta->methodLists, meta->info,
                    w?w[0]:0, w?w[1]:0, w?w[2]:0, w?w[3]:0, w?w[4]:0, w?w[5]:0);
            fflush(stderr);
         }
         uint64_t imp = find_method_in_lists(meta->methodLists, sel_name);
         if (imp) { return imp; }
      }
      uint64_t cimp = find_category_class_method(cur, sel_name);
      if (cimp) { return cimp; }
      if (!c || !ptr_ok(c->super_class, 1)) { return 0; }
      cur = (const char *)(uintptr_t)c->super_class;
      c = legacy_registry_lookup(cur);
      if (!c) { return 0; }
   }
   return 0;
}

/* Instance methods added to a class via a legacy category. */
static uint64_t find_category_instance_method(const char *clsname, const char *sel_name) {
   if (!clsname) { return 0; }
   for (uint32_t i = 0; i < g_legacy_cat_cnt; ++i) {
      const struct legacy_objc_category *cat = g_legacy_cats[i];
      if (!ptr_ok(cat->class_name, 1)) { continue; }
      const char *cn = (const char *)(uintptr_t)cat->class_name;
      if (strcmp(cn, clsname) == 0) {
         uint64_t imp = find_method_in_lists(cat->instance_methods, sel_name);
         if (imp) { return imp; }
      }
   }
   return 0;
}

/* The i386 IMP of a legacy INSTANCE method on `clsname` (walking the legacy
 * super_class chain + category instance methods), or 0. The instance analogue of
 * legacy_class_method_imp_byname; like it, this resolves through the RAW __OBJC
 * metadata indexed in EVERY libabiconv copy (legacy_registry_lookup / g_legacy_reg)
 * — NOT the per-copy g_rmeth table, which is populated only in the single copy
 * that won the reverse-registration race. The forward bridge runs in the caller's
 * copy, so it must use this cross-copy path (iWeb -[BLCapabilitiesSingleton
 * pSetOperationalLicenseState:] was registered in the SFLicense copy but called
 * from the iWeb-main copy). */
static uint64_t legacy_instance_method_imp_byname(const char *clsname,
                                                  const char *sel_name) {
   if (!clsname || !sel_name || clsname[0] == '\0') { return 0; }
   const struct legacy_objc_class *c = legacy_registry_lookup(clsname);
   const char *cur = clsname;
   for (int depth = 0; depth < 256; ++depth) {
      if (c) {
         uint64_t imp = find_method_in_lists(c->methodLists, sel_name);
         if (imp) { return imp; }
      }
      uint64_t cimp = find_category_instance_method(cur, sel_name);
      if (cimp) { return cimp; }
      if (!c || !ptr_ok(c->super_class, 1)) { return 0; }
      cur = (const char *)(uintptr_t)c->super_class;
      c = legacy_registry_lookup(cur);
      if (!c) { return 0; }
   }
   return 0;
}

/* Thin wrapper: the i386 receiver slot (args32[0]) for a message to one of our
 * OWN legacy classes is a __cls_refs class-name cstring pointer. Validate it
 * and dispatch by name. (When the receiver is instead a proxy handle — e.g. a
 * Class obtained from `+class` — the caller derives the name via
 * class_getName() and calls legacy_class_method_imp_byname directly.) */
static uint64_t legacy_class_method_imp(uint32_t self32, const char *sel_name) {
   if (self32 == 0 || !sel_name) { return 0; }
   if (!mem_readable((uintptr_t)self32, 1)) { return 0; }
   return legacy_class_method_imp_byname((const char *)(uintptr_t)self32,
                                         sel_name);
}

/* Public entry: index every legacy class/category in the image into the
 * registry above. Called from objc_slide.c's add-image callback AFTER the
 * __OBJC slide-fixup, so every metadata pointer is a correct runtime address.
 * Idempotent: re-indexing the same image overwrites in place (the N co-located
 * libabiconv copies each run their own add-image callback). */
void _86x64_objc_index_legacy_classes(const struct mach_header_64 *mh,
                                      intptr_t slide) {
   if (!mh) { return; }
   size_t modinfo_size = 0;
   const struct legacy_objc_module *modules =
      find_module_info(mh, slide, &modinfo_size);
   if (!modules) { return; }
   const size_t nmodules = modinfo_size / sizeof(struct legacy_objc_module);
   const uint32_t before = g_legacy_reg_cnt;

   for (size_t i = 0; i < nmodules; ++i) {
      const struct legacy_objc_module *mod = &modules[i];
      if (mod->version != 7 || mod->size != sizeof(struct legacy_objc_module)) {
         continue;
      }
      if (!ptr_ok(mod->symtab, sizeof(struct legacy_objc_symtab))) { continue; }
      const struct legacy_objc_symtab *symtab =
         (const struct legacy_objc_symtab *)(uintptr_t)mod->symtab;
      const uint32_t ndefs =
         (uint32_t)symtab->cls_def_cnt + (uint32_t)symtab->cat_def_cnt;
      if (!ptr_ok(mod->symtab + (uint32_t)sizeof(*symtab),
                  (size_t)ndefs * sizeof(uint32_t))) { continue; }
      const uint32_t *defs = (const uint32_t *)
         ((const char *)symtab + sizeof(struct legacy_objc_symtab));

      for (uint16_t j = 0; j < symtab->cls_def_cnt; ++j) {
         if (!ptr_ok(defs[j], sizeof(struct legacy_objc_class))) { continue; }
         legacy_registry_insert(
            (const struct legacy_objc_class *)(uintptr_t)defs[j]);
      }
      for (uint16_t j = 0; j < symtab->cat_def_cnt; ++j) {
         uint32_t cref = defs[symtab->cls_def_cnt + j];
         if (!ptr_ok(cref, sizeof(struct legacy_objc_category))) { continue; }
         const struct legacy_objc_category *cat =
            (const struct legacy_objc_category *)(uintptr_t)cref;
         /* dedup by pointer (idempotent across repeated callbacks) */
         int seen = 0;
         for (uint32_t k = 0; k < g_legacy_cat_cnt; ++k) {
            if (g_legacy_cats[k] == cat) { seen = 1; break; }
         }
         if (!seen && g_legacy_cat_cnt < LEGACY_CAT_CAP) {
            g_legacy_cats[g_legacy_cat_cnt++] = cat;
         }
      }
   }

   /* Directly enumerate the __OBJC,__category section: translated __module_info
    * symtabs are frequently garbled/truncated, so the cat_def walk above reaches
    * only a fraction of an image's categories (iWeb.dylib: 44 of 581) — silently
    * dropping categories that carry methods the forward bridge must dispatch
    * (iWeb's Private category on BLCapabilitiesSingleton holds
    * pSetOperationalLicenseState:). The __category section holds every
    * legacy_objc_category struct (20 bytes) contiguously, independent of symtab
    * integrity. Universal: catches all categories in any image; dedup-by-pointer
    * keeps it idempotent across repeated callbacks and overlap with the cat_def
    * walk. (The legacy metadata model is low-4GB; bail if the section loaded high.) */
   size_t catsec_size = 0;
   const struct legacy_objc_category *cats =
      (const struct legacy_objc_category *)
         find_objc_section(mh, slide, "__category", &catsec_size);
   if (cats && (uintptr_t)cats <= 0xFFFFFFFFu) {
      const size_t ncats = catsec_size / sizeof(struct legacy_objc_category);
      for (size_t i = 0; i < ncats; ++i) {
         const struct legacy_objc_category *cat = &cats[i];
         if (!ptr_ok((uint32_t)(uintptr_t)cat, sizeof(*cat))) { continue; }
         /* A real category names a NON-EMPTY class AND category. The __category
          * section carries zero/garbage-named padding entries (iWeb.dylib: 168 of
          * 581) that the symtab cat_def path skipped — an empty class_name would
          * later strcmp-match an empty receiver name in find_category_* and walk a
          * garbage class_methods pointer (SIGSEGV in scan_one_list). ptr_ok alone
          * accepts a 0-length cstring, so check the first byte too. */
         if (!legacy_cstr_ok(cat->class_name) ||
             !legacy_cstr_ok(cat->category_name)) { continue; }
         if (*(const char *)(uintptr_t)cat->class_name == '\0') { continue; }
         int seen = 0;
         for (uint32_t k = 0; k < g_legacy_cat_cnt; ++k) {
            if (g_legacy_cats[k] == cat) { seen = 1; break; }
         }
         if (!seen && g_legacy_cat_cnt < LEGACY_CAT_CAP) {
            g_legacy_cats[g_legacy_cat_cnt++] = cat;
         }
      }
   }

   if (getenv("ABICONV_OBJC_SLIDE_VERBOSE") && g_legacy_reg_cnt != before) {
      fprintf(stderr, "objc_shim: indexed %u legacy classes (+%u this image), "
              "%u categories total\n", g_legacy_reg_cnt,
              g_legacy_reg_cnt - before, g_legacy_cat_cnt);
      fflush(stderr);
   }
}

/*
 * External ObjC-object DATA-constant shadows.
 *
 * An i386 reference to a Foundation data constant such as
 * `NSString * const NSArgumentDomain` compiles to a non-lazy symbol pointer
 * the linker fills with the symbol's address, followed by a double-deref in
 * code: `movl slot,%reg` (-> &NSArgumentDomain) then `movl (%reg),%reg`
 * (-> the NSString*). On x86_64 both the variable's address AND the object
 * pointer are 64-bit, so the i386 4-byte `movl` truncates them -> bad deref.
 *
 * abigen emits, for each such constant in the consider set, a low-4GB shadow
 * variable ___SYM plus an entry in the table below {&___SYM, "SYM"}.
 * static-interpose rewrites the binary's non-lazy bind for _SYM to point at
 * ___SYM (a low libabiconv __DATA address). This ctor populates each shadow
 * with a 32-bit proxy HANDLE wrapping the real 64-bit object, so the i386
 * double-deref (slot -> &___SYM -> handle) yields a usable id that the objc
 * bridge unwraps on the next message send.
 *
 * The table/count symbols are always defined by abigen (possibly empty).
 * Runs once per libabiconv copy; x64_objc_wrap uses the process-shared arena,
 * so handles minted here are valid across all copies.
 */
extern void    *x64_data_shadows[];      /* flat triples: [&shadow,name,info,...] */
extern uint64_t x64_data_shadows_count;  /* number of TRIPLES */

/* Object data-shadows that resolved to nil when the constructor ran are MUTABLE
 * object globals (the canonical case is NSApp, which is nil until the app
 * creates its NSApplication). The constructor snapshot would pin them to nil
 * forever, so record them here and refresh lazily from the forward bridge. */
#define X64_MAX_PENDING_SHADOWS 64
static uint64_t   *g_ps_shadow[X64_MAX_PENDING_SHADOWS];
static void       *g_ps_addr[X64_MAX_PENDING_SHADOWS];
static const char *g_ps_name[X64_MAX_PENDING_SHADOWS];
static int         g_ps_n;
static volatile int g_ps_remaining;

/* The table below used to be populated ONLY from a constructor. That is unsafe
 * for a libabiconv COPY: a bundle carries several co-located copies (MacOS/ and
 * one inside each bundled framework), dyld maps them all, but the initializers of
 * a given copy may never run — and a translated image binds its data-shadow
 * symbols to whichever copy dyld picked. When that copy is the uninitialized
 * one, EVERY ObjC/CF data constant reads nil. Civ IV: its bind for
 * kCFPreferencesCurrentApplication landed on MacOS/libabiconv.dylib (whose ctor
 * never ran) while the QuickTime.framework copy was the one that populated, so
 * CFPreferencesGetAppBooleanValue(@"ASLShowFPS", nil, ...) reached native
 * CoreFoundation, which dereferences the nil identifier -> deterministic
 * SIGSEGV before the launcher (the long-standing "rc=139 startup" crash).
 *
 * The proxy arena immediately above never had this problem because it has
 * always been LAZY (arena_init() on first use). Give the shadow table the same
 * treatment: an idempotent populate re-driven from the forward bridge, so
 * whichever copy actually executes bridge code populates its own table.
 * Triggers on the structural fact that this copy's table is unpopulated, never
 * on app identity, so it is a no-op wherever the constructor already ran.
 *
 * A dlsym miss (the defining framework is not loaded yet at populate time) no
 * longer silently pins the shadow to nil forever: it marks the pass PARTIAL and
 * a later pass re-runs once the dyld image count changes (i.e. new frameworks
 * arrived), which is the only cheap, structural "something might resolve now"
 * signal available. */
static os_unfair_lock  g_shadow_pop_lock = OS_UNFAIR_LOCK_INIT;
static volatile int      g_shadows_done;    /* a full pass has completed */
static volatile int      g_shadows_partial; /* >=1 dlsym miss: retry on new images */
static volatile uint32_t g_shadows_imgs;    /* _dyld_image_count() at last pass */

/* Populate ONE data-shadow table — ours, or that of a sibling libabiconv copy.
 * `track_pending` records object globals that are still nil at populate time
 * (NSApp) for later lazy refresh; only meaningful for our OWN table, since the
 * pending arrays are per-copy state. Returns 1 if any dlsym missed. */
static int x64_fill_shadow_table(void **tbl, uint64_t n, int track_pending,
                                 intptr_t delta) {
   int partial = 0;
   if (track_pending) { g_ps_n = 0; }   /* this pass rebuilds the pending list */
   for (uint64_t i = 0; i < n; ++i) {
      /* `tbl` is ALWAYS our own (fully fixed-up) table; for a sibling copy we
       * merely shift the destination by the image delta. Never read the
       * sibling's own table: when this runs from a constructor the sibling may
       * not have been rebased yet, so its entries would still hold raw file
       * values and we would scribble through wild pointers. */
      uint64_t *shadow = (uint64_t *)((uintptr_t)tbl[3 * i] + delta);
      const char *name = (const char *)tbl[3 * i + 1];   /* our copy's string */
      const uint64_t info = (uint64_t)tbl[3 * i + 2];
      if (!shadow || !name) { continue; }
      void *addr = dlsym(RTLD_DEFAULT, name);   /* &realvar */
      if (!addr) { partial = 1; continue; }     /* framework not loaded YET */
      if (info != 0) {
         /* SCALAR data constant: the i386 code derefs &shadow ONCE to read the
          * value, so the shadow must hold a low-4GB COPY of the value bytes
          * (info = byte width, 1..8). No handle wrap — the bits ARE the datum. */
         *shadow = 0;
         memcpy(shadow, addr, (size_t)info);
         continue;
      }
      uint64_t v = *(uint64_t *)addr;           /* the object pointer */
      if (v >= 0x100000000ULL) {
         *shadow = x64_objc_wrap(v);            /* 32-bit handle, zero-extended */
      } else if (v != 0) {
         *shadow = (uint32_t)v;                 /* already low; pass through */
      } else {
         /* nil at constructor time: a mutable object global set later (NSApp).
          * Pinning it now would leave the i386 single-deref reading nil; record
          * it for lazy refresh from the forward bridge. */
         *shadow = 0;
         if (track_pending && g_ps_n < X64_MAX_PENDING_SHADOWS) {
            g_ps_shadow[g_ps_n] = shadow;
            g_ps_addr[g_ps_n]   = addr;
            g_ps_name[g_ps_n]   = name;
            g_ps_n++;
         }
      }
   }
   return partial;
}

/* Fill the shadow table of every OTHER libabiconv copy mapped in this process.
 *
 * This is what actually cures the multi-copy case, and it does NOT depend on the
 * other copy ever executing any of our code: a copy whose initializers dyld
 * never ran would otherwise keep an all-nil table for the life of the process,
 * and it is precisely that copy a translated image may have bound its
 * data-shadow symbols to. Both copies ARE in dyld's image list even when only
 * one is initialized, so we can reach the other's table directly.
 *
 * Sibling identity is established structurally, never by bundle layout: an image
 * counts as a sibling only if its LC_UUID equals ours, i.e. it is byte-identical
 * to us. That makes the table reachable by pure arithmetic — same build means
 * our table sits at the same offset from the mach header — so we never call
 * dlopen/dlsym here. That matters: this runs from a static constructor, and
 * calling back into dyld's loader from an initializer risks deadlocking against
 * the very lock that is running us. Filling is idempotent — x64_objc_wrap dedups
 * through the process-shared arena, so a sibling's shadow gets the SAME handle
 * ours did, and the sibling's unwrap resolves it. */
static const uint8_t *x64_image_uuid(const struct mach_header_64 *h) {
   if (!h || h->magic != MH_MAGIC_64) { return NULL; }
   const struct load_command *lc = (const struct load_command *)(h + 1);
   for (uint32_t i = 0; i < h->ncmds; ++i) {
      if (lc->cmd == LC_UUID) { return ((const struct uuid_command *)lc)->uuid; }
      if (lc->cmdsize < sizeof *lc) { break; }
      lc = (const struct load_command *)((const uint8_t *)lc + lc->cmdsize);
   }
   return NULL;
}

static void x64_populate_sibling_tables(int *partial_io) {
   Dl_info self_info;
   if (!dladdr((void *)&g_shadows_done, &self_info) || !self_info.dli_fbase) { return; }
   const struct mach_header_64 *self_hdr =
      (const struct mach_header_64 *)self_info.dli_fbase;
   const uint8_t *self_uuid = x64_image_uuid(self_hdr);
   if (!self_uuid) { return; }
   /* our table symbols as offsets from our own mach header */
   const uintptr_t cnt_off = (uintptr_t)&x64_data_shadows_count - (uintptr_t)self_hdr;
   const int verbose = getenv("ABICONV_OBJC_SLIDE_VERBOSE") != NULL;
   for (uint32_t i = 0, n = _dyld_image_count(); i < n; ++i) {
      const struct mach_header_64 *hdr =
         (const struct mach_header_64 *)_dyld_get_image_header(i);
      if (!hdr || hdr == self_hdr) { continue; }
      const uint8_t *u = x64_image_uuid(hdr);
      if (!u || memcmp(u, self_uuid, 16) != 0) { continue; }   /* not our build */
      /* Canary: the sibling's count word sits at the same offset and is a plain
       * constant needing no fixup, so reading it proves its __DATA is mapped and
       * really is our build before we write a single shadow. */
      const uint64_t *cnt = (const uint64_t *)((uintptr_t)hdr + cnt_off);
      if (*cnt != x64_data_shadows_count) { continue; }
      const intptr_t delta = (intptr_t)((uintptr_t)hdr - (uintptr_t)self_hdr);
      if (x64_fill_shadow_table(x64_data_shadows, x64_data_shadows_count, 0, delta)) {
         if (partial_io) { *partial_io = 1; }
      }
      if (verbose) {
         const char *path = _dyld_get_image_name(i);
         fprintf(stderr, "objc_shim: filled %llu data-constant shadows in "
                         "sibling copy %s\n",
                 (unsigned long long)x64_data_shadows_count,
                 path ? path : "(unnamed)");
         fflush(stderr);
      }
   }
}

static void x64_populate_data_shadows(void) {
   const uint64_t n = x64_data_shadows_count;
   /* Serialize: the bridge drives this from any thread, and a pass rebuilds the
    * pending list from scratch. Re-running is otherwise harmless — x64_objc_wrap
    * dedups through the shared map, so a re-wrap yields the SAME handle the
    * translated code already holds. */
   os_unfair_lock_lock(&g_shadow_pop_lock);
   g_shadows_imgs = _dyld_image_count();
   int partial = x64_fill_shadow_table(x64_data_shadows, n, 1, 0);
   x64_populate_sibling_tables(&partial);
   g_ps_remaining    = g_ps_n;
   g_shadows_partial = partial;
   g_shadows_done    = 1;
   os_unfair_lock_unlock(&g_shadow_pop_lock);
   if (getenv("ABICONV_OBJC_SLIDE_VERBOSE")) {
      fprintf(stderr, "objc_shim: populated %llu data-constant shadows%s "
                      "(copy @%p)\n",
              (unsigned long long)n, partial ? " [PARTIAL: retry on new images]" : "",
              (void *)&g_shadows_done);
      fflush(stderr);
   }
}

/* Idempotent entry point. Cheap in the common case: one load of g_shadows_done.
 * A copy whose constructor never ran populates here on its first bridge call;
 * a copy that populated while some framework was still unloaded re-runs only
 * when the dyld image count has actually changed. */
static void x64_ensure_data_shadows(void) {
   if (__builtin_expect(!g_shadows_done, 0)) { x64_populate_data_shadows(); return; }
   if (__builtin_expect(g_shadows_partial, 0) &&
       _dyld_image_count() != g_shadows_imgs) {
      x64_populate_data_shadows();
   }
}

__attribute__((constructor))
static void x64_init_data_shadows(void) { x64_populate_data_shadows(); }

/* Look up a VALUE-copy data-shadow (scalar/small-record, info != 0) by symbol
 * name WITHOUT the leading underscore. Returns &shadow (a low-4GB slot) and its
 * byte width, and (re)fills it from the live native symbol via dlsym — so the
 * value is correct even if the constructor above ran before the owning
 * framework was loaded (e.g. HIToolbox's kHIViewWindowContentID, resolved only
 * when Civ's Carbon dependency is in). Object/CF shadows (info==0, handle-wrap)
 * are NOT served here — they need the objc arena + double-deref semantics that
 * the translate-time static-interpose path already provides. Used by
 * patch_import_pointers (objc_slide.c) to redirect a classic __IMPORT,__pointers
 * slot to the shadow at load time (the RESYNC-class application of the
 * translate-time data-shadow table for a value-typed data constant that a
 * 32-bit load would otherwise truncate). NULL if not a value-shadow. */
uint64_t *x64_value_data_shadow(const char *name_no_underscore) {
   const uint64_t n = x64_data_shadows_count;
   for (uint64_t i = 0; i < n; ++i) {
      const char *nm = (const char *)x64_data_shadows[3 * i + 1];
      const uint64_t info = (uint64_t)x64_data_shadows[3 * i + 2];
      if (info == 0 || !nm || strcmp(nm, name_no_underscore) != 0) { continue; }
      uint64_t *shadow = (uint64_t *)x64_data_shadows[3 * i];
      if (!shadow) { return NULL; }
      void *addr = dlsym(RTLD_DEFAULT, name_no_underscore);
      if (!addr) { return NULL; }       /* owning framework not loaded yet */
      *shadow = 0;
      memcpy(shadow, addr, info > 8 ? 8 : (size_t)info);
      return shadow;
   }
   return NULL;
}

/* Lazily resolve object data-shadows that were nil at constructor time (mutable
 * object globals like NSApp, which AppKit sets only once the app's
 * NSApplication exists). Called from the forward bridge on each translated
 * msgSend; a single guarded load makes it a no-op once everything is resolved.
 * x64_objc_wrap dedups via the shared map, so the handle matches the one the
 * translated code already holds for that instance. */
void x64_refresh_data_shadows(void) {
   if (g_ps_remaining <= 0) { return; }
   int rem = 0;
   for (int i = 0; i < g_ps_n; ++i) {
      uint64_t *shadow = g_ps_shadow[i];
      if (!shadow) { continue; }                 /* already resolved */
      uint64_t v = *(uint64_t *)g_ps_addr[i];
      if (v == 0) { ++rem; continue; }           /* still nil */
      *shadow = (v >= 0x100000000ULL) ? x64_objc_wrap(v) : (uint32_t)v;
      g_ps_shadow[i] = NULL;                      /* mark done (benign race) */
      if (getenv("ABICONV_SHADOW_TRACE")) {
         fprintf(stderr, "[shadow] late-resolved %s -> 0x%x\n",
                 g_ps_name[i], (uint32_t)*shadow);
         fflush(stderr);
      }
   }
   g_ps_remaining = rem;
}

/* ======================================================================
 * Reverse bridge: modern x86_64 ObjC runtime -> translated i386 legacy IMP.
 *
 * Registration (reverse_register_image) creates a real modern class for each
 * legacy __OBJC class, with the SINGLE generic IMP _86x64_reverse_imp
 * (objc_reverse.asm) for every method. The legacy class's instance ivars are
 * NOT added to the modern class — they live in a low-4GB i386-layout SHADOW
 * buffer per instance, so the translated IMPs (which access ivars at hardcoded
 * i386 offsets) read/write the shadow, while the real modern instance carries
 * the superclass (e.g. NSApplication) state for AppKit/[super ...].
 * ====================================================================== */

#include <objc/message.h>

struct reverse_plan {
   uint64_t legacy_imp;
   uint64_t lowstack_top;
   uint64_t lowstack_base;
   int32_t  ret_kind;        /* 0 scalar, 1 object(unwrap), 2 void, 3 stret,
                              * 4 fp (st0 -> fp_out[0] -> xmm0),
                              * 6 small struct (eax:edx or i386 hidden buf ->
                              *   widened to rax/rdx/xmm0/xmm1) */
   uint32_t frame_words;
   uint32_t frame[64];
   /* stret (ret_kind 3): the i386 IMP wrote the struct (i386 layout, hidden-
    * pointer convention) at stret_src on the lowstack; reverse_ret widens it
    * field-by-field into the native caller's buffer stret_dst. Kind 6 with
    * isz>8 reuses stret_src as the source. The asm reads only fp_out (+312:
    * st0 bank-in at %%back, native xmm0:xmm1 loads at %%out). */
   uint64_t    stret_dst;    /* +288 */
   uint32_t    stret_src;    /* +296 */
   uint32_t    _pad;
   const char *stret_types;  /* +304 legacy method encoding (ret type first) */
   uint64_t    fp_out[2];    /* +312 */
   /* C-only inherited-ivar write-back bookkeeping (the asm never reads past
    * +320; these live in the 560-byte plan reservation's slack). */
   uint32_t    wb_active;    /* +328 1 if sync_inherited_ivars pushed a frame */
   uint32_t    wb_mark;      /* +332 g_snap index at this call's frame start */
   /* Native-super value-dispatch (fills 48211bb's gap): when a legacy [super sel]
    * targets a NATIVE super method that RETURNS A VALUE, reverse_prep invokes it
    * and stashes the native return here; reverse_ret returns it instead of nil.
    * C-only, in the 560-byte reservation slack past +320 (asm never reads it). */
   uint64_t    native_super_ret; /* +336 native super method's return (in rax) */
   uint32_t    has_native_super; /* +344 1 = return native_super_ret, no i386 IMP */
   /* Reverse out-object copy-back (native `out id*` / `^@`): each such arg gets
    * an i386-writable scratch slot (the out-ptr itself when it is <4GB, else a
    * shim-malloc'd low-4GB slot); reverse_ret unwraps the handle the i386 method
    * wrote into it and stores the full 64-bit native object into the caller's
    * native *id (native_out). Freed only when native_out>=4GB (the malloc'd
    * case). Per-call (plan is stack-allocated) => reentrant. C-only slack +320. */
   uint32_t    n_out_obj;        /* +348 count of out-object params this call */
   struct { uint32_t scratch; uint64_t native_out; } out_obj[4];  /* +352.. */
   /* Legacy draw-clip contract (see the block at the end of reverse_prep): the
    * CGContext we CGContextSaveGState'd + clipped before invoking a legacy
    * drawRect: / NSCell-draw IMP; reverse_ret restores it. 0 = nothing to
    * restore. C-only, in the plan reservation slack (asm never reads past +320). */
   uint64_t    draw_clip_ctx;
};

extern void _86x64_reverse_imp(void);        /* objc_reverse.asm */
extern void _86x64_reverse_imp_stret(void);  /* objc_reverse.asm */

/* ---- (lookup_class, sel) -> legacy method map. lookup_class is
 * object_getClass(self): the registered class for instance methods, its
 * metaclass for class methods. ---- */
/*
 * Core variadic detector (see legacy_method_is_variadic's comment for the why):
 * scan a legacy (translated) IMP's prologue for the i386 `va_start` idiom —
 * `lea (8+framesize)(%rbp), r32`, taking the address of the first stack slot
 * PAST the declared args, which a non-variadic cdecl method never does.
 * `framesize` = the i386 arg-frame byte count, the digits after the return type
 * in the encoding. Pure read; no caching (callers memoize).
 */
static int imp_is_variadic(uint64_t imp, const char *types) {
   if (!imp || !types) { return 0; }
   long F = strtol(enc_skip_type(types), NULL, 10);   /* arg-frame bytes */
   if (F < 12) { return 0; }                  /* need self+_cmd+>=1 fixed arg */
   long want = 8 + F;                          /* ebp offset of first vararg */
   if (want > 0x7fffffff) { return 0; }
   const uint8_t *c = (const uint8_t *)(uintptr_t)imp;
   /* Window must clear an expanded prologue: a PIC get_pc_thunk (i386
    * `call .+0;pop` -> a ~25-byte `lea rip;push;jmp;pop` in the translation)
    * plus saved regs/zeroed locals can push va_start ~70 bytes in (vs ~40 for a
    * non-PIC 2006 binary). A bigger window is false-positive-safe: only va_start
    * `lea`s the slot PAST the last declared arg; a non-variadic method's leas of
    * its own args/locals all use offsets < want. */
#define VA_SCAN 192
   if (!mem_readable((uintptr_t)c, VA_SCAN)) { return 0; }
   for (int i = 0; i + 6 < VA_SCAN; ++i) {
      int j = i;
      /* optional REX that does NOT set .B (so rm=101 still means rbp): the
       * va_start lea may target r8d-r15d (REX.R) but never re-bases off .B. */
      if ((c[j] & 0xf8) == 0x40 && !(c[j] & 0x01)) { ++j; }
      if (c[j] != 0x8d) { continue; }                  /* lea */
      uint8_t modrm = c[j + 1];
      if ((modrm & 0x07) != 0x05) { continue; }        /* rm == rbp */
      unsigned mod = modrm >> 6;
      long disp;
      if (mod == 1) { disp = (int8_t)c[j + 2]; }
      else if (mod == 2) { int32_t d; memcpy(&d, &c[j + 2], 4); disp = d; }
      else { continue; }                               /* mod 0 = rip-rel */
      if (disp == want) { return 1; }
   }
   return 0;
#undef VA_SCAN
}

/* Shared cross-copy registry of variadic legacy methods (ctrl->rvariadic):
 * populated by the registering copy at rmeth_insert, queried by the forward
 * bridge (which runs in the caller's copy, with no local g_rmeth). */
static uint32_t rvar_hash(Class c, SEL s) {
   uint64_t h = (uint64_t)(uintptr_t)c * 1099511628211ULL ^ (uint64_t)(uintptr_t)s;
   return (uint32_t)((h ^ (h >> 32)) & (RVAR_CAP - 1));
}
static void rvar_insert(Class c, SEL s) {
   if (!g_ctrl || !g_ctrl->rvariadic || !c || !s) { return; }
   struct rvar_ent *t = (struct rvar_ent *)(uintptr_t)g_ctrl->rvariadic;
   uint32_t i = rvar_hash(c, s);
   for (uint32_t n = 0; n < RVAR_CAP; ++n) {
      if (t[i].cls == NULL || (t[i].cls == c && t[i].sel == s)) {
         t[i].cls = c; t[i].sel = s; return;
      }
      i = (i + 1) & (RVAR_CAP - 1);
   }
}
static int rvar_lookup1(Class c, SEL s) {
   if (!g_ctrl || !g_ctrl->rvariadic || !c) { return 0; }
   struct rvar_ent *t = (struct rvar_ent *)(uintptr_t)g_ctrl->rvariadic;
   uint32_t i = rvar_hash(c, s);
   for (uint32_t n = 0; n < RVAR_CAP; ++n) {
      if (t[i].cls == NULL) { return 0; }
      if (t[i].cls == c && t[i].sel == s) { return 1; }
      i = (i + 1) & (RVAR_CAP - 1);
   }
   return 0;
}
/* Walk the superclass chain so an inherited variadic legacy method is found
 * (mirrors reverse_prep's owner walk); only reached for legacy receivers. */
static int rvar_contains(Class start, SEL s) {
   for (Class c = start; c; c = class_getSuperclass(c)) {
      if (rvar_lookup1(c, s)) { return 1; }
   }
   return 0;
}

/* power of two; keyed by (class,selector) so it must exceed the TOTAL legacy
 * method count across all loaded frameworks (iWork SF*: 100k+). Saturation
 * here both hangs registration (O(n) per insert) and silently drops method
 * IMPs, breaking later reverse dispatch — see SELTYPES_CAP. */
#define RMETH_CAP 262144u
struct rmeth_ent { Class cls; SEL sel; uint64_t imp; const char *types;
                   int8_t variadic; /* -1 unknown, 0 no, 1 yes (cached) */ };
static struct rmeth_ent g_rmeth[RMETH_CAP];
static uint32_t rmeth_hash(Class c, SEL s) {
   uint64_t h = (uint64_t)(uintptr_t)c * 1099511628211ULL ^ (uint64_t)(uintptr_t)s;
   return (uint32_t)((h ^ (h >> 32)) & (RMETH_CAP - 1));
}
static void rmeth_insert(Class c, SEL s, uint64_t imp, const char *types) {
   uint32_t i = rmeth_hash(c, s);
   for (uint32_t n = 0; n < RMETH_CAP; ++n) {
      if (g_rmeth[i].imp == 0 || (g_rmeth[i].cls == c && g_rmeth[i].sel == s)) {
         g_rmeth[i].cls = c; g_rmeth[i].sel = s;
         g_rmeth[i].imp = imp; g_rmeth[i].types = types;
         g_rmeth[i].variadic = (int8_t)imp_is_variadic(imp, types);
         /* publish variadic legacy methods cross-copy for the forward bridge */
         if (g_rmeth[i].variadic) { rvar_insert(c, s); }
         return;
      }
      i = (i + 1) & (RMETH_CAP - 1);
   }
}
static struct rmeth_ent *rmeth_lookup(Class c, SEL s) {
   uint32_t i = rmeth_hash(c, s);
   for (uint32_t n = 0; n < RMETH_CAP; ++n) {
      if (g_rmeth[i].imp == 0) { return NULL; }
      if (g_rmeth[i].cls == c && g_rmeth[i].sel == s) { return &g_rmeth[i]; }
      i = (i + 1) & (RMETH_CAP - 1);
   }
   return NULL;
}

/* The owning reverse-IMP entry for (start-class, sel): the dispatch path keys
 * g_rmeth on the class that actually registered the legacy method, which for an
 * inherited selector is an ancestor — so walk up exactly like reverse_prep. */
static struct rmeth_ent *rmeth_find_owner(Class start, SEL s) {
   for (Class c = start; c; c = class_getSuperclass(c)) {
      struct rmeth_ent *e = rmeth_lookup(c, s);
      if (e) { return e; }
   }
   return NULL;
}

/*
 * Is this legacy (reverse-bridge) method VARIADIC — i.e. does it take a
 * nil-terminated id list past its single declared argument (e.g. an i386
 * `-(void)addAnimations:(ATAnimation*)first, ...`)?  The ObjC type encoding
 * never models `...` (addAnimations: encodes as the plain `v12@0:4@8`), so the
 * forward/reverse marshallers would otherwise stop at the first declared arg
 * and the IMP's va_arg walk would read leftover lowstack words as bogus object
 * pointers (the Quinn 0x00080000 garbage-receiver sort crash).
 *
 * Detected STRUCTURALLY, not by selector/class name: the i386 `va_start`
 * idiom takes the address of the first stack slot PAST the fixed args
 * (`leal (8+framesize)(%ebp), %reg`), which a non-variadic cdecl method never
 * does (its locals live at NEGATIVE %ebp offsets). The translator copies the
 * i386 frame semantics verbatim — %rbp stays the i386 %ebp — so the translated
 * IMP prologue still carries `lea (8+framesize)(%rbp), r32`. `framesize` is the
 * total i386 arg-frame byte count, the digits right after the return type in
 * the encoding. Result cached on the rmeth entry (computed once per method).
 */
static int legacy_method_is_variadic(struct rmeth_ent *m) {
   if (!m) { return 0; }
   if (m->variadic >= 0) { return m->variadic; }
   m->variadic = (int8_t)imp_is_variadic(m->imp, m->types);
   return m->variadic;
}

static uint64_t legacy_instance_method_imp(Class c, SEL s) {
   /* Resolve through the raw __OBJC metadata (indexed in EVERY libabiconv copy),
    * NOT the per-copy g_rmeth table: the forward bridge runs in the caller's copy,
    * whose g_rmeth is empty for a class some OTHER copy reverse-registered. */
   const char *cn = class_getName(c);
   return cn ? legacy_instance_method_imp_byname(cn, sel_getName(s)) : 0;
}

/* ---- thread-local super-dispatch hint (see objc_bridge_prep_super) ----
 * A legacy `[super sel]` resolves (via objc_msgSendSuper) to the SUPER's
 * reverse_imp method, but reverse_prep would otherwise re-derive the method
 * from object_getClass(self) = the RECEIVER's most-derived class and run the
 * subclass override again — infinite recursion when sub and super both define
 * sel (the iPhoto 10th blocker: +[IPPublishPluginManager initialize] calling
 * [super initialize] where IP_IPHPluginManager also defines +initialize).
 * objc_bridge_prep_super records the resolved super lookup-class here ONLY
 * when that super method is itself a legacy method; the very next reverse_prep
 * for the same (self,sel) consumes it. Thread-local + consume-once makes it
 * exact: the super-send synchronously dispatches straight into reverse_imp on
 * the same thread, with no intervening reverse_prep. */
static uint64_t super_hint_tid(void) {
   uint64_t t = 0;
   pthread_threadid_np(NULL, &t);
   return t ? t : 1;
}

/* Record the resolved super lookup-class for the calling thread. No local
 * rmeth gating here: this runs in the caller's copy whose g_rmeth may be empty
 * (cross-copy). reverse_prep (registering copy) validates against the real map
 * and falls back if the hint doesn't name a legacy method. */
static void reverse_note_super(id recv, SEL sel, Class super_lookup) {
   arena_init();
   if (!g_ctrl || !super_lookup) { return; }
   uint64_t tid = super_hint_tid();
   unsigned i = (unsigned)(tid % SUPER_HINT_SLOTS);
   g_ctrl->super_hints[i].recv = (uint64_t)(uintptr_t)recv;
   g_ctrl->super_hints[i].sel = (uint64_t)(uintptr_t)sel;
   g_ctrl->super_hints[i].lookup = (uint64_t)(uintptr_t)super_lookup;
   g_ctrl->super_hints[i].tid = tid;
   g_ctrl->super_hints[i].valid = 1;
   if (getenv("OBJC_SUPER_TRACE")) {
      fprintf(stderr, "[note_super] tid=%llu recv=%p sel=%s super_lookup=%s\n",
              (unsigned long long)tid, (void*)recv, sel_getName(sel),
              class_getName(super_lookup));
   }
}

/* Consume this thread's super hint if it matches (self,sel). Returns the
 * super lookup-class (validated to name a legacy method) or NULL. */
static Class reverse_take_super(id self_, SEL sel) {
   if (!g_ctrl) { return NULL; }
   uint64_t tid = super_hint_tid();
   unsigned i = (unsigned)(tid % SUPER_HINT_SLOTS);
   if (!g_ctrl->super_hints[i].valid ||
       g_ctrl->super_hints[i].tid != tid ||
       g_ctrl->super_hints[i].recv != (uint64_t)(uintptr_t)self_ ||
       g_ctrl->super_hints[i].sel != (uint64_t)(uintptr_t)sel) {
      return NULL;
   }
   g_ctrl->super_hints[i].valid = 0;   /* consume-once */
   return (Class)(uintptr_t)g_ctrl->super_hints[i].lookup;
}

/* ---- registered Class -> legacy class metadata addr + instance_size ---- */
static uint32_t rcls_hash(Class c) {
   uint64_t h = (uint64_t)(uintptr_t)c * 2654435761ULL;
   return (uint32_t)((h ^ (h >> 32)) & (RCLS_CAP - 1));
}
static void rcls_insert(Class c, uint32_t addr, uint32_t isz) {
   if (!g_ctrl || !g_ctrl->rcls) { return; }
   struct rcls_ent *t = (struct rcls_ent *)(uintptr_t)g_ctrl->rcls;
   uint32_t i = rcls_hash(c);
   for (uint32_t n = 0; n < RCLS_CAP; ++n) {
      if (!t[i].cls || t[i].cls == c) {
         t[i].legacy_addr = addr; t[i].instance_size = isz;
         __sync_synchronize();        /* publish size/addr before the key */
         t[i].cls = c; return;
      }
      i = (i + 1) & (RCLS_CAP - 1);
   }
}
static struct rcls_ent *rcls_lookup(Class c) {
   if (!g_ctrl || !g_ctrl->rcls) { return NULL; }
   struct rcls_ent *t = (struct rcls_ent *)(uintptr_t)g_ctrl->rcls;
   uint32_t i = rcls_hash(c);
   for (uint32_t n = 0; n < RCLS_CAP; ++n) {
      if (!t[i].cls) { return NULL; }
      if (t[i].cls == c) { return &t[i]; }
      i = (i + 1) & (RCLS_CAP - 1);
   }
   return NULL;
}

/* ---- i386 shadow arena: per legacy-class instance, a low-4GB i386-layout
 * buffer preceded by an 8-byte header holding the real object pointer, so the
 * forward bridge can map a shadow address back to its real object. ---- */
/* The shadow arena lives in the shared objc_shared_ctrl (see arena_init) so
 * every libabiconv copy agrees on its extent. */
static void shadow_arena_init(void) {
   arena_init();                        /* ensure g_ctrl is attached/created */
   if (!g_ctrl || g_ctrl->shadow_base) { return; }
   const size_t SZ = 64u * 1024u * 1024u;   /* 64MB i386 instance space */
   for (uintptr_t a = 0x90000000UL; a + SZ <= 0xF0000000UL; a += SZ) {
      mach_vm_address_t addr = a;
      if (mach_vm_allocate(mach_task_self(), &addr, SZ, VM_FLAGS_FIXED)
          == KERN_SUCCESS) {
         g_ctrl->shadow_base = (uint64_t)addr;
         g_ctrl->shadow_cur  = (uint64_t)addr;
         g_ctrl->shadow_end  = (uint64_t)addr + SZ;
         return;
      }
   }
   fprintf(stderr, "objc_shim: no low-4GB region for shadow arena\n");
}
static int is_shadow(uint32_t s) {
   if (!g_ctrl || !s) { return 0; }
   return (uint64_t)s >= g_ctrl->shadow_base + 8 && (uint64_t)s < g_ctrl->shadow_cur;
}
/* Stable associated-object key shared across all libabiconv copies: g_ctrl is
 * the same pointer in every copy (published via env, adopted by address), so
 * its address is a process-stable, collision-free key. Lets a shadow attached
 * by the forward arg-bridge (any copy) be found by get_or_create_shadow (the
 * registering copy). */
static const void *shadow_assoc_key(void) { return (const void *)g_ctrl; }

static id shadow_real(uint32_t s) {
   if (is_shadow(s)) { return *(id *)((uintptr_t)s - 8); }
   /* A raw legacy i386 instance we've already paired with a real modern R'. */
   return lpair_lookup(s);
}
static uint32_t get_or_create_shadow(id real, Class cls) {
   const void *assoc_key = shadow_assoc_key();
   uint32_t s = (uint32_t)(uintptr_t)objc_getAssociatedObject(real, assoc_key);
   if (s) { return s; }
   shadow_arena_init();
   if (!g_ctrl || !g_ctrl->shadow_base) { return 0; }
   struct rcls_ent *ce = rcls_lookup(cls);
   uint32_t isz = ce ? ce->instance_size : 64;
   uint32_t isa = ce ? ce->legacy_addr : 0;
   /* A legacy class's reported instance_size is unreliable: iPhoto's
    * PhotoCDManager reports 20 although its superclass IP_IPHostReachabilityMgr
    * reports 52, and even the latter's -init writes an ivar past its own 52.
    * A subclass -init runs [super init], which writes the SUPERCLASS's ivars at
    * the superclass's (larger) i386 offsets, so the shadow must cover the whole
    * chain. Take the max instance_size over cls and its registered ancestors,
    * then add slack to absorb the under-reporting. */
   for (Class sc = class_getSuperclass(cls); sc; sc = class_getSuperclass(sc)) {
      struct rcls_ent *se = rcls_lookup(sc);
      if (se && se->instance_size > isz) { isz = se->instance_size; }
   }
   if (isz < 4) { isz = 4; }
   size_t need = 8 + ((isz + 15) & ~(size_t)15) + SHADOW_SLACK;
   /* Atomic bump: concurrent reverse-bridge instance creation on multiple
    * threads (background reachability mgr + main-thread PhotoCDManager) would
    * otherwise lose-update shadow_cur and hand two instances the same buffer,
    * so one instance's ivar writes clobber the other's real-object header
    * (-> object_getClass on a garbage receiver). */
   uint64_t hdr64 = __atomic_fetch_add(&g_ctrl->shadow_cur, need,
                                       __ATOMIC_SEQ_CST);
   if (hdr64 + need > g_ctrl->shadow_end) { return 0; }   /* arena exhausted */
   uintptr_t hdr = (uintptr_t)hdr64;
   *(uint64_t *)hdr = (uint64_t)real;
   uintptr_t s2 = hdr + 8;
   memset((void *)s2, 0, isz);
   *(uint32_t *)s2 = isa;          /* i386 object's isa slot */
   s = (uint32_t)s2;
   objc_setAssociatedObject(real, assoc_key, (id)(uintptr_t)s,
                            OBJC_ASSOCIATION_ASSIGN);
   return s;
}

/* ---- inherited native-superclass ivar sync ------------------------------
 * A legacy instance keeps its ivars in a zeroed i386-layout SHADOW; a native
 * superclass's ivars live ONLY in the real x86_64 object. iPhoto's legacy
 * subclasses read/write INHERITED protected ivars directly at hardcoded i386
 * (fragile-ABI, ABSOLUTE) offsets (e.g. NSControl._cell @84) -> they hit the
 * zeroed shadow -> 0/nil -> garbage geometry (collapsed sidebar, 1x1 card
 * text). We keep the shadow's inherited region coherent with the real object
 * in BOTH directions, around each reverse-bridge dispatch of a legacy IMP:
 *   - on ENTRY (sync_inherited_ivars): real[x64] -> shadow[i386], so the IMP's
 *     direct reads of inherited ivars see the live native values;
 *   - on EXIT  (ivar_wb_flush): shadow[i386] -> real[x64] for ONLY the slots
 *     the IMP actually changed (dirty-tracked against the entry snapshot), so
 *     native superclass code / KVC / archiving see the IMP's direct writes,
 *     without clobbering ivars a nested native call may have changed meanwhile.
 * The i386 offset/width come from the generated map (10.6 framework __OBJC
 * metadata); the x64 offset/width/signedness are resolved at RUNTIME by name
 * (class_getInstanceVariable + ivar_getTypeEncoding), so the converter is
 * robust to the modern runtime's layout drift (NSInteger widened 4->8, CGFloat
 * float->double, reordered/removed ivars skipped). See gen_native_ivar_map.py. */
#include "native_ivar_map.gen.h"

static int nivar_cls_cmp(const void *k, const void *e) {
   return strcmp((const char *)k, ((const struct nivar_cls *)e)->cls);
}
static const struct nivar_cls *native_ivar_map_lookup(const char *cls) {
   if (!cls) { return NULL; }
   /* g_native_ivar_map is emitted sorted by class name (Python sorted()). */
   return (const struct nivar_cls *)bsearch(cls, g_native_ivar_map,
      G_NATIVE_IVAR_MAP_N, sizeof(struct nivar_cls), nivar_cls_cmp);
}

/* modern x86_64 scalar/pointer width (bytes) + signedness from an @encode
 * first char; 0 = unhandled (skip). */
static uint32_t modern_enc_size(const char *enc, int *is_signed) {
   if (is_signed) { *is_signed = 0; }
   if (!enc || !enc[0]) { return 0; }
   switch (enc[0]) {
      case 'c': case 'B': if (is_signed) { *is_signed = (enc[0] == 'c'); } return 1;
      case 'C':           return 1;
      case 's': if (is_signed) { *is_signed = 1; } return 2;
      case 'S': return 2;
      case 'i': if (is_signed) { *is_signed = 1; } return 4;
      case 'I': return 4;
      case 'l': case 'q': if (is_signed) { *is_signed = 1; } return 8; /* LP64 */
      case 'L': case 'Q': return 8;
      case 'f': return 4;
      case 'd': return 8;
      case '@': case '#': case ':': case '^': case '*': return 8;
      default:  return 0;
   }
}

/* Per-call write-back snapshot stack (thread-local; reverse_prep/ret nest LIFO,
 * each frame is [plan->wb_mark, g_snap_n)). Records what we deposited into each
 * inherited shadow slot at entry so the exit flush writes back ONLY the slots
 * the IMP changed. i386 slots are <=4 bytes (8-byte i386 scalars are not
 * mapped), so the snapshot fits in a uint32_t. */
struct ivar_snap {
   uint8_t *real_slot;     /* &real[x64_off]   (write-back target) */
   uint8_t *shadow_slot;   /* &shadow[i386_off] (the IMP's working copy) */
   uint32_t snap;          /* value deposited at entry (i386_sz low bytes) */
   uint8_t  kind;          /* 'P' object/class ptr, 'F' float, 'I' integer */
   uint8_t  i386_sz;       /* 1/2/4 */
   uint8_t  msz;           /* modern slot width 1/2/4/8 */
   uint8_t  msign;         /* modern signedness (sign-extend ints when widening) */
};
static __thread struct ivar_snap *g_snap;
static __thread uint32_t g_snap_n, g_snap_cap;

/* Push one dirty-tracking snapshot frame (shared by the inherited-ivar sync and
 * the own-ivar/outlet sync below). g_snap grows LIFO; ivar_wb_flush reads it. */
static void ivar_snap_push(uint8_t *real_slot, uint8_t *shadow_slot, uint32_t val,
                           uint8_t kind, uint8_t i386_sz, uint8_t msz, uint8_t msign) {
   if (g_snap_n == g_snap_cap) {
      uint32_t nc = g_snap_cap ? g_snap_cap * 2 : 64;
      struct ivar_snap *ns = realloc(g_snap, (size_t)nc * sizeof *ns);
      if (!ns) { return; }                    /* OOM: skip write-back tracking */
      g_snap = ns; g_snap_cap = nc;
   }
   struct ivar_snap *s = &g_snap[g_snap_n++];
   s->real_slot = real_slot; s->shadow_slot = shadow_slot; s->snap = val;
   s->kind = kind; s->i386_sz = i386_sz; s->msz = msz; s->msign = msign;
}

/* ---- legacy class OWN object-ivar (nib outlet) registration + R->S sync -----
 * A reverse-registered legacy class is built via objc_allocateClassPair(...,0)
 * with NO ivars, so the modern runtime can't find "myBoardView"/"myPlayerInfoView"
 * etc.: -[NSNibOutletConnector establishConnection] -> object_setInstanceVariable
 * -> class_getInstanceVariable == NULL -> logs "missing setter or instance
 * variable" and leaves the outlet nil (Quinn: board fills the window + the stats
 * sidebar & image wells never wire up; classic-ObjC1 nib apps at large). We
 * register each legacy class's OWN object-typed ivars on the modern class
 * (class_addIvar), so the nib connector writes the connected NATIVE object into
 * the real object R at a modern offset; then, on each reverse dispatch, we mirror
 * those connected slots into the i386 SHADOW S the translated IMP reads (native
 * ptr -> wrapped 32-bit handle) at the classic i386 offset. Only slots the nib
 * actually connected (R non-nil) are mirrored, so ivars a legacy IMP manages
 * purely in the shadow are never disturbed (no regression to the shadow-only
 * ExtendedApplication/iPhoto path). The existing ivar_wb_flush 'P' write-back
 * carries an IMP's reassignment back to R. UNIVERSAL: any classic-ObjC1 outlet.
 * Gate ABICONV_NO_OUTLET_IVARS restores the old no-ivar behavior. */
struct own_ivar {
   const char *name;      /* legacy ivar name (app __OBJC cstring, persistent) */
   uint32_t    i386_off;  /* classic ivar offset (translated IMP reads here)   */
   int32_t     moff;      /* modern ivar offset in R (resolved post-register)  */
};
struct own_ivars_ent {
   Class            cls;
   struct own_ivar *iv;
   uint32_t         n, cap;
};
#define OWN_CAP 4096u                          /* power of two */
static struct own_ivars_ent g_own[OWN_CAP];    /* per-copy: only the registering
                                                * copy runs registration + sync  */
static uint32_t own_hash(Class c) {
   uint64_t h = (uint64_t)(uintptr_t)c * 2654435761ULL;
   return (uint32_t)((h ^ (h >> 32)) & (OWN_CAP - 1));
}
static struct own_ivars_ent *own_lookup(Class c) {
   uint32_t i = own_hash(c);
   for (uint32_t n = 0; n < OWN_CAP; ++n) {
      if (!g_own[i].cls) { return NULL; }
      if (g_own[i].cls == c) { return &g_own[i]; }
      i = (i + 1) & (OWN_CAP - 1);
   }
   return NULL;
}
static struct own_ivars_ent *own_intern(Class c) {   /* find or create slot */
   uint32_t i = own_hash(c);
   for (uint32_t n = 0; n < OWN_CAP; ++n) {
      if (g_own[i].cls == c) { return &g_own[i]; }
      if (!g_own[i].cls) { g_own[i].cls = c; return &g_own[i]; }
      i = (i + 1) & (OWN_CAP - 1);
   }
   return NULL;
}

/* Registration (before objc_registerClassPair): add each OWN object ivar to the
 * modern class so the nib outlet connector can find & write it. */
static void reverse_add_ivars(Class target, const struct legacy_objc_class *cls) {
   static int disabled = -1;
   if (disabled < 0) { disabled = getenv("ABICONV_NO_OUTLET_IVARS") ? 1 : 0; }
   if (disabled) { return; }
   if (!cls->ivars ||
       !ptr_ok(cls->ivars, sizeof(struct legacy_objc_ivar_list))) { return; }
   const struct legacy_objc_ivar_list *ivl =
      (const struct legacy_objc_ivar_list *)(uintptr_t)cls->ivars;
   int32_t cnt = ivl->ivar_count;
   if (cnt <= 0 || cnt > 4096) { return; }
   if (!ptr_ok(cls->ivars, sizeof(struct legacy_objc_ivar_list)
                           + (size_t)cnt * sizeof(struct legacy_objc_ivar))) { return; }
   const struct legacy_objc_ivar *ivs = (const struct legacy_objc_ivar *)(ivl + 1);
   Class super = class_getSuperclass(target);
   struct own_ivars_ent *e = NULL;
   for (int32_t k = 0; k < cnt; ++k) {
      if (!legacy_cstr_ok(ivs[k].name) || !legacy_cstr_ok(ivs[k].type)) { continue; }
      const char *name = (const char *)(uintptr_t)ivs[k].name;
      const char *type = (const char *)(uintptr_t)ivs[k].type;
      if (type[0] != '@' && type[0] != '#') { continue; }  /* only connectable objects */
      /* don't shadow an inherited ivar (native super sync owns those). */
      if (super && class_getInstanceVariable(super, name)) { continue; }
      if (!class_addIvar(target, name, sizeof(id),
                         (uint8_t)__builtin_ctz((unsigned)sizeof(id)), type)) { continue; }
      if (!e) { e = own_intern(target); if (!e) { return; } }
      if (e->n == e->cap) {
         uint32_t nc = e->cap ? e->cap * 2 : 8;
         struct own_ivar *ni = realloc(e->iv, (size_t)nc * sizeof *ni);
         if (!ni) { return; }
         e->iv = ni; e->cap = nc;
      }
      e->iv[e->n].name = name;
      e->iv[e->n].i386_off = ivs[k].offset;
      e->iv[e->n].moff = -1;
      e->n++;
   }
}

/* Resolve modern ivar offsets (valid only after objc_registerClassPair). */
static void own_resolve_offsets(Class target) {
   struct own_ivars_ent *e = own_lookup(target);
   if (!e) { return; }
   for (uint32_t k = 0; k < e->n; ++k) {
      Ivar iv = class_getInstanceVariable(target, e->iv[k].name);
      e->iv[k].moff = iv ? (int32_t)ivar_getOffset(iv) : -1;
   }
}

/* SYNC (reverse-dispatch entry): mirror class c's nib-connected OWN object
 * ivars from the real object R into the i386 shadow S. Only non-nil (actually
 * connected) slots are mirrored; a wrapped handle is deposited so the IMP's
 * `[outlet ...]` round-trips through the forward bridge. Snapshotted for
 * ivar_wb_flush so an IMP reassignment carries back to R. */
static void push_own_object_ivars(uint8_t *sh, id real, Class c) {
   struct own_ivars_ent *e = own_lookup(c);
   if (!e) { return; }
   for (uint32_t k = 0; k < e->n; ++k) {
      struct own_ivar *iv = &e->iv[k];
      if (iv->moff < 0 || iv->i386_off > 0x8000u) { continue; }
      uint8_t *src = (uint8_t *)real + iv->moff;    /* R modern slot (8B ptr) */
      if (!mem_readable((uintptr_t)src, 8)) { continue; }
      uint64_t p; memcpy(&p, src, 8);
      if (!p) { continue; }                          /* not connected: leave shadow */
      uint32_t val = x64_objc_wrap(p);
      uint8_t *dst = sh + iv->i386_off;              /* S i386 slot (4B handle) */
      memcpy(dst, &val, 4);
      ivar_snap_push(src, dst, val, 'P', 4, 8, 0);
   }
}

/* ENTRY: refresh the shadow's inherited-ivar region from the real object and
 * push a dirty-tracking frame onto the thread-local snapshot stack. */
static void sync_inherited_ivars(struct reverse_plan *plan, uint32_t shadow,
                                 id real, Class cls) {
   plan->wb_active = 0;
   if (!shadow || !real || !cls) { return; }
   uint8_t *sh = (uint8_t *)(uintptr_t)shadow;
   uint32_t mark = g_snap_n;
   /* The leaf legacy class + any LEGACY ancestor: mirror their nib-connected OWN
    * object ivars (outlets) from the real object into the shadow, so the IMP's
    * direct reads see the connected views/controllers, not the zeroed shadow. */
   for (Class c = cls; c; c = class_getSuperclass(c)) {
      if (rcls_lookup(c)) { push_own_object_ivars(sh, real, c); }
   }
   /* Skip the leaf legacy class and any LEGACY ancestor (their ivars are the
    * shadow's OWN region, written by the legacy IMPs directly); sync only the
    * NATIVE ancestors, whose ivars live in the real object. */
   for (Class c = class_getSuperclass(cls); c; c = class_getSuperclass(c)) {
      if (rcls_lookup(c)) { continue; }
      const struct nivar_cls *m = native_ivar_map_lookup(class_getName(c));
      if (!m) { continue; }
      for (uint32_t k = 0; k < m->n; ++k) {
         const struct nivar_ent *e = &m->iv[k];
         Ivar iv = class_getInstanceVariable(c, e->name);
         if (!iv) { continue; }                 /* renamed/removed in modern */
         ptrdiff_t xoff = ivar_getOffset(iv);
         if (xoff <= 0) { continue; }
         int msign = 0;
         uint32_t msz = modern_enc_size(ivar_getTypeEncoding(iv), &msign);
         if (msz == 0 || e->i386_sz > 4 || e->i386_sz > msz) { continue; }
         uint8_t *src = (uint8_t *)real + xoff;   /* real (modern) slot */
         uint8_t *dst = sh + e->i386_off;         /* shadow (i386) slot */
         if (!mem_readable((uintptr_t)src, msz)) { continue; }
         uint32_t val = 0;                        /* i386-side value (<=4B) */
         if (e->kind == 'P') {
            uint64_t p; memcpy(&p, src, 8);
            val = p ? x64_objc_wrap(p) : 0;
            memcpy(dst, &val, 4);
         } else if (e->kind == 'F') {
            if (msz == 8) { double d; memcpy(&d, src, 8);
                            float f = (float)d; memcpy(&val, &f, 4); }
            else          { memcpy(&val, src, 4); }    /* genuine 4-byte float */
            memcpy(dst, &val, e->i386_sz);
         } else { /* 'I' */
            memcpy(&val, src, e->i386_sz);              /* low i386_sz bytes */
            memcpy(dst, &val, e->i386_sz);
         }
         if (g_snap_n == g_snap_cap) {
            uint32_t nc = g_snap_cap ? g_snap_cap * 2 : 64;
            struct ivar_snap *ns = realloc(g_snap, (size_t)nc * sizeof *ns);
            if (!ns) { continue; }                /* OOM: skip write-back tracking */
            g_snap = ns; g_snap_cap = nc;
         }
         struct ivar_snap *s = &g_snap[g_snap_n++];
         s->real_slot = src; s->shadow_slot = dst; s->snap = val;
         s->kind = e->kind; s->i386_sz = e->i386_sz;
         s->msz = (uint8_t)msz; s->msign = (uint8_t)msign;
      }
   }
   plan->wb_mark = mark;
   plan->wb_active = 1;
}

/* EXIT: flush back the inherited shadow slots the IMP changed since entry. */
static void ivar_wb_flush(uint32_t mark) {
   for (uint32_t i = mark; i < g_snap_n; ++i) {
      struct ivar_snap *s = &g_snap[i];
      uint32_t cur = 0;
      memcpy(&cur, s->shadow_slot, s->i386_sz);
      if (cur == s->snap) { continue; }          /* IMP left it untouched */
      if (!mem_readable((uintptr_t)s->real_slot, s->msz)) { continue; }
      if (s->kind == 'P') {
         uint64_t p = cur ? unwrap_obj_arg(cur) : 0;
         memcpy(s->real_slot, &p, 8);            /* direct ivar store (MRC: no retain) */
      } else if (s->kind == 'F') {
         float f; memcpy(&f, &cur, 4);
         if (s->msz == 8) { double d = (double)f; memcpy(s->real_slot, &d, 8); }
         else             { memcpy(s->real_slot, &f, 4); }
      } else { /* 'I': zero/sign-extend the i386 value to the modern width */
         uint64_t v;
         if (s->msign) {
            int64_t sv;
            switch (s->i386_sz) {
               case 1:  sv = (int8_t)cur;  break;
               case 2:  sv = (int16_t)cur; break;
               default: sv = (int32_t)cur; break;
            }
            v = (uint64_t)sv;
         } else {
            switch (s->i386_sz) {
               case 1:  v = (uint8_t)cur;  break;
               case 2:  v = (uint16_t)cur; break;
               default: v = (uint32_t)cur; break;
            }
         }
         memcpy(s->real_slot, &v, s->msz);       /* msz in {1,2,4,8} */
      }
   }
}

/* Public: map a shadow self32 back to its real object (forward bridge). 0 if
 * not a shadow. */
id _86x64_shadow_real(uint32_t s) { return shadow_real(s); }

/* Public: fully resolve an i386 `@`/`#` value to a real x86_64 id — the same
 * canonical resolver the forward arg-bridge uses (arena proxy handles, R/S
 * shadows, paired/raw legacy objects, real x86 objects, else passthrough).
 * Used by C-API object bridges (e.g. NSMapTable/NSHashTable) so a legacy
 * object's SHADOW is never handed to native code as the object itself. */
uint64_t _86x64_unwrap_obj_arg(uint32_t a) {
   uint64_t r = unwrap_obj_arg(a);
   /* Call-site recovery (see CALLSITE_TRACE). This is structural, not a guess
    * about layout: an abigen `.l1` shim does `push rbp; mov rsp,rbp` and
    * thereafter only scratches rsp, and THIS function builds a normal frame, so
    * __builtin_frame_address(1) IS the shim's rbp. In that frame the i386
    * caller's 4-byte return address sits at [rbp+8] (the i386 cdecl args start
    * at [rbp+0xc]); dladdr then names the translated image and offset, which is
    * directly disassemblable because the translated __TEXT mirrors the i386
    * layout. */
   if (__builtin_expect(r < 0x1000 || CALLSITE_ALL_TRACE(), 0)) {
      const int want = (r >= 0x1000) ? 1
                     : r            ? (CALLSITE_TRACE() || CALLSITE_ALL_TRACE())
                                    : (CALLSITE_NIL_TRACE() || CALLSITE_ALL_TRACE());
      if (want) {
         void *fp = __builtin_frame_address(1);
         uint32_t ret = fp ? *(const uint32_t *)((uintptr_t)fp + 8) : 0;
         Dl_info di;
         if (ret && dladdr((void *)(uintptr_t)ret, &di) && di.dli_fname) {
            const char *b = strrchr(di.dli_fname, '/');
            fprintf(stderr, "[callsite] arg=0x%08x -> 0x%llx  i386ret=0x%08x  %s+0x%lx\n",
                    a, (unsigned long long)r, ret, b ? b + 1 : di.dli_fname,
                    (unsigned long)((uintptr_t)ret - (uintptr_t)di.dli_fbase));
         } else {
            fprintf(stderr, "[callsite] arg=0x%08x -> 0x%llx  i386ret=0x%08x (unresolved)\n",
                    a, (unsigned long long)r, ret);
         }
         fflush(stderr);
      }
   }
   return r;
}

/* ---------------------------------------------------------------------------
 * objc_setProperty / objc_getProperty — the modern-runtime @synthesize'd
 * accessor helpers. iPhoto's property setters/getters call them via classic
 * symbol stubs, but they are libobjc functions abigen never shimmed (libobjc
 * is not in the consider set), so the translated i386 cdecl call reached NATIVE
 * objc_setProperty, which read x86_64 REGISTER args -> garbage (self=nil,
 * newValue=tiny) -> EXC_BAD_ACCESS retaining junk during nib unarchiving.
 *
 * We cannot just call native objc_setProperty on the bridged object: `offset`
 * is the i386 ivar offset, and a legacy instance keeps its ivars in a low-4GB
 * i386-layout SHADOW buffer (self32 = the shadow), NOT in the real modern
 * object (whose NSObject/superclass ivars occupy those offsets). So we operate
 * on the i386 ivar slot directly and bridge retain/copy/release to the real
 * object. Atomicity is ignored (nib loading is single-threaded main-thread).
 *
 * i386 cdecl frames (4-byte slots, MTSHIM hands &args at a[0]):
 *   setProperty: a0 self, a1 _cmd, a2 offset, a3 newValue, a4 atomic, a5 copy
 *   getProperty: a0 self, a1 _cmd, a2 offset, a3 atomic
 * ------------------------------------------------------------------------- */
extern void objc_release(id);

/* The i386 ivar slot (a 4-byte id) for property `offset` on i386 object obj32,
 * or NULL if obj32 isn't a legacy instance we can safely store into. */
static uint32_t *prop_ivar_slot(uint32_t obj32, uint32_t offset) {
   if (!obj32) { return NULL; }
   /* legacy instance: self32 = its i386 shadow buffer; ivars at i386 offsets. */
   if (is_shadow(obj32)) { return (uint32_t *)(uintptr_t)(obj32 + offset); }
   /* a proxy-arena handle wraps a NATIVE object: an i386 offset is meaningless
    * there and a 4-byte store would corrupt the handle slots — never touch it. */
   if ((uintptr_t)obj32 >= g_arena_base && (uintptr_t)obj32 < g_arena_end) {
      return NULL;
   }
   /* a raw low-4GB legacy instance with in-place i386 ivars (not shadow-backed). */
   if (mem_readable((uintptr_t)obj32 + offset, 4)) {
      return (uint32_t *)(uintptr_t)(obj32 + offset);
   }
   return NULL;
}

uint32_t shim_objc_setProperty(const uint32_t *a) {
   uint32_t  self32     = a[0];
   uint32_t  offset     = a[2];
   uint32_t  newValue32 = a[3];
   int       shouldCopy = (int)a[5];

   uint32_t *slot = prop_ivar_slot(self32, offset);
   if (!slot) { return 0; }

   /* Hold a strong ref to the new value's real object (retain), or store a
    * copy. The ivar keeps the i386 representation (handle / shadow / low ptr). */
   uint32_t store32 = newValue32;
   if (newValue32) {
      id nv = resolve_self(newValue32);
      if (nv) {
         if (shouldCopy) {
            id cp = msg_id1(nv, sel_registerName("copyWithZone:"), (id)0);
            store32 = cp ? x64_objc_wrap((uint64_t)(uintptr_t)cp) : newValue32;
         } else {
            objc_retain(nv);
         }
      }
   }

   uint32_t old = *slot;
   *slot = store32;
   if (old && old != store32) {
      id ov = resolve_self(old);
      if (ov) { objc_release(ov); }
   }
   return 0;
}

uint32_t shim_objc_getProperty(const uint32_t *a) {
   uint32_t *slot = prop_ivar_slot(a[0], a[2]);
   return slot ? *slot : 0;
}

/* Deprecated CarbonCore volume-notification SPIs. On modern macOS they are
 * no-ops ("Volume Notification SPI is no longer supported"), but iPhoto's
 * PhotoCDManager registerWithDiskArb: calls them via classic stubs that bind to
 * the NATIVE functions, whose x86_64 64-bit `ret` over-pops the translated
 * i386 4-byte return address -> fused PC crash. Shim them as no-ops returning
 * noErr; MTSHIM makes the call respect the i386 ABI. */
uint32_t shim_RequestVolumeNotification(const uint32_t *a) { (void)a; return 0; }
uint32_t shim_DeclineVolumeNotification(const uint32_t *a) { (void)a; return 0; }

/* Return-value wrap that PRESERVES legacy-object identity (objc_msgSend asm
 * ret_kind==1 path). A (super) call returning a legacy-class instance — most
 * importantly `self = [super init]` — must come back to the i386 caller as that
 * object's i386 SHADOW, not a fresh proxy handle. The caller then writes ivars
 * through the returned self (`mov [self+off], x`); a proxy handle is just an
 * 8-byte arena slot with no ivar space, so the store lands in (and corrupts)
 * adjacent handle slots — the high 32 bits of a sibling become a stray handle,
 * producing a fused object pointer (0x<handle><lowptr>) that later crashes in
 * object_getClass/object_isClass. The shadow carries the i386 ivar layout, so
 * ivar writes are correct and resolve_self maps it back to the real object for
 * re-sends. A returned object with no shadow association (every native object)
 * mints a proxy handle exactly as before, so only our own legacy instances
 * change behavior. Tagged pointers have no association -> proxy path, unchanged. */
uint32_t x64_objc_wrap_ret(uint64_t real) {
   /* env-gated (ABICONV_CALLRING) call-ring: complete this send's pending entry
    * with the native OBJECT return (kind 1 = the common id-returning path,
    * including -[NSArray objectAtIndex:] and every array-vending API). Records
    * the return's class + collection count so a Swift-trap SIGILL names the API
    * that handed a translated caller an empty collection. Inert unless the env
    * var is set. See callring_shim.c. */
   _86x64_callring_ret(real);

   /* Only walk the class chain if `real` is genuinely a readable object. A
    * method whose return-type ENCODING is misclassified as object (ret_is_obj=1
    * for a plain struct-pointer matched by enc_is_objptr_struct/enc_is_cfptr, or
    * a stale/garbage 64-bit value) would otherwise make object_getClass /
    * class_getSuperclass deref a non-isa -> SIGSEGV (iPhoto album load:
    * representedAlbum/versionsResult/moveToSQL path crashed here). Guard every
    * deref; on a non-object, fall straight through to x64_objc_wrap (a proxy
    * handle, no dereference) — the i386 caller reads only a 4-byte value either
    * way. Universal hardening: wrap_ret must never fault on a non-object. */
   if (real >= 0x100000000ULL && g_ctrl &&
       mem_readable((uintptr_t)real, sizeof(void *))) {
      uint32_t s = (uint32_t)(uintptr_t)objc_getAssociatedObject(
                      (id)(uintptr_t)real, shadow_assoc_key());
      if (s) { return s; }
      /* No shadow association yet, but if `real` is an instance of one of OUR
       * reverse-registered legacy classes, the i386 code receiving it accesses
       * ivars at hardcoded i386 offsets and may STORE through it. A proxy
       * handle is an 8-byte arena slot with no ivar space, so such a store
       * clobbers a sibling slot's high half -> a fused 0x<handle><lowptr>
       * pointer that later faults in objc_retain inside native setObject:forKey:
       * (s13). Hand it the i386-layout shadow instead, exactly as the
       * reverse-bridge `self` path (get_or_create_shadow) does. Native objects
       * are never in rcls, so they keep the proxy-handle path; tagged pointers
       * resolve to a tagged class that isn't ours -> proxy path too. */
      Class cls = object_getClass((id)(uintptr_t)real);
      for (Class c = cls; c && mem_readable((uintptr_t)c, sizeof(void *) * 2);
           c = class_getSuperclass(c)) {
         if (rcls_lookup(c)) {
            uint32_t sh = get_or_create_shadow((id)(uintptr_t)real, cls);
            if (sh) { return sh; }   /* 0 == shadow arena exhausted: fall back */
            break;
         }
      }
   }
   return x64_objc_wrap(real);
}

/* ---------------------------------------------------------------------------
 * NIB IBOutlet connection bridge: object_set/getInstanceVariable for legacy
 * reverse-registered classes.
 *
 * AppKit's NIB loader connects an IBOutlet that has NO `set<Outlet>:` setter via
 * `object_setInstanceVariable(owner, "<ivarName>", target)` — a raw runtime ivar
 * write keyed by NAME (verified by disassembling -[NSNibOutletConnector
 * establishConnection]: it builds set<Label>:, performSelector:s it if the
 * object respondsToSelector:, else falls back to object_setInstanceVariable).
 * Our reverse-registered legacy classes carry NO named ivars in the modern
 * runtime — their ivars live in the i386 SHADOW at hardcoded i386 offsets (see
 * reverse_register_image) — so the native call finds no ivar and SILENTLY
 * no-ops, leaving the outlet nil. The translated code then derefs the nil outlet
 * and crashes (iPhoto: -[InfoController dataChanged:] derefs nil mSuperInfo@0x40).
 *
 * Fix: interpose both calls. For a legacy instance, resolve the ivar NAME to its
 * absolute i386 offset via the legacy __OBJC ivar metadata and read/write the
 * i386 shadow slot (the same slot the translated IMPs access as [self+off]);
 * otherwise fall through to the real libobjc function. Universal: every no-setter
 * IBOutlet of every legacy class (setter outlets already connect through the
 * reverse bridge), plus any other native object_set/getInstanceVariable on a
 * legacy object. ------------------------------------------------------------- */
#include <dlfcn.h>
#include <objc/runtime.h>

/* Absolute i386 ivar offset for `name` on `cls` or any registered legacy
 * ancestor (fragile-ABI ivar offsets are already absolute). Returns 1 on hit. */
static int legacy_ivar_offset(Class cls, const char *name, uint32_t *off_out) {
   for (Class c = cls; c; c = class_getSuperclass(c)) {
      struct rcls_ent *ce = rcls_lookup(c);
      if (!ce || !ce->legacy_addr) { continue; }
      if (!ptr_ok(ce->legacy_addr, sizeof(struct legacy_objc_class))) { continue; }
      const struct legacy_objc_class *lc =
         (const struct legacy_objc_class *)(uintptr_t)ce->legacy_addr;
      if (!lc->ivars || !ptr_ok(lc->ivars, sizeof(struct legacy_objc_ivar_list))) {
         continue;
      }
      const struct legacy_objc_ivar_list *il =
         (const struct legacy_objc_ivar_list *)(uintptr_t)lc->ivars;
      if (!ptr_ok(lc->ivars, sizeof(struct legacy_objc_ivar_list) +
                  (size_t)(il->ivar_count > 0 ? il->ivar_count : 0) *
                  sizeof(struct legacy_objc_ivar))) {
         continue;
      }
      const struct legacy_objc_ivar *iv =
         (const struct legacy_objc_ivar *)(il + 1);
      for (int i = 0; i < il->ivar_count; ++i) {
         const char *inm = (const char *)(uintptr_t)iv[i].name;
         if (inm && mem_readable((uintptr_t)inm, 1) && strcmp(inm, name) == 0) {
            *off_out = (uint32_t)iv[i].offset;
            return 1;
         }
      }
   }
   return 0;
}

/* Is `cls` (or an ancestor) one of our reverse-registered legacy classes? */
static int class_is_legacy(Class cls) {
   for (Class c = cls; c; c = class_getSuperclass(c)) {
      if (rcls_lookup(c)) { return 1; }
   }
   return 0;
}

/* The REAL object_[sg]etInstanceVariable, reimplemented INLINE. We must NOT call
 * the libobjc symbol here: x64_object_[sg]etInstanceVariable interpose it via
 * __DATA,__interpose, and dyld redirects EVERY bind to that symbol process-wide —
 * including dlsym(RTLD_NEXT/RTLD_DEFAULT, "object_setInstanceVariable"), which
 * resolves back to our own interposer → unbounded self-recursion → stack overflow
 * (iWeb's NSSpellChecker NIB outlet connection on a NON-legacy owner takes this
 * fall-through). class_getInstanceVariable + a raw write at ivar_getOffset
 * reproduces object_setInstanceVariable's exact (non-retaining) semantics without
 * touching the interposed symbol (class_getInstanceVariable/ivar_getOffset are not
 * interposed). Universal. */
static Ivar real_object_setInstanceVariable(id o, const char *n, void *v) {
   if (!o || !n) { return NULL; }
   Ivar iv = class_getInstanceVariable(object_getClass(o), n);
   if (iv) { *(void **)((char *)o + ivar_getOffset(iv)) = v; }
   return iv;
}
static Ivar real_object_getInstanceVariable(id o, const char *n, void **v) {
   if (!o || !n) { if (v) { *v = NULL; } return NULL; }
   Ivar iv = class_getInstanceVariable(object_getClass(o), n);
   if (v) { *v = iv ? *(void **)((char *)o + ivar_getOffset(iv)) : NULL; }
   return iv;
}

static Ivar x64_object_setInstanceVariable(id obj, const char *name, void *value) {
   if (obj && name && class_is_legacy(object_getClass(obj))) {
      Class cls = object_getClass(obj);
      uint32_t off;
      if (legacy_ivar_offset(cls, name, &off)) {
         uint32_t sh = get_or_create_shadow(obj, cls);
         uint32_t *slot = prop_ivar_slot(sh, off);
         if (slot) {
            /* store the outlet target in the i386 representation the translated
             * code expects: a SHADOW for a legacy object, a proxy handle for a
             * native one. Raw store (no retain), matching native semantics —
             * the nib retains its top-level objects / view hierarchy. */
            *slot = value ? x64_objc_wrap_ret((uint64_t)(uintptr_t)value) : 0;
         }
         /* ALSO write the real (reverse_add_ivars-synthesized) modern ivar on R,
          * when the class carries one, and hand its Ivar back to the caller:
          *   - -[NSNibOutletConnector establishConnection] TESTS the returned
          *     Ivar and, on NULL, logs "Failed to connect (…) outlet … missing
          *     setter or instance variable" (disassembled: the write above still
          *     landed, but every outlet looked failed and R's slot stayed nil);
          *   - keeping R's slot populated makes the outlet visible to NATIVE
          *     readers (KVC valueForKey:, archiving, class_getInstanceVariable
          *     users) and seeds push_own_object_ivars' R->S mirror on reverse
          *     dispatch, instead of leaving R permanently nil.
          * Raw 8-byte store at the modern offset = native (non-retaining)
          * object_setInstanceVariable semantics. An ivar found on a NATIVE
          * ancestor gets exactly the store the un-interposed call would do. */
         Ivar riv = class_getInstanceVariable(cls, name);
         if (riv) {
            ptrdiff_t moff = ivar_getOffset(riv);
            if (moff > 0) { *(void **)((char *)obj + moff) = value; }
         }
         if (getenv("ABICONV_OUTLET_TRACE")) {
            fprintf(stderr, "[outlet] -[%s set ivar %s @0x%x] = %p (slot=%p rivar=%p)\n",
                    class_getName(cls), name, off, value, (void *)slot, (void *)riv);
         }
         return riv;
      }
      return NULL;   /* named ivar unknown to the legacy metadata: connector warns */
   }
   return real_object_setInstanceVariable(obj, name, value);
}

static Ivar x64_object_getInstanceVariable(id obj, const char *name, void **outValue) {
   if (obj && name && class_is_legacy(object_getClass(obj))) {
      Class cls = object_getClass(obj);
      uint32_t off;
      if (legacy_ivar_offset(cls, name, &off)) {
         uint32_t sh = get_or_create_shadow(obj, cls);
         uint32_t *slot = prop_ivar_slot(sh, off);
         Ivar riv = class_getInstanceVariable(cls, name);
         if (outValue) {
            /* Prefer the shadow (the legacy IMPs' authoritative copy). But an
             * outlet connected purely through R's modern synthesized ivar (the
             * setter interpose / KVC / nib connector all write R) may not have
             * seeded this freshly-created shadow yet (push_own_object_ivars
             * mirrors R->S only on reverse dispatch). Fall back to R's modern
             * slot so a forward read observes the connected value either way. */
            id v = slot ? resolve_self(*slot) : NULL;
            if (!v && riv) {
               ptrdiff_t moff = ivar_getOffset(riv);
               if (moff > 0) { v = *(id *)((char *)obj + moff); }
            }
            *outValue = (void *)v;
         }
         /* non-NULL Ivar on success, mirroring the setter interpose: callers
          * (AppKit nib machinery among them) test the return to decide whether
          * the ivar exists at all. */
         return riv;
      }
      if (outValue) { *outValue = NULL; }
      return NULL;
   }
   return real_object_getInstanceVariable(obj, name, outValue);
}

typedef struct { const void *replacement; const void *replacee; } objc_iv_interpose_t;
__attribute__((used)) static const objc_iv_interpose_t __objc_iv_interposers[]
__attribute__((section("__DATA,__interpose"))) = {
   { (const void *)x64_object_setInstanceVariable,
     (const void *)object_setInstanceVariable },
   { (const void *)x64_object_getInstanceVariable,
     (const void *)object_getInstanceVariable },
};

/* ---------------------------------------------------------------------------
 * Legacy i386 object -> real modern object bridge (14th iPhoto blocker).
 *
 * When translated i386 code hands one of its OWN ObjC objects BY VALUE to a
 * real Foundation/AppKit method (e.g. -[NSMutableDictionary setObject:forKey:]
 * with an instance of an iPhoto legacy class, or storing a Class object), the
 * raw i386 pointer reaches the x86_64 runtime, which derefs the object's 4-byte
 * legacy isa as an 8-byte modern isa and crashes in objc_retain. The forward
 * arg marshaller previously only knew arena proxy handles and R/S shadows, so a
 * raw legacy pointer passed straight through.
 *
 * Fix: recognize a raw legacy object/class arg and substitute the real modern
 * object the framework can safely retain/message:
 *   - a legacy CLASS object -> the registered modern Class (objc_getClass).
 *   - a legacy INSTANCE     -> a paired modern instance R' of the registered
 *                              class, with the original i386 object as R''s i386
 *                              shadow, so any legacy method the framework later
 *                              sends to R' runs against the real i386 ivar data
 *                              via the reverse bridge.
 * ------------------------------------------------------------------------- */
#define LCLS_CLASS 0x1u
#define LCLS_META  0x2u

static uint32_t lpair_hash(uint32_t p) {
   return (p * 2654435761u) & (LPAIR_CAP - 1);
}
static id lpair_lookup(uint32_t p) {
   if (!g_ctrl || !g_ctrl->lpair || !p) { return (id)0; }
   struct lpair_ent *t = (struct lpair_ent *)(uintptr_t)g_ctrl->lpair;
   uint32_t i = lpair_hash(p);
   for (uint32_t n = 0; n < LPAIR_CAP; ++n) {
      if (t[i].p == 0) { return (id)0; }
      if (t[i].p == p) { return (id)(uintptr_t)t[i].real; }
      i = (i + 1) & (LPAIR_CAP - 1);
   }
   return (id)0;
}
static void lpair_insert(uint32_t p, id real) {
   if (!g_ctrl || !g_ctrl->lpair || !p) { return; }
   struct lpair_ent *t = (struct lpair_ent *)(uintptr_t)g_ctrl->lpair;
   uint32_t i = lpair_hash(p);
   for (uint32_t n = 0; n < LPAIR_CAP; ++n) {
      if (t[i].p == 0 || t[i].p == p) {
         t[i].real = (uint64_t)(uintptr_t)real;
         t[i].p    = p;            /* publish key last */
         return;
      }
      i = (i + 1) & (LPAIR_CAP - 1);
   }
}

/* Pair a raw i386 instance with a fresh modern instance of its registered
 * class. The i386 object IS the shadow: legacy IMPs dispatched on R' read/write
 * their ivars at hardcoded i386 offsets directly in the original object. */
static id legacy_instance_pair(uint32_t p, Class c) {
   id r = lpair_lookup(p);
   if (r) { return r; }
   r = class_createInstance(c, 0);
   if (!r) { return (id)0; }
   objc_retain(r);                 /* keep alive; the i386 side owns lifetime */
   objc_setAssociatedObject(r, shadow_assoc_key(), (id)(uintptr_t)p,
                            OBJC_ASSOCIATION_ASSIGN);
   lpair_insert(p, r);
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[lo] paired legacy instance 0x%08x -> %s %p\n",
              p, class_getName(c), (void *)r);
      fflush(stderr);
   }
   return r;
}

/* True iff `p` points to a short, NUL-terminated printable C identifier that
 * objc_getClass() resolves to a REAL class -- i.e. p is a class-name cstring
 * (an ObjC class-message receiver from __cls_refs/__cstring), not a legacy
 * object whose first word we should dereference as an isa. Bounded scan so
 * objc_getClass never reads past mapped memory. A genuine legacy object's
 * first bytes are an isa pointer, essentially never a printable string that
 * names a live class, so this discriminates structurally, not by app. */
static int cstring_names_real_class(uint32_t p) {
   const char *s = (const char *)(uintptr_t)p;
   char buf[160];
   size_t i = 0;
   for (;;) {
      if (i >= sizeof(buf) - 1) { return 0; }      /* too long for a class name */
      if (!mem_readable((uintptr_t)p + i, 1)) { return 0; }
      char ch = s[i];
      if (ch == '\0') { break; }
      int ident = (ch == '_') ||
                  (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                  (i > 0 && ch >= '0' && ch <= '9');
      if (!ident) { return 0; }                     /* not an identifier char */
      buf[i++] = ch;
   }
   if (i == 0) { return 0; }
   buf[i] = '\0';
   return objc_getClass(buf) != NULL;
}

/* If `p` is a raw legacy i386 object or class object in the translated image,
 * return the real modern object/Class the runtime can safely use; else 0. */
static id legacy_obj_to_real(uint32_t p) {
   if (!ptr_ok(p, 4)) { return (id)0; }
   /* A class-name cstring (e.g. "QTMovie") can have its leading bytes form a
    * coincidentally-mapped address ('QTMo' = 0x6f4d5451) that passes the isa
    * checks below and mis-pairs the string as a bogus legacy instance. If p
    * actually names a real class, it is such a cstring: return 0 and let the
    * caller's objc_getClass(name) path (resolve_self) or passthrough use it. */
   if (cstring_names_real_class(p)) { return (id)0; }
   uint32_t isa = *(const uint32_t *)(uintptr_t)p;
   if (!ptr_ok(isa, sizeof(struct legacy_objc_class))) { return (id)0; }
   const struct legacy_objc_class *k =
      (const struct legacy_objc_class *)(uintptr_t)isa;
   if (k->info & LCLS_META) {
      /* p is a CLASS object — its isa is the metaclass; resolve p by name. */
      const struct legacy_objc_class *self =
         (const struct legacy_objc_class *)(uintptr_t)p;
      if (!ptr_ok(self->name, 1)) { return (id)0; }
      Class c = objc_getClass((const char *)(uintptr_t)self->name);
      if (c && BRIDGE_TRACE()) {
         fprintf(stderr, "[lo] legacy class 0x%08x \"%s\" -> %p\n",
                 p, (const char *)(uintptr_t)self->name, (void *)c);
         fflush(stderr);
      }
      return (id)c;
   }
   if (k->info & LCLS_CLASS) {
      /* p is an INSTANCE; isa==k is its class. */
      if (!ptr_ok(k->name, 1)) { return (id)0; }
      Class c = objc_getClass((const char *)(uintptr_t)k->name);
      if (!c) {
         if (BRIDGE_TRACE()) {
            fprintf(stderr, "[lo] legacy instance 0x%08x class \"%s\" NOT "
                    "registered -> passthrough\n",
                    p, (const char *)(uintptr_t)k->name);
            fflush(stderr);
         }
         return (id)0;
      }
      return legacy_instance_pair(p, c);
   }
   return (id)0;
}

/* An i386 __builtin CFString constant (CFSTR("...") / @"..." compiled into the
 * i386 image's __DATA,__cfstring section) has the fragile 16-byte layout
 *   { uint32_t isa; uint32_t flags; uint32_t cStr; uint32_t length; }
 * with flags == 0x7c8 for an 8-bit (ASCII) constant. Passed raw to a native
 * method (e.g. the key of -[NSUserDefaults objectForKey:@"requiredUpdate"]),
 * the modern runtime reads it as the 8-byte-field x86_64 NSString layout, so
 * -hash / -isEqual: / -UTF8String / CFStringCreateWithBytes see a garbage
 * isa+length and SIGBUS or abort. Convert it to a real immortal NSString the
 * runtime can message. The isa field is a load-time-bound relocation (so
 * unreliable for detection); key on flags plus an exact NUL-terminated
 * strlen==length match. Cached by constant address (constants are immortal). */
#define CFSTR_FLAGS_ASCII8 0x7c8u
static struct { uint32_t i386; id real; } g_cfstr_cache[2048];
static unsigned       g_cfstr_cache_n;
static os_unfair_lock g_cfstr_lock = OS_UNFAIR_LOCK_INIT;

/* Slide of the loaded image whose mapped segments contain `addr`, or 0 if none
 * (addr in no image, or the image loaded at its preferred vmaddr). Recovers an
 * UNSLID absolute pointer embedded in a translated image — the CFConstantString
 * `str` field, which objc_slide's slide_cfstrings range-checks against the
 * __OBJC span and so skips when `str` points into __TEXT,__cstring. */
static intptr_t image_slide_for_addr(uintptr_t addr) {
   uint32_t nimg = _dyld_image_count();
   for (uint32_t i = 0; i < nimg; ++i) {
      const struct mach_header_64 *mh =
         (const struct mach_header_64 *)_dyld_get_image_header(i);
      if (!mh || mh->magic != MH_MAGIC_64) { continue; }
      intptr_t slide = _dyld_get_image_vmaddr_slide(i);
      const struct load_command *lc =
         (const struct load_command *)((const uint8_t *)mh + sizeof *mh);
      for (uint32_t c = 0; c < mh->ncmds; ++c) {
         if (lc->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *sg =
               (const struct segment_command_64 *)lc;
            uintptr_t lo = (uintptr_t)((int64_t)sg->vmaddr + (int64_t)slide);
            if (sg->vmsize && addr >= lo && addr < lo + (uintptr_t)sg->vmsize) {
               return slide;
            }
         }
         lc = (const struct load_command *)((const uint8_t *)lc + lc->cmdsize);
      }
   }
   return 0;
}

static id i386_cfstr_to_real(uint32_t p) {
   if (!ptr_ok(p, 16)) { return (id)0; }
   uint32_t cstr, length;
   if (*(const uint32_t *)(uintptr_t)(p + 4) == CFSTR_FLAGS_ASCII8) {
      /* i386 16-byte CFConstantString: {isa,flags,cstr,length} (4-byte fields). */
      cstr   = *(const uint32_t *)(uintptr_t)(p + 8);
      length = *(const uint32_t *)(uintptr_t)(p + 12);
   } else if (ptr_ok(p, 32) &&
              *(const uint32_t *)(uintptr_t)(p + 8)  == CFSTR_FLAGS_ASCII8 &&
              *(const uint32_t *)(uintptr_t)(p + 12) == 0 &&     /* flags high half */
              *(const uint32_t *)(uintptr_t)(p + 20) == 0) {     /* cstr  high half */
      /* x86_64 32-byte CFConstantString: {isa(8),flags(8),cstr(8),length(8)}.
       * The translator emits CFSTR("...") constants as the NATIVE 32-byte
       * record (8-byte fields), and their isa is xrel-bound to the low-4GB
       * WRAPPED handle of __CFConstantStringClassReference. Passing such a raw
       * constant to a native ObjC API (e.g. +[NSDictionary
       * dictionaryWithObject:forKey:]) makes objc read the wrapped handle as a
       * Class -> "Attempt to use unknown class 0x800xxxxx" -> _objc_fatal. The
       * string payload is valid x86_64 layout: flags at +8, cstr ptr at +16
       * (low-4GB, high half 0), length at +24. Convert to a real native string
       * below, exactly like the i386 case. (Portal2/Source; structural trigger
       * on the 32-byte record + ASCII8 flags, not app-specific.) */
      cstr   = *(const uint32_t *)(uintptr_t)(p + 16);
      length = *(const uint32_t *)(uintptr_t)(p + 24);
   } else {
      /* Env-gated reject diagnostic (OBJC_BRIDGE_TRACE): a candidate whose
       * first 8 bytes LOOK like a wrapped-handle isa (0x800xxxxx low /
       * anything readable) but whose layout checks failed. Pin silent
       * passthroughs (-> native "unknown class 0x800xxxxx" _objc_fatal)
       * to the exact failed check. */
      if (BRIDGE_TRACE()) {
         unsigned long long w0 = *(const unsigned long long *)(uintptr_t)p;
         unsigned long long w1 =
            ptr_ok(p, 16) ? *(const unsigned long long *)(uintptr_t)(p + 8) : 0;
         fprintf(stderr, "[cfstr] 0x%08x REJECT layout isa=0x%llx w1=0x%llx\n",
                 p, w0, w1);
         fflush(stderr);
      }
      return (id)0;
   }
   if (length >= (1u << 24)) { return (id)0; }
   /* Locate the char payload. Two candidates, each validated by the same
    * exact-C-string probe (readable AND strnlen == length — a coincidental
    * non-constant can't survive it):
    *   1. `str` AS-IS: correct when the record was slid (slide_cfstrings)
    *      or the image loaded at its preferred base.
    *   2. `str` + the record's OWN image slide: recovers a record whose str
    *      escaped sliding (the i386 source has no dyld rebases, so macho-tool
    *      embeds UNSLID vmaddrs; Halo's CFURL path constants hit this).
    * Candidate 2 must be tried even when candidate 1 is READABLE: an unslid
    * preferred address can land inside an unrelated live mapping, and
    * trusting readability alone made the recognizer read foreign bytes, fail
    * the strnlen probe, and pass the RAW record to native CF -> objc_msgSend
    * on isa = the wrapped class handle -> "Attempt to use unknown class
    * 0x800xxxxx" _objc_fatal (Civ IV CFStringReplace, trace-verified:
    * "[cfstr] REJECT strnlen cstr=0x10dae5f4 got=2"). For an already-slid
    * record candidate 2 is a double-slide -> fails the probe -> harmless. */
   uintptr_t sp = 0;
   if (ptr_ok(cstr, (size_t)length + 1) &&
       strnlen((const char *)(uintptr_t)cstr, (size_t)length + 1) == length) {
      sp = (uintptr_t)cstr;
   } else {
      intptr_t sl = image_slide_for_addr((uintptr_t)p);
      uintptr_t slid = (uintptr_t)cstr + (uintptr_t)sl;
      if (sl != 0 && mem_readable(slid, (size_t)length + 1) &&
          strnlen((const char *)slid, (size_t)length + 1) == length) {
         sp = slid;
      } else {
         if (BRIDGE_TRACE()) {
            fprintf(stderr, "[cfstr] 0x%08x REJECT str cstr=0x%x len=%u "
                    "slide=0x%lx slid=0x%lx\n", p, cstr, length,
                    (unsigned long)sl, (unsigned long)slid);
            fflush(stderr);
         }
         return (id)0;
      }
   }
   const char *s = (const char *)sp;

   os_unfair_lock_lock(&g_cfstr_lock);
   for (unsigned i = 0; i < g_cfstr_cache_n; ++i) {
      if (g_cfstr_cache[i].i386 == p) {
         id r = g_cfstr_cache[i].real;
         os_unfair_lock_unlock(&g_cfstr_lock);
         return r;
      }
   }
   /* +1 retained, kept forever (a constant string is immortal). UTF-8 first;
    * fall back to MacRoman so any 8-bit byte still yields a string. */
   CFStringRef cf = CFStringCreateWithBytes(NULL, (const UInt8 *)s, length,
                                            kCFStringEncodingUTF8, false);
   if (!cf) {
      cf = CFStringCreateWithBytes(NULL, (const UInt8 *)s, length,
                                   kCFStringEncodingMacRoman, false);
   }
   if (cf && g_cfstr_cache_n < sizeof(g_cfstr_cache) / sizeof(g_cfstr_cache[0])) {
      g_cfstr_cache[g_cfstr_cache_n].i386 = p;
      g_cfstr_cache[g_cfstr_cache_n].real = (id)cf;
      ++g_cfstr_cache_n;
   }
   os_unfair_lock_unlock(&g_cfstr_lock);
   return (id)cf;
}

/* Unwrap an i386 `@`/`#` argument to a real x86_64 id for a call into the
 * modern runtime. Handles arena proxy handles, R/S shadows, paired legacy
 * objects, raw legacy objects/classes, and otherwise passes through. */
/* Render a resolved native id/CFStringRef into buf for the ARGSTR diagnostic.
 * Toll-free: CFStringGetCString works on both NSString and CFStringRef. Prints
 * "(nil)" for 0, "(-1 sentinel)" for the CF default-text sentinel, and
 * "(non-string CFTypeID=N)" for a non-CFString object; never dereferences a
 * raw non-object value. Diagnostic-only; guarded by the caller. */
static void argstr_describe(uint64_t real, char *buf, size_t n) {
   if (real == 0)      { snprintf(buf, n, "(nil)"); return; }
   if (real == ~0ULL)  { snprintf(buf, n, "(-1 sentinel)"); return; }
   /* Decide whether it's safe to hand `real` to CF. A TAGGED pointer (low bit
    * set on x86_64, or an obfuscated NSString with the tag bit) is NOT
    * dereferenceable but CFGetTypeID/CFStringGet* decode it fine — pass it
    * through. An UNTAGGED pointer must be 8-aligned AND readable (its isa slot
    * readable) to be a real object; otherwise it is a passthrough low i386
    * value (unwrap's final fallthrough) — describe it verbatim rather than
    * faulting CFGetTypeID inside a diagnostic. */
   const int tagged = (real & 0x1) != 0;          /* x86_64 tagged-ptr low bit */
   if (!tagged) {
      /* An untagged object pointer must be 8-aligned and readable AND its isa
       * must itself point to a readable, aligned class whose own isa
       * (metaclass) is readable — the is_real_x86_object isa-chain probe. This
       * is a DIAGNOSTIC (ABICONV_ARGSTR_TRACE): it must never itself fault the
       * traced program. Handing CFGetTypeID/objc_msgSend a readable-but-
       * non-object pointer (arbitrary data whose first word merely looks like
       * an isa) derefs deep inside the runtime and SIGSEGVs — observed on a
       * RunStandardAlert arg whose resolved value was not a real object
       * (argstr_describe -> _CF_IS_OBJC/objc_msgSend_uncached -> KERN at 0x0).
       * The earlier "CFGetTypeID robustly validates it IS a CF object" claim
       * was WRONG: CFGetTypeID trusts the isa. Pre-validate the isa chain so
       * only a provable object reaches CF. A tagged pointer (handled above by
       * skipping this block) has no in-memory isa and is decoded safely by CF. */
      if ((real & 0x7) || !mem_readable((uintptr_t)real, 8)) {
         snprintf(buf, n, "(raw 0x%llx, not an object)",
                  (unsigned long long)real);
         return;
      }
      uint64_t isa = *(const uint64_t *)(uintptr_t)real;
      /* strip the non-pointer-isa bits (x86_64 ISA_MASK 0x00007ffffffffff8) so
       * a real object with a packed/tagged isa still validates. */
      uint64_t isa_ptr = isa & 0x00007ffffffffff8ULL;
      if (isa_ptr < 0x1000 || !mem_readable(isa_ptr, 8)) {
         snprintf(buf, n, "(raw 0x%llx, isa 0x%llx not a class)",
                  (unsigned long long)real, (unsigned long long)isa);
         return;
      }
      uint64_t meta = *(const uint64_t *)(uintptr_t)isa_ptr;   /* class's isa */
      uint64_t meta_ptr = meta & 0x00007ffffffffff8ULL;
      if (meta_ptr < 0x1000 || !mem_readable(meta_ptr, 8)) {
         snprintf(buf, n, "(raw 0x%llx, not an object)",
                  (unsigned long long)real);
         return;
      }
   }
   CFTypeRef t = (CFTypeRef)(uintptr_t)real;
   if (CFGetTypeID(t) == CFStringGetTypeID()) {
      CFIndex len = CFStringGetLength((CFStringRef)t);
      char tmp[256];
      if (CFStringGetCString((CFStringRef)t, tmp, sizeof tmp,
                             kCFStringEncodingUTF8)) {
         snprintf(buf, n, "CFString len=%ld \"%s\"", (long)len, tmp);
      } else {
         snprintf(buf, n, "CFString len=%ld (uncopyable)", (long)len);
      }
   } else {
      snprintf(buf, n, "(non-string CFTypeID=%lu)",
               (unsigned long)CFGetTypeID(t));
   }
}

static uint64_t unwrap_obj_arg_core(uint32_t a) {
   if (!a) { return 0; }
   if (a == 0xffffffffu) {
      /* The all-ones i386 word is the classic Carbon/CF "-1 sentinel", not a
       * pointer: kAlertDefaultOKText/CancelText/OtherText = (CFStringRef)-1,
       * kCFNotFound-style handles, MAP_FAILED. 0xffffffff can never be a real
       * i386 object/handle (the last page is never mapped), so widen it to
       * the native 64-bit -1 the callee compares against. Zero-extending
       * handed HIToolbox 0x00000000ffffffff -> treated as a live CFStringRef
       * -> CFStringCreateCopy faulted (Civ IV CreateStandardAlert title). */
      return ~0ULL;
   }
   /* Attach THIS copy to the shared proxy arena before any handle test below.
    * x64_objc_wrap has always called arena_init(); unwrap never did, and simply
    * READ g_arena_base/g_arena_end. In a copy whose initializers dyld never ran
    * those are still 0, so every VALID handle falls through the range check and
    * is handed to native code raw — Civ IV: an arena handle arrived at
    * _CFRuntimeCreateInstance as the CFAllocatorRef, and CF wrote through it
    * into the read-only shared cache (SIGBUS). arena_init() is a single load
    * once attached, and it rendezvouses onto the existing arena rather than
    * creating a second one, so this cannot fork the handle space. */
   arena_init();
   const int utrace = BRIDGE_TRACE();
   id sr = shadow_real(a);                 /* R/S shadow or paired legacy obj */
   if (sr) {
      if (utrace) {
         fprintf(stderr, "[uo] 0x%08x shadow/pair -> %p\n", a, (void *)sr);
         fflush(stderr);
      }
      return (uint64_t)(uintptr_t)sr;
   }
   if ((uintptr_t)a >= g_arena_base && (uintptr_t)a < g_arena_end) {
      return *(uint64_t *)(uintptr_t)a;    /* arena proxy handle */
   }
   {
      /* An i386 block passed as a typed @?/@ argument to a native API the
       * runtime will INVOKE (enumerate/sort/GCD/completion): hand native a
       * synthesized native Block_layout whose invoke trampolines back to the
       * i386 invoke. Triggers on the block's structural isa, not the app. */
      uint64_t nb = i386_block_to_native(a);
      if (nb) {
         if (utrace) {
            fprintf(stderr, "[uo] 0x%08x i386-block -> native 0x%llx\n",
                    a, (unsigned long long)nb);
            fflush(stderr);
         }
         return nb;
      }
   }
   if (is_real_x86_object(a)) {            /* raw constant x86_64 obj (cfstring) */
      if (utrace) { fprintf(stderr, "[uo] 0x%08x real-obj passthrough\n", a); }
      return (uint64_t)a;
   }
   id lr = legacy_obj_to_real(a);          /* raw legacy obj/class -> real */
   if (lr) {
      if (utrace) {
         fprintf(stderr, "[uo] 0x%08x legacy -> %p\n", a, (void *)lr);
         fflush(stderr);
      }
      return (uint64_t)(uintptr_t)lr;
   }
   id cf = i386_cfstr_to_real(a);          /* i386 CFSTR("...") constant -> real NSString */
   if (cf) {
      if (utrace) {
         fprintf(stderr, "[uo] 0x%08x i386-cfstr -> %p\n", a, (void *)cf);
         fflush(stderr);
      }
      return (uint64_t)(uintptr_t)cf;
   }
   /* Diagnose silent passthrough of high pointers (candidate isa-impedance
    * crashes): show why nothing matched. */
   if (utrace && a >= 0x01000000u) {
      fprintf(stderr, "[uo] 0x%08x PASSTHROUGH (shadow=[0x%llx..0x%llx) "
              "arena=[0x%lx..0x%lx))\n", a,
              g_ctrl ? (unsigned long long)g_ctrl->shadow_base : 0ULL,
              g_ctrl ? (unsigned long long)g_ctrl->shadow_cur : 0ULL,
              (unsigned long)g_arena_base, (unsigned long)g_arena_end);
      fflush(stderr);
   }
   return (uint64_t)a;                      /* nil-ish / already-low passthrough */
}

static uint64_t unwrap_obj_arg(uint32_t a) {
   uint64_t r = unwrap_obj_arg_core(a);
   if (__builtin_expect(ARGSTR_TRACE(), 0)) {
      char desc[320];
      argstr_describe(r, desc, sizeof desc);
      fprintf(stderr, "[argstr] i386=0x%08x -> %s (caller=%p)\n",
              a, desc, __builtin_return_address(0));
      fflush(stderr);
   }
   return r;
}

/* ---- ObjC type-encoding scanners ---- */
static const char *enc_skip_quals(const char *t) {
   while (*t && strchr(ENC_QUALS, *t)) { ++t; }
   return t;
}
/* Advance past ONE complete type (handles ^ptr, {struct}, [array], (union),
 * bN bitfield). Does not skip trailing offset digits. */
static const char *enc_skip_type(const char *t) {
   t = enc_skip_quals(t);
   if (!*t) { return t; }
   char c = *t++;
   switch (c) {
   case '^': return enc_skip_type(t);        /* pointer-to */
   case 'b': while (*t >= '0' && *t <= '9') { ++t; } return t;  /* bitfield */
   case '{': case '[': case '(': {
      char close = (c == '{') ? '}' : (c == '[') ? ']' : ')';
      int depth = 1;
      while (*t && depth) { if (*t == c) { ++depth; } else if (*t == close) { --depth; } ++t; }
      return t;
   }
   default: return t;
   }
}
static const char *enc_skip_digits(const char *t) {
   while (*t >= '0' && *t <= '9') { ++t; }
   return t;
}

/* ===========================================================================
 * ObjC BLOCK marshalling bridge.
 *
 * A translated i386 block that crosses into the native runtime breaks two ways:
 *  - LAYOUT: the i386 Block_layout has 4-byte fields (isa@0 flags@4 reserved@8
 *    invoke@12 descriptor@16), the x86_64 one has 8-byte fields (isa@0 flags@8
 *    reserved@12 invoke@16 descriptor@24) -> native _Block_copy reads a garbage
 *    descriptor/size and crashes.
 *  - ISA: the block's isa (__NSConcrete*Block) was stored by the i386 code via a
 *    4-byte load of a __nl_symbol_ptr bound to the NATIVE 64-bit class, so it is
 *    TRUNCATED to a low-32 class pointer (the __IMPORT,__pointers family).
 *
 * Two complementary fixes, BOTH triggered on STRUCTURE (the block's isa is one
 * of the three concrete block classes) and never on app identity:
 *
 *  (1) bp_block_copy (forward prep): [block copy]/retain/release on an i386
 *      block is handled inline to produce an i386-LAYOUT heap copy (i386_block_
 *      copy) the translated code keeps invoking via the i386 layout.
 *
 *  (2) i386_block_to_native (via unwrap_obj_arg): an i386 block passed as a @?/@
 *      argument to a native method that will INVOKE it (enumerate/sort/GCD/
 *      completion) is replaced with a synthesized NATIVE Block_layout whose
 *      `invoke` is block_tramp.asm -> x64_blk_invoke, marshalling the native
 *      SysV call down to the i386 cdecl invoke via _86x64_call_i386, with the
 *      real native isa. The marshalling signature is parsed from the block's
 *      own descriptor signature string.
 * ======================================================================== */

/* libsystem_blocks concrete block classes — the real native isa values. The
 * i386 block stores the low-32 half of these. (Declared as the runtime does.) */
extern void *_NSConcreteStackBlock[32];
extern void *_NSConcreteGlobalBlock[32];
extern void *_NSConcreteMallocBlock[32];

enum {
   I386_BLOCK_DEALLOCATING     = 0x0001u,
   I386_BLOCK_REFCOUNT_MASK    = 0xfffeu,
   I386_BLOCK_NEEDS_FREE       = (1u << 24),
   I386_BLOCK_HAS_COPY_DISPOSE = (1u << 25),
   I386_BLOCK_IS_GLOBAL        = (1u << 28),
   I386_BLOCK_HAS_SIGNATURE    = (1u << 30),
};

/* cb_bridge arg/ret kind codes, mirrored locally for the block dispatcher. */
enum { BLK_I32 = 0, BLK_I64 = 1, BLK_PTR = 2, BLK_OBJ = 3, BLK_F32 = 4, BLK_F64 = 5 };
enum { BLKR_VOID = 0, BLKR_I32 = 1, BLKR_PTR = 2, BLKR_OBJ = 3, BLKR_I32SX = 4 };

#define BLK_MAX_ARGS   16
#define BLK_LOWSTACK_SZ (256u * 1024u)

struct blk_sig {
   uint8_t nargs;                       /* incl. arg0 (the block self) */
   uint8_t ret_kind;                    /* BLKR_* */
   uint8_t arg_kinds[BLK_MAX_ARGS];     /* BLK_* */
};

/* Native x86_64 Block_layout + a private trailer x64_blk_invoke reads. */
struct x64_native_block {
   void    *isa;                 /* +0  real native concrete-block class */
   int32_t  flags;              /* +8                                   */
   int32_t  reserved;           /* +12                                  */
   void    *invoke;             /* +16 -> _x64_blk_invoke_tramp          */
   const void *descriptor;      /* +24                                  */
   /* trailer (native side only): */
   uint32_t i386_block;         /* +32 the i386 block address (low-4GB)  */
   uint32_t i386_invoke;        /* +36 the i386 invoke fn (low-4GB)      */
   struct blk_sig sig;          /* +40 marshalling descriptor            */
};

static const struct { uint64_t reserved; uint64_t size; } g_blk_desc = {
   0, sizeof(struct x64_native_block)
};

/* block_tramp.asm exports the Mach-O symbol `_x64_blk_invoke_tramp`; the C
 * name omits the leading underscore (the toolchain adds it), as cb_bridge does
 * for x64_cb_tramp_table. */
extern void x64_blk_invoke_tramp(void);

/* Structural block detection: returns 1 stack / 2 global / 3 malloc / 0 none.
 * Compares the truncated 4-byte isa against the low-32 of each concrete block
 * class, then hardens with a sane descriptor+size so a coincidental isa value
 * cannot false-match a non-block object. */
static int i386_block_kind(uint32_t p) {
   if (!p || !mem_readable(p, 20)) { return 0; }
   uint32_t isa = *(const uint32_t *)(uintptr_t)p;
   int kind;
   if      (isa == (uint32_t)(uintptr_t)_NSConcreteStackBlock)  { kind = 1; }
   else if (isa == (uint32_t)(uintptr_t)_NSConcreteMallocBlock) { kind = 3; }
   else if (isa == (uint32_t)(uintptr_t)_NSConcreteGlobalBlock) { kind = 2; }
   else { return 0; }
   uint32_t desc = *(const uint32_t *)(uintptr_t)(p + 16);
   if (!mem_readable(desc, 8)) { return 0; }
   uint32_t size = *(const uint32_t *)(uintptr_t)(desc + 4);
   if (size < 16 || size > 0x100000u) { return 0; }
   return kind;
}

/* The block's ObjC signature string (i386 char*), or NULL. Lives in the
 * descriptor past reserved(4)+size(4), plus copy(4)+dispose(4) iff the block
 * HAS_COPY_DISPOSE.
 *
 * NB a GLOBAL block (kind 2) is statically allocated in the translated image,
 * and the translator widened its isa to a full 8-byte native pointer IN PLACE,
 * clobbering the 4-byte flags field at offset 4 with the isa's high half. So
 * flags is unreadable for globals — but a global block is captureless by
 * construction (a capturing block would be a STACK block), hence never has
 * copy/dispose, so its signature is unconditionally at descriptor+8. Stack and
 * malloc blocks keep an intact 4-byte isa + flags, so use flags normally. */
static const char *i386_block_signature(uint32_t p, int kind) {
   uint32_t desc = *(const uint32_t *)(uintptr_t)(p + 16);
   uint32_t off  = 8;                                   /* reserved(4)+size(4) */
   if (kind != 2) {
      uint32_t flags = *(const uint32_t *)(uintptr_t)(p + 4);
      if (!(flags & I386_BLOCK_HAS_SIGNATURE)) { return NULL; }
      if (flags & I386_BLOCK_HAS_COPY_DISPOSE) { off += 8; }  /* copy(4)+dispose(4) */
   }
   if (!mem_readable((uintptr_t)desc + off, 4)) { return NULL; }
   uint32_t sigp = *(const uint32_t *)(uintptr_t)(desc + off);
   if (!sigp || !legacy_cstr_ok(sigp)) { return NULL; }
   return (const char *)(uintptr_t)sigp;
}

/* Map one ObjC type-encoding to a BLK arg/ret kind (l/L are 4-byte on i386). */
static int blk_arg_kind(const char *t) {
   t = enc_skip_quals(t);
   switch (*t) {
   case '@': case '#':                                 return BLK_OBJ;
   case ':': case '^': case '*':                       return BLK_PTR;
   case 'f':                                           return BLK_F32;
   case 'd':                                           return BLK_F64;
   case 'q': case 'Q':                                 return BLK_I64;
   case 'i': case 'I': case 's': case 'S': case 'c':
   case 'C': case 'l': case 'L': case 'B':             return BLK_I32;
   default:                                            return -1;
   }
}
static int blk_ret_kind(const char *t) {
   t = enc_skip_quals(t);
   switch (*t) {
   case 'v':                                           return BLKR_VOID;
   case '@': case '#':                                 return BLKR_OBJ;
   case '^': case '*': case ':':                       return BLKR_PTR;
   case 'i': case 's': case 'c': case 'l':             return BLKR_I32SX;
   default:                                            return BLKR_I32;
   }
}

/* Advance past ONE complete type encoding + its trailing frame-offset digits.
 * enc_skip_type stops right after '@' without consuming the block marker '?'
 * (@?) or a '"ClassName"' protocol/class suffix (@"NSString"), which appear in
 * block signatures — handle those here so the per-arg walk stays in sync. */
static const char *blk_skip_one(const char *t) {
   t = enc_skip_quals(t);
   char base = *t;
   const char *after = enc_skip_type(t);
   if (base == '@') {
      if (*after == '?') {
         ++after;                                    /* block: @? */
      } else if (*after == '"') {                    /* @"ClassName" */
         ++after;
         while (*after && *after != '"') { ++after; }
         if (*after == '"') { ++after; }
      }
   }
   return enc_skip_digits(after);
}

/* Parse the block signature into sig. Returns 0 on failure (no signature, a
 * struct/array arg we cannot marshal, or too many args). Arg0 (the block self,
 * @?) is recorded but replaced by the i386 block pointer at invoke time. */
static int blk_parse_sig(const char *types, struct blk_sig *sig) {
   if (!types) { return 0; }
   const char *t = enc_skip_quals(types);
   sig->ret_kind = (uint8_t)blk_ret_kind(t);
   t = blk_skip_one(t);                              /* return + frame size */
   unsigned n = 0;
   while (*t && n < BLK_MAX_ARGS) {
      int ak = blk_arg_kind(t);
      if (ak < 0) { return 0; }                      /* struct/array arg */
      sig->arg_kinds[n++] = (uint8_t)ak;
      t = blk_skip_one(t);
   }
   if (*t || n == 0) { return 0; }                   /* overflow / no self arg */
   sig->nargs = (uint8_t)n;
   return 1;
}

/* (i386_block, i386_invoke) -> synthesized native block. Dedup keeps the table
 * bounded; the native block forwards to whatever i386 block currently lives at
 * the address (captures are read live by the i386 invoke), so reusing a binding
 * for a recycled stack address that holds the same block literal is correct. */
#define BLK_BIND_CAP 2048u
struct blk_bind_ent {
   uint32_t i386_block;
   uint32_t i386_invoke;
   struct x64_native_block *nat;
};
static struct blk_bind_ent g_blk_bind[BLK_BIND_CAP];
static os_unfair_lock      g_blk_bind_lock = OS_UNFAIR_LOCK_INIT;

static struct x64_native_block *blk_make(uint32_t p, uint32_t invoke,
                                         const struct blk_sig *sig) {
   struct x64_native_block *b = (struct x64_native_block *)malloc(sizeof *b);
   if (!b) { return NULL; }
   /* Present as a GLOBAL block: native _Block_copy returns it unchanged and
    * _Block_release no-ops, so the binding (and the i386 block it forwards to)
    * outlive the synchronous native call without an allocator mismatch. */
   b->isa        = (void *)_NSConcreteGlobalBlock;
   b->flags      = (int32_t)I386_BLOCK_IS_GLOBAL;
   b->reserved   = 0;
   b->invoke     = (void *)x64_blk_invoke_tramp;
   b->descriptor = &g_blk_desc;
   b->i386_block = p;
   b->i386_invoke = invoke;
   b->sig        = *sig;
   return b;
}

static uint64_t i386_block_to_native(uint32_t p) {
   int kind = i386_block_kind(p);
   if (!kind) { return 0; }
   uint32_t invoke = *(const uint32_t *)(uintptr_t)(p + 12);
   if (!invoke) { return 0; }
   struct blk_sig sig;
   if (!blk_parse_sig(i386_block_signature(p, kind), &sig)) {
      /* No parseable signature: cannot reliably marshal native->i386 args.
       * Pass the i386 block through unchanged (legacy behavior). */
      return 0;
   }
   uint32_t h = (p * 2654435761u + invoke) & (BLK_BIND_CAP - 1);
   os_unfair_lock_lock(&g_blk_bind_lock);
   for (uint32_t n = 0; n < BLK_BIND_CAP; ++n) {
      struct blk_bind_ent *e = &g_blk_bind[h];
      if (!e->nat) {
         struct x64_native_block *b = blk_make(p, invoke, &sig);
         if (b) { e->i386_block = p; e->i386_invoke = invoke; e->nat = b; }
         os_unfair_lock_unlock(&g_blk_bind_lock);
         return b ? (uint64_t)(uintptr_t)b : 0;
      }
      if (e->i386_block == p && e->i386_invoke == invoke) {
         os_unfair_lock_unlock(&g_blk_bind_lock);
         return (uint64_t)(uintptr_t)e->nat;
      }
      h = (h + 1) & (BLK_BIND_CAP - 1);
   }
   os_unfair_lock_unlock(&g_blk_bind_lock);
   /* Table full (extreme): allocate uncached so we never crash. */
   {
      struct x64_native_block *b = blk_make(p, invoke, &sig);
      return b ? (uint64_t)(uintptr_t)b : 0;
   }
}

/* Run an i386 function (cdecl) from native code on a fresh low-4GB stack. */
static uint32_t blk_call_i386(uint32_t fn, uint32_t nwords, const uint32_t *words) {
   void *stk = malloc(BLK_LOWSTACK_SZ);
   if (!stk) { return 0; }
   uint64_t top = ((uint64_t)(uintptr_t)stk + BLK_LOWSTACK_SZ) & ~0xfULL;
   x64_cb_enter();
   uint32_t r = _86x64_call_i386(fn, nwords, words, top);
   x64_cb_leave();
   free(stk);
   return r;
}

/* Produce an i386-LAYOUT heap copy (low-4GB) of an i386 block so the translated
 * code keeps invoking it via the i386 layout. Mirrors _Block_copy for i386
 * blocks: malloc size bytes, memcpy, retype isa to the malloc class, set the
 * NEEDS_FREE/refcount flags, and run the i386 copy helper (retains captured
 * objects/blocks) if HAS_COPY_DISPOSE. Global blocks copy to themselves. */
static uint32_t i386_block_copy(uint32_t p) {
   int kind = i386_block_kind(p);
   if (kind == 0) { return p; }
   if (kind == 2) { return p; }                       /* global: identity */
   uint32_t desc  = *(const uint32_t *)(uintptr_t)(p + 16);
   uint32_t flags = *(const uint32_t *)(uintptr_t)(p + 4);
   uint32_t size  = *(const uint32_t *)(uintptr_t)(desc + 4);
   if (size < 16 || size > 0x100000u || !mem_readable(p, size)) { return p; }
   void *q = malloc(size);                             /* low-4GB shim malloc */
   if (!q || (uintptr_t)q >= 0x100000000UL) { free(q); return p; }
   memcpy(q, (const void *)(uintptr_t)p, size);
   uint8_t *qb = (uint8_t *)q;
   *(uint32_t *)qb = (uint32_t)(uintptr_t)_NSConcreteMallocBlock;   /* malloc isa */
   uint32_t f = *(uint32_t *)(qb + 4);
   f &= ~(uint32_t)(I386_BLOCK_REFCOUNT_MASK | I386_BLOCK_DEALLOCATING);
   f |= I386_BLOCK_NEEDS_FREE | (1u << 1);             /* refcount = 1 */
   *(uint32_t *)(qb + 4) = f;
   if (flags & I386_BLOCK_HAS_COPY_DISPOSE) {
      uint32_t copyfn = *(const uint32_t *)(uintptr_t)(desc + 8);
      if (copyfn) {
         uint32_t words[2] = { (uint32_t)(uintptr_t)q, p };  /* copy(dst, src) */
         blk_call_i386(copyfn, 2, words);
      }
   }
   return (uint32_t)(uintptr_t)q;
}

/* The C dispatcher behind every synthesized native block's invoke trampoline
 * (block_tramp.asm). Recovers the bound i386 block + invoke from the native
 * block, marshals the native SysV args down to the i386 cdecl frame per the
 * block signature (the block self becomes the i386 invoke's first arg), and
 * re-enters the translated invoke. Classification mirrors cb_dispatch. */
uint64_t x64_blk_invoke(uint64_t blockp, const uint64_t *gp, const uint64_t *fp,
                        const uint64_t *stk);
uint64_t x64_blk_invoke(uint64_t blockp, const uint64_t *gp, const uint64_t *fp,
                        const uint64_t *stk) {
   struct x64_native_block *b = (struct x64_native_block *)(uintptr_t)blockp;
   const struct blk_sig *sig = &b->sig;
   uint32_t words[BLK_MAX_ARGS * 2];
   uint32_t w = 0, gpi = 1, fpi = 0, sti = 0;          /* gp[0]/rdi = the block */
   words[w++] = b->i386_block;                         /* i386 invoke self arg */
   for (uint32_t i = 1; i < sig->nargs; ++i) {         /* skip arg0 (the block) */
      uint8_t kind = sig->arg_kinds[i];
      uint64_t v = (kind == BLK_F32 || kind == BLK_F64)
                   ? (fpi < 8 ? fp[fpi++] : stk[sti++])
                   : (gpi < 6 ? gp[gpi++] : stk[sti++]);
      switch (kind) {
      case BLK_I32: case BLK_PTR: case BLK_F32:
         words[w++] = (uint32_t)v;
         break;
      case BLK_OBJ:
         words[w++] = v >= 0x100000000ULL ? x64_objc_wrap(v) : (uint32_t)v;
         break;
      case BLK_I64: case BLK_F64:
         words[w++] = (uint32_t)v;
         words[w++] = (uint32_t)(v >> 32);
         break;
      default:
         break;
      }
   }
   uint32_t eax = blk_call_i386(b->i386_invoke, w, words);
   switch (sig->ret_kind) {
   case BLKR_VOID:  return 0;
   case BLKR_OBJ:   return x64_objc_unwrap(eax);       /* handle -> real object */
   case BLKR_I32SX: return (uint64_t)(int64_t)(int32_t)eax;
   default:         return eax;                        /* I32 / PTR */
   }
}

/* ---- the C prep called by _86x64_reverse_imp ---- */
#define REV_STACK_SZ (512u * 1024u)

/* Per-thread pool of retired reverse-IMP low stacks.
 *
 * A reverse-IMP runs the translated legacy IMP on a private low-4GB stack and,
 * on return, used to free() that stack straight back to the shared low-4GB
 * heap (malloc_shim). But a just-returned IMP frame can still be referenced for
 * an instant: a forward C-shim called from the IMP writes its out-parameter
 * (e.g. FSGetDataForkName -> *HFSUniStr255) back into a buffer on that frame, a
 * method returns a pointer into its frame, etc. If the block has gone back to
 * the general heap, an unrelated allocation repurposes it and the lingering
 * access reads/writes foreign data — read back as 0 it became a translated
 * `ret` to 0 (rip=0); the deep library-open reverse-IMP nesting hit exactly
 * this. Keeping retired stacks in a reverse-stack-ONLY per-thread LIFO pool
 * (never handed to general malloc) closes the window: the memory is only ever
 * reused by another reverse-IMP, LIFO, after the referencing frame is dead. It
 * also removes the malloc/free churn the nesting generates. Per-thread + per
 * libabiconv-copy: each copy frees what it allocated, so the pool is consistent
 * with the stash/LIFO discipline. */
#define REV_POOL_MAX 128
static __thread void *g_rev_pool[REV_POOL_MAX];
static __thread uint32_t g_rev_pool_n;

static void *rev_stack_alloc(void) {
   if (g_rev_pool_n) { return g_rev_pool[--g_rev_pool_n]; }
   return malloc(REV_STACK_SZ);
}
static void rev_stack_free(void *p) {
   if (!p) { return; }
   if (getenv("ABICONV_REVSTACK_LEAK")) { return; }   /* diagnostic: never reuse */
   if (g_rev_pool_n < REV_POOL_MAX) { g_rev_pool[g_rev_pool_n++] = p; return; }
   free(p);
}

/* Zero-I/O reverse-IMP breadcrumb ring (mirror of cb_bridge's). reverse_prep
 * records each legacy IMP it is about to dispatch (no fprintf — preserves the
 * thread timing the return-to-0 crash depends on); a crash handler dumps it via
 * dlsym("x64_rev_dump_ring"). depth = current native->legacy nesting. */
struct rev_crumb {
   uint64_t self_; uint64_t sel; uint64_t imp;
   uint64_t lowstack_base; uint64_t lowstack_top;
   uint32_t tid; uint32_t depth; uint32_t frame_words; uint32_t valid;
};
#define REV_RING 64
static struct rev_crumb g_rev_ring[REV_RING];
static uint32_t         g_rev_pos;
static int              g_rev_depth;

/* Cross-copy callback-depth (shared ctrl). All native->translated entry paths
 * call enter/leave so any copy's crash handler sees the true global nesting. */
int  x64_cb_active(void);
void x64_cb_enter(void);
void x64_cb_leave(void);
int  x64_cb_active(void) { arena_init(); return g_ctrl ? __atomic_load_n(&g_ctrl->cb_active_depth, __ATOMIC_SEQ_CST) : -1; }
void x64_cb_enter(void)  { arena_init(); if (g_ctrl) __atomic_add_fetch(&g_ctrl->cb_active_depth, 1, __ATOMIC_SEQ_CST); }
void x64_cb_leave(void)  { if (g_ctrl) __atomic_sub_fetch(&g_ctrl->cb_active_depth, 1, __ATOMIC_SEQ_CST); }

void x64_fwd_dump_ring(void);
void x64_fwd_dump_ring(void) {
   fprintf(stderr, "[xdepth] cross-copy callback depth = %d\n", x64_cb_active());
   uint32_t pos = __atomic_load_n(&g_fwd_pos, __ATOMIC_SEQ_CST);
   fprintf(stderr, "[fwd-ring] last objc sends (newest first), min_sp=0x%llx:\n",
           (unsigned long long)g_fwd_min_sp);
   for (int i = 0; i < 24; ++i) {
      uint32_t idx = (pos - 1 - i) & (FWD_RING - 1);
      struct fwd_crumb *c = &g_fwd_ring[idx];
      if (!c->valid) continue;
      const char *sn = (c->sel && mem_readable((uintptr_t)c->sel, 1))
                       ? sel_getName((SEL)(uintptr_t)c->sel) : "?";
      fprintf(stderr, "  [%2d] self32=0x%-8x sel=%-28s caller_ra=0x%-8x sp=0x%llx t=%x\n",
              i, c->self32, sn, c->caller_ra, (unsigned long long)c->sp, c->tid);
   }
   fflush(stderr);
}

/* ── native-callee breadcrumb ───────────────────────────────────────────────
 * Records which abigen-generated shim a thread last entered (the shim's own
 * address, which dladdr resolves to the bridged native fn's name) plus the i386
 * caller's return address. Lets a crash handler (geoshim) name the native
 * function a faulting thread was executing when the low-4GB stack is unwalkable
 * — the durable fix for iPhoto's "native code through 0 on a worker thread"
 * (s42) and a native-call trace for Portal 2's silent launcher early-exit.
 *
 * Per-copy static ring, exactly like g_fwd_ring; geoshim already enumerates
 * every libabiconv copy and calls each copy's x64_*_dump_ring. The writer is
 * called from a shim's prologue ONLY when abiconv.asm was generated with
 * ABICONV_GEN_NATIVE_CRUMB (default build never calls it → ring stays empty and
 * x64_native_dump_ring is a no-op), so it is zero-cost unless built to diagnose. */
struct native_crumb { uint64_t shim; uint32_t caller_ra; uint32_t tid; uint32_t valid; };
#define NATIVE_RING 256
static struct native_crumb g_native_ring[NATIVE_RING];
static uint32_t            g_native_pos;

void abiconv_native_crumb(uint64_t shim, uint64_t caller_ra);
void abiconv_native_crumb(uint64_t shim, uint64_t caller_ra) {
   uint32_t ri = __atomic_fetch_add(&g_native_pos, 1, __ATOMIC_SEQ_CST) & (NATIVE_RING - 1);
   struct native_crumb *c = &g_native_ring[ri];
   c->shim = shim; c->caller_ra = (uint32_t)caller_ra;
   c->tid = pthread_mach_thread_np(pthread_self()); c->valid = 1;
}

void x64_native_dump_ring(void);
void x64_native_dump_ring(void) {
   uint32_t pos = __atomic_load_n(&g_native_pos, __ATOMIC_SEQ_CST);
   if (!pos && !g_native_ring[0].valid) return;   /* built without the crumb */
   fprintf(stderr, "[native-ring] last native shims entered (newest first):\n");
   for (int i = 0; i < 24; ++i) {
      uint32_t idx = (pos - 1 - i) & (NATIVE_RING - 1);
      struct native_crumb *c = &g_native_ring[idx];
      if (!c->valid) continue;
      Dl_info info; const char *nm = "?";
      if (dladdr((void *)(uintptr_t)c->shim, &info) && info.dli_sname) nm = info.dli_sname;
      fprintf(stderr, "  [%2d] shim=%-30s (0x%llx) caller_ra=0x%-8x t=%x\n",
              i, nm, (unsigned long long)c->shim, c->caller_ra, c->tid);
   }
   fflush(stderr);
}

void x64_rev_dump_ring(void);
void x64_rev_dump_ring(void) {
   uint32_t pos = __atomic_load_n(&g_rev_pos, __ATOMIC_SEQ_CST);
   fprintf(stderr, "[rev-ring] last legacy IMPs (newest first), depth=%d:\n",
           __atomic_load_n(&g_rev_depth, __ATOMIC_SEQ_CST));
   for (int i = 0; i < REV_RING; ++i) {
      uint32_t idx = (pos - 1 - i) & (REV_RING - 1);
      struct rev_crumb *c = &g_rev_ring[idx];
      if (!c->valid) continue;
      const char *sn = (c->sel && mem_readable((uintptr_t)c->sel, 1))
                       ? sel_getName((SEL)(uintptr_t)c->sel) : "?";
      fprintf(stderr, "  [%2d] imp=0x%-8llx self=0x%-11llx sel=%s t=%x d=%u "
              "fw=%u stk=[0x%llx..0x%llx]\n",
              i, (unsigned long long)c->imp, (unsigned long long)c->self_, sn,
              c->tid, c->depth, c->frame_words,
              (unsigned long long)c->lowstack_base,
              (unsigned long long)c->lowstack_top);
   }
   fflush(stderr);
}

/* ======================================================================
 * GL-drawable probe (env ABICONV_GL_PROBE) — Quinn black-splash diagnostic.
 * QuinnSplashView (a legacy NSOpenGLView subclass) receives drawRect:, issues
 * GL, and tail-calls [[self openGLContext] flushBuffer], yet nothing
 * composites (content area is black). Swizzle the native NSOpenGLContext
 * present/attach entry points and log the attached view + layer state so we
 * can see whether the context has a valid drawable. Installed lazily on the
 * first reverse-dispatch (AppKit is fully up by then). Inert unless the env
 * var is set. Pure native ObjC/C — no legacy bridge re-entry.
 * ==================================================================== */
static IMP g_glp_flushBuffer, g_glp_setView, g_glp_makeCurrent, g_glp_clearDrawable;

static void glp_dump_ctx(const char *tag, id ctx) {
   if (!ctx) { fprintf(stderr, "[glp] %s ctx=nil\n", tag); return; }
   id view = ((id(*)(id,SEL))objc_msgSend)(ctx, sel_registerName("view"));
   const char *vc = view ? object_getClassName(view) : "(nil)";
   long opaque = 0, wantsLayer = 0; void *layer = NULL, *win = NULL;
   if (view) {
      opaque     = ((long(*)(id,SEL))objc_msgSend)(view, sel_registerName("isOpaque"));
      wantsLayer = ((long(*)(id,SEL))objc_msgSend)(view, sel_registerName("wantsLayer"));
      layer      = (void*)((id(*)(id,SEL))objc_msgSend)(view, sel_registerName("layer"));
      win        = (void*)((id(*)(id,SEL))objc_msgSend)(view, sel_registerName("window"));
   }
   /* view frame (NSRect via objc_msgSend_stret on x86_64 for >16B struct) */
   struct { double x,y,w,h; } fr = {0,0,0,0};
   if (view) {
      ((void(*)(void*,id,SEL))objc_msgSend_stret)(&fr, view, sel_registerName("frame"));
   }
   void *cgl = (void*)((id(*)(id,SEL))objc_msgSend)(ctx, sel_registerName("CGLContextObj"));
   const char *lc = layer ? object_getClassName((id)layer) : "(nil)";
   fprintf(stderr,
      "[glp] %s ctx=%p view=%p<%s> frame=%.0fx%.0f@(%.0f,%.0f) opaque=%ld "
      "wantsLayer=%ld layer=%p<%s> window=%p cgl=%p\n",
      tag, (void*)ctx, (void*)view, vc, fr.w, fr.h, fr.x, fr.y, opaque,
      wantsLayer, layer, lc, win, cgl);
   fflush(stderr);
}

/* Native GL entry points (libabiconv links OpenGL.framework — it hosts the
 * ___gl* abigen shims). Sample the back buffer to see if GL actually rendered
 * content before the swap. */
extern unsigned glGetError(void);
extern void glReadBuffer(unsigned);
extern void glReadPixels(int,int,int,int,unsigned,unsigned,void*);
extern void glFinish(void);
extern void glClearColor(float,float,float,float);
extern void glClear(unsigned);
extern void glDrawBuffer(unsigned);
extern void glDisable(unsigned);
extern void glMatrixMode(unsigned);
extern void glPushMatrix(void);
extern void glPopMatrix(void);
extern void glLoadIdentity(void);
extern void glColor4f(float,float,float,float);
extern void glBegin(unsigned);
extern void glEnd(void);
extern void glVertex3f(float,float,float);
extern void glViewport(int,int,int,int);
extern void glTexEnvi(unsigned,unsigned,int);
extern void glEnable(unsigned);
extern void glTexCoord2f(float,float);
extern void glBindTexture(unsigned,unsigned);
extern void glGetIntegerv(unsigned,int*);
extern unsigned char glIsTexture(unsigned);
extern void glGetTexLevelParameteriv(unsigned,int,unsigned,int*);
extern void glGetTexParameteriv(unsigned,unsigned,int*);
extern void glGetTexImage(unsigned,int,unsigned,unsigned,void*);
#define GLP_GL_BACK 0x0405
#define GLP_GL_RGBA 0x1908
#define GLP_GL_UBYTE 0x1401
#define GLP_GL_COLOR_BIT 0x4000
/* (Removed dyld GL interposers: they never caught libabiconv's own stub calls,
 * and interposing glGenTextures recursed via dlsym(RTLD_NEXT) when APPKIT's
 * NSCGLSurface flush path called it -> stack overflow, crash 2026-07-02-210828.) */

static void glp_flushBuffer(id self, SEL _cmd) {
   static int n = 0;
   if (getenv("ABICONV_GL_TESTCLEAR")) {
      /* Overwrite the back buffer with solid red just before the swap: if the
       * window turns red, the present path works and Quinn's own GL rendering
       * is the culprit; if still black, presentation itself is broken. */
      glDrawBuffer(GLP_GL_BACK);
      glClearColor(1.f, 0.f, 0.f, 1.f);
      glClear(GLP_GL_COLOR_BIT);
      glFinish();
   }
   if (getenv("ABICONV_GL_TESTQUAD")) {
      /* Overlay a solid green quad (no texture) over Quinn's render using
       * native immediate mode + identity matrices. Green => geometry/raster
       * work and Quinn's texture is empty; no green => a global GL-state issue
       * suppresses all drawing. */
      glDrawBuffer(GLP_GL_BACK);
      glDisable(0x0DE1 /*TEXTURE_2D*/); glDisable(0x0BE2 /*BLEND*/);
      glDisable(0x0B71 /*DEPTH_TEST*/); glDisable(0x0C11 /*SCISSOR_TEST*/);
      glMatrixMode(0x1701 /*PROJECTION*/); glPushMatrix(); glLoadIdentity();
      glMatrixMode(0x1700 /*MODELVIEW*/);  glPushMatrix(); glLoadIdentity();
      glColor4f(0.f, 1.f, 0.f, 1.f);
      glBegin(0x0007 /*QUADS*/);
      glVertex3f(-0.8f,-0.8f,0.f); glVertex3f(0.8f,-0.8f,0.f);
      glVertex3f(0.8f,0.8f,0.f);   glVertex3f(-0.8f,0.8f,0.f);
      glEnd();
      glPopMatrix(); glMatrixMode(0x1701); glPopMatrix();
      glFinish();
   }
   if (getenv("ABICONV_GL_TESTTEX")) {
      /* Draw a full-viewport quad textured with each candidate texture id and
       * log which ids are live textures. If the logo appears, the texture data
       * is valid and the bug is Quinn's matrices/viewport; if black, the
       * texture upload produced empty data. */
      /* Quinn uses GL_TEXTURE_RECTANGLE_ARB (0x84F5), non-normalized coords. */
      #define GLP_RECT 0x84F5
      int wq=0, hq=0;
      static int logged = 0;
      if (!logged) { logged = 1;
         char b[256]; int p = 0;
         for (unsigned t = 1; t <= 16; t++)
            if (glIsTexture(t)) p += snprintf(b+p, sizeof(b)-p, "%u ", t);
         fprintf(stderr, "[glp] liveTex={ %s}\n", b);
         for (unsigned t = 1; t <= 13; t++) {
            if (!glIsTexture(t)) continue;
            glBindTexture(GLP_RECT, t);
            int w=0,h=0,ifmt=0,minf=0,magf=0;
            glGetTexLevelParameteriv(GLP_RECT,0,0x1000,&w);
            glGetTexLevelParameteriv(GLP_RECT,0,0x1001,&h);
            glGetTexLevelParameteriv(GLP_RECT,0,0x1003,&ifmt);
            glGetTexParameteriv(GLP_RECT,0x2801,&minf);
            glGetTexParameteriv(GLP_RECT,0x2800,&magf);
            /* read back the pixels: count non-zero + row/col distribution */
            long nzc = -1, nza = -1;
            int r0=-1, r1=-1, nrows=0, c0=-1, c1=-1;
            if (w > 0 && h > 0 && (long)w*h*4 < 8*1024*1024) {
               unsigned char *buf = (unsigned char*)calloc((size_t)w*h, 4);
               if (buf) {
                  glGetTexImage(GLP_RECT,0,0x1908/*RGBA*/,0x1401/*UBYTE*/,buf);
                  nzc = nza = 0;
                  for (int y = 0; y < h; y++) {
                     int rownz = 0;
                     for (int x = 0; x < w; x++) {
                        long i = (long)y*w + x;
                        int nz = buf[i*4]|buf[i*4+1]|buf[i*4+2]|buf[i*4+3];
                        if (buf[i*4]|buf[i*4+1]|buf[i*4+2]) nzc++;
                        if (buf[i*4+3]) nza++;
                        if (nz) { rownz = 1;
                           if (c0 < 0 || x < c0) c0 = x;
                           if (x > c1) c1 = x; }
                     }
                     if (rownz) { if (r0 < 0) r0 = y; r1 = y; nrows++; }
                  }
                  free(buf);
               }
            }
            fprintf(stderr,"[glp]  RECTtex%u L0=%dx%d ifmt=0x%x nzRGBpx=%ld nzApx=%ld rows=[%d..%d]n=%d cols=[%d..%d]\n",
                    t,w,h,ifmt,nzc,nza,r0,r1,nrows,c0,c1);
         }
         fflush(stderr);
      }
      const char *tid = getenv("ABICONV_GL_TEXID");
      unsigned useid = tid ? (unsigned)atoi(tid) : 1u;
      glBindTexture(GLP_RECT, useid);
      glGetTexLevelParameteriv(GLP_RECT,0,0x1000,&wq);
      glGetTexLevelParameteriv(GLP_RECT,0,0x1001,&hq);
      glDrawBuffer(GLP_GL_BACK);
      glViewport(0,0,1600,1400);          /* full drawable, bypass Quinn's 1x vp */
      glDisable(0x0BE2 /*BLEND*/); glDisable(0x0B71 /*DEPTH*/);
      glDisable(0x0C11 /*SCISSOR*/); glDisable(0x0DE1 /*TEXTURE_2D*/);
      glEnable(GLP_RECT);
      glTexEnvi(0x2300/*TEXTURE_ENV*/,0x2200/*ENV_MODE*/,0x1E01/*REPLACE*/);
      glMatrixMode(0x1701); glPushMatrix(); glLoadIdentity();
      glMatrixMode(0x1700); glPushMatrix(); glLoadIdentity();
      glColor4f(1.f,1.f,1.f,1.f);
      float fw = wq>0?(float)wq:1.f, fh = hq>0?(float)hq:1.f;
      glBegin(0x0007);
      glTexCoord2f(0,fh);   glVertex3f(-1.f,-1.f,0.f);
      glTexCoord2f(fw,fh);  glVertex3f(1.f,-1.f,0.f);
      glTexCoord2f(fw,0);   glVertex3f(1.f,1.f,0.f);
      glTexCoord2f(0,0);    glVertex3f(-1.f,1.f,0.f);
      glEnd();
      glDisable(GLP_RECT);
      glPopMatrix(); glMatrixMode(0x1701); glPopMatrix();
      glFinish();
   }
   if (n < 8) {
      glp_dump_ctx("flushBuffer", self);
      /* Scan a grid of the back buffer for any non-black pixel. */
      glFinish();
      glReadBuffer(GLP_GL_BACK);
      unsigned e0 = glGetError();
      int W = 785, H = 702, nonblack = 0; unsigned char mx = 0;
      for (int gy = 0; gy < 6; gy++) for (int gx = 0; gx < 6; gx++) {
         unsigned char px[4] = {0,0,0,0};
         glReadPixels((gx+1)*W/8, (gy+1)*H/8, 1, 1, GLP_GL_RGBA, GLP_GL_UBYTE, px);
         if (px[0]|px[1]|px[2]) { nonblack++;
            if (px[0]>mx) mx=px[0]; if (px[1]>mx) mx=px[1]; if (px[2]>mx) mx=px[2]; }
      }
      unsigned e1 = glGetError();
      fprintf(stderr, "[glp] backbuffer scan: nonblack=%d/36 maxc=%u glErr(read)=0x%x/0x%x\n",
              nonblack, mx, e0, e1); fflush(stderr);
   }
   n++;
   ((void(*)(id,SEL))g_glp_flushBuffer)(self, _cmd);
}
static void glp_setView(id self, SEL _cmd, id v) {
   fprintf(stderr, "[glp] setView: ctx=%p view=%p<%s>\n", (void*)self, (void*)v,
           v ? object_getClassName(v) : "(nil)"); fflush(stderr);
   ((void(*)(id,SEL,id))g_glp_setView)(self, _cmd, v);
   glp_dump_ctx("post-setView", self);
}
static void glp_makeCurrent(id self, SEL _cmd) {
   static int n = 0;
   if (n++ < 4) { fprintf(stderr, "[glp] makeCurrentContext ctx=%p\n", (void*)self); fflush(stderr); }
   ((void(*)(id,SEL))g_glp_makeCurrent)(self, _cmd);
}
static void glp_clearDrawable(id self, SEL _cmd) {
   fprintf(stderr, "[glp] clearDrawable ctx=%p\n", (void*)self); fflush(stderr);
   ((void(*)(id,SEL))g_glp_clearDrawable)(self, _cmd);
}

/* Pixel-format + context creation probes (hypothesis a: nil pf -> nil ctx). */
static IMP g_glp_pfInit, g_glp_ctxInit, g_glp_openGLContext, g_glp_pixelFormat;
static id glp_pfInit(id self, SEL _cmd, const uint32_t *attrs) {
   char buf[512]; int p = 0;
   buf[0] = 0;
   if (attrs) {
      for (int i = 0; i < 40 && attrs[i]; i++)
         p += snprintf(buf+p, sizeof(buf)-p, "%u ", attrs[i]);
   }
   id r = ((id(*)(id,SEL,const uint32_t*))g_glp_pfInit)(self, _cmd, attrs);
   fprintf(stderr, "[glp] NSOpenGLPixelFormat initWithAttributes:{ %s} -> %p\n",
           buf, (void*)r); fflush(stderr);
   return r;
}
static id glp_ctxInit(id self, SEL _cmd, id fmt, id share) {
   id r = ((id(*)(id,SEL,id,id))g_glp_ctxInit)(self, _cmd, fmt, share);
   fprintf(stderr, "[glp] NSOpenGLContext initWithFormat:%p shareContext:%p -> %p\n",
           (void*)fmt, (void*)share, (void*)r); fflush(stderr);
   return r;
}
static id glp_openGLContext(id self, SEL _cmd) {
   id r = ((id(*)(id,SEL))g_glp_openGLContext)(self, _cmd);
   static int n = 0;
   if (n++ < 6)
      fprintf(stderr, "[glp] -[%s openGLContext] -> ctx=%p\n",
              object_getClassName(self), (void*)r), fflush(stderr);
   return r;
}
static id glp_pixelFormat(id self, SEL _cmd) {
   id r = ((id(*)(id,SEL))g_glp_pixelFormat)(self, _cmd);
   static int n = 0;
   if (n++ < 6)
      fprintf(stderr, "[glp] -[%s pixelFormat] -> pf=%p\n",
              object_getClassName(self), (void*)r), fflush(stderr);
   return r;
}

static void glp_swz(Class C, const char *sel, IMP fn, IMP *slot) {
   if (!C) return;
   Method m = class_getInstanceMethod(C, sel_registerName(sel));
   if (m) { *slot = method_getImplementation(m); method_setImplementation(m, fn); }
   else fprintf(stderr, "[glp] no method %s on %s\n", sel, class_getName(C));
}

/* Force NSOpenGLView non-layer-backed (ABICONV_GL_NOLAYER test). setWantsLayer:
 * / wantsLayer live on NSView, so the swizzled Method is SHARED by every view —
 * coerce ONLY for NSOpenGLView instances (else the titlebar etc. break). */
static IMP g_glp_setWantsLayer, g_glp_wantsLayer;
static Class g_glp_nsoglview;
static int glp_is_glview(id o) {
   if (!g_glp_nsoglview) g_glp_nsoglview = objc_getClass("NSOpenGLView");
   for (Class c = object_getClass(o); c; c = class_getSuperclass(c))
      if (c == g_glp_nsoglview) return 1;
   return 0;
}
static void glp_setWantsLayer(id self, SEL _cmd, BOOL want) {
   if (glp_is_glview(self)) want = NO;
   ((void(*)(id,SEL,BOOL))g_glp_setWantsLayer)(self, _cmd, want);
}
static BOOL glp_wantsLayer(id self, SEL _cmd) {
   if (glp_is_glview(self)) return NO;
   return ((BOOL(*)(id,SEL))g_glp_wantsLayer)(self, _cmd);
}

/* CI probe: log every CIContext drawImage (the actual splash render lane —
 * QuinnSplashView drawRect: composites the logo via CIColorMatrix ->
 * [CIContext drawImage:atPoint:fromRect:]) + sample the CIImage's CONTENT by
 * rendering it into a scratch bitmap through a separate software CIContext. */
static IMP g_glp_ciDrawAt, g_glp_ciDrawIn;
static void glp_ci_sample(id ciimg, const char *tag) {
   /* extent (CGRect, stret) */
   struct { double x,y,w,h; } ex = {0,0,0,0};
   ((void(*)(void*,id,SEL))objc_msgSend_stret)(&ex, ciimg, sel_registerName("extent"));
   long nz = -1; unsigned maxb = 0;
   int w = (int)ex.w, h = (int)ex.h;
   const char *why = "";
   if (w > 0 && h > 0 && w < 4096 && h < 4096) {
      static id swctx;
      if (!swctx) {
         swctx = ((id(*)(id,SEL,id))objc_msgSend)((id)objc_getClass("CIContext"),
                    sel_registerName("contextWithOptions:"), (id)0);
         if (swctx) ((id(*)(id,SEL))objc_msgSend)(swctx, sel_registerName("retain"));
      }
      if (!swctx) { why = " NOCTX"; }
      else {
         CGRect b = CGRectMake(ex.x, ex.y, w, h);
         CGImageRef cg = ((CGImageRef(*)(id,SEL,id,CGRect))objc_msgSend)(
            swctx, sel_registerName("createCGImage:fromRect:"), ciimg, b);
         if (!cg) { why = " NOCGIMG"; }
         else {
            CGDataProviderRef dp = CGImageGetDataProvider(cg);
            CFDataRef data = dp ? CGDataProviderCopyData(dp) : NULL;
            if (!data) { why = " NODATA"; }
            else {
               const unsigned char *p = CFDataGetBytePtr(data);
               long n = CFDataGetLength(data);
               nz = 0;
               for (long i = 0; i < n; i++)
                  if (p[i]) { nz++; if (p[i] > maxb) maxb = p[i]; }
               CFRelease(data);
            }
            CGImageRelease(cg);
         }
      }
   }
   fprintf(stderr, "[ci] %s img=%p extent=%.0fx%.0f@(%.0f,%.0f) nzbytes=%ld max=%u%s\n",
           tag, (void*)ciimg, ex.w, ex.h, ex.x, ex.y, nz, maxb, why);
   fflush(stderr);
}

/* Sampler CONTROL: a natively-built solid red 8x8 CIImage must read non-zero
 * through the same path, proving the sampler works. Runs once. */
static void glp_ci_control(void) {
   static int done; if (done) return; done = 1;
   id c = ((id(*)(id,SEL,double,double,double,double))objc_msgSend)(
      (id)objc_getClass("CIColor"), sel_registerName("colorWithRed:green:blue:alpha:"),
      1.0, 0.0, 0.0, 1.0);
   id img = ((id(*)(id,SEL,id))objc_msgSend)((id)objc_getClass("CIImage"),
      sel_registerName("imageWithColor:"), c);
   img = ((id(*)(id,SEL,CGRect))objc_msgSend)(img,
      sel_registerName("imageByCroppingToRect:"), CGRectMake(0,0,8,8));
   glp_ci_sample(img, "CONTROL(red8x8)");
}
static void glp_ciDrawAt(id self, SEL _cmd, id img, CGPoint p, CGRect from) {
   static int n = 0;
   if (n++ < 12) { glp_ci_control(); glp_ci_sample(img, "drawImage:atPoint:"); }
   ((void(*)(id,SEL,id,CGPoint,CGRect))g_glp_ciDrawAt)(self, _cmd, img, p, from);
}
static void glp_ciDrawIn(id self, SEL _cmd, id img, CGRect in, CGRect from) {
   static int n = 0;
   if (n++ < 12) glp_ci_sample(img, "drawImage:inRect:");
   ((void(*)(id,SEL,id,CGRect,CGRect))g_glp_ciDrawIn)(self, _cmd, img, in, from);
}

/* Value-level probes on the CI creators + the file loader. */
static IMP g_glp_ciColor4, g_glp_ciColor3, g_glp_ciVec2, g_glp_ciVec4, g_glp_ciURL;
static id glp_ciColor4(id self, SEL _cmd, double r, double g, double b, double a) {
   id res = ((id(*)(id,SEL,double,double,double,double))g_glp_ciColor4)(self,_cmd,r,g,b,a);
   fprintf(stderr, "[civ] colorWithRGBA(%g,%g,%g,%g) -> %p\n", r,g,b,a,(void*)res);
   fflush(stderr); return res;
}
static id glp_ciColor3(id self, SEL _cmd, double r, double g, double b) {
   id res = ((id(*)(id,SEL,double,double,double))g_glp_ciColor3)(self,_cmd,r,g,b);
   fprintf(stderr, "[civ] colorWithRGB(%g,%g,%g) -> %p\n", r,g,b,(void*)res);
   fflush(stderr); return res;
}
static id glp_ciVec2(id self, SEL _cmd, double x, double y) {
   id res = ((id(*)(id,SEL,double,double))g_glp_ciVec2)(self,_cmd,x,y);
   fprintf(stderr, "[civ] vectorXY(%g,%g) -> %p\n", x,y,(void*)res);
   fflush(stderr); return res;
}
static id glp_ciVec4(id self, SEL _cmd, double x, double y, double z, double w2) {
   id res = ((id(*)(id,SEL,double,double,double,double))g_glp_ciVec4)(self,_cmd,x,y,z,w2);
   fprintf(stderr, "[civ] vectorXYZW(%g,%g,%g,%g) -> %p\n", x,y,z,w2,(void*)res);
   fflush(stderr); return res;
}
/* Log every CIColorMatrix setValue:forKey: (key + value description). */
static IMP g_glp_fltSVFK;
static void glp_fltSVFK(id self, SEL _cmd, id val, id key) {
   if (!strcmp(object_getClassName(self), "CIColorMatrix")) {
      const char *ks = key ? ((const char*(*)(id,SEL))objc_msgSend)(key, sel_registerName("UTF8String")) : "?";
      id d = val ? ((id(*)(id,SEL))objc_msgSend)(val, sel_registerName("description")) : 0;
      const char *ds = d ? ((const char*(*)(id,SEL))objc_msgSend)(d, sel_registerName("UTF8String")) : "(nil)";
      fprintf(stderr, "[cmx] set %s = %s\n", ks ? ks : "?", ds ? ds : "?");
      fflush(stderr);
   }
   ((void(*)(id,SEL,id,id))g_glp_fltSVFK)(self, _cmd, val, key);
}

/* Sample every CIFilter outputImage fetch: pinpoints WHICH filter step drops
 * the content (first ~24 fetches). */
static IMP g_glp_fltVFK;
static id glp_fltVFK(id self, SEL _cmd, id key) {
   id res = ((id(*)(id,SEL,id))g_glp_fltVFK)(self, _cmd, key);
   static int n = 0;
   if (n < 24 && res && key) {
      const char *ks = ((const char*(*)(id,SEL))objc_msgSend)(key, sel_registerName("UTF8String"));
      if (ks && !strcmp(ks, "outputImage")) {
         Class cimg = objc_getClass("CIImage");
         char tag[128];
         snprintf(tag, sizeof(tag), "  %s.output", object_getClassName(self));
         int iskind = 0;
         for (Class c = object_getClass(res); c; c = class_getSuperclass(c))
            if (c == cimg) { iskind = 1; break; }
         if (iskind) { n++; glp_ci_sample(res, tag); }
      }
   }
   return res;
}

static id glp_ciURL(id self, SEL _cmd, id url) {
   id res = ((id(*)(id,SEL,id))g_glp_ciURL)(self, _cmd, url);
   id us = url ? ((id(*)(id,SEL))objc_msgSend)(url, sel_registerName("absoluteString")) : 0;
   const char *ustr = us ? ((const char*(*)(id,SEL))objc_msgSend)(us, sel_registerName("UTF8String")) : "(nil)";
   fprintf(stderr, "[civ] imageWithContentsOfURL:%s -> %p\n", ustr ? ustr : "?", (void*)res);
   fflush(stderr);
   if (res) glp_ci_sample(res, "  fileImage");
   return res;
}

static void glp_install_once(void) {
   static int done; if (done) return; done = 1;
   Class CI = objc_getClass("CIContext");
   if (CI) {
      glp_swz(CI, "drawImage:atPoint:fromRect:", (IMP)glp_ciDrawAt, &g_glp_ciDrawAt);
      glp_swz(CI, "drawImage:inRect:fromRect:",  (IMP)glp_ciDrawIn, &g_glp_ciDrawIn);
   }
   Class CIC = objc_getClass("CIColor"), CIV = objc_getClass("CIVector"),
         CII = objc_getClass("CIImage");
   if (CIC) {
      glp_swz(object_getClass((id)CIC), "colorWithRed:green:blue:alpha:",
              (IMP)glp_ciColor4, &g_glp_ciColor4);
      glp_swz(object_getClass((id)CIC), "colorWithRed:green:blue:",
              (IMP)glp_ciColor3, &g_glp_ciColor3);
   }
   if (CIV) {
      glp_swz(object_getClass((id)CIV), "vectorWithX:Y:", (IMP)glp_ciVec2, &g_glp_ciVec2);
      glp_swz(object_getClass((id)CIV), "vectorWithX:Y:Z:W:", (IMP)glp_ciVec4, &g_glp_ciVec4);
   }
   if (CII)
      glp_swz(object_getClass((id)CII), "imageWithContentsOfURL:", (IMP)glp_ciURL, &g_glp_ciURL);
   Class CIF = objc_getClass("CIFilter");
   if (CIF) {
      glp_swz(CIF, "valueForKey:", (IMP)glp_fltVFK, &g_glp_fltVFK);
      glp_swz(CIF, "setValue:forKey:", (IMP)glp_fltSVFK, &g_glp_fltSVFK);
   }
   Class C = objc_getClass("NSOpenGLContext");
   if (!C) { fprintf(stderr, "[glp] NSOpenGLContext class not found\n"); return; }
   glp_swz(C, "flushBuffer",         (IMP)glp_flushBuffer,  &g_glp_flushBuffer);
   glp_swz(C, "setView:",            (IMP)glp_setView,      &g_glp_setView);
   glp_swz(C, "makeCurrentContext",  (IMP)glp_makeCurrent,  &g_glp_makeCurrent);
   glp_swz(C, "clearDrawable",       (IMP)glp_clearDrawable,&g_glp_clearDrawable);
   glp_swz(C, "initWithFormat:shareContext:", (IMP)glp_ctxInit, &g_glp_ctxInit);
   glp_swz(objc_getClass("NSOpenGLPixelFormat"), "initWithAttributes:",
           (IMP)glp_pfInit, &g_glp_pfInit);
   glp_swz(objc_getClass("NSOpenGLView"), "openGLContext",
           (IMP)glp_openGLContext, &g_glp_openGLContext);
   glp_swz(objc_getClass("NSOpenGLView"), "pixelFormat",
           (IMP)glp_pixelFormat, &g_glp_pixelFormat);
   if (getenv("ABICONV_GL_NOLAYER")) {
      glp_swz(objc_getClass("NSOpenGLView"), "setWantsLayer:",
              (IMP)glp_setWantsLayer, &g_glp_setWantsLayer);
      if (getenv("ABICONV_GL_NOLAYER_GETTER"))
         glp_swz(objc_getClass("NSOpenGLView"), "wantsLayer",
                 (IMP)glp_wantsLayer, &g_glp_wantsLayer);
      fprintf(stderr, "[glp] NSOpenGLView setWantsLayer: coerced to NO%s\n",
              getenv("ABICONV_GL_NOLAYER_GETTER") ? " (+getter)" : "");
   }
   fprintf(stderr, "[glp] NSOpenGLContext swizzles installed\n"); fflush(stderr);
}

/* -[NSView bounds] as a plain CGRect (32B on x86_64 -> struct-return ABI).
 * Casting objc_msgSend to a CGRect-returning fn ptr emits the STRET convention
 * (hidden retbuf ptr in rdi, self in rsi) while still calling plain objc_msgSend,
 * which reads rdi as self -> dispatch on garbage. Route CGRect returns through
 * objc_msgSend_stret explicitly. Generic (no app/heartbeat scaffolding). */
static CGRect abiconv_view_bounds(id v) {
   CGRect r = {{0, 0}, {0, 0}};
   if (!v) { return r; }
   ((void (*)(CGRect *, id, SEL))objc_msgSend_stret)(
      &r, v, sel_registerName("bounds"));
   return r;
}

void _86x64_reverse_prep(struct reverse_plan *plan, const uint64_t *regs,
                         uint32_t is_stret) {
   /* objc_msgSend_stret shifts the register file by one: rdi is the hidden
    * native return buffer, so self/_cmd/args slide to regs[1]/regs[2]/regs[3+].
    * The i386 stret IMP wants the SAME shape — a hidden i386 struct pointer as
    * its first cdecl word — so frame[0] becomes a low-4GB scratch buffer the
    * IMP writes through, widened back into regs[0] in reverse_ret. */
   const unsigned gp0 = is_stret ? 2u : 1u;     /* GP index of _cmd */
   id self_ = (id)regs[is_stret ? 1u : 0u];
   SEL sel  = (SEL)regs[gp0];

   /* ProKit re-swizzles the NSColor getters behind our back (see
    * appkit_color_compat_reassert) — re-take them on every reverse entry. */
   appkit_color_compat_reassert();

   plan->ret_kind = 2;          /* default void */
   plan->lowstack_base = (uint64_t)(uintptr_t)rev_stack_alloc();     /* low-4GB pool */
   plan->lowstack_top  = (plan->lowstack_base + REV_STACK_SZ) & ~(uint64_t)0xf;
   if (getenv("ABICONV_HEAP_TRACE")) {
      fprintf(stderr, "[rstk] ALLOC base=0x%llx t=%x\n",
              (unsigned long long)plan->lowstack_base,
              pthread_mach_thread_np(pthread_self()));
      fflush(stderr);
   }
   plan->stret_dst = 0;
   plan->stret_src = 0;
   plan->stret_types = NULL;
   plan->fp_out[0] = plan->fp_out[1] = 0;
   plan->wb_active = 0;     /* no inherited-ivar frame unless we reach the sync */
   plan->has_native_super = 0;   /* set only for a value-returning native super */
   plan->n_out_obj = 0;          /* reverse out-object (out id*) copy-back list */
   plan->draw_clip_ctx = 0;      /* no legacy draw-clip gstate to restore */

   /* A pending super-dispatch hint for exactly this (self,sel) overrides the
    * derived-class lookup so [super sel] runs the SUPER's legacy method, not
    * the receiver's override again (consume-once). */
   /* A pending super-dispatch hint for this (self,sel) routes the lookup to the
    * SUPER's legacy method instead of the receiver's derived override (prevents
    * +initialize-style recursion). Validate it against the authoritative map;
    * fall back to the derived class if it doesn't name a legacy method. */
   Class lookup = object_getClass(self_);

   /* Env-gated reverse-DISPATCH trace: ABICONV_REV_DISPATCH_TRACE=<substr>
    * logs every native->legacy method dispatch whose selector contains
    * <substr> ("=" alone or "*" logs all). Diagnostic for display-cycle
    * questions of the form "does AppKit ever send drawRect:/drawWithFrame:
    * to the app's legacy views?" (Quinn black-board). Read once. */
   {
      static const char *dt_filter; static int dt_init;
      if (!dt_init) { dt_filter = getenv("ABICONV_REV_DISPATCH_TRACE"); dt_init = 1; }
      if (dt_filter) {
         const char *sn = sel_getName(sel);
         if (dt_filter[0] == '\0' || dt_filter[0] == '*' || strstr(sn, dt_filter)) {
            fprintf(stderr, "[rdis] %s[%s]\n", class_getName(lookup), sn);
            fflush(stderr);
         }
      }
   }

   /* GL-drawable probe: install NSOpenGLContext swizzles once, lazily (AppKit
    * fully up here). See glp_install_once above. */
   if (getenv("ABICONV_GL_PROBE")) glp_install_once();

   struct rmeth_ent *m = NULL;
   Class hint = reverse_take_super(self_, sel);
   if (hint) {
      /* [super sel]: resolve sel from the HINT (super) class UPWARD its own
       * superclass chain, and NEVER from the receiver's class. The receiver's
       * class is the SUBCLASS that just executed [super sel]; re-dispatching sel
       * there re-runs that same override -> infinite recursion. This bites when
       * the super class does not define its OWN legacy override of sel but
       * INHERITS it (from a legacy ancestor, or a native class): the old code
       * looked sel up only ON the hint class, missed, then fell through to
       * rmeth_lookup(receiver_class,sel) which re-found the subclass override
       * (iPhoto: MWAlbumDetailView:CAVariableGridView:CAGridView:NSView, where
       * only MWAlbumDetailView defines a legacy awakeFromNib -> [super
       * awakeFromNib] recursed 1000+ deep -> reverse rsp-stash overflow). Walking
       * hint's chain finds the owning legacy ancestor if any (a hit at c==hint
       * keeps the s10 +[...initialize] case working); if NONE, sel is native on
       * the super side and legacy_imp stays 0 (reverse_ret returns nil) — the
       * correct no-op for [super sel] into a native ancestor (e.g. -[NSView
       * awakeFromNib]). Universal: any legacy [super sel] whose super inherits
       * sel rather than defining its own legacy override. */
      for (Class c = hint; c; c = class_getSuperclass(c)) {
         m = rmeth_lookup(c, sel);
         if (m) { lookup = c; break; }
      }
      /* [super sel] into a NATIVE super that RETURNS A VALUE: invoke the true
       * native super IMP and return ITS value. 48211bb returned nil here — right
       * for a void no-op super (-[NSView awakeFromNib], which cured the recursion)
       * but WRONG for a value getter: iPhoto -[AlbumView numberOfRows] calls
       * [super numberOfRows] for NSOutlineView's real displayed row count, and
       * nil made it undercount -> NSOutlineView expandItem: _locationOfRow:
       * index>numRows assertion (the invisible-sidebar wall). Find the native
       * IMP by walking hint upward, SKIPPING our reverse trampolines
       * (method_is_legacy). Scope: 0-extra-arg, rax-returnable (non-void)
       * getters — the common [super getter] shape. Arg-carrying or fp/struct-
       * returning supers are left to the nil path (do not guess-marshal). The
       * value is already native; the forward bridge that issued the [super sel]
       * narrows/wraps it back to i386. Universal. */
      if (!m) {
         Method nm = NULL;
         for (Class c = hint; c; c = class_getSuperclass(c)) {
            Method mm = class_getInstanceMethod(c, sel);
            if (!mm) { break; }
            if (!method_is_legacy(mm)) { nm = mm; break; }
         }
         if (nm && method_getNumberOfArguments(nm) == 2) {
            char rt[8] = {0};
            method_getReturnType(nm, rt, sizeof rt);
            if (rt[0] && strchr("cCsSiIlLqQB@#:*^", rt[0])) {
               IMP nimp = method_getImplementation(nm);
               plan->native_super_ret =
                  ((uint64_t (*)(id, SEL))nimp)(self_, sel);
               plan->has_native_super = 1;
            }
         }
      }
      if (getenv("OBJC_SUPER_TRACE"))
         fprintf(stderr, "[prep] SUPER self=%p sel=%s hint=%s -> %s\n",
                 (void*)self_, sel_getName(sel), class_getName(hint),
                 m ? class_getName(lookup)
                   : (plan->has_native_super ? "(native super value)"
                                             : "(native super -> nil)"));
   } else {
      m = rmeth_lookup(lookup, sel);
      /* Inherited reverse method: the receiver's class never registered this sel
       * — a reverse-registered legacy SUPERCLASS did, and the modern runtime
       * reached its _86x64_reverse_imp by ordinary inheritance (e.g. native
       * -[NSDocument initWithType:error:] sending init to an ArchiveDocument
       * whose init lives on a legacy ancestor). Walk the modern superclass chain
       * for the owning entry; the shadow below still uses the receiver's own
       * class, whose fragile-ABI layout embeds the ancestor's ivars first. */
      if (!m) {
         for (Class c = class_getSuperclass(lookup); c; c = class_getSuperclass(c)) {
            m = rmeth_lookup(c, sel);
            if (m) break;
         }
      }
   }
   /* An entry whose imp is not a plausible code address (a junk legacy method
    * list entry, or a bad runtime class_addMethod) must not be jumped to — fall
    * back to the no-legacy-method path (returns nil) rather than crash. */
   if (m && m->imp < 0x1000) { m = NULL; }
   if (!m) {
      /* No legacy method. For a [super sel] into a NATIVE ancestor (hint set,
       * the walk above found no legacy owner) this is EXPECTED — return nil
       * quietly; the native super method (e.g. -[NSView awakeFromNib]) is a
       * no-op for our purposes. Otherwise it is a registration gap, or native
       * code invoking the shared tramp address NOT as an objc IMP (direct
       * IMP-cache call with nil receiver, block-invoke confusion, ...) —
       * regs[0]/[1] are then not self/sel at all; warn. legacy_imp=0 makes the
       * asm SKIP the i386 call and return 0 via reverse_ret (which also frees
       * the lowstack); jmp'ing to 0 here used to kill the process. regs[7] = the
       * native caller's return address, for identifying the caller. */
      if (!hint) {
         fprintf(stderr, "objc_shim: reverse_prep: no legacy method for %s[%s] "
                 "(caller ra=%p self=%p sel=%p)\n",
                 class_getName(lookup),
                 (sel && mem_readable((uintptr_t)sel, 1)) ? sel_getName(sel)
                                                          : "(unreadable)",
                 (void *)(uintptr_t)regs[7], (void *)self_, (void *)sel);
         fflush(stderr);
      }
      plan->legacy_imp = 0;
      return;
   }
   plan->legacy_imp = m->imp;

   /* self32: a class message wraps the modern Class as a proxy handle (so the
    * IMP's `[self ...]` resolves back through resolve_self -> unwrap -> Class ->
    * reverse bridge); an instance message uses the i386 shadow (so the IMP's
    * ivar accesses at hardcoded i386 offsets land in the shadow buffer). */
   uint32_t self32;
   if (object_isClass(self_)) {
      self32 = x64_objc_wrap((uint64_t)(uintptr_t)self_);
   } else {
      self32 = get_or_create_shadow(self_, lookup);
      /* Refresh the inherited native-superclass ivars into the shadow so the
       * IMP's direct reads of protected superclass ivars (NSControl._cell etc.)
       * see live native values, not the zeroed shadow; pushes a dirty-tracking
       * frame flushed back in reverse_ret. Walk the receiver's OWN class chain
       * (object_getClass(self_)), independent of any super-dispatch hint. */
      sync_inherited_ivars(plan, self32, self_, object_getClass(self_));
   }

   /* Return-kind decision. An i386 hidden struct pointer is prepended for
    * BOTH a native stret entry (>16B native) and an i386-stret-only method
    * (i386 size > 8 but native <= 16: the native caller called the plain IMP
    * expecting a register return, while the i386 IMP wants a hidden pointer
    * and pops it with `ret $4`). The i386 scratch buffer lives at the BASE of
    * the lowstack (the asm builds the cdecl frame down from the TOP). */
   unsigned head = 0;
   {
      char rb = encoding_base_type(m->types);
      uint32_t i386buf = (uint32_t)((plan->lowstack_base + 15) & ~(uint64_t)0xf);
      if (is_stret) {
         head = 1;
         plan->frame[0]  = i386buf;
         plan->stret_dst = regs[0];              /* native return buffer */
         plan->stret_src = i386buf;
         plan->stret_types = m->types;
         plan->ret_kind  = 3;                    /* widen in reverse_ret */
      } else if (rb == 'v') {
         plan->ret_kind = 2;
      } else if (rb == '@' || rb == '#' || enc_is_objptr_struct(m->types)
                 || enc_is_cfptr(m->types)) {
         plan->ret_kind = 1;
      } else if (rb == 'f' || rb == 'd') {
         plan->ret_kind = 4;                     /* st0 -> fp_out -> xmm0 */
      } else if (rb == '{' || rb == '(' || rb == '[') {
         /* register-class native return (<=16B, else we'd be in is_stret) */
         size_t isz = 0, nsz = 0; uint8_t sse[8];
         enc_classify(enc_skip_quals(m->types), CONV_I386, &isz, &nsz, sse);
         plan->ret_kind = 6;
         plan->stret_types = m->types;
         if (isz > 8) {                          /* i386 side IS stret */
            head = 1;
            plan->frame[0]  = i386buf;
            plan->stret_src = i386buf;
         }                                       /* else: source = eax:edx */
      } else {
         plan->ret_kind = 0;
      }
   }
   plan->frame[head + 0] = self32;
   plan->frame[head + 1] = (uint32_t)(uintptr_t)sel;

   /* Walk arg types: skip ret+offset, self(@)+offset, cmd(:)+offset, then
    * map each explicit arg from its SysV position (GP reg / XMM reg / native
    * stack) down to i386 cdecl frame words, narrowing doubles -> CGFloat
    * floats and native struct layouts -> i386 layouts per the legacy
    * encoding. regs[8+k] = xmm_k; regs[6] = native stack-arg base, consumed
    * left-to-right by every overflow/MEMORY arg. */
   const char *t = m->types;
   t = enc_skip_digits(enc_skip_type(t));    /* return type */
   t = enc_skip_digits(enc_skip_type(t));    /* self @ */
   t = enc_skip_digits(enc_skip_type(t));    /* _cmd : */
   unsigned w = head + 2;     /* next frame word (after [hidden] self cmd) */
   unsigned gp = gp0 + 1;     /* next GP reg index (first explicit arg) */
   unsigned xmm = 0;          /* next XMM reg index */
   size_t stk = 0;            /* native stack-args byte cursor */
   unsigned rect_w = 0;       /* frame index of the FIRST NSRect arg (draw-clip) */
#define REV_GP(vout) do { \
      if (gp < 6) { (vout) = regs[gp++]; } \
      else { (vout) = *(const uint64_t *)((uintptr_t)regs[6] + stk); stk += 8; } \
   } while (0)
#define REV_XMM(vout) do { \
      if (xmm < 8) { (vout) = regs[8 + xmm++]; } \
      else { (vout) = *(const uint64_t *)((uintptr_t)regs[6] + stk); stk += 8; } \
   } while (0)
   while (*t && w < 60) {
      const char *tb = enc_skip_quals(t);
      char b = *tb;
      if (b == '@' || b == '#' || enc_is_objptr_struct(t) || enc_is_cfptr(t)) {
         uint64_t v; REV_GP(v);
         /* A value already representable in 32 bits is an i386-side object the
          * native caller is handing back (a proxy handle, a legacy-instance
          * shadow, or a raw low-4GB legacy ptr) — pass it through verbatim.
          * Re-wrapping it would mint a FRESH proxy handle (an 8-byte arena
          * slot) where the i386 IMP expects its OWN object: a `[arg ...]`
          * send still resolves, but any ivar store through the arg writes
          * into the arena slot and clobbers a sibling handle's high half,
          * producing a fused 0x<handle><lowptr> pointer that later faults in
          * objc_retain inside native setObject:forKey: (s13). Only a true
          * >4GB native object needs a handle; wrap_ret (not wrap) returns the
          * existing shadow if that object is one of OUR legacy instances,
          * identity-preserving exactly like the `self=[super init]` path. */
         plan->frame[w++] = (v >= 0x100000000ULL)
                               ? x64_objc_wrap_ret(v) : (uint32_t)v;
      } else if (b == ':') {
         uint64_t v; REV_GP(v);
         plan->frame[w++] = x64_objc_sel_wrap(v);
      } else if (b == 'q' || b == 'Q') {
         uint64_t v; REV_GP(v);
         plan->frame[w++] = (uint32_t)v;
         plan->frame[w++] = (uint32_t)(v >> 32);
      } else if (b == 'f') {
         /* legacy CGFloat: native caller passed a double in the next xmm */
         uint64_t bits; REV_XMM(bits);
         double dv; memcpy(&dv, &bits, 8);
         float f = (float)dv;
         memcpy(&plan->frame[w++], &f, 4);
      } else if (b == 'd') {
         uint64_t bits; REV_XMM(bits);
         memcpy(&plan->frame[w], &bits, 8);
         w += 2;
      } else if (b == '{' || b == '(' || b == '[') {
         size_t isz = 0, nsz = 0; uint8_t sse[8] = {0};
         unsigned nebs = enc_classify(tb, CONV_I386, &isz, &nsz, sse);
         unsigned iwords = (unsigned)((isz + 3) / 4);
         if (!rect_w && (!strncmp(tb, "{_NSRect=", 9) ||
                         !strncmp(tb, "{CGRect=", 8))) {
            rect_w = w;               /* remember for the draw-clip contract */
         }
         if (nsz > 0 && nsz <= 16) {
            uint8_t nat[16] = {0};
            for (unsigned k = 0; k < nebs && k < 2; ++k) {
               uint64_t eb;
               if (sse[k]) { REV_XMM(eb); } else { REV_GP(eb); }
               memcpy(nat + 8*k, &eb, 8);
            }
            if (w + iwords <= 60) {
               enc_narrow(tb, CONV_I386, nat, (uint8_t *)&plan->frame[w]);
               w += iwords;
            }
         } else if (nsz > 16) {                  /* MEMORY: on the native stack */
            stk = align_up_sz(stk, 8);
            if (w + iwords <= 60) {
               enc_narrow(tb, CONV_I386,
                          (const uint8_t *)((uintptr_t)regs[6] + stk),
                          (uint8_t *)&plan->frame[w]);
               w += iwords;
            }
            stk += align_up_sz(nsz, 8);
         }
      } else if (b == '^' && (tb[1] == '@' || tb[1] == '#')) {
         /* native `out id*` / `Class*` out-param (getObjectValue:'s obj,
          * NSError**, ...). The reverse bridge is entered ONLY from a NATIVE
          * caller (native->legacy by definition), so the out-slot's consumer is
          * ALWAYS native and expects a 64-bit id there -- REGARDLESS of the
          * out-ptr's address. The i386 IMP can only write a 4-byte ARENA HANDLE,
          * which native then uses as an object -> objc_opt_isKindOfClass on a raw
          * handle -> fault (iPhoto: AlbumViewCellFormatter getObjectValue: -> the
          * cell's native _contents; the real out-ptr is a LOW-4GB native stack
          * local -- exactly why an address-range gate is the WRONG discriminator,
          * the bug bd92b4a's >=4GB gate missed). So register a copy-back for
          * EVERY out-id*: reverse_ret unwraps the handle the IMP wrote -> native
          * object -> writes the full 64-bit ptr back into the caller's *id. Two
          * cases for WHERE the i386 IMP writes: a <4GB out-ptr is i386-addressable
          * so the IMP writes it in place (scratch = the out-ptr itself); a >=4GB
          * out-ptr needs a low-4GB shim-malloc scratch. Both wrap the current
          * native *id in (in/out) and unwrap out on copy-back. Universal: any
          * legacy method with an out object-ptr (getObjectValue:, NSError**, ...). */
         uint64_t v; REV_GP(v);
         uint32_t *scr = NULL;
         if (plan->n_out_obj < 4) {
            if (v < 0x100000000ULL) {
               scr = (uint32_t *)(uintptr_t)v;  /* i386 writes the low-4GB out-ptr in place */
            } else {
               scr = (uint32_t *)malloc(4);      /* >=4GB: low-4GB shim-malloc scratch */
               if (scr && (uintptr_t)scr >= 0x100000000ULL) { free(scr); scr = NULL; }
            }
         }
         if (scr) {
            uint64_t cur = mem_readable(v, 8)
                              ? *(const uint64_t *)(uintptr_t)v : 0;
            *scr = cur ? x64_objc_wrap(cur) : 0;
            plan->out_obj[plan->n_out_obj].scratch = (uint32_t)(uintptr_t)scr;
            plan->out_obj[plan->n_out_obj].native_out = v;
            plan->n_out_obj++;
            plan->frame[w++] = (uint32_t)(uintptr_t)scr;
         } else {
            plan->frame[w++] = (uint32_t)v;   /* table full / OOM: best-effort raw */
         }
      } else {
         uint64_t v; REV_GP(v);
         plan->frame[w++] = (uint32_t)v;
      }
      t = enc_skip_digits(enc_skip_type(t));
   }
   /* Variadic legacy method (e.g. -[ATAnimationGroup addAnimations:a,b,nil]):
    * the encoding models only the fixed args, so the loop above stopped at the
    * last declared one. The forward bridge spilled the rest of the nil-
    * terminated id list into the GP/stack arg sequence — copy them down into the
    * i386 cdecl frame (each object wrapped back to its shadow/handle) so the
    * IMP's va_arg walk reads real receivers, not leftover lowstack words. */
   if (legacy_method_is_variadic(m)) {
      while (w < 62) {
         uint64_t v; REV_GP(v);
         uint32_t fw = (v >= 0x100000000ULL) ? x64_objc_wrap_ret(v) : (uint32_t)v;
         plan->frame[w++] = fw;
         if (fw == 0) { break; }          /* placed the nil terminator */
      }
      if (plan->frame[w - 1] != 0 && w < 63) { plan->frame[w++] = 0; }
   }
#undef REV_GP
#undef REV_XMM
   plan->frame_words = w;

   /* Env-gated reverse-ARG value trace: ABICONV_REV_ARG_TRACE=<substr>[,<substr>...]
    * logs the marshalled i386 frame VALUES of every native->legacy dispatch
    * whose selector contains one of the substrings ("*" = all). Complements
    * ABICONV_REV_DISPATCH_TRACE (names only): prints each explicit arg word as
    * hex + as a {short,short} pair so 16-bit cell geometry (Quinn IGRect /
    * IGPoint) reads off directly. Diagnostic only; zero cost when unset. */
   {
      static const char *at_filter; static int at_init;
      if (!at_init) { at_filter = getenv("ABICONV_REV_ARG_TRACE"); at_init = 1; }
      if (at_filter && sel) {
         const char *sn = sel_getName(sel);
         int hit = (at_filter[0] == '*');
         if (!hit) {
            const char *p = at_filter;
            while (*p && !hit) {
               const char *e = strchr(p, ',');
               size_t n = e ? (size_t)(e - p) : strlen(p);
               char tok[64];
               if (n > 0 && n < sizeof tok) {
                  memcpy(tok, p, n); tok[n] = '\0';
                  if (strstr(sn, tok)) { hit = 1; }
               }
               p = e ? e + 1 : p + strlen(p);
            }
         }
         if (hit) {
            char buf[512]; int off = 0;
            off += snprintf(buf + off, sizeof buf - off, "[rarg] %s[%s]",
                            class_getName(lookup), sn);
            unsigned a0 = head + 2;              /* first explicit arg word */
            for (unsigned k = a0; k < w && k < a0 + 8 &&
                                  off < (int)sizeof buf - 40; ++k) {
               uint32_t fv = plan->frame[k];
               off += snprintf(buf + off, sizeof buf - off, " %08x(%d,%d)",
                               fv, (short)(fv & 0xffff), (short)(fv >> 16));
            }
            fprintf(stderr, "%s\n", buf);
            fflush(stderr);
         }
      }
   }

   /* ---- LEGACY DRAW-CLIP CONTRACT ------------------------------------------
    * Legacy apps were written against the pre-10.14 AppKit drawing contract:
    * the dirty rect handed to -drawRect: was INTERSECTED with the view's
    * bounds, and the context was CLIPPED to the view before the method ran, so
    * "fill the rect I was handed" could never paint outside the view. Modern
    * AppKit's recursive re-render (cacheDisplayInRect:, non-layer window draws)
    * hands each subview the FULL target rect in local coords with NO per-view
    * clip (verified natively: a windowless parent/child cacheDisplay gives the
    * child rect=[-150 -100 400 300] and a full-area clip). A legacy view whose
    * -drawRect: fills its whole background (via NSRectFill / CGContextFillRect)
    * then stomps every sibling that was already painted (observed: a board view
    * white-filling rect [-198 -264 785 702] over the already-drawn surround +
    * sidebar -> the whole play area whited out).
    *
    * Restore the old contract on every reverse dispatch into a legacy DRAW
    * method that carries an NSRect arg + a live CGContext:
    *   -drawRect: (NSView subclass, honoring wantsDefaultClipping): intersect
    *     the NSRect arg with [self bounds] in the already-narrowed i386 frame,
    *     then save-gstate + clip the current context to that rect;
    *   NSCell draw family (drawWithFrame:inView: / drawInteriorWithFrame:inView:
    *     / highlight:withFrame:inView:): save-gstate + clip to the cellFrame arg
    *     (legacy AppKit confined cell painting to the control's clip; cells draw
    *     in the controlView coord space, so the rect clips directly).
    * reverse_ret restores the gstate (plan->draw_clip_ctx).
    *
    * Triggers on STRUCTURE (a reverse-dispatched legacy draw method with an
    * NSRect arg + a current bitmap/window context), never on an app/class name,
    * so any translated legacy AppKit target benefits. ABICONV_DRAWCLIP overrides
    * the mode for A/B bisection: 1=full (default), 0=off, 2=intersect-arg only
    * (no clip), 3=clip only (don't rewrite the arg). ABICONV_DRAWCLIP_TRACE logs
    * each decision. */
   {
      static int dcl_mode = -1;
      if (dcl_mode < 0) {
         const char *dm = getenv("ABICONV_DRAWCLIP");
         dcl_mode = dm ? atoi(dm) : 1;
      }
      if (dcl_mode && rect_w && !object_isClass(self_) &&
          sel && mem_readable((uintptr_t)sel, 1)) {
         const char *dsn = sel_getName(sel);
         int is_drawrect = dsn && !strcmp(dsn, "drawRect:");
         int is_celldraw = dsn && (!strcmp(dsn, "drawWithFrame:inView:") ||
                                   !strcmp(dsn, "drawInteriorWithFrame:inView:") ||
                                   !strcmp(dsn, "highlight:withFrame:inView:"));
         if (is_drawrect || is_celldraw) {
            id dgc = ((id (*)(Class, SEL))objc_msgSend)(
               objc_getClass("NSGraphicsContext"),
               sel_registerName("currentContext"));
            CGContextRef dcc = NULL;
            if (dgc)
               dcc = ((CGContextRef (*)(id, SEL))objc_msgSend)(
                  dgc, sel_registerName("CGContext"));
            if (dcc) {
               float rx, ry, rw, rh;
               memcpy(&rx, &plan->frame[rect_w + 0], 4);
               memcpy(&ry, &plan->frame[rect_w + 1], 4);
               memcpy(&rw, &plan->frame[rect_w + 2], 4);
               memcpy(&rh, &plan->frame[rect_w + 3], 4);
               CGRect clipr = CGRectMake(rx, ry, rw, rh);
               int do_clip = 1;
               if (is_drawrect) {
                  /* NSView-kind gate: drawRect: on a non-view -> leave alone. */
                  int is_view = 0;
                  Class nsview = objc_getClass("NSView");
                  for (Class c = object_getClass(self_); c;
                       c = class_getSuperclass(c)) {
                     if (c == nsview) { is_view = 1; break; }
                  }
                  if (is_view) {
                     CGRect b = abiconv_view_bounds(self_);
                     CGRect inter = CGRectIntersection(clipr, b);
                     if (CGRectIsNull(inter)) { inter = CGRectZero; }
                     clipr = inter;
                     if (dcl_mode != 3) {
                        float ox = (float)inter.origin.x, oy = (float)inter.origin.y;
                        float ow = (float)inter.size.width, oh = (float)inter.size.height;
                        memcpy(&plan->frame[rect_w + 0], &ox, 4);
                        memcpy(&plan->frame[rect_w + 1], &oy, 4);
                        memcpy(&plan->frame[rect_w + 2], &ow, 4);
                        memcpy(&plan->frame[rect_w + 3], &oh, 4);
                     }
                     do_clip = ((signed char (*)(id, SEL))objc_msgSend)(
                        self_, sel_registerName("wantsDefaultClipping")) != 0;
                  } else {
                     do_clip = 0;
                  }
               }
               if (dcl_mode == 2) { do_clip = 0; }
               if (do_clip) {
                  CGContextSaveGState(dcc);
                  CGContextClipToRect(dcc, clipr);
                  plan->draw_clip_ctx = (uint64_t)(uintptr_t)dcc;
               }
               if (getenv("ABICONV_DRAWCLIP_TRACE")) {
                  fprintf(stderr, "[dcl] %s[%s] clip=[%.1f %.1f %.1f %.1f] "
                          "applied=%d ctx=%p mode=%d\n",
                          class_getName(lookup), dsn,
                          clipr.origin.x, clipr.origin.y,
                          clipr.size.width, clipr.size.height,
                          do_clip, (void *)dcc, dcl_mode);
                  fflush(stderr);
               }
            }
         }
      }
   }

   /* breadcrumb (zero-I/O): a valid legacy IMP is about to run on lowstack */
   {
      uint32_t depth = (uint32_t)__atomic_add_fetch(&g_rev_depth, 1, __ATOMIC_SEQ_CST);
      uint32_t ri = __atomic_fetch_add(&g_rev_pos, 1, __ATOMIC_SEQ_CST) & (REV_RING - 1);
      struct rev_crumb *c = &g_rev_ring[ri];
      c->self_ = (uint64_t)(uintptr_t)self_; c->sel = (uint64_t)(uintptr_t)sel;
      c->imp = m->imp; c->lowstack_base = plan->lowstack_base;
      c->lowstack_top = plan->lowstack_top; c->tid = pthread_mach_thread_np(pthread_self());
      c->depth = depth; c->frame_words = w; c->valid = 1;
   }
   x64_cb_enter();   /* a legacy IMP is about to run (balanced in reverse_ret) */

   /* Round-2 Quinn black-board gate trace. The capture proved FastDrawCells
    * (-> CGContextSetAlpha) is NEVER reached: drawPieceInRect AND drawBoardInRect
    * both open with `if (self->myPlayer == nil) return;` (myPlayer = QuinnBoardView
    * ivar @ i386 offset 0x54). self32 is the i386 SHADOW the legacy IMP reads its
    * ivars from (= get_or_create_shadow(self_,lookup)). For every reverse call into
    * a *BoardView, log the shadow + myPlayer@0x54 (and, for setPlayer:, the player
    * being written). This shows whether the shadow is STABLE across the view's
    * calls and whether the player write ever lands on the shadow the draw guard
    * reads — i.e. core (translated ivar) vs runtime (legacy-shadow aliasing). */
   if (getenv("QUINN_CELL_TRACE") && !object_isClass(self_)) {
      const char *cn = class_getName(lookup);
      const char *sn = (sel && mem_readable((uintptr_t)sel, 1)) ? sel_getName(sel)
                                                                 : "(unreadable)";
      /* WINDOW-IDENTITY line for every legacy-view drawRect: — discriminates
       * "draws into the visible game window" from "draws into a detached /
       * offscreen / hidden host" (the steady-state blank-sidebar question:
       * all draw calls fire with sane args, so if the content never shows,
       * either the view isn't parented where we think or something composites
       * over it). window/superview/isVisible are plain native NSView/NSWindow
       * getters on the real instance — safe before the legacy IMP runs. */
      if (sn && !strcmp(sn, "drawRect:") && cn &&
          (strstr(cn, "Quinn") || strstr(cn, "LCD"))) {
         id win = ((id(*)(id, SEL))objc_msgSend)(self_, sel_registerName("window"));
         id sv  = ((id(*)(id, SEL))objc_msgSend)(self_, sel_registerName("superview"));
         signed char vis = win ? ((signed char(*)(id, SEL))objc_msgSend)(
                                    win, sel_registerName("isVisible")) : 0;
         long wnum = win ? ((long(*)(id, SEL))objc_msgSend)(
                              win, sel_registerName("windowNumber")) : -1;
         signed char hid = ((signed char(*)(id, SEL))objc_msgSend)(
            self_, sel_registerName("isHiddenOrHasHiddenAncestor"));
         fprintf(stderr, "[win] %s win=%p num=%ld vis=%d hiddenAnc=%d sv=%s\n",
                 cn, (void *)win, wnum, (int)vis, (int)hid,
                 sv ? object_getClassName(sv) : "(nil)");
      }
      int board_cls = cn && strstr(cn, "BoardView");
      /* Round-3: the board's draw methods are registered but AppKit never calls
       * drawRect:/drawBoardInRect:/drawPieceInRect: -> the board is never
       * invalidated/redrawn. The redraw chain is:
       *   <game tick> -> *[boardDidChange:matrices:] -> [view setNeedsDisplayInCell*]
       *               -> AppKit -> [view drawRect:] -> FastDrawCells.
       * Trace the chain regardless of class so we find WHERE it stops: the
       * model->view notification (boardDidChange) and the view invalidation
       * (any *NeedsDisplay*), plus the per-move hook. board_cls calls keep the
       * shadow+myPlayer detail. */
      int chain_sel = sn && (strstr(sn, "boardDidChange") || strstr(sn, "NeedsDisplay")
                             || strstr(sn, "pieceDidMove") || strstr(sn, "boardChanged")
                             || !strcmp(sn, "boardDidChange:matrices:"));
      if (board_cls || chain_sel) {
         uint32_t shadow = self32;
         /* The REAL x86_64 object is the ground-truth instance identity: the
          * shadow is keyed on it (objc_getAssociatedObject(real,g_ctrl)), so
          * two calls with the SAME real but DIFFERENT shadow == a shadow-aliasing
          * bug. myPlayer@0x54 lives on QuinnBoardView (the super) and is written
          * by -[QuinnBoardView setPlayer:] via the subclass's [super setPlayer:];
          * drawRect:/drawBoardInRect: read it. Logging real+shadow+myPlayer for
          * setPlayer: (write) AND drawRect:/drawBoardInRect: (read) shows whether
          * the write and the read land on the same shadow of the same instance. */
         uint64_t real64 = (uint64_t)(uintptr_t)shadow_real(shadow);
         if (board_cls) {
            uint32_t myplayer = (shadow && mem_readable((uintptr_t)shadow + 0x54, 4))
                                ? *(const uint32_t *)(uintptr_t)(shadow + 0x54) : 0xBADBAD;
            fprintf(stderr, "[guard] %s[%s] real=0x%llx shadow=0x%x myPlayer@0x54=0x%x%s\n",
                    cn ? cn : "?", sn, (unsigned long long)real64, shadow, myplayer,
                    myplayer == 0 ? "  <-- NIL: board draw will bail" : "");
            if (sn && (!strcmp(sn, "setPlayer:")))
               fprintf(stderr, "[guard]   -> setPlayer: writes newPlayer=0x%x into"
                       " real=0x%llx shadow=0x%x+0x54\n", plan->frame[head + 2],
                       (unsigned long long)real64, shadow);
         } else {
            fprintf(stderr, "[guard] REDRAW-CHAIN: %s[%s] real=0x%llx self32=0x%x\n",
                    cn ? cn : "?", sn, (unsigned long long)real64, shadow);
         }
         fflush(stderr);
      }
   }

   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[rev] t=%x %s[%s] imp=0x%llx self32=0x%x words=%u kind=%d "
              "%splan=%p lowstack=0x%llx tramp=%p\n",
              pthread_mach_thread_np(pthread_self()),
              class_getName(lookup), sel_getName(sel),
              (unsigned long long)m->imp, plan->frame[head], plan->frame_words,
              plan->ret_kind, is_stret ? "STRET " : "", (void *)plan,
              (unsigned long long)plan->lowstack_top,
              (void *)_86x64_reverse_imp);
      fflush(stderr);
   }
   quinn_play_trace("rev", class_getName(lookup), sel);
}

unsigned __int128 _86x64_reverse_ret(struct reverse_plan *plan,
                                     uint32_t eax, uint32_t edx) {
   /* Legacy draw-clip contract: pop the gstate reverse_prep pushed around the
    * legacy drawRect:/cell-draw IMP (see the contract block in reverse_prep). */
   if (plan->draw_clip_ctx) {
      CGContextRestoreGState((CGContextRef)(uintptr_t)plan->draw_clip_ctx);
      plan->draw_clip_ctx = 0;
   }
   /* balance the breadcrumb depth (only the valid-IMP path incremented it) */
   if (plan->legacy_imp != 0) {
      __atomic_sub_fetch(&g_rev_depth, 1, __ATOMIC_SEQ_CST);
      x64_cb_leave();
   }
   /* Flush inherited native-superclass ivars the IMP changed (shadow -> real),
    * then pop this call's snapshot frame (LIFO with reverse_prep). */
   if (plan->wb_active) {
      ivar_wb_flush(plan->wb_mark);
      g_snap_n = plan->wb_mark;
   }
   /* Reverse out-object copy-back: the i386 method wrote an arena handle into
    * each `out id*` scratch slot; unwrap it to the native object and store the
    * FULL 64-bit ptr into the caller's native *id (symmetric to the object-return
    * unwrap below; overwrites the handle + any stale upper bytes for a <4GB
    * in-place slot). Runs only for a real legacy-IMP call (n_out_obj is 0 on
    * every other path). Free ONLY a >=4GB scratch (that one was shim-malloc'd; a
    * <4GB scratch IS the caller's out-ptr and must not be freed). */
   for (uint32_t oi = 0; oi < plan->n_out_obj; ++oi) {
      uint32_t h = *(const uint32_t *)(uintptr_t)plan->out_obj[oi].scratch;
      uint64_t obj = h ? unwrap_obj_arg(h) : 0;
      uint64_t *nout = (uint64_t *)(uintptr_t)plan->out_obj[oi].native_out;
      if (mem_readable((uintptr_t)nout, 8)) { *nout = obj; }
      if (plan->out_obj[oi].native_out >= 0x100000000ULL) {
         free((void *)(uintptr_t)plan->out_obj[oi].scratch);
      }
   }
   /* Native-super value-dispatch: reverse_prep already invoked the native super
    * IMP; return its native value directly (no i386-return widening — the value
    * is already native, and the forward bridge that made the [super sel] call
    * narrows/wraps it for the i386 caller). Free the lowstack like every path. */
   if (plan->has_native_super) {
      unsigned __int128 nr = (unsigned __int128)plan->native_super_ret;
      if (plan->lowstack_base) {
         rev_stack_free((void *)(uintptr_t)plan->lowstack_base);
      }
      return nr;
   }
   unsigned __int128 r;
   if (plan->ret_kind == 1) { r = unwrap_obj_arg(eax); }    /* object */
   else if (plan->ret_kind == 2) { r = 0; }                 /* void */
   else if (plan->ret_kind == 3) {                          /* stret struct */
      /* Widen the i386-layout struct the IMP wrote (at stret_src on the low
       * stack) into the native caller's buffer (stret_dst). The x86_64 stret
       * convention also returns the buffer pointer in rax. */
      if (plan->stret_dst && plan->stret_src && plan->stret_types) {
         enc_widen(enc_skip_quals(plan->stret_types), CONV_I386,
                   (const uint8_t *)(uintptr_t)plan->stret_src,
                   (uint8_t *)(uintptr_t)plan->stret_dst);
      }
      r = plan->stret_dst;
   }
   else if (plan->ret_kind == 4) { r = 0; }   /* fp: asm banked st0 in fp_out */
   else if (plan->ret_kind == 6) {            /* small struct -> native regs */
      const char *enc = plan->stret_types
         ? enc_skip_quals(plan->stret_types) : NULL;
      r = (unsigned __int128)eax;
      if (enc) {
         uint8_t i386[16] = {0};
         if (plan->stret_src) {                /* i386 IMP wrote a hidden buf */
            memcpy(i386, (const void *)(uintptr_t)plan->stret_src,
                   sizeof i386);
         } else {                              /* i386 8-byte struct: eax:edx */
            memcpy(i386, &eax, 4);
            memcpy(i386 + 4, &edx, 4);
         }
         uint8_t nat[16] = {0};
         size_t isz = 0, nsz = 0; uint8_t sse[8] = {0};
         unsigned nebs = enc_classify(enc, CONV_I386, &isz, &nsz, sse);
         enc_widen(enc, CONV_I386, i386, nat);
         /* distribute eightbytes: INTEGER -> rax,rdx; SSE -> fp_out (the asm
          * reloads xmm0/xmm1 from there) */
         uint64_t ir[2] = {0, 0}; unsigned ii = 0, fi = 0;
         for (unsigned k = 0; k < nebs && k < 2; ++k) {
            uint64_t eb; memcpy(&eb, nat + 8*k, 8);
            if (sse[k]) { if (fi < 2) { plan->fp_out[fi++] = eb; } }
            else        { if (ii < 2) { ir[ii++] = eb; } }
         }
         r = ((unsigned __int128)ir[1] << 64) | ir[0];
      }
   }
   else { r = (unsigned __int128)eax; }                     /* scalar */
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[revret] t=%x plan=%p kind=%d eax=0x%x edx=0x%x -> 0x%llx\n",
              pthread_mach_thread_np(pthread_self()),
              (void *)plan, plan->ret_kind, eax, edx,
              (unsigned long long)(uint64_t)r);
      fflush(stderr);
   }
   if (plan->lowstack_base) {
      if (getenv("ABICONV_HEAP_TRACE")) {
         fprintf(stderr, "[rstk] FREE base=0x%llx t=%x\n",
                 (unsigned long long)plan->lowstack_base,
                 pthread_mach_thread_np(pthread_self()));
         fflush(stderr);
      }
      rev_stack_free((void *)(uintptr_t)plan->lowstack_base);
   }
   return r;
}

/* ===========================================================================
 * Table-driven C-function shims for struct-by-value signatures (28th blocker).
 *
 * abigen cannot express these: CGFloat's width is decided by #if at header-
 * parse time, so one clang AST can't describe both the i386 (float) and
 * x86_64 (double) sides. Instead each function gets an i386-convention
 * encoding string here and is marshalled at runtime by the SAME classifier
 * the ObjC bridge uses, then called through _86x64_plan_call. The i386 entry
 * trampolines live in maptable_tramp.asm (GEOSHIM_I/_F/_S) — the INDEX into
 * g_geo is baked there: KEEP BOTH LISTS IN THE SAME ORDER.
 *
 * Natives are dlsym'd lazily (libabiconv links neither AppKit nor CG).
 * =========================================================================== */
extern unsigned __int128 _86x64_plan_call(struct objc_call_plan *plan, void *fn);

#define ENC_NSRECT  "{_NSRect={_NSPoint=ff}{_NSSize=ff}}"
#define ENC_NSPOINT "{_NSPoint=ff}"
#define ENC_NSSIZE  "{_NSSize=ff}"
#define ENC_NSRANGE "{_NSRange=II}"
#define ENC_CGAFF   "{CGAffineTransform=ffffff}"
#define ENC_CT3D    "{CATransform3D=ffffffffffffffff}"

struct geo_ent { const char *name; const char *sig; void *fn; };
/* sig: return encoding then arg encodings, concatenated, CONV_I386 widths.
 * KEEP IN SYNC with the GEOSHIM_* instantiations in maptable_tramp.asm. */
static struct geo_ent g_geo[] = {
   /*  0 */ { "NSUnionRect",        ENC_NSRECT ENC_NSRECT ENC_NSRECT, NULL },
   /*  1 */ { "NSIntersectionRect", ENC_NSRECT ENC_NSRECT ENC_NSRECT, NULL },
   /*  2 */ { "NSInsetRect",        ENC_NSRECT ENC_NSRECT "ff",       NULL },
   /*  3 */ { "NSOffsetRect",       ENC_NSRECT ENC_NSRECT "ff",       NULL },
   /*  4 */ { "NSIntegralRect",     ENC_NSRECT ENC_NSRECT,            NULL },
   /*  5 */ { "NSRectFromString",   ENC_NSRECT "@",                   NULL },
   /*  6 */ { "NSPointFromString",  ENC_NSPOINT "@",                  NULL },
   /*  7 */ { "NSSizeFromString",   ENC_NSSIZE "@",                   NULL },
   /*  8 */ { "NSContainsRect",     "c" ENC_NSRECT ENC_NSRECT,        NULL },
   /*  9 */ { "NSEqualPoints",      "c" ENC_NSPOINT ENC_NSPOINT,      NULL },
   /* 10 */ { "NSEqualRects",       "c" ENC_NSRECT ENC_NSRECT,        NULL },
   /* 11 */ { "NSEqualSizes",       "c" ENC_NSSIZE ENC_NSSIZE,        NULL },
   /* 12 */ { "NSIntersectsRect",   "c" ENC_NSRECT ENC_NSRECT,        NULL },
   /* 13 */ { "NSIsEmptyRect",      "c" ENC_NSRECT,                   NULL },
   /* 14 */ { "NSMouseInRect",      "c" ENC_NSPOINT ENC_NSRECT "c",   NULL },
   /* 15 */ { "NSPointInRect",      "c" ENC_NSPOINT ENC_NSRECT,       NULL },
   /* 16 */ { "NSStringFromPoint",  "@" ENC_NSPOINT,                  NULL },
   /* 17 */ { "NSStringFromRange",  "@" ENC_NSRANGE,                  NULL },
   /* 18 */ { "NSStringFromRect",   "@" ENC_NSRECT,                   NULL },
   /* 19 */ { "NSStringFromSize",   "@" ENC_NSSIZE,                   NULL },
   /* 20 */ { "NSEraseRect",        "v" ENC_NSRECT,                   NULL },
   /* 21 */ { "NSFrameRect",        "v" ENC_NSRECT,                   NULL },
   /* 22 */ { "NSFrameRectWithWidth", "v" ENC_NSRECT "f",             NULL },
   /* 23 */ { "NSRectClip",         "v" ENC_NSRECT,                   NULL },
   /* 24 */ { "NSRectFill",         "v" ENC_NSRECT,                   NULL },
   /* 25 */ { "NSRectFillUsingOperation", "v" ENC_NSRECT "I",         NULL },
   /* 26 */ { "CGRectUnion",        ENC_NSRECT ENC_NSRECT ENC_NSRECT, NULL },
   /* 27 */ { "CGRectIntersection", ENC_NSRECT ENC_NSRECT ENC_NSRECT, NULL },
   /* 28 */ { "CGRectInset",        ENC_NSRECT ENC_NSRECT "ff",       NULL },
   /* 29 */ { "CGRectOffset",       ENC_NSRECT ENC_NSRECT "ff",       NULL },
   /* 30 */ { "CGRectIntegral",     ENC_NSRECT ENC_NSRECT,            NULL },
   /* 31 */ { "CGRectApplyAffineTransform", ENC_NSRECT ENC_NSRECT ENC_CGAFF, NULL },
   /* 32 */ { "CGRectContainsPoint", "c" ENC_NSRECT ENC_NSPOINT,      NULL },
   /* 33 */ { "CGRectContainsRect", "c" ENC_NSRECT ENC_NSRECT,        NULL },
   /* 34 */ { "CGRectEqualToRect",  "c" ENC_NSRECT ENC_NSRECT,        NULL },
   /* 35 */ { "CGRectIntersectsRect", "c" ENC_NSRECT ENC_NSRECT,      NULL },
   /* 36 */ { "CGRectIsEmpty",      "c" ENC_NSRECT,                   NULL },
   /* 37 */ { "CGRectIsInfinite",   "c" ENC_NSRECT,                   NULL },
   /* 38 */ { "CGRectIsNull",       "c" ENC_NSRECT,                   NULL },
   /* 39 */ { "CGRectGetHeight",    "f" ENC_NSRECT,                   NULL },
   /* 40 */ { "CGRectGetWidth",     "f" ENC_NSRECT,                   NULL },
   /* 41 */ { "CGRectGetMaxX",      "f" ENC_NSRECT,                   NULL },
   /* 42 */ { "CGRectGetMaxY",      "f" ENC_NSRECT,                   NULL },
   /* 43 */ { "CGRectGetMidX",      "f" ENC_NSRECT,                   NULL },
   /* 44 */ { "CGRectGetMidY",      "f" ENC_NSRECT,                   NULL },
   /* 45 */ { "CGRectGetMinX",      "f" ENC_NSRECT,                   NULL },
   /* 46 */ { "CGRectGetMinY",      "f" ENC_NSRECT,                   NULL },
   /* Private CG "style" (drop-shadow) chain — unshimmed by abigen (struct +
    * CGFloat by value, opaque CF-ptr args/return). iLifeKit's GradientView
    * draws a drop shadow via these. The opaque CF ptrs (CGColorRef/CGContextRef/
    * CGStyleRef) are handle-bridged by geo_call: arena handles unwrap to the
    * real ptr, an unrecoverable truncated ptr becomes NULL (the draw degrades
    * to no shadow rather than dereferencing garbage). */
   /* 47 */ { "CGStyleCreateShadow", "^{CGStyle=}" ENC_NSSIZE "f^{CGColor=}", NULL },
   /* 48 */ { "CGContextSetStyle",   "v^{CGContext=}^{CGStyle=}",     NULL },
   /* 49 */ { "CGStyleRelease",      "v^{CGStyle=}",                  NULL },
   /* CG context 2D drawing family — abigen skips these (CGRect/CGFloat/
    * CGAffineTransform by value). The leading CGContextRef and any CGImageRef/
    * CGColorRef are handle-bridged by geo_call (geo_cfptr_arg / '^' return). */
   /* 50 */ { "CGContextFillRect",        "v^{CGContext=}" ENC_NSRECT,        NULL },
   /* 51 */ { "CGContextStrokeRect",      "v^{CGContext=}" ENC_NSRECT,        NULL },
   /* 52 */ { "CGContextStrokeRectWithWidth", "v^{CGContext=}" ENC_NSRECT "f", NULL },
   /* 53 */ { "CGContextClearRect",       "v^{CGContext=}" ENC_NSRECT,        NULL },
   /* 54 */ { "CGContextClipToRect",      "v^{CGContext=}" ENC_NSRECT,        NULL },
   /* 55 */ { "CGContextAddRect",         "v^{CGContext=}" ENC_NSRECT,        NULL },
   /* 56 */ { "CGContextFillEllipseInRect",   "v^{CGContext=}" ENC_NSRECT,    NULL },
   /* 57 */ { "CGContextStrokeEllipseInRect", "v^{CGContext=}" ENC_NSRECT,    NULL },
   /* 58 */ { "CGContextAddEllipseInRect","v^{CGContext=}" ENC_NSRECT,        NULL },
   /* 59 */ { "CGContextMoveToPoint",     "v^{CGContext=}ff",                 NULL },
   /* 60 */ { "CGContextAddLineToPoint",  "v^{CGContext=}ff",                 NULL },
   /* 61 */ { "CGContextTranslateCTM",    "v^{CGContext=}ff",                 NULL },
   /* 62 */ { "CGContextScaleCTM",        "v^{CGContext=}ff",                 NULL },
   /* 63 */ { "CGContextRotateCTM",       "v^{CGContext=}f",                  NULL },
   /* 64 */ { "CGContextConcatCTM",       "v^{CGContext=}" ENC_CGAFF,         NULL },
   /* 65 */ { "CGContextSetLineWidth",    "v^{CGContext=}f",                  NULL },
   /* 66 */ { "CGContextSetAlpha",        "v^{CGContext=}f",                  NULL },
   /* 67 */ { "CGContextSetRGBFillColor", "v^{CGContext=}ffff",              NULL },
   /* 68 */ { "CGContextSetRGBStrokeColor","v^{CGContext=}ffff",             NULL },
   /* 69 */ { "CGContextSetGrayFillColor","v^{CGContext=}ff",               NULL },
   /* 70 */ { "CGContextSetGrayStrokeColor","v^{CGContext=}ff",             NULL },
   /* 71 */ { "CGContextDrawImage",       "v^{CGContext=}" ENC_NSRECT "^{CGImage=}", NULL },
   /* 72 */ { "CGColorCreateGenericRGB",  "^{CGColor=}ffff",                 NULL },
   /* struct returns (i386 hidden-ptr stret -> GEOSHIM_S) */
   /* 73 */ { "CGContextGetClipBoundingBox", ENC_NSRECT "^{CGContext=}",     NULL },
   /* 74 */ { "CGContextGetCTM",          ENC_CGAFF "^{CGContext=}",         NULL },
   /* 75 */ { "CGContextSetPatternPhase", "v^{CGContext=}" ENC_NSSIZE,       NULL },
   /* CG text + path + gradient + shadow draw family. These take CGFloat /
    * CGPoint / CGSize / CGAffineTransform / CGRect BY VALUE, so abigen either
    * SKIPS them (struct-by-value: the gradient/shadow/text-matrix/layer group)
    * or — worse — emits a shim that reads each CGFloat as an 8-byte DOUBLE from
    * the i386 4-byte float slot (the scalar-CGFloat group: ShowTextAtPoint /
    * SetTextPosition / AddArc... — every following arg then mis-aligns). Either
    * way the i386 app's custom-view drawRect:/drawInContext: drew text and
    * shapes at garbage positions (iPhoto's EtchedText / AlbumView /
    * IWWindowBackgroundView rendered blank/black/skewed). Listing them in
    * custom.syms excludes abigen's broken shim; geo_call marshals each with the
    * i386 (float) classifier. Opaque CG ptrs (CGGradient/CGColor/CGLayer) are
    * handle-bridged by geo_cfptr_arg; the deprecated char* text path passes its
    * low-4GB string pointer straight through. */
   /* 76 */ { "CGContextShowTextAtPoint",  "v^{CGContext=}ff*I",            NULL },
   /* 77 */ { "CGContextSetTextPosition",  "v^{CGContext=}ff",             NULL },
   /* 78 */ { "CGContextSelectFont",       "v^{CGContext=}*fI",            NULL },
   /* 79 */ { "CGContextAddArc",           "v^{CGContext=}fffffi",         NULL },
   /* 80 */ { "CGContextAddArcToPoint",    "v^{CGContext=}fffff",          NULL },
   /* 81 */ { "CGContextAddQuadCurveToPoint", "v^{CGContext=}ffff",        NULL },
   /* 82 */ { "CGContextAddCurveToPoint",  "v^{CGContext=}ffffff",         NULL },
   /* 83 */ { "CGContextDrawLinearGradient", "v^{CGContext=}^{CGGradient=}"
                                            ENC_NSPOINT ENC_NSPOINT "I",   NULL },
   /* 84 */ { "CGContextDrawRadialGradient", "v^{CGContext=}^{CGGradient=}"
                                            ENC_NSPOINT "f" ENC_NSPOINT "f" "I", NULL },
   /* 85 */ { "CGContextSetShadow",        "v^{CGContext=}" ENC_NSSIZE "f", NULL },
   /* 86 */ { "CGContextSetShadowWithColor", "v^{CGContext=}" ENC_NSSIZE
                                            "f^{CGColor=}",                 NULL },
   /* 87 */ { "CGContextSetTextMatrix",    "v^{CGContext=}" ENC_CGAFF,      NULL },
   /* 88 */ { "CGContextDrawLayerInRect",  "v^{CGContext=}" ENC_NSRECT
                                            "^{CGLayer=}",                  NULL },
   /* CATransform3D (CoreAnimation) C builder family. abigen SKIPS these — a
    * 16-field CGFloat struct exceeds its by-value-arg classifier ("field count
    * not in 1..2") — so they bound RAW to native QuartzCore. Taking/returning
    * CATransform3D (16 CGFloats = 64B i386 / 128B native) + CGFloat angles BY
    * VALUE, the raw i386 cdecl call (stack args + 64B hidden-ptr stret) mismatches
    * the SysV ABI (xmm doubles + 128B stret) -> garbage matrices -> iPhoto's
    * welcome-screen layer transforms came out as sheared parallelograms. geo_call
    * marshals them via enc_widen/enc_narrow exactly like the CGAffineTransform
    * twins (CGContextGetCTM idx 74 / ConcatCTM idx 64), just a bigger struct (128B
    * fits stret_buf 184B; two 128B args fit plan.stack 352B). Struct returns use
    * the i386 hidden-ptr stret convention -> GEOSHIM_S. KEEP IN SYNC with the
    * GEOSHIM_* list in maptable_tramp.asm and the symbols in custom.syms. */
   /* 89 */ { "CATransform3DMakeRotation",      ENC_CT3D "ffff",            NULL },
   /* 90 */ { "CATransform3DMakeScale",         ENC_CT3D "fff",             NULL },
   /* 91 */ { "CATransform3DMakeTranslation",   ENC_CT3D "fff",             NULL },
   /* 92 */ { "CATransform3DRotate",            ENC_CT3D ENC_CT3D "ffff",   NULL },
   /* 93 */ { "CATransform3DScale",             ENC_CT3D ENC_CT3D "fff",    NULL },
   /* 94 */ { "CATransform3DTranslate",         ENC_CT3D ENC_CT3D "fff",    NULL },
   /* 95 */ { "CATransform3DConcat",            ENC_CT3D ENC_CT3D ENC_CT3D, NULL },
   /* 96 */ { "CATransform3DInvert",            ENC_CT3D ENC_CT3D,          NULL },
   /* 97 */ { "CATransform3DMakeAffineTransform", ENC_CT3D ENC_CGAFF,       NULL },
   /* 98 */ { "CATransform3DGetAffineTransform",  ENC_CGAFF ENC_CT3D,       NULL },
   /* 99 */ { "CATransform3DIsIdentity",        "c" ENC_CT3D,              NULL },
};

struct geo_res { uint64_t lo, hi; double fp; };

/* Bridge an opaque CF/CG pointer arg (e.g. CGColorRef/CGContextRef/CGStyleRef,
 * encoded `^{Name=}`) from its i386 32-bit slot to the real 64-bit pointer:
 *  - an arena handle unwraps to the real object;
 *  - 0 stays NULL;
 *  - any other bare 32-bit value is a TRUNCATED native pointer (genuine CF/CG
 *    objects live above 4GB) that we cannot reconstruct, so we pass NULL and let
 *    the native call degrade gracefully rather than dereference garbage. */
static uint64_t geo_cfptr_arg(uint32_t raw) {
   if (raw == 0) { return 0; }
   uint64_t r = x64_objc_unwrap(raw);
   if (r == (uint64_t)raw) { return 0; }   /* not a handle -> truncated -> NULL */
   return r;
}

static struct geo_res geo_call(unsigned idx, const uint32_t *a32) {
   struct geo_res res = {0, 0, 0.0};
   if (idx >= sizeof g_geo / sizeof g_geo[0]) { return res; }
   struct geo_ent *e = &g_geo[idx];
   if (!e->fn) {
      e->fn = dlsym(RTLD_DEFAULT, e->name);
      if (!e->fn) {
         fprintf(stderr, "objc_shim: geo shim: dlsym(%s) failed\n", e->name);
         fflush(stderr);
         return res;
      }
   }
   struct objc_call_plan plan;
   memset(&plan, 0, sizeof plan);
   struct mcur cur = {0, 0, 0};
   const char *ret  = e->sig;
   const char *args = enc_skip_type(e->sig);
   unsigned ai = 0;
   /* Narrow diagnostic (separate from the OBJC_BRIDGE_TRACE firehose): dump the
    * first 6 i386 arg slots as hex + float so a brief play reveals the actual CG
    * draw geometry/pointers reaching native (e.g. CGContextDrawImage ctx@slot0,
    * rect@slots1-4, image@slot5). Quinn invisible-blocks investigation. */
   if (getenv("GEO_PTR_TRACE")) {
      float f1, f2, f3, f4;
      memcpy(&f1, &a32[1], 4); memcpy(&f2, &a32[2], 4);
      memcpy(&f3, &a32[3], 4); memcpy(&f4, &a32[4], 4);
      fprintf(stderr, "[geoin] %s slots=[%08x %08x %08x %08x %08x %08x] "
              "rectf=[%.2f %.2f %.2f %.2f]\n", e->name,
              a32[0], a32[1], a32[2], a32[3], a32[4], a32[5],
              (double)f1, (double)f2, (double)f3, (double)f4);
      fflush(stderr);
   }
   /* Focused Quinn cell-draw trace (clean signal vs the GEO_PTR_TRACE firehose).
    * The operative block draw is _QuinnGeneralFastDrawCells: it calls
    * CGContextSetAlpha ONCE per matrix (function entry), then per OCCUPIED cell
    * either CGContextDrawImage (sprite cell, slot5=cell image) or
    * CGContextFillRect (solid 0xFF cell), into the offscreen/current context.
    * NSRectFill(UsingOperation) is the offscreen clear + the slow board path.
    * This reveals (a) whether FastDrawCells is reached at all (any SetAlpha),
    * (b) how many occupied cells it draws per matrix (the count between two
    * SetAlphas), and (c) whether each cell sprite image unwraps to a real
    * 64-bit CGImage. Zero output unless QUINN_CELL_TRACE is set. */
   if (getenv("QUINN_CELL_TRACE")) {
      static unsigned q_matrices, q_cells_in_matrix, q_total_cells;
      const char *n = e->name;
      if (!strcmp(n, "CGContextSetAlpha")) {
         if (q_matrices)
            fprintf(stderr, "[cell]   ^matrix #%u drew %u occupied cells\n",
                    q_matrices, q_cells_in_matrix);
         q_matrices++; q_cells_in_matrix = 0;
         float al; memcpy(&al, &a32[1], 4);
         fprintf(stderr, "[cell] SetAlpha(#%u) ctx=0x%08x alpha=%.3f\n",
                 q_matrices, a32[0], (double)al);
      } else if (!strcmp(n, "CGContextDrawImage")) {
         q_cells_in_matrix++; q_total_cells++;
         uint64_t real = geo_cfptr_arg(a32[5]);
         float x,y,w,h; memcpy(&x,&a32[1],4); memcpy(&y,&a32[2],4);
         memcpy(&w,&a32[3],4); memcpy(&h,&a32[4],4);
         fprintf(stderr, "[cell] DrawImage ctx=0x%08x rect=[%.1f %.1f %.1f %.1f]"
                 " img=0x%08x->0x%llx%s (total=%u)\n", a32[0],
                 (double)x,(double)y,(double)w,(double)h, a32[5],
                 (unsigned long long)real, real ? "" : " NULL!", q_total_cells);
      } else if (!strcmp(n, "CGContextFillRect")) {
         q_cells_in_matrix++; q_total_cells++;
         float x,y,w,h; memcpy(&x,&a32[1],4); memcpy(&y,&a32[2],4);
         memcpy(&w,&a32[3],4); memcpy(&h,&a32[4],4);
         fprintf(stderr, "[cell] FillRect ctx=0x%08x rect=[%.1f %.1f %.1f %.1f]"
                 " (total=%u)\n", a32[0],
                 (double)x,(double)y,(double)w,(double)h, q_total_cells);
      } else if (!strcmp(n, "NSRectFill") ||
                 !strcmp(n, "NSRectFillUsingOperation")) {
         float x,y,w,h; memcpy(&x,&a32[0],4); memcpy(&y,&a32[1],4);
         memcpy(&w,&a32[2],4); memcpy(&h,&a32[3],4);
         fprintf(stderr, "[cell] %s rect=[%.1f %.1f %.1f %.1f]\n", n,
                 (double)x,(double)y,(double)w,(double)h);
      }
      fflush(stderr);
   }
   char rb = *enc_skip_quals(ret);
   size_t risz = 0, rnsz = 0; uint8_t rsse[8] = {0}; unsigned rnebs = 0;
   int kind = 0;   /* 0 scalar/void, 1 wrap obj, 3 stret narrow, 4 fp,
                    * 5 reg-struct -> eax:edx, 6 reg-struct -> i386 buf */
   uint32_t dst32 = 0;
   if (rb == '{' || rb == '(' || rb == '[') {
      rnebs = enc_classify(ret, CONV_I386, &risz, &rnsz, rsse);
      if (risz > 8) { dst32 = a32[ai++]; }    /* i386 hidden ptr, callee-pop */
      if (rnsz > 16) {
         plan.reg[cur.gp++] = (uint64_t)(uintptr_t)plan.stret_buf;
         kind = 3;
      } else {
         kind = (risz > 8) ? 6 : 5;
      }
   } else if (rb == '@' || rb == '^') {
      kind = 1;   /* '^' = opaque CF/CG ptr return -> wrap as arena handle */
   } else if (rb == 'f' || rb == 'd') {
      kind = 4;
   }
   for (const char *t = args; *t; t = enc_skip_type(t)) {
      const char *tb = enc_skip_quals(t);
      if (*tb == '^') {            /* opaque CF/CG ptr arg -> handle-bridge */
         uint32_t praw = a32[ai++];
         uint64_t preal = geo_cfptr_arg(praw);
         if (getenv("GEO_PTR_TRACE")) {
            fprintf(stderr, "[geoptr] %s ^arg raw=0x%08x -> real=0x%llx%s\n",
                    e->name, praw, (unsigned long long)preal,
                    preal == 0 ? "  (NULL!)" : "");
            fflush(stderr);
         }
         mcur_put_gp(&plan, &cur, preal);
         continue;
      }
      marshal_arg_fwd(&plan, &cur, t, CONV_I386, a32, &ai);
   }
   plan.nstack = (uint32_t)((cur.stk + 7) / 8);
   plan.nxmm   = cur.xmm;
   unsigned __int128 rr = _86x64_plan_call(&plan, e->fn);
   uint64_t vrax = (uint64_t)rr, vrdx = (uint64_t)(rr >> 64);
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[geo] %s kind=%d gp=%u xmm=%u stk=%zu rax=0x%llx\n",
              e->name, kind, cur.gp, cur.xmm, cur.stk,
              (unsigned long long)vrax);
      fflush(stderr);
   }
   switch (kind) {
   case 1:
      res.lo = x64_objc_wrap(vrax);
      break;
   case 3:
      if (dst32) {
         enc_narrow(ret, CONV_I386, plan.stret_buf,
                    (uint8_t *)(uintptr_t)dst32);
      }
      res.lo = dst32;
      break;
   case 4:
      memcpy(&res.fp, &plan.fp_out[0], 8);    /* native double (CGFloat) */
      break;
   case 5: case 6: {
      uint8_t nat[16] = {0};
      unsigned ii = 0, fi = 0;
      for (unsigned k = 0; k < rnebs && k < 2; ++k) {
         uint64_t eb;
         if (rsse[k]) { eb = plan.fp_out[fi++]; }
         else         { eb = ii++ ? vrdx : vrax; }
         memcpy(nat + 8*k, &eb, 8);
      }
      if (kind == 6) {
         if (dst32) {
            enc_narrow(ret, CONV_I386, nat, (uint8_t *)(uintptr_t)dst32);
         }
         res.lo = dst32;
      } else {
         uint8_t out[8] = {0};
         enc_narrow(ret, CONV_I386, nat, out);
         uint32_t lo, hi;
         memcpy(&lo, out, 4); memcpy(&hi, out + 4, 4);
         res.lo = lo; res.hi = hi;
      }
      break;
   }
   default:
      res.lo = vrax; res.hi = vrdx;
      break;
   }
   return res;
}

/* asm-facing dispatchers (maptable_tramp.asm GEOSHIM_*):
 *   _i: integer/object/struct-in-regs returns -> rax (=eax) : rdx (=edx)
 *   _f: CGFloat/double returns -> long double return = x87 st0 */
unsigned __int128 x64_geo_i(unsigned idx, const uint32_t *a32) {
   struct geo_res r = geo_call(idx, a32);
   return ((unsigned __int128)r.hi << 64) | r.lo;
}
long double x64_geo_f(unsigned idx, const uint32_t *a32) {
   struct geo_res r = geo_call(idx, a32);
   return (long double)r.fp;
}

/* NSDivideRect has NSRect OUT-pointers — hand-marshalled. i386 frame:
 * a[0..3] inRect (4 floats), a[4] NSRect* slice, a[5] NSRect* remainder,
 * a[6] CGFloat amount, a[7] NSRectEdge. MTSHIM trampoline (returns void). */
uint32_t mt_NSDivideRect(const uint32_t *a) {
   struct nr64 { double v[4]; } in, slice, rem;
   memset(&slice, 0, sizeof slice); memset(&rem, 0, sizeof rem);
   enc_widen(ENC_NSRECT, CONV_I386, (const uint8_t *)&a[0], (uint8_t *)&in);
   static void (*fn)(struct nr64, struct nr64 *, struct nr64 *, double,
                     uint32_t);
   if (!fn) {
      fn = (void (*)(struct nr64, struct nr64 *, struct nr64 *, double,
                     uint32_t))dlsym(RTLD_DEFAULT, "NSDivideRect");
      if (!fn) { return 0; }
   }
   float amf; memcpy(&amf, &a[6], 4);
   fn(in, &slice, &rem, (double)amf, a[7]);
   if (a[4]) {
      enc_narrow(ENC_NSRECT, CONV_I386, (const uint8_t *)&slice,
                 (uint8_t *)(uintptr_t)a[4]);
   }
   if (a[5]) {
      enc_narrow(ENC_NSRECT, CONV_I386, (const uint8_t *)&rem,
                 (uint8_t *)(uintptr_t)a[5]);
   }
   return 0;
}

/* ===========================================================================
 * CGPatternCreate — abigen skips it (CGRect/CGAffineTransform/CGFloat by value
 * AND a CGPatternCallbacks* whose drawPattern/releaseInfo are i386 code that
 * native CG will CALL BACK with the x86_64 ABI). Hand-marshalled here:
 *  - the by-value geometry is widened i386-float -> x86_64-double;
 *  - the callback fn-ptrs are bridged through x64_cb_wrap (cb_bridge.c), so
 *    native CG re-enters the translated callbacks on a fresh low stack. The
 *    CGContextRef handed to drawPattern is a high native ptr -> wrapped to an
 *    arena handle (CBA_OBJ) the i386 callback can feed back to the CGContext*
 *    geo-shims; `info` is opaque and round-trips as a raw 32-bit word.
 *  - the returned CGPatternRef is wrapped as an arena handle (eax).
 * i386 cdecl frame (caller-pop, structs expanded inline), 4-byte slots:
 *   a[0]      info
 *   a[1..4]   CGRect bounds        (4 floats)
 *   a[5..10]  CGAffineTransform    (6 floats)
 *   a[11]     xStep  a[12] yStep   (floats)
 *   a[13]     tiling  a[14] isColored  a[15] callbacks*
 * =========================================================================== */
/* x64_cb_sig_t / x64_cb_wrap declared earlier (near enc_ew). */
/* arg kinds: PTR=2 OBJ=3 ; ret: VOID=0  (mirror cb_bridge.c) */
static const x64_cb_sig_t g_cgpat_draw_sig    = { 2, 0, { 2, 3 } };
static const x64_cb_sig_t g_cgpat_release_sig = { 1, 0, { 2 } };

struct cg_rect64 { double a, b, c, d; };       /* >16B -> MEMORY class, like CGRect */
struct cg_aff64  { double a, b, c, d, e, f; }; /* >16B -> MEMORY class */
struct cg_patcb64 {
   unsigned int version;
   void (*drawPattern)(void *info, void *ctx);
   void (*releaseInfo)(void *info);
};

uint32_t shim_CGPatternCreate(const uint32_t *a) {
   static void *(*fn)(void *, struct cg_rect64, struct cg_aff64, double, double,
                      uint32_t, int, const struct cg_patcb64 *);
   if (!fn) {
      fn = (void *(*)(void *, struct cg_rect64, struct cg_aff64, double, double,
                      uint32_t, int, const struct cg_patcb64 *))
           dlsym(RTLD_DEFAULT, "CGPatternCreate");
      if (!fn) {
         fprintf(stderr, "objc_shim: dlsym(CGPatternCreate) failed\n");
         return 0;
      }
   }
   float f;
   struct cg_rect64 bounds;
   memcpy(&f, &a[1], 4); bounds.a = f;
   memcpy(&f, &a[2], 4); bounds.b = f;
   memcpy(&f, &a[3], 4); bounds.c = f;
   memcpy(&f, &a[4], 4); bounds.d = f;
   struct cg_aff64 m;
   memcpy(&f, &a[5],  4); m.a = f;
   memcpy(&f, &a[6],  4); m.b = f;
   memcpy(&f, &a[7],  4); m.c = f;
   memcpy(&f, &a[8],  4); m.d = f;
   memcpy(&f, &a[9],  4); m.e = f;
   memcpy(&f, &a[10], 4); m.f = f;
   double xStep, yStep;
   memcpy(&f, &a[11], 4); xStep = f;
   memcpy(&f, &a[12], 4); yStep = f;
   uint32_t tiling   = a[13];
   int      isColored = (int)(a[14] & 0xff);

   struct cg_patcb64 ncb, *ncbp = NULL;
   if (a[15]) {
      const uint32_t *icb = (const uint32_t *)(uintptr_t)a[15];
      ncb.version     = icb[0];
      ncb.drawPattern = (void (*)(void *, void *))
                        (uintptr_t)x64_cb_wrap(icb[1], &g_cgpat_draw_sig);
      ncb.releaseInfo = (void (*)(void *))
                        (uintptr_t)x64_cb_wrap(icb[2], &g_cgpat_release_sig);
      ncbp = &ncb;
   }
   void *info = (void *)(uintptr_t)a[0];
   void *pat  = fn(info, bounds, m, xStep, yStep, tiling, isColored, ncbp);
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[geo] CGPatternCreate colored=%d cb=0x%x -> %p\n",
              isColored, a[15], pat);
      fflush(stderr);
   }
   return x64_objc_wrap((uint64_t)(uintptr_t)pat);
}

/* i386-LAYOUT shadows for CG/NS geometry DATA constants. The native symbols
 * are structs of x86_64 *doubles* living above 4GB; an external i386 reader
 * (a) takes them through a non-lazy GOT pointer that a 32-bit `movl` TRUNCATES
 * to the low 4GB -> dereference faults, and (b) even unbroken would read the
 * double layout as i386 floats and see garbage. static-interpose redirects each
 * non-lazy bind to its __-twin here (same mechanism as the ObjC/CF data
 * shadows): a low-4GB symbol holding the i386 (float) layout, so the GOT
 * pointer fits in 32 bits AND the field reads land on correct float values.
 * Required for ALL such struct constants accessed via the GOT, not just the
 * non-zero ones — CGAffineTransformIdentity's truncated pointer was the
 * iPhoto -[NSCustomView nibInstantiate] SIGSEGV. (Each is listed in
 * custom.syms so abigen skips it and static-interpose can find the twin.) */
const float __CGRectNull[4]  = { __builtin_inff(), __builtin_inff(), 0, 0 };
const float __CGAffineTransformIdentity[6] = { 1, 0, 0, 1, 0, 0 };
const float __CATransform3DIdentity[16] = { 1, 0, 0, 0,  0, 1, 0, 0,
                                            0, 0, 1, 0,  0, 0, 0, 1 };
const float __CGRectZero[4]  = { 0, 0, 0, 0 };
const float __CGPointZero[2] = { 0, 0 };
const float __CGSizeZero[2]  = { 0, 0 };
const float __NSZeroRect[4]  = { 0, 0, 0, 0 };
const float __NSZeroPoint[2] = { 0, 0 };
const float __NSZeroSize[2]  = { 0, 0 };

/* ---- registration ---- */

/* Enumerate one legacy class's method lists (handles both the single-direct-
 * list and array-of-list-ptrs layouts, like find_method_in_lists) and add each
 * to target_class via class_addMethod(_86x64_reverse_imp), recording the legacy
 * IMP in the (target_class,sel) map. */
static void reverse_add_methods(Class target, uint32_t methodLists) {
   if (!ptr_ok(methodLists, 8)) { return; }
   const uint32_t *word = (const uint32_t *)(uintptr_t)methodLists;
   /* single direct list iff word[0]==0 (obsolete) && word[1] plausible count */
   int single = (word[0] == 0 && (int32_t)word[1] > 0 && word[1] < 100000);
   const uint32_t *lists; uint32_t nlists; uint32_t single_arr[1];
   if (single) {
      single_arr[0] = methodLists; lists = single_arr; nlists = 1;
   } else {
      /* NULL/-1 terminated array of method_list ptrs */
      lists = word;
      nlists = 0;
      while (nlists < 4096 && ptr_ok((uint32_t)(uintptr_t)&lists[nlists], 4)
             && lists[nlists] != 0 && lists[nlists] != 0xFFFFFFFFu) { ++nlists; }
   }
   for (uint32_t li = 0; li < nlists; ++li) {
      uint32_t mlp = lists[li];
      if (!ptr_ok(mlp, sizeof(struct legacy_objc_method_list))) { continue; }
      const struct legacy_objc_method_list *ml =
         (const struct legacy_objc_method_list *)(uintptr_t)mlp;
      int32_t cnt = ml->method_count;
      if (cnt <= 0 || cnt > 100000) { continue; }
      const struct legacy_objc_method *meth = (const struct legacy_objc_method *)
         ((const char *)ml + sizeof(struct legacy_objc_method_list));
      /* Range-check the WHOLE method array (mirrors scan_one_list): a junk
       * list pointer can pass the 8-byte header check while the entries run
       * into a translated image's __LINKEDIT vm slack — mapped beyond the
       * file's EOF, so the first touch SIGBUSes (KERN_MEMORY_ERROR) inside
       * dyld's add-image callback, before main. */
      if (!ptr_ok(mlp + (uint32_t)sizeof(*ml),
                  (size_t)cnt * sizeof(*meth))) { continue; }
      for (int32_t k = 0; k < cnt; ++k) {
         if (!legacy_cstr_ok(meth[k].name)) { continue; }
         /* The imp must be a plausible, mapped code address. A few legacy method
          * lists carry a junk entry whose imp is a tiny sentinel (e.g. 0xb) and
          * whose types pointer is also bad — registering it shadows the native
          * implementation (NSObject -description, etc.) with our reverse tramp,
          * which then jumps straight to the junk imp. Reject it so the selector
          * falls through to the real native superclass method. (== 0 was the old
          * filter; ptr_ok also catches <0x1000 and unmapped.) */
         if (!ptr_ok(meth[k].imp, 1)) {
            if (BRIDGE_TRACE()) {
               fprintf(stderr, "[rt] skip junk method \"%s\" imp=0x%x (implausible)\n",
                       legacy_cstr_ok(meth[k].name)
                          ? (const char *)(uintptr_t)meth[k].name : "?",
                       meth[k].imp);
               fflush(stderr);
            }
            continue;
         }
         const char *sname = (const char *)(uintptr_t)meth[k].name;
         const char *types = legacy_cstr_ok(meth[k].types)
            ? (const char *)(uintptr_t)meth[k].types : "v8@0:4";
         SEL sel = sel_registerName(sname);
         /* ABICONV_REV_REG_TRACE: log every reverse-registration, and loudly flag
          * any entry with a 0/implausible legacy imp (should be caught by ptr_ok
          * above, but trace defensively). */
         if (getenv("ABICONV_REV_REG_TRACE")) {
            const char *cn = class_getName(target);
            if (meth[k].imp == 0) {
               fprintf(stderr, "[REV_REG] imp=0 ZERO-IMP class=%s sel=%s types=%s\n",
                       cn ? cn : "?", sname, types);
            } else if (meth[k].imp < 0x1000) {
               fprintf(stderr, "[REV_REG] imp=0x%x JUNK-IMP class=%s sel=%s types=%s\n",
                       meth[k].imp, cn ? cn : "?", sname, types);
            } else {
               fprintf(stderr, "[REV_REG] imp=0x%x class=%s sel=%s types=%s\n",
                       meth[k].imp, cn ? cn : "?", sname, types);
            }
            fflush(stderr);
         }
         /* A struct-by-value return (>16B, e.g. -adjustScroll: -> NSRect) is
          * dispatched by native callers through objc_msgSend_stret, whose ABI
          * puts a hidden return-buffer pointer in rdi. Register the stret-aware
          * trampoline for those so self/_cmd aren't read one register early. */
         IMP tramp = enc_ret_is_stret(types) ? (IMP)_86x64_reverse_imp_stret
                                             : (IMP)_86x64_reverse_imp;
         class_addMethod(target, sel, tramp, types);
         /* record the legacy imp regardless (class_addMethod fails when a
          * later list re-adds an override; the map still needs the entry) */
         rmeth_insert(target, sel, meth[k].imp, types);
         /* the app's own metadata is the authoritative i386 encoding for
          * this selector — lets the forward bridge disambiguate CGFloat vs
          * double / NSInteger vs long long when marshalling args to the
          * NATIVE method of the same name (e.g. [super adjustScroll:]) */
         seltypes_insert(sel, types);
      }
   }
}

/* Register one legacy class. Returns 1 if registered, 0 if deferred (super not
 * ready), -1 if skipped (collision / bad). */
static int reverse_register_one(const struct legacy_objc_class *cls) {
   /* Defensive NULL-base guard (belt-and-suspenders for the multicopy re-scan
    * race the cross-copy claim in slide_objc now prevents): a class def slot
    * that a mid-mapped image handed back as 0 must be skipped, never
    * dereferenced at cls->name / +0x88. */
   if (!cls) { return -1; }
   if (!legacy_cstr_ok(cls->name) || !legacy_cstr_ok(cls->super_class)) {
      return -1;
   }
   const char *name = (const char *)(uintptr_t)cls->name;
   if (objc_getClass(name)) { return -1; }   /* name already exists — skip */
   const char *supername = (const char *)(uintptr_t)cls->super_class;
   Class super = objc_getClass(supername);
   if (!super) { return 0; }                  /* super not registered yet */
   Class newcls = objc_allocateClassPair(super, name, 0);
   if (!newcls) { return -1; }
   /* own object ivars (nib outlets) — must precede objc_registerClassPair */
   reverse_add_ivars(newcls, cls);
   /* instance methods */
   reverse_add_methods(newcls, cls->methodLists);
   /* class methods live on the metaclass; legacy metaclass = cls->isa */
   if (ptr_ok(cls->isa, sizeof(struct legacy_objc_class))) {
      const struct legacy_objc_class *meta =
         (const struct legacy_objc_class *)(uintptr_t)cls->isa;
      Class newmeta = object_getClass((id)newcls);
      reverse_add_methods(newmeta, meta->methodLists);
   }
   objc_registerClassPair(newcls);
   own_resolve_offsets(newcls);    /* modern ivar offsets valid post-register */
   rcls_insert(newcls, (uint32_t)(uintptr_t)cls, cls->instance_size);
   return 1;
}

/* Replay a reverse-registered legacy class's fragile-ObjC1 `+load`.
 *
 * The pre-ObjC2 runtime's call_load_methods() invoked +load at image load,
 * superclass-first, after the class and its categories were attached.
 * objc_registerClassPair() (and class_addMethod) do NOT trigger +load, so a
 * legacy class that relies on +load for one-time setup/registration/swizzling
 * would silently skip it. We invoke each class's own +load here.
 *
 * +load is a CLASS method, so it lives on the legacy metaclass (cls->isa). We
 * call the translated i386 IMP through the same native->i386 path the block and
 * C-callback bridges use (blk_call_i386, fresh low-4GB cdecl stack), passing
 * self = the modern Class as a wrapped handle (so the IMP's `[self ...]`
 * re-enters the forward bridge -> reverse bridge) and _cmd = 0 (+load bodies
 * effectively never read _cmd). Ordering: `reg[]` restricts this to classes we
 * actually registered (never a native/pre-existing class), `loaded[]` makes it
 * run-once, and a class's superclass +load is forced first by recursion. Guard
 * ABICONV_NO_LEGACY_LOAD disables the whole pass if a target's +load misbehaves
 * under the bridge (it runs app code inside the dyld add-image callback). */
static void invoke_legacy_load(const struct legacy_objc_class **defs,
                               size_t ndefs, const char *reg, char *loaded,
                               size_t i) {
   if (loaded[i]) { return; }
   loaded[i] = 1;
   if (!reg[i]) { return; }                 /* only classes we registered */
   const struct legacy_objc_class *cls = defs[i];
   if (!legacy_cstr_ok(cls->name)) { return; }
   /* superclass-first: run an in-image legacy superclass's +load before ours. */
   if (legacy_cstr_ok(cls->super_class)) {
      const char *sn = (const char *)(uintptr_t)cls->super_class;
      for (size_t j = 0; j < ndefs; ++j) {
         if (j == i || !legacy_cstr_ok(defs[j]->name)) { continue; }
         if (strcmp((const char *)(uintptr_t)defs[j]->name, sn) == 0) {
            invoke_legacy_load(defs, ndefs, reg, loaded, j);
            break;
         }
      }
   }
   /* +load must be declared in this class's OWN metaclass method list (legacy
    * metaclass = cls->isa); an inherited +load is the superclass's, run above. */
   if (!ptr_ok(cls->isa, sizeof(struct legacy_objc_class))) { return; }
   const struct legacy_objc_class *meta =
      (const struct legacy_objc_class *)(uintptr_t)cls->isa;
   uint64_t imp = find_method_in_lists(meta->methodLists, "load");
   if (!imp || imp < 0x1000 || imp >= 0x100000000ULL) { return; }
   Class modern = objc_getClass((const char *)(uintptr_t)cls->name);
   if (!modern) { return; }
   uint32_t self32 = x64_objc_wrap((uint64_t)(uintptr_t)modern);
   uint32_t words[2] = { self32, 0u };       /* self, _cmd(=0) */
   if (getenv("ABICONV_OBJC_SLIDE_VERBOSE")) {
      fprintf(stderr, "objc_shim: +load %s imp=0x%llx\n",
              (const char *)(uintptr_t)cls->name, (unsigned long long)imp);
      fflush(stderr);
   }
   blk_call_i386((uint32_t)imp, 2, words);
}

static void reverse_register_image(const struct mach_header_64 *mh,
                                   intptr_t slide) {
   if (!mh) { return; }
   size_t modinfo_size = 0;
   const struct legacy_objc_module *modules =
      find_module_info(mh, slide, &modinfo_size);
   if (!modules) { return; }
   const size_t nmodules = modinfo_size / sizeof(struct legacy_objc_module);

   /* collect class defs */
   enum { CAP = 4096 };
   const struct legacy_objc_class *defs[CAP];
   size_t ndefs = 0;
   for (size_t i = 0; i < nmodules; ++i) {
      const struct legacy_objc_module *mod = &modules[i];
      if (mod->version != 7 || mod->size != sizeof(*mod)) { continue; }
      if (!ptr_ok(mod->symtab, sizeof(struct legacy_objc_symtab))) { continue; }
      const struct legacy_objc_symtab *st =
         (const struct legacy_objc_symtab *)(uintptr_t)mod->symtab;
      const uint32_t *cdefs = (const uint32_t *)
         ((const char *)st + sizeof(struct legacy_objc_symtab));
      for (uint16_t j = 0; j < st->cls_def_cnt && ndefs < CAP; ++j) {
         if (!ptr_ok(cdefs[j], sizeof(struct legacy_objc_class))) { continue; }
         defs[ndefs++] = (const struct legacy_objc_class *)(uintptr_t)cdefs[j];
      }
   }

   /* iterate to fixpoint so superclasses register before subclasses */
   char done[CAP]; memset(done, 0, ndefs);
   char reg[CAP];  memset(reg, 0, ndefs);   /* r==1: a class WE registered */
   uint32_t registered = 0;
   for (int pass = 0; pass < 64; ++pass) {
      int progress = 0;
      for (size_t i = 0; i < ndefs; ++i) {
         if (done[i]) { continue; }
         int r = reverse_register_one(defs[i]);
         if (r != 0) { done[i] = 1; progress = 1;
                       if (r == 1) { ++registered; reg[i] = 1; } }
      }
      if (!progress) { break; }
   }

   /* Legacy categories on EXISTING classes: add their instance/class methods
    * to the real runtime class via the reverse bridge. Native targets (e.g.
    * NSClipView) are always present; legacy targets were just registered
    * above. class_addMethod adds only NEW selectors, so a native method of the
    * same name is never clobbered — but iWeb's added selectors (the
    * `p_replacement*` swizzle sources looked up via class_getInstanceMethod)
    * become real, dispatchable methods. Without this, category instance
    * methods were merely indexed for class-method lookup and stayed invisible
    * to class_getInstanceMethod / direct messaging. */
   uint32_t cats_applied = 0;
   int cat_trace = getenv("ABICONV_CAT_TRACE") != NULL;
   for (size_t i = 0; i < nmodules; ++i) {
      const struct legacy_objc_module *mod = &modules[i];
      if (mod->version != 7 || mod->size != sizeof(*mod)) {
         if (cat_trace) {
            fprintf(stderr, "[cat] mod[%zu] BAD-HDR version=0x%x size=0x%x symtab=0x%x\n",
                    i, mod->version, mod->size, mod->symtab); fflush(stderr);
         }
         continue;
      }
      if (!ptr_ok(mod->symtab, sizeof(struct legacy_objc_symtab))) {
         if (cat_trace) {
            fprintf(stderr, "[cat] mod[%zu] BAD-SYMTAB symtab=0x%x\n", i, mod->symtab);
            fflush(stderr);
         }
         continue;
      }
      const struct legacy_objc_symtab *st =
         (const struct legacy_objc_symtab *)(uintptr_t)mod->symtab;
      const uint32_t *ddefs = (const uint32_t *)
         ((const char *)st + sizeof(struct legacy_objc_symtab));
      if (cat_trace && st->cat_def_cnt) {
         fprintf(stderr, "[cat] mod[%zu] symtab=0x%x cls_def_cnt=%u cat_def_cnt=%u\n",
                 i, mod->symtab, st->cls_def_cnt, st->cat_def_cnt); fflush(stderr);
      }
      for (uint16_t j = 0; j < st->cat_def_cnt; ++j) {
         uint32_t cref = ddefs[(uint32_t)st->cls_def_cnt + j];
         if (!ptr_ok(cref, sizeof(struct legacy_objc_category))) {
            if (cat_trace) {
               fprintf(stderr, "[cat]   j=%u BAD-CREF cref=0x%x\n", j, cref);
               fflush(stderr);
            }
            continue;
         }
         const struct legacy_objc_category *cat =
            (const struct legacy_objc_category *)(uintptr_t)cref;
         if (!legacy_cstr_ok(cat->class_name)) {
            if (cat_trace) {
               fprintf(stderr, "[cat]   j=%u BAD-CLSNAME cref=0x%x class_name=0x%x\n",
                       j, cref, cat->class_name); fflush(stderr);
            }
            continue;
         }
         Class tgt = objc_getClass((const char *)(uintptr_t)cat->class_name);
         if (getenv("ABICONV_CAT_TRACE")) {
            fprintf(stderr, "[cat] %s (+%s) tgt=%p imeths=0x%x\n",
                    (const char *)(uintptr_t)cat->class_name,
                    legacy_cstr_ok(cat->category_name)
                       ? (const char *)(uintptr_t)cat->category_name : "?",
                    (void *)tgt, cat->instance_methods);
            fflush(stderr);
         }
         if (!tgt) { continue; }   /* target class not present yet */
         if (cat->instance_methods) {
            reverse_add_methods(tgt, cat->instance_methods);
         }
         if (cat->class_methods) {
            Class meta = object_getClass((id)tgt);
            if (meta) { reverse_add_methods(meta, cat->class_methods); }
         }
         ++cats_applied;
      }
   }

   /* +load pass: replay each registered legacy class's fragile-ObjC1 +load,
    * superclass-first, AFTER all classes and categories are attached (matching
    * the old call_load_methods timing). See invoke_legacy_load. */
   if (registered && !getenv("ABICONV_NO_LEGACY_LOAD")) {
      char loaded[CAP]; memset(loaded, 0, ndefs);
      for (size_t i = 0; i < ndefs; ++i) {
         invoke_legacy_load(defs, ndefs, reg, loaded, i);
      }
   }

   if (getenv("ABICONV_OBJC_SLIDE_VERBOSE")) {
      fprintf(stderr, "objc_shim: registered %u/%zu legacy classes, "
              "applied %u categories\n", registered, ndefs, cats_applied);
      fflush(stderr);
   }
}

/* ======================================================================
 * Legacy ObjC1 runtime compat (first hit: iWeb's iWork-shared SF* frameworks).
 *
 * Modern libobjc dropped the ObjC1 setjmp exception machinery
 * (objc_exception_try_enter/try_exit/extract/match + the longjmp half of
 * objc_exception_throw), the `id (*_dealloc)(id)` allocation vector,
 * _objc_setNilReceiver and class_nextMethodList. SFUtility etc. bind all of
 * them (NS_DURING/NS_HANDLER expand to try_enter + _setjmp), so dyld refuses
 * to load the translated images. static-interpose redirects those binds to
 * the __-prefixed exports below (i386-cdecl trampolines in maptable_tramp.asm).
 *
 * NS_DURING also calls _setjmp on the 18-int i386 jmp_buf embedded in
 * _objc_exception_data (88 bytes total). The native x86_64 _setjmp writes 148
 * bytes into that 72-byte buffer -> silent stack smash on every NS_DURING,
 * and its buf couldn't be longjmp'd by us anyway. ____setjmp
 * (maptable_tramp.asm) instead captures a compact translated-i386 context
 * {rbx,rsi,rdi,rbp,rsp,eip} + magic into the same buffer; our
 * objc_exception_throw restores it via x64_exc_longjmp. We own BOTH ends:
 * no iWeb payload imports any longjmp — the only longjmp lived inside the
 * old runtime's throw.
 *
 * Known limits (traced loudly when hit):
 *  - an exception raised by NATIVE code (e.g. +[NSException raise:...] deep
 *    in Foundation) unwinds as a C++ exception and never reaches this chain;
 *  - a longjmp across reverse-bridge re-entries abandons their native frames;
 *    their per-thread rsp stash (g_rstash) is now rewound at throw (try_enter
 *    snapshots the depth, throw restores it — without this the next reverse-IMP
 *    unstashed a stale rsp and `ret`'d to 0). Other per-copy/cross-copy
 *    bookkeeping pushed by abandoned frames (cb_active_depth, super hints) is
 *    still not unwound, but is not rsp-critical.
 * ====================================================================== */

#define EXC32_MAGIC      0x36346a62u
#define EXC_CHAIN_SLOTS  64

/* i386 _objc_exception_data: { int buf[18]; void *pointers[4]; }.
 * buf[18] (72B) holds our ____setjmp capture instead of a real jmp_buf. */
struct exc_data32 {
   uint64_t regs[6];      /* rbx rsi rdi rbp rsp eip — written by ____setjmp */
   uint32_t magic;        /* EXC32_MAGIC when regs[] is ours */
   uint32_t pad[5];       /* rest of buf[18] */
   uint32_t pointers[4];  /* [0]=thrown id32  [1]=chain next (i386 ptr) */
};
_Static_assert(sizeof(struct exc_data32) == 88, "exc_data32 layout");

extern void x64_exc_longjmp(uint64_t *regs, int val) __attribute__((noreturn));

/* This thread's chain-top slot in the shared ctrl (cross-copy, see above). */
static uint32_t *exc_top_slot(void) {
   arena_init();
   if (!g_ctrl) { return NULL; }
   const uint64_t tid = super_hint_tid();
   const unsigned start = (unsigned)(tid % EXC_CHAIN_SLOTS);
   for (unsigned k = 0; k < EXC_CHAIN_SLOTS; ++k) {
      const unsigned i = (start + k) % EXC_CHAIN_SLOTS;
      if (g_ctrl->exc_chain[i].tid == tid) { return &g_ctrl->exc_chain[i].top; }
   }
   for (unsigned k = 0; k < EXC_CHAIN_SLOTS; ++k) {
      const unsigned i = (start + k) % EXC_CHAIN_SLOTS;
      const uint64_t old = g_ctrl->exc_chain[i].tid;
      if (g_ctrl->exc_chain[i].top == 0 &&
          __sync_bool_compare_and_swap(&g_ctrl->exc_chain[i].tid, old, tid)) {
         return &g_ctrl->exc_chain[i].top;
      }
   }
   fprintf(stderr, "[exc1] no free exception-chain slot (>%d threads in "
           "NS_DURING)\n", EXC_CHAIN_SLOTS);
   fflush(stderr);
   return NULL;
}

uint32_t shim_objc_exception_try_enter(uint32_t *a) {
   struct exc_data32 *d = (struct exc_data32 *)(uintptr_t)a[0];
   uint32_t *top = exc_top_slot();
   if (!d || !top) { return 0; }
   /* Snapshot the reverse-stash depth NOW (immediately before the matching
    * _setjmp; no reverse-IMP runs in between). A throw that longjmps back here
    * rewinds the stash to this depth — see x64_rstash_set_depth. Stored in a
    * free buf[18] slot that ____setjmp does not overwrite. */
   d->pad[0] = x64_rstash_depth();
   d->pointers[1] = *top;
   *top = a[0];
   return 0;
}

uint32_t shim_objc_exception_try_exit(uint32_t *a) {
   struct exc_data32 *d = (struct exc_data32 *)(uintptr_t)a[0];
   uint32_t *top = exc_top_slot();
   if (!d || !top) { return 0; }
   *top = d->pointers[1];
   return 0;
}

uint32_t shim_objc_exception_extract(uint32_t *a) {
   struct exc_data32 *d = (struct exc_data32 *)(uintptr_t)a[0];
   return d ? d->pointers[0] : 0;
}

/* ----------------------------------------------------------------------
 * Classic Foundation NS_DURING (pre-@try SDKs — Quinn, Civ IV, and every
 * i386 app built before the fragile runtime switched NS_DURING to expand to
 * objc_exception_try_enter). NS_DURING/NS_HANDLER emitted the OLDER
 * NSHandler2 spelling:
 *
 *     NSHandler2 h;                    // >= 88 bytes, on the caller's stack
 *     _NSAddHandler2(&h);              // register (== objc_exception_try_enter)
 *     if (_setjmp(&h) == 0) {          // jmp_buf at offset 0 of NSHandler2
 *         <body>
 *         _NSRemoveHandler2(&h);       // normal exit (== try_exit)
 *     } else {
 *         id e = _NSExceptionObjectFromHandler2(&h);  // (== extract)
 *         <handler, reads e name/reason>
 *     }
 *
 * This is the SAME per-thread setjmp-exception model as objc_exception_*,
 * just an older name: the app passes &h to BOTH _NSAddHandler2 and _setjmp
 * (verified in -[ATAnimation runFrom:to:]: `lea -0x70(%ebp),%ebx` fed to
 * both), so the jmp_buf lives at offset 0 exactly like _objc_exception_data,
 * and our exc_data32 layout (regs@0, magic@48, rstash@52, thrown@72,
 * chain-next@76 = 88 bytes) fits inside the >=88-byte NSHandler2. Routing
 * both spellings through the ONE per-thread exc_chain lets a single
 * shim_objc_exception_throw longjmp back to whichever frame (NSHandler2 or
 * @try) is on top — nesting the two mechanisms works for free.
 *
 * Unshimmed, `_NSAddHandler2` bound to NATIVE modern Foundation, which reads
 * the handler ptr from %rdi (i386 passed it on the stack) and whose 8-byte
 * `ret` over-pops the i386 4-byte return frame -> fused-PC SIGSEGV, fired
 * even when NO exception is thrown (the Quinn play crash, r11=_NSAddHandler2).
 * ---------------------------------------------------------------------- */
uint32_t shim_NSAddHandler2(uint32_t *a) {
   return shim_objc_exception_try_enter(a);
}
uint32_t shim_NSRemoveHandler2(uint32_t *a) {
   return shim_objc_exception_try_exit(a);
}
uint32_t shim_NSExceptionObjectFromHandler2(uint32_t *a) {
   return shim_objc_exception_extract(a);
}

/* int objc_exception_match(Class cls, id exception) — isKindOf walk. */
uint32_t shim_objc_exception_match(uint32_t *a) {
   id cls = resolve_self(a[0]);
   id exc = resolve_self(a[1]);
   if (!cls || !exc) { return 0; }
   for (Class c = object_getClass(exc); c; c = class_getSuperclass(c)) {
      if (c == (Class)cls) { return 1; }
   }
   return 0;
}

extern void objc_exception_throw(id exception);

uint32_t shim_objc_exception_throw(uint32_t *a) {
   const uint32_t exc32 = a[0];
   const int trace = BRIDGE_TRACE();
   uint32_t *top = exc_top_slot();
   if (top && *top) {
      struct exc_data32 *d = (struct exc_data32 *)(uintptr_t)*top;
      *top = d->pointers[1];                /* pop: handler runs un-entered */
      d->pointers[0] = exc32;
      if (d->magic == EXC32_MAGIC) {
         /* Rewind the reverse-stash to its depth at the handler's try_enter:
          * the longjmp abandons every reverse-IMP frame pushed since, none of
          * which will run its %%back/unstash, so their stale stash pairs must
          * be dropped here or the next unstash returns onto a wrong rsp. */
         x64_rstash_set_depth(d->pad[0]);
         if (trace) {
            fprintf(stderr, "[exc1] throw exc32=0x%08x -> longjmp data=%p "
                    "(rstash rewind -> %u)\n", exc32, (void *)d, d->pad[0]);
            fflush(stderr);
         }
         x64_exc_longjmp(d->regs, 1);       /* noreturn */
      }
      fprintf(stderr, "[exc1] throw: _objc_exception_data %p not written by "
              "our _setjmp (magic=0x%08x) — falling through to native throw\n",
              (void *)d, d->magic);
      fflush(stderr);
   }
   id real = resolve_self(exc32);
   fprintf(stderr, "[exc1] throw exc32=0x%08x real=%p with EMPTY legacy chain "
           "-> native objc_exception_throw (likely fatal)\n",
           exc32, (void *)real);
   fflush(stderr);
   objc_exception_throw(real);
   __builtin_unreachable();
}

/* struct objc_method_list *class_nextMethodList(Class, void **) — ObjC1
 * method-list iterator. No caller has surfaced yet; report an empty list and
 * trace so a real consumer is visible immediately. */
uint32_t shim_class_nextMethodList(uint32_t *a) {
   fprintf(stderr, "[exc1] class_nextMethodList(cls32=0x%08x) unimplemented "
           "-> NULL (no method lists)\n", a[0]);
   fflush(stderr);
   return 0;
}

/* id _objc_setNilReceiver(id) — nil-message hook, removed from the modern
 * runtime. Accept and discard; previous receiver was always nil. */
uint32_t shim_objc_setNilReceiver(uint32_t *a) {
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[exc1] _objc_setNilReceiver(0x%08x) ignored\n", a[0]);
      fflush(stderr);
   }
   return 0;
}

/* Target of the `id (*_dealloc)(id)` vector: terminal instance free, the old
 * runtime's _internal_object_dispose. The instance is a reverse-bridged REAL
 * object (legacy classes are registered with the modern runtime), so dispose
 * the real peer without re-running -dealloc. */
uint32_t shim_dealloc_vec(uint32_t *a) {
   id real = resolve_self(a[0]);
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[exc1] (*_dealloc)(0x%08x) -> real %p object_dispose\n",
              a[0], (void *)real);
      fflush(stderr);
   }
   if (real) { object_dispose(real); }
   return 0;
}

/* The vector itself: i386 code movl-loads the 4-byte function pointer through
 * its (interposed) __dealloc non-lazy bind and calls it, or overwrites it
 * with a hook of its own (which the modern runtime simply never consults).
 * Both the variable and the trampoline live in libabiconv = low-4GB. */
extern void x64_dealloc_vec_tramp(void);
uint32_t ___dealloc = 0;            /* exported as ____dealloc */

/* AppKit/Foundation/QuartzCore methods with CGFloat explicit args — on i386 a
 * 4-byte float, on x86_64 an 8-byte double. The translated app only CALLS these
 * (it doesn't declare/override them), so the seltypes registry — which learns
 * i386 widths from the app's own __OBJC metadata — never sees them, and the
 * forward bridge falls back to the NATIVE 'd' encoding (CGFloat and double are
 * BOTH 'd' there) and reads 8 bytes off the i386 frame (a 4-byte float + 4 bytes
 * of the NEXT slot) -> a fused garbage double AND a one-slot misalignment of
 * every following arg. iPhoto: -[NSTableView setRowHeight:] got -6.9e38,
 * collapsing the main library window to 0x0; iWeb:
 * +[NSRulerView registerUnitWithName:...unitToPointsConversionFactor:...] got a
 * denormal ~0 conversion factor (the CGFloat is arg2, BETWEEN object args, so
 * the fix MUST be positional) -> "Registration information not complete or
 * valid" uncaught exception. The bitmask (bit k => explicit arg k is CGFloat)
 * makes the bridge read each marked arg as ONE 4-byte i386 float and
 * cvtss2sd-widen it, at any position in any-arity method.
 *
 * Keyed by BARE SEL (applies to every class's same-named method), so list only
 * selectors whose marked args are unambiguously CGFloat across ALL frameworks —
 * never a double/NSTimeInterval/NSInteger elsewhere. This is a curated
 * STRUCTURAL seed; the complete fix (eliminating the list) is to seed the
 * seltypes registry from the i386 system frameworks' own __OBJC metadata. */
static const struct { const char *name; uint32_t mask; } g_cgfloat_sels[] = {
   { "setRowHeight:",            0x1 },  /* NSTableView / NSOutlineView (grid + source list) */
   { "setIndentationPerLevel:",  0x1 },  /* NSOutlineView              (source list)         */
   { "setAlphaValue:",           0x1 },  /* NSView / NSWindow / NSCell                        */
   { "setLineWidth:",            0x1 },  /* NSBezierPath                                      */
   { "registerUnitWithName:abbreviation:unitToPointsConversionFactor:"
     "stepUpCycle:stepDownCycle:", 0x4 },/* +[NSRulerView ...]: arg2 is the CGFloat factor   */
   { "colorWithCalibratedRed:green:blue:alpha:", 0xF }, /* +[NSColor ...]: 4 CGFloats        */
   { "colorWithDeviceRed:green:blue:alpha:",     0xF }, /* +[NSColor ...]: 4 CGFloats        */
   /* NSColor WHITE/HSB/component creators — same all-CGFloat shape as the RGBA
    * creators above, and the SAME standalone-'d' denormal~0 failure the app
    * never declares. -[LCDCell] builds its digit ON colour (glowing light) and
    * OFF/ghost colour via +[NSColor colorWithCalibratedWhite:alpha:] and its
    * labels' light-gray text likewise -> unmasked, both CGFloats read as fused
    * garbage doubles -> a black/transparent colour -> Quinn's LCD lit digits
    * rendered DARK, the faint unlit-ghost segments collapsed to alpha~0
    * (invisible), and the NEXT/SCORE/LINES/LEVEL/LPM labels went invisible
    * (light text -> dark on the dark box). Guard: tests-i386 lcd-color. */
   { "colorWithCalibratedWhite:alpha:",          0x3 }, /* +[NSColor ...]: white, alpha      */
   { "colorWithDeviceWhite:alpha:",              0x3 }, /* +[NSColor ...]: white, alpha      */
   { "colorWithWhite:alpha:",                    0x3 }, /* +[NSColor ...] (10.9+ generic)    */
   { "colorWithCalibratedHue:saturation:brightness:alpha:", 0xF }, /* +[NSColor ...]: 4 CGFloats */
   { "colorWithDeviceHue:saturation:brightness:alpha:",     0xF }, /* +[NSColor ...]: 4 CGFloats */
   { "colorWithHue:saturation:brightness:alpha:",           0xF }, /* +[NSColor ...] (generic)  */
   { "colorWithAlphaComponent:",                 0x1 }, /* -[NSColor ...]: single CGFloat    */
   { "highlightWithLevel:",                      0x1 }, /* -[NSColor ...]                    */
   { "shadowWithLevel:",                         0x1 }, /* -[NSColor ...]                    */
   { "blendedColorWithFraction:ofColor:",        0x1 }, /* -[NSColor ...]: CGFloat then obj  */
   { "scaleBy:",                 0x1 },  /* -[NSAffineTransform ...]                          */
   { "scaleXBy:yBy:",            0x3 },  /* -[NSAffineTransform ...]: 2 CGFloats              */
   { "translateXBy:yBy:",        0x3 },  /* -[NSAffineTransform ...]: 2 CGFloats              */
   { "rotateByDegrees:",         0x1 },  /* -[NSAffineTransform ...]                          */
   /* -[NSImage] compositing family: a CGFloat `fraction:` (opacity) at the END,
    * AFTER CGPoint/CGRect struct args. Standalone 'd' the registry never learns
    * (NSImage isn't declared by the app) -> read as an 8-byte double = a denormal
    * ~0 -> the image composites fully TRANSPARENT. Quinn renders each falling/
    * placed Tetris piece into an offscreen NSImage, then composites it with
    * `drawAtPoint:fromRect:operation:fraction:(boardOpacity)` -> fraction~0 -> the
    * blocks drew INVISIBLY (board chrome, drawn separately, stayed visible). The
    * `fraction:` index varies by arity; masks below mark exactly that arg. */
   { "drawAtPoint:fromRect:operation:fraction:",       0x8 }, /* pt,rect,op,FRAC -> arg3 */
   { "drawInRect:fromRect:operation:fraction:",        0x8 }, /* rect,rect,op,FRAC -> arg3 */
   { "compositeToPoint:fromRect:operation:fraction:",  0x8 }, /* pt,rect,op,FRAC -> arg3 */
   { "compositeToPoint:operation:fraction:",           0x4 }, /* pt,op,FRAC -> arg2 */
   { "dissolveToPoint:fromRect:fraction:",             0x4 }, /* pt,rect,FRAC -> arg2 */
   { "dissolveToPoint:fraction:",                      0x2 }, /* pt,FRAC -> arg1 */
   /* Core Image value-object creators: all-CGFloat arg lists (same denormal~0
    * mechanism as the fraction: family above — unmasked they read as 8-byte
    * doubles -> ~0). A transparent CIColor turns CIConstantColorGenerator +
    * CISourceInCompositing composites fully EMPTY; a zero CIVector collapses
    * CILinearGradient endpoints / CIColorMatrix vectors. (Quinn splash+board
    * paint everything through exactly these: logo, credits, background
    * gradient, opacity fades — all-black content areas, correct extents.) */
   { "colorWithRed:green:blue:alpha:",  0xF }, /* +[CIColor / NSColor ...] */
   { "colorWithRed:green:blue:",        0x7 }, /* +[CIColor ...]           */
   { "vectorWithX:",                    0x1 }, /* +[CIVector ...]          */
   { "vectorWithX:Y:",                  0x3 }, /* +[CIVector ...]          */
   { "vectorWithX:Y:Z:",                0x7 }, /* +[CIVector ...]          */
   { "vectorWithX:Y:Z:W:",              0xF }, /* +[CIVector ...]          */
   /* NSFont size-taking creators (CGFloat point size). Unmasked, the native
    * 'd' reads TWO i386 slots from the caller's ONE float slot -> a denormal
    * ~0-point font -> zero-size text -> "Cannot lock focus on image ... size
    * zero" NSImageCacheException (Quinn pausedImage lane). */
   { "fontWithName:size:",          0x2 },
   { "fontWithDescriptor:size:",    0x2 },
   { "convertFont:toSize:",         0x2 }, /* -[NSFontManager ...] */
   { "systemFontOfSize:",           0x1 },
   { "boldSystemFontOfSize:",       0x1 },
   { "userFontOfSize:",             0x1 },
   { "userFixedPitchFontOfSize:",   0x1 },
   { "labelFontOfSize:",            0x1 },
   { "menuFontOfSize:",             0x1 },
   { "menuBarFontOfSize:",          0x1 },
   { "messageFontOfSize:",          0x1 },
   { "paletteFontOfSize:",          0x1 },
   { "titleBarFontOfSize:",         0x1 },
   { "toolTipsFontOfSize:",         0x1 },
   { "controlContentFontOfSize:",   0x1 },
};

__attribute__((constructor))
static void x64_init_objc1_compat(void) {
   ___dealloc = (uint32_t)(uintptr_t)&x64_dealloc_vec_tramp;
   for (unsigned i = 0; i < sizeof g_cgfloat_sels / sizeof g_cgfloat_sels[0]; ++i) {
      cgfloat_mask_insert(sel_registerName(g_cgfloat_sels[i].name),
                          g_cgfloat_sels[i].mask);
   }
}

/* ======================================================================
 * ObjC runtime C functions called directly by translated code (iPhoto 29th
 * blocker: object_getClass reached the NATIVE entry with i386 stack args ->
 * native read a stale/fused rdi). libobjc was never in abigen's consider set
 * (its types need bridge-aware marshalling, not deep copies), so every
 * direct runtime call had this hazard. Uniform i386-cdecl shims for the
 * whole imported set (trampolines in maptable_tramp.asm; static-interpose
 * redirects the binds on the next translate).
 *
 * Conventions, mirroring the msgSend bridge:
 *   id/Class args   -> resolve_self (handle / shadow / raw / class-name)
 *   id/Class rets   -> x64_objc_wrap (32-bit arena handle, deduped)
 *   SEL args        -> resolve_sel (i386 SEL == selector-name cstring ptr)
 *   SEL rets        -> x64_objc_sel_wrap (low name ptr; identity for low in)
 *   const char* ret -> x64_objc_bounce_cstr (low ring copy)
 *   Method/Ivar     -> arena handles (dedup map makes equality compares work)
 *   malloc'd arrays -> shim malloc (low-4GB) of 4-byte slots; caller free()s
 *                      through the interposed free, which is the same heap.
 * ====================================================================== */

extern void _objc_flush_caches(Class cls);
extern id objc_retain(id obj);
extern int objc_sync_enter(id obj);   /* objc/objc-sync.h */
extern int objc_sync_exit(id obj);

uint32_t shim_object_getClass(uint32_t *a) {
   id real = resolve_self(a[0]);
   return real ? x64_objc_wrap((uint64_t)(uintptr_t)object_getClass(real)) : 0;
}

uint32_t shim_objc_getClass(uint32_t *a) {
   if (!legacy_cstr_ok(a[0])) { return 0; }
   Class c = objc_getClass((const char *)(uintptr_t)a[0]);
   return c ? x64_objc_wrap((uint64_t)(uintptr_t)c) : 0;
}

uint32_t shim_class_getSuperclass(uint32_t *a) {
   id cls = resolve_self(a[0]);
   if (!cls) { return 0; }
   Class s = class_getSuperclass((Class)cls);
   return s ? x64_objc_wrap((uint64_t)(uintptr_t)s) : 0;
}

uint32_t shim_class_getName(uint32_t *a) {
   id cls = resolve_self(a[0]);
   return cls ? x64_objc_bounce_cstr(class_getName((Class)cls)) : 0;
}

uint32_t shim_object_getClassName(uint32_t *a) {
   id real = resolve_self(a[0]);
   return real ? x64_objc_bounce_cstr(object_getClassName(real)) : 0;
}

uint32_t shim_class_isMetaClass(uint32_t *a) {
   id cls = resolve_self(a[0]);
   return cls ? (uint32_t)class_isMetaClass((Class)cls) : 0;
}

uint32_t shim_object_isClass(uint32_t *a) {
   id obj = resolve_self(a[0]);
   return obj ? (uint32_t)object_isClass(obj) : 0;
}

uint32_t shim_object_setClass(uint32_t *a) {
   id obj = resolve_self(a[0]);
   id cls = resolve_self(a[1]);
   if (!obj || !cls) { return 0; }
   Class old = object_setClass(obj, (Class)cls);
   return old ? x64_objc_wrap((uint64_t)(uintptr_t)old) : 0;
}

uint32_t shim_objc_retain(uint32_t *a) {
   id real = resolve_self(a[0]);
   if (real) { objc_retain(real); }
   return a[0];                       /* objc_retain returns its argument */
}

uint32_t shim_objc_sync_enter(uint32_t *a) {
   id real = resolve_self(a[0]);
   return real ? (uint32_t)objc_sync_enter(real) : 0;
}

uint32_t shim_objc_sync_exit(uint32_t *a) {
   id real = resolve_self(a[0]);
   return real ? (uint32_t)objc_sync_exit(real) : 0;
}

uint32_t shim_objc_setAssociatedObject(uint32_t *a) {
   id obj = resolve_self(a[0]);
   id val = resolve_self(a[2]);       /* nil is fine (clears) */
   if (!obj) { return 0; }
   /* key: any i386 pointer, used opaquely; consistent as long as the same
    * i386 code passes the same low address. */
   objc_setAssociatedObject(obj, (const void *)(uintptr_t)a[1], val,
                            (objc_AssociationPolicy)a[3]);
   return 0;
}

uint32_t shim_objc_getAssociatedObject(uint32_t *a) {
   id obj = resolve_self(a[0]);
   if (!obj) { return 0; }
   id v = objc_getAssociatedObject(obj, (const void *)(uintptr_t)a[1]);
   return v ? x64_objc_wrap((uint64_t)(uintptr_t)v) : 0;
}

uint32_t shim_sel_registerName(uint32_t *a) {
   if (!legacy_cstr_ok(a[0])) { return 0; }
   sel_registerName((const char *)(uintptr_t)a[0]);  /* ensure registered */
   return a[0];                       /* i386 SEL == the name pointer */
}

uint32_t shim_sel_getName(uint32_t *a) {
   return legacy_cstr_ok(a[0]) ? a[0] : 0;  /* identity: SEL IS a name ptr */
}

uint32_t shim_sel_isEqual(uint32_t *a) {
   if (a[0] == a[1]) { return 1; }
   SEL s1 = resolve_sel(a[0]);
   SEL s2 = resolve_sel(a[1]);
   return (uint32_t)(s1 && s1 == s2);
}

uint32_t shim_objc_flush_caches(uint32_t *a) {
   id cls = resolve_self(a[0]);
   if (cls) { _objc_flush_caches((Class)cls); }
   return 0;
}

uint32_t shim_class_createInstance(uint32_t *a) {
   id cls = resolve_self(a[0]);
   if (!cls) { return 0; }
   id obj = class_createInstance((Class)cls, (size_t)a[1]);
   return obj ? x64_objc_wrap((uint64_t)(uintptr_t)obj) : 0;
}

uint32_t shim_objc_allocateClassPair(uint32_t *a) {
   id super = resolve_self(a[0]);     /* Nil superclass is legal */
   if (!legacy_cstr_ok(a[1])) { return 0; }
   Class c = objc_allocateClassPair((Class)super,
                                    (const char *)(uintptr_t)a[1],
                                    (size_t)a[2]);
   return c ? x64_objc_wrap((uint64_t)(uintptr_t)c) : 0;
}

uint32_t shim_objc_registerClassPair(uint32_t *a) {
   id cls = resolve_self(a[0]);
   if (cls) { objc_registerClassPair((Class)cls); }
   return 0;
}

/* BOOL class_addMethod(Class, SEL, IMP, const char *types) — the IMP is an
 * i386 function pointer: register the generic reverse trampoline exactly as
 * legacy-image registration does and record (class,sel)->imp32 in the
 * reverse-method map + the i386 encoding in the seltypes registry. */
uint32_t shim_class_addMethod(uint32_t *a) {
   id cls = resolve_self(a[0]);
   SEL sel = resolve_sel(a[1]);
   const uint32_t imp32 = a[2];
   const char *types = legacy_cstr_ok(a[3])
      ? (const char *)(uintptr_t)a[3] : "v8@0:4";
   if (!cls || !sel || !imp32) { return 0; }
   IMP tramp = enc_ret_is_stret(types) ? (IMP)_86x64_reverse_imp_stret
                                       : (IMP)_86x64_reverse_imp;
   BOOL ok = class_addMethod((Class)cls, sel, tramp, types);
   rmeth_insert((Class)cls, sel, imp32, types);
   seltypes_insert(sel, types);
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[rt] class_addMethod %s %s imp32=0x%08x -> %d\n",
              class_getName((Class)cls), sel_getName(sel), imp32, (int)ok);
      fflush(stderr);
   }
   return (uint32_t)ok;
}

/* The i386 swizzle helpers (SFReplaceMethodImplementation*, and method-
 * swizzling code generally) read a Method's fragile ObjC1 layout directly —
 * {SEL name@0; char *types@4; IMP imp@8}, all 32-bit — and even WRITE the imp
 * field to install a new implementation. A modern Method is opaque with a
 * different 64-bit layout, and our arena handle is just an 8-byte slot;
 * dereferencing either at offset 8 crashes (the observed NSClipView
 * poseAsClass:/swizzle blocker). So class_get{Instance,Class}Method hand back a
 * pointer to a synthetic low-4GB struct in i386 layout. Reads see a plausible
 * {name,types,imp}; the swizzle's write to imp lands in our scratch — the
 * effect on the real runtime is a no-op, consistent with poseAsClass: itself
 * already being a no-op on the modern runtime. Cached per real Method so the
 * two lookups a swizzle performs return stable, comparable pointers. */
struct i386_method32 { uint32_t name; uint32_t types; uint32_t imp; };

#define I386METH_CAP 8192u
static Method   g_i386meth_key[I386METH_CAP];
static uint32_t g_i386meth_val[I386METH_CAP];
static Class    g_i386meth_cls[I386METH_CAP];   /* class this method was queried on */
static uint32_t g_i386meth_cnt;

static uint32_t i386_method_wrap(Method m, Class queried) {
   if (!m) { return 0; }
   for (uint32_t i = 0; i < g_i386meth_cnt; ++i) {
      if (g_i386meth_key[i] == m) {
         if (!g_i386meth_cls[i] && queried) { g_i386meth_cls[i] = queried; }
         return g_i386meth_val[i];
      }
   }
   struct i386_method32 *s = (struct i386_method32 *)malloc(sizeof(*s));
   if (!s || (uintptr_t)s >= 0x100000000UL) { return 0; }
   s->name  = x64_objc_sel_wrap((uint64_t)(uintptr_t)method_getName(m));
   s->types = x64_objc_bounce_cstr(method_getTypeEncoding(m));
   s->imp   = x64_objc_wrap((uint64_t)(uintptr_t)method_getImplementation(m));
   if (g_i386meth_cnt < I386METH_CAP) {
      g_i386meth_key[g_i386meth_cnt] = m;
      g_i386meth_val[g_i386meth_cnt] = (uint32_t)(uintptr_t)s;
      g_i386meth_cls[g_i386meth_cnt] = queried;
      g_i386meth_cnt++;
   }
   return (uint32_t)(uintptr_t)s;
}

/* The class a synthetic i386_method32 handle was queried on (class_get*Method),
 * for recovering the (class,sel) key the reverse-IMP map uses. NULL if the
 * handle came from copyMethodList (no class context) or isn't ours. */
static Class i386_method_queried_class(uint32_t v) {
   for (uint32_t i = 0; i < g_i386meth_cnt; ++i) {
      if (g_i386meth_val[i] == v) { return g_i386meth_cls[i]; }
   }
   return NULL;
}

/* TOMBSTONE Method structs: class_get{Instance,Class}Method must hand i386
 * code a NON-NULL pointer even for an absent method, because some legacy
 * swizzlers (e.g. iWeb's SFReplaceMethodImplementationWithSelectorOnClass,
 * which swizzles the now-missing private WebKit class WebClipView) deref the
 * returned Method UNGUARDED (`movl 0x8(%rax),%esi`). A tombstone is a synthetic
 * i386_method32 with imp==0/types==0 so the deref reads 0 -> the swizzle no-ops
 * and the caller's graceful "couldn't fix" path runs. The lie is bounded: every
 * Method i386 code sees is already a synthetic copy and method_setImplementation
 * is a no-op, so a tombstone cannot corrupt any real libobjc method; the only
 * residual is that class_get*Method(c,s)!=NULL is no longer a valid existence
 * test. We track tombstones so method_get{Name,Implementation,TypeEncoding}
 * report them as absent (return 0). Deduped per selector handle. */
#define I386TOMB_CAP 4096u
static uint32_t g_tomb_sel[I386TOMB_CAP];
static uint32_t g_tomb_val[I386TOMB_CAP];
static uint32_t g_tomb_cnt;

static int i386_method_is_tombstone(uint32_t v) {
   for (uint32_t i = 0; i < g_tomb_cnt; ++i) {
      if (g_tomb_val[i] == v) { return 1; }
   }
   return 0;
}

static uint32_t i386_method_tombstone(SEL sel) {
   uint32_t selh = sel ? x64_objc_sel_wrap((uint64_t)(uintptr_t)sel) : 0;
   for (uint32_t i = 0; i < g_tomb_cnt; ++i) {
      if (g_tomb_sel[i] == selh) { return g_tomb_val[i]; }
   }
   struct i386_method32 *s = (struct i386_method32 *)malloc(sizeof(*s));
   if (!s || (uintptr_t)s >= 0x100000000UL) { return 0; }
   s->name  = selh;   /* a real sel handle, so name queries still work */
   s->types = 0;
   s->imp   = 0;      /* the unguarded `0x8(%rax)` deref reads 0 -> no-op */
   if (g_tomb_cnt < I386TOMB_CAP) {
      g_tomb_sel[g_tomb_cnt] = selh;
      g_tomb_val[g_tomb_cnt] = (uint32_t)(uintptr_t)s;
      g_tomb_cnt++;
   }
   return (uint32_t)(uintptr_t)s;
}

/* Inverse of i386_method_wrap: map a synthetic i386_method32 pointer back to
 * the real Method. The method_* shims accept whatever class_get*Method handed
 * the i386 code, so recognise our synthetic pointers; fall back to the legacy
 * arena-handle unwrap for any other value. Tombstones map back to NULL (absent),
 * so the method_* shims report them as a missing method. */
static Method i386_method_unwrap(uint32_t v) {
   if (!v) { return NULL; }
   for (uint32_t i = 0; i < g_i386meth_cnt; ++i) {
      if (g_i386meth_val[i] == v) { return g_i386meth_key[i]; }
   }
   if (i386_method_is_tombstone(v)) { return NULL; }
   return (Method)(uintptr_t)x64_objc_unwrap(v);
}

uint32_t shim_class_getInstanceMethod(uint32_t *a) {
   id cls = resolve_self(a[0]);
   SEL sel = resolve_sel(a[1]);
   Method m = (cls && sel) ? class_getInstanceMethod((Class)cls, sel) : NULL;
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[rt] class_getInstanceMethod a0=0x%08x a1=0x%08x "
              "cls=%s sel=%s -> m=%p\n", a[0], a[1],
              cls ? class_getName((Class)cls) : "(nil)",
              sel ? sel_getName(sel) : "(nil)", (void *)m);
      fflush(stderr);
   }
   /* Absent method on a valid class+sel -> hand back a tombstone (non-NULL,
    * imp==0) so an unguarded i386 swizzler deref no-ops instead of faulting. */
   if (!m && cls && sel) { return i386_method_tombstone(sel); }
   return i386_method_wrap(m, (Class)cls);
}

uint32_t shim_class_getClassMethod(uint32_t *a) {
   id cls = resolve_self(a[0]);
   SEL sel = resolve_sel(a[1]);
   Method m = (cls && sel) ? class_getClassMethod((Class)cls, sel) : NULL;
   if (!m && cls && sel) { return i386_method_tombstone(sel); }
   return i386_method_wrap(m, object_getClass((id)cls));
}

uint32_t shim_class_copyMethodList(uint32_t *a) {
   id cls = resolve_self(a[0]);
   unsigned n = 0;
   Method *list = cls ? class_copyMethodList((Class)cls, &n) : NULL;
   uint32_t *out = NULL;
   if (list && n) {
      out = (uint32_t *)malloc((n + 1) * sizeof(uint32_t)); /* low heap */
      if (out) {
         for (unsigned i = 0; i < n; ++i) {
            out[i] = x64_objc_wrap((uint64_t)(uintptr_t)list[i]);
         }
         out[n] = 0;
      }
   }
   free(list);
   if (a[1] && ptr_ok(a[1], 4)) { *(uint32_t *)(uintptr_t)a[1] = out ? n : 0; }
   return (uint32_t)(uintptr_t)out;
}

uint32_t shim_method_getName(uint32_t *a) {
   Method m = i386_method_unwrap(a[0]);
   return m ? x64_objc_sel_wrap((uint64_t)(uintptr_t)method_getName(m)) : 0;
}

uint32_t shim_method_getTypeEncoding(uint32_t *a) {
   Method m = i386_method_unwrap(a[0]);
   return m ? x64_objc_bounce_cstr(method_getTypeEncoding(m)) : 0;
}

uint32_t shim_method_getNumberOfArguments(uint32_t *a) {
   Method m = i386_method_unwrap(a[0]);
   return m ? method_getNumberOfArguments(m) : 0;
}

/* The returned IMP is a HANDLE: equality compares work (dedup map), calling
 * it from i386 code would not. Trace so a calling consumer is visible. */
uint32_t shim_method_getImplementation(uint32_t *a) {
   Method m = i386_method_unwrap(a[0]);
   if (!m) { return 0; }
   if (BRIDGE_TRACE()) {
      fprintf(stderr, "[rt] method_getImplementation(0x%08x) -> handle "
              "(compare-only)\n", a[0]);
      fflush(stderr);
   }
   return x64_objc_wrap((uint64_t)(uintptr_t)method_getImplementation(m));
}

/* Replacing a native Method's IMP with an i386 one needs a (class,sel) keyed
 * reverse entry, which a bare Method does not identify. Unimplemented. */
uint32_t shim_method_setImplementation(uint32_t *a) {
   fprintf(stderr, "[rt] method_setImplementation(m32=0x%08x, imp32=0x%08x) "
           "UNIMPLEMENTED -> 0\n", a[0], a[1]);
   fflush(stderr);
   return 0;
}

/* method_exchangeImplementations(Method a, Method b) — atomically swap two
 * methods' implementations (the textbook swizzle primitive).
 *
 * For reverse-registered LEGACY methods the modern Method's IMP is the shared
 * _86x64_reverse_imp trampoline, which re-derives the i386 IMP from (class,sel)
 * at dispatch — so the bare runtime exchange is a NO-OP (both slots hold the
 * same trampoline) and, reached natively with our synthetic 12-byte
 * i386_method32 args reinterpreted as 64-bit objc_method structs, it reads/
 * writes the imp field at offset 16 PAST the struct and corrupts the heap — the
 * observed swizzle SIGSEGV. The fix swaps the authoritative (class,sel)->legacy-
 * IMP entries in g_rmeth so SEL-keyed dispatch honours the exchange, and ALSO
 * runs the real exchange so the trampoline KIND (plain vs _stret) and libobjc's
 * own Method/IMP identity follow. Native<->native exchanges fall straight
 * through to the real runtime. Involutive: a second exchange restores both. */
uint32_t shim_method_exchangeImplementations(uint32_t *a) {
   Method m1 = i386_method_unwrap(a[0]);
   Method m2 = i386_method_unwrap(a[1]);
   if (!m1 || !m2 || m1 == m2) { return 0; }
   SEL s1 = method_getName(m1), s2 = method_getName(m2);
   Class q1 = i386_method_queried_class(a[0]);
   Class q2 = i386_method_queried_class(a[1]);
   struct rmeth_ent *e1 = (q1 && s1) ? rmeth_find_owner(q1, s1) : NULL;
   struct rmeth_ent *e2 = (q2 && s2) ? rmeth_find_owner(q2, s2) : NULL;
   if (e1 && e2) {
      uint64_t ti = e1->imp; const char *tt = e1->types;
      e1->imp = e2->imp; e1->types = e2->types;
      e2->imp = ti;       e2->types = tt;
      if (BRIDGE_TRACE()) {
         fprintf(stderr, "[rt] method_exchangeImplementations legacy %s <-> %s\n",
                 s1 ? sel_getName(s1) : "?", s2 ? sel_getName(s2) : "?");
         fflush(stderr);
      }
   } else if (e1 || e2) {
      /* mixed legacy<->native: routing native dispatch to an i386 IMP (or vice
       * versa) needs more than an imp swap; do the real exchange best-effort and
       * flag it rather than silently mis-dispatch. */
      fprintf(stderr, "[rt] method_exchangeImplementations mixed legacy/native "
              "(%s <-> %s) — best effort\n",
              s1 ? sel_getName(s1) : "?", s2 ? sel_getName(s2) : "?");
      fflush(stderr);
   }
   method_exchangeImplementations(m1, m2);
   return 0;
}

static uint32_t method_copy_type_common(const char *t) {
   if (!t) { return 0; }
   const size_t n = strlen(t) + 1;
   char *low = (char *)malloc(n);           /* shim malloc, low-4GB */
   if (!low) { return 0; }
   memcpy(low, t, n);
   return (uint32_t)(uintptr_t)low;
}

uint32_t shim_method_copyReturnType(uint32_t *a) {
   Method m = i386_method_unwrap(a[0]);
   if (!m) { return 0; }
   char *t = method_copyReturnType(m);
   uint32_t r = method_copy_type_common(t);
   free(t);
   return r;
}

uint32_t shim_method_copyArgumentType(uint32_t *a) {
   Method m = i386_method_unwrap(a[0]);
   if (!m) { return 0; }
   char *t = method_copyArgumentType(m, a[1]);
   uint32_t r = method_copy_type_common(t);
   free(t);
   return r;
}

uint32_t shim_objc_getClassList(uint32_t *a) {
   int total = objc_getClassList(NULL, 0);
   if (a[0] && (int)a[1] > 0 && total > 0 && ptr_ok(a[0], 4)) {
      int want = (int)a[1] < total ? (int)a[1] : total;
      Class *tmp = (Class *)malloc((size_t)total * sizeof(Class));
      if (tmp) {
         total = objc_getClassList(tmp, total);
         if (want > total) { want = total; }
         uint32_t *out = (uint32_t *)(uintptr_t)a[0];
         for (int i = 0; i < want; ++i) {
            out[i] = x64_objc_wrap((uint64_t)(uintptr_t)tmp[i]);
         }
         free(tmp);
      }
   }
   return (uint32_t)total;
}

uint32_t shim_objc_copyClassList(uint32_t *a) {
   unsigned n = 0;
   Class *list = objc_copyClassList(&n);
   uint32_t *out = NULL;
   if (list && n) {
      out = (uint32_t *)malloc((n + 1) * sizeof(uint32_t));
      if (out) {
         for (unsigned i = 0; i < n; ++i) {
            out[i] = x64_objc_wrap((uint64_t)(uintptr_t)list[i]);
         }
         out[n] = 0;
      }
   }
   free(list);
   if (a[0] && ptr_ok(a[0], 4)) { *(uint32_t *)(uintptr_t)a[0] = out ? n : 0; }
   return (uint32_t)(uintptr_t)out;
}

/* TRANSLATED-caller direction of the object_[sg]etInstanceVariable bridge (i386
 * cdecl -> SysV). object_setInstanceVariable is NOT abigen-shimmed (libobjc is
 * not in the consider set), so a translated i386 call to it bound straight to
 * NATIVE libobjc, which read x86_64 REGISTER args -> garbage self/name/value ->
 * EXC_BAD_ACCESS (any fragile-ObjC1 app that connects its own outlets in code —
 * i.e. calls object_setInstanceVariable itself rather than through AppKit's nib
 * connector). object_getInstanceVariable HAS a hand shim but called native
 * libobjc, which MISSES a legacy class's SHADOW ivars (the ivars live in the
 * per-instance i386 shadow, not the modern class) and only traced.
 *
 * Both now route through the SAME legacy-aware interposers the NATIVE-caller
 * direction uses (x64_object_[sg]etInstanceVariable): resolve the i386 self/value
 * handles to their real objects, apply the shadow<->R logic, and re-wrap the
 * results into the i386 representation the translated caller expects. Universal:
 * the twin of the native nib-outlet bridge, for the in-code direction. */
uint32_t shim_object_setInstanceVariable(uint32_t *a) {
   id real = resolve_self(a[0]);
   if (!real || !legacy_cstr_ok(a[1])) { return 0; }
   /* the i386 `value` arg is an i386 representation (shadow / arena handle /
    * raw legacy obj); resolve it to the real object so the interpose stores the
    * correct target (it re-wraps it back into the shadow's i386 handle). */
   void *val = a[2] ? (void *)(uintptr_t)resolve_self(a[2]) : NULL;
   Ivar iv = x64_object_setInstanceVariable(real,
                                            (const char *)(uintptr_t)a[1], val);
   return iv ? x64_objc_wrap((uint64_t)(uintptr_t)iv) : 0;
}

/* Legacy-registered classes carry their ivars in the i386 SHADOW, not the
 * modern class; the legacy-aware interpose reads the shadow (native libobjc
 * would miss it). Route through it, then re-wrap the out-value + returned Ivar
 * into the i386 representation the translated caller expects. */
uint32_t shim_object_getInstanceVariable(uint32_t *a) {
   id real = resolve_self(a[0]);
   if (!real || !legacy_cstr_ok(a[1])) { return 0; }
   void *val = NULL;
   Ivar iv = x64_object_getInstanceVariable(real, (const char *)(uintptr_t)a[1],
                                            &val);
   if (a[2] && ptr_ok(a[2], 4)) {
      uint64_t v = (uint64_t)(uintptr_t)val;
      *(uint32_t *)(uintptr_t)a[2] =
         v >= 0x100000000ULL ? x64_objc_wrap(v) : (uint32_t)v;
   }
   return iv ? x64_objc_wrap((uint64_t)(uintptr_t)iv) : 0;
}
