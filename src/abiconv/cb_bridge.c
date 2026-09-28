/*
 * cb_bridge.c — generic i386-callback -> native-trampoline bridge (the fix
 * for the 22nd-blocker class of crashes).
 *
 * abigen cannot pass a fn-ptr argument through its deep-copy machinery: the
 * value is an i386 CODE address the native API will later CALL with the
 * x86_64 ABI. Instead, the generated shim calls x64_cb_wrap(fn32, sig) here,
 * which binds the i386 callback (+ its signature descriptor, emitted by
 * abigen from the header prototype) to a trampoline slot (cb_tramp.asm) and
 * returns the slot's native entry point. When the native side invokes it,
 * x64_cb_dispatch marshals the native arguments down to an i386 cdecl word
 * frame per the descriptor and re-enters the translated callback through
 * _86x64_call_i386 on a fresh low-4GB stack (fresh per invocation: a callback
 * that pumps the run loop can re-enter the same slot before returning).
 *
 * Pointers from native code fit in 32 bits (the process heap is constrained
 * low-4GB), so plain pointers truncate; objc/CF OBJECT pointers may live high
 * (dyld shared cache constants, tagged pointers) and are wrapped into low
 * arena handles via x64_objc_wrap — the same convention the objc bridge and
 * the data-symbol shadows use, so handles unwrap on the next message send.
 */

#include <os/lock.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include "cb_bridge.h"
extern void *x64_lowstack_get(size_t sz);          /* lowstack_pool.c */
extern void  x64_lowstack_put(void *p, size_t sz);

/* returns the i386 result as edx:eax combined in rax: the low 32 bits are the
 * usual eax result, the high 32 the edx half of a 64-bit (long long) return. */
uint64_t _86x64_call_i386(uint64_t fn, uint64_t nwords,
                          const uint32_t *words, uint64_t lowstack_top);
void     x64_cb_enter(void);               /* objc_shim.c: cross-copy depth */
void     x64_cb_leave(void);

extern const uint64_t x64_cb_tramp_table[];  /* cb_tramp.asm */
extern const uint64_t x64_cb_nslots;

#define CB_LOWSTACK_SZ (4u * 1024u * 1024u)  /* was 256KB: deep native (QuickTime) call chains from callbacks need real stack */

typedef struct {
   uint32_t          fn32;
   const x64_cb_sig *sig;
} cb_binding;

static cb_binding     g_bind[8192];    /* MUST match cb_tramp.asm CB_SLOTS */
static uint32_t       g_nbind;
static os_unfair_lock g_bind_lock = OS_UNFAIR_LOCK_INIT;

/* Bytes actually readable at `p`, capped at `want`: the header-derived pointee
 * size is NOT trustworthy against the runtime, so the bounce must never read
 * past the record's own VM region.
 *
 * ★MEASURED, and this is not hypothetical: abigen parses modern headers, where
 * `struct dirent` is the 64-bit-inode layout of 1048 bytes — but Portal 2
 * imports plain `_scandir` (not `$INODE64`), and modern libSystem still ships
 * that legacy entry returning 268-byte, 32-bit-inode records. Copying the
 * header's 1048 bytes from a 268-byte record over-reads ~780 bytes, which is
 * usually harmless heap and occasionally the end of a mapping. Clamping turns
 * a rare crash into a short copy; the tail is zeroed so the callee never reads
 * undefined bytes either way. */
uint64_t cb_readable_span(uint64_t p, uint64_t want) {
   mach_vm_address_t a = (mach_vm_address_t)p;
   mach_vm_size_t    rsz = 0;
   vm_region_basic_info_data_64_t info;
   mach_msg_type_number_t cnt = VM_REGION_BASIC_INFO_COUNT_64;
   mach_port_t obj = MACH_PORT_NULL;
   if (mach_vm_region(mach_task_self(), &a, &rsz, VM_REGION_BASIC_INFO_64,
                      (vm_region_info_t)&info, &cnt, &obj) != KERN_SUCCESS) {
      return 0;
   }
   if (a > p) { return 0; }                        /* p sits in a hole */
   if ((info.protection & VM_PROT_READ) == 0) { return 0; }
   const uint64_t avail = (uint64_t)a + (uint64_t)rsz - p;
   return want < avail ? want : avail;
}

/* Kill switch for the CBA_PTR_REC bounce (A/B harness): restores the old
 * truncation of a >4GB callback pointer argument. */
static int cb_no_ptr_bounce(void) {
   static int t = -1;
   if (t < 0) { t = getenv("M64_NO_CB_PTR_BOUNCE") != NULL; }
   return t;
}

/* Zero-I/O crash breadcrumb ring. cb_dispatch records each invocation here with
 * plain stores (no fprintf/lock/syscall), so it does NOT perturb the thread
 * timing that the callback-return-to-0 race depends on. A crash handler
 * (geoshim, via dlsym of x64_cb_dump_ring) prints the last entries to identify
 * which callback/stack was live. depth = current native->translated nesting. */
typedef struct {
   uint32_t fn32;
   uint32_t tid;
   uint32_t nargs;
   uint32_t ret_kind;
   uint64_t stk_lo;     /* malloc'd low stack base */
   uint64_t stk_hi;     /* top (frame carve start) */
   uint32_t w;          /* words marshalled */
   uint32_t depth;      /* nesting at entry */
   uint32_t done;       /* 0 = still inside the callback, 1 = returned */
} cb_crumb;
#define CB_RING 64
static cb_crumb       g_ring[CB_RING];
static uint32_t       g_ring_pos;
static int            g_cb_depth;

void x64_cb_dump_ring(void);   /* exported; called from a crash handler */
void x64_cb_dump_ring(void) {
   uint32_t pos = __atomic_load_n(&g_ring_pos, __ATOMIC_SEQ_CST);
   fprintf(stderr, "[cb-ring] last callbacks (newest first), depth=%d:\n",
           __atomic_load_n(&g_cb_depth, __ATOMIC_SEQ_CST));
   for (int i = 0; i < CB_RING; ++i) {
      uint32_t idx = (pos - 1 - i) & (CB_RING - 1);
      cb_crumb *c = &g_ring[idx];
      if (c->fn32 == 0 && c->stk_lo == 0) continue;
      fprintf(stderr, "  [%2d] fn=0x%-8x t=%x d=%u %s nargs=%u ret=%u w=%u "
              "stk=[0x%llx..0x%llx]\n",
              i, c->fn32, c->tid, c->depth, c->done ? "ret " : "IN  ",
              c->nargs, c->ret_kind, c->w,
              (unsigned long long)c->stk_lo, (unsigned long long)c->stk_hi);
   }
   fflush(stderr);
}

/* Bind an i386 callback to a native trampoline. Bindings are immutable and
 * deduped on (fn, sig), so re-registering the same callback (timers
 * recreated per window, etc.) reuses its slot. NULL passes through as NULL
 * (optional callbacks). */
/* If fn32 is the address of one of THIS copy's own callback trampolines,
 * return its slot index (else -1). The program can hand a callback WE gave it
 * (a cb trampoline address) back to another bridged API — a UPP passed on, a
 * proc stored then re-registered. Wrapping such a pointer a SECOND time as if
 * it were i386 code makes cb_dispatch invoke it via _86x64_call_i386, which
 * lays a 4-byte i386 return frame and jmps to the (native) trampoline — whose
 * native 8-byte `ret` then over-pops the frame and fuses the adjacent 4-byte
 * argument (a stack pointer) into the high half of the return PC -> jump to a
 * non-canonical address (Civ IV s28: rip=0x80935a28`02182977, low = the
 * trampoline path's own .cback, high = an i386-stack address). The stubs are
 * contiguous and monotonic (cb_tramp.asm emits _x64_cb_tramp_0..N-1 in order). */
static int cb_tramp_slot(uint32_t fn32) {
   uint64_t f = (uint64_t)fn32;
   uint64_t lo = x64_cb_tramp_table[0];
   uint64_t stride = x64_cb_tramp_table[1] - x64_cb_tramp_table[0];
   uint64_t hi = x64_cb_tramp_table[x64_cb_nslots - 1] + stride;
   if (f < lo || f >= hi || stride == 0) { return -1; }
   return (int)((f - lo) / stride);
}

/* Is fn32 one of THIS copy's native callback trampolines?  Authoritative
 * (tramp-table range) test, exported so native-vs-i386 callback-value
 * classification elsewhere (ae_shim.c's AE handler dispatch) can decide by
 * MECHANISM instead of image-name heuristics. */
int x64_cb_fn_is_tramp(uint32_t fn32) {
   return cb_tramp_slot(fn32) >= 0;
}

uint64_t x64_cb_wrap(uint32_t fn32, const x64_cb_sig *sig) {
   if (fn32 == 0) { return 0; }

   /* Re-registration of a callback we already handed out: bind the ORIGINAL
    * i386 function (recovered from the trampoline's slot) under the CURRENT
    * signature, so dispatch enters real translated code — whose i386 4-byte
    * `ret` idiom is high-half-clean — instead of jmp'ing to a native trampoline
    * through the 4-byte frame that over-pops (s28). Using the current sig (not
    * the slot's original) keeps marshalling correct if the same callback is
    * registered with a different API's argument shape. The original fn is never
    * itself a trampoline (it was screened here at first wrap), so the recursion
    * bottoms out immediately. */
   int osl = cb_tramp_slot(fn32);
   if (osl >= 0) {
      uint32_t orig = ((uint32_t)osl < g_nbind) ? g_bind[osl].fn32 : 0;
      if (orig && orig != fn32) { return x64_cb_wrap(orig, sig); }
      return (uint64_t)fn32;   /* degenerate: hand the native stub straight back */
   }

   os_unfair_lock_lock(&g_bind_lock);
   for (uint32_t i = 0; i < g_nbind; ++i) {
      if (g_bind[i].fn32 == fn32 && g_bind[i].sig == sig) {
         os_unfair_lock_unlock(&g_bind_lock);
         return x64_cb_tramp_table[i];
      }
   }
   if (g_nbind >= x64_cb_nslots ||
       g_nbind >= sizeof(g_bind) / sizeof(g_bind[0])) {
      os_unfair_lock_unlock(&g_bind_lock);
      fprintf(stderr, "cb_bridge: trampoline slots exhausted (fn=0x%x)\n", fn32);
      return 0;
   }
   const uint32_t slot = g_nbind;
   g_bind[slot].fn32 = fn32;
   g_bind[slot].sig  = sig;
   ++g_nbind;
   os_unfair_lock_unlock(&g_bind_lock);

   return x64_cb_tramp_table[slot];
}

/* The common dispatcher behind every trampoline stub. gp[6]/fp[8] are the
 * saved argument registers; stk points at the native caller's stack args.
 * Classification mirrors SysV: int-domain args consume gp then stack, FP
 * args consume xmm then stack (8-byte slots either way). */
uint64_t x64_cb_dispatch(uint64_t slot, const uint64_t *gp, const uint64_t *fp,
                         const uint64_t *stk) {
   if (slot >= g_nbind) {
      fprintf(stderr, "cb_bridge: dispatch on unbound slot %llu\n",
              (unsigned long long)slot);
      return 0;
   }
   const cb_binding   b   = g_bind[slot];
   const x64_cb_sig  *sig = b.sig;

   uint32_t words[X64_CB_MAX_ARGS * 2];
   struct { void *lo; uint64_t native; uint32_t size; int back; }
      bounce[X64_CB_MAX_ARGS];
   uint32_t nbounce = 0;
   uint32_t w = 0, gpi = 0, fpi = 0, sti = 0;
   for (uint32_t i = 0; i < sig->nargs; ++i) {
      const uint8_t kind = sig->arg_kinds[i];
      uint64_t v;
      if (kind == CBA_F32 || kind == CBA_F64) {
         v = fpi < 8 ? fp[fpi++] : stk[sti++];
      } else {
         v = gpi < 6 ? gp[gpi++] : stk[sti++];
      }
      switch (kind) {
      case CBA_I32:
      case CBA_PTR:
      case CBA_F32:                /* float bits = xmm/stack slot low 4 bytes */
         words[w++] = (uint32_t)v;
         break;
      case CBA_PTR_REC:
         /* ★A pointer to a record the NATIVE side owns. Truncating it hands the
          * i386 callback a plausible-looking unmapped address (Portal 2:
          * scandir's `const struct dirent *` at 0x7f95fb809400 -> 0x748ea200,
          * SIGSEGV in the callback's strcmp). Bounce the pointee through the
          * low-4GB shim heap so a 4-byte pointer can actually reach it.
          *
          * A pointer already below 4GB is left EXACTLY as before — it is
          * reachable, and it is usually the i386 side's own context word coming
          * back — so this changes behaviour only where it was already broken. */
         if (v >= 0x100000000ULL && !cb_no_ptr_bounce()) {
            const uint32_t meta = sig->arg_sizes[i];
            const uint32_t sz   = meta & ~CBA_SIZE_COPYBACK;
            const uint64_t n = sz ? cb_readable_span(v, sz) : 0;
            void *lo = n ? malloc(sz) : NULL;    /* shim malloc -> low-4GB */
            if (lo) {
               memcpy(lo, (const void *)(uintptr_t)v, (size_t)n);
               if (n < sz) { memset((char *)lo + n, 0, (size_t)(sz - n)); }
               bounce[nbounce].lo     = lo;
               bounce[nbounce].native = v;
               bounce[nbounce].size   = (uint32_t)n;   /* copy BACK only what we read */
               bounce[nbounce].back   = (meta & CBA_SIZE_COPYBACK) != 0;
               ++nbounce;
               words[w++] = (uint32_t)(uintptr_t)lo;
               break;
            }
            /* Allocation failed, or abigen recorded no size: fall through to the
             * old truncation rather than pass 0 — a loud fault at the old
             * address beats a silent NULL deref inside the callback. */
         }
         words[w++] = (uint32_t)v;
         break;
      case CBA_OBJ:
         words[w++] = v >= 0x100000000ULL ? x64_objc_wrap(v) : (uint32_t)v;
         break;
      case CBA_I64:
      case CBA_F64:
         words[w++] = (uint32_t)v;
         words[w++] = (uint32_t)(v >> 32);
         break;
      default:
         fprintf(stderr, "cb_bridge: bad arg kind %u (slot %llu)\n", kind,
                 (unsigned long long)slot);
         return 0;
      }
   }

   void *stkbuf = x64_lowstack_get(CB_LOWSTACK_SZ);   /* per-thread cached, low-4GB */
   if (!stkbuf) { return 0; }
   const uint64_t top =
      ((uint64_t)(uintptr_t)stkbuf + CB_LOWSTACK_SZ) & ~0xfULL;

   /* breadcrumb: zero-I/O record before entering the translated callback */
   uint32_t depth = (uint32_t)__atomic_add_fetch(&g_cb_depth, 1, __ATOMIC_SEQ_CST);
   uint32_t ri = __atomic_fetch_add(&g_ring_pos, 1, __ATOMIC_SEQ_CST) & (CB_RING - 1);
   cb_crumb *cr = &g_ring[ri];
   cr->fn32 = b.fn32; cr->tid = pthread_mach_thread_np(pthread_self());
   cr->nargs = sig->nargs; cr->ret_kind = sig->ret_kind; cr->w = w;
   cr->stk_lo = (uint64_t)(uintptr_t)stkbuf; cr->stk_hi = top;
   cr->depth = depth; cr->done = 0;

   x64_cb_enter();
   const uint64_t r64 = _86x64_call_i386(b.fn32, w, words, top);
   x64_cb_leave();
   const uint32_t eax = (uint32_t)r64;   /* low 32 = the i386 eax result */

   cr->done = 1;
   __atomic_sub_fetch(&g_cb_depth, 1, __ATOMIC_SEQ_CST);
   /* Copy a non-const bounced pointee back before releasing it, so a callback
    * that WRITES through its argument is seen by the native caller. A const
    * pointee is never written back — its page may genuinely be read-only. */
   for (uint32_t k = 0; k < nbounce; ++k) {
      if (bounce[k].back) {
         memcpy((void *)(uintptr_t)bounce[k].native, bounce[k].lo, bounce[k].size);
      }
      free(bounce[k].lo);
   }
   x64_lowstack_put(stkbuf, CB_LOWSTACK_SZ);

   switch (sig->ret_kind) {
   case CBR_VOID:  return 0;
   case CBR_I32:
   case CBR_PTR:   return eax;
   case CBR_I32SX: return (uint64_t)(int64_t)(int32_t)eax;
   case CBR_I64:   return r64;                  /* full i386 edx:eax -> rax */
   case CBR_OBJ:   return x64_objc_unwrap(eax); /* handle -> real; raw passes */
   default:        return eax;
   }
}
