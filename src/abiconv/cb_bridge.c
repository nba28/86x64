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

/* must match the codes in typeconv.cc (cb_arg_code / cb_ret_code) and the
 * blob layout emitted by cb_sig_emit */
#define X64_CB_MAX_ARGS 16
typedef struct {
   uint32_t nargs;
   uint32_t ret_kind;
   uint8_t  arg_kinds[X64_CB_MAX_ARGS];
} x64_cb_sig;

enum { CBA_I32 = 0, CBA_I64 = 1, CBA_PTR = 2, CBA_OBJ = 3,
       CBA_F32 = 4, CBA_F64 = 5 };
enum { CBR_VOID = 0, CBR_I32 = 1, CBR_PTR = 2, CBR_OBJ = 3, CBR_I32SX = 4 };

uint32_t _86x64_call_i386(uint64_t fn, uint64_t nwords,
                          const uint32_t *words, uint64_t lowstack_top);
uint32_t x64_objc_wrap(uint64_t real);     /* objc_shim.c */
uint64_t x64_objc_unwrap(uint32_t h);      /* objc_shim.c */

extern const uint64_t x64_cb_tramp_table[];  /* cb_tramp.asm */
extern const uint64_t x64_cb_nslots;

#define CB_LOWSTACK_SZ (256u * 1024u)

typedef struct {
   uint32_t          fn32;
   const x64_cb_sig *sig;
} cb_binding;

static cb_binding     g_bind[512];     /* >= x64_cb_nslots; checked at bind */
static uint32_t       g_nbind;
static os_unfair_lock g_bind_lock = OS_UNFAIR_LOCK_INIT;

static int cb_trace(void) {
   static int t = -1;
   if (t < 0) { t = getenv("CB_BRIDGE_TRACE") != NULL; }
   return t;
}

/* Bind an i386 callback to a native trampoline. Bindings are immutable and
 * deduped on (fn, sig), so re-registering the same callback (timers
 * recreated per window, etc.) reuses its slot. NULL passes through as NULL
 * (optional callbacks). */
uint64_t x64_cb_wrap(uint32_t fn32, const x64_cb_sig *sig) {
   if (fn32 == 0) { return 0; }

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

   if (cb_trace()) {
      fprintf(stderr, "[cb] wrap fn=0x%x sig=%p nargs=%u ret=%u -> slot %u\n",
              fn32, (const void *)sig, sig->nargs, sig->ret_kind, slot);
   }
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

   if (cb_trace()) {
      fprintf(stderr, "[cb] t=%x slot %llu -> fn 0x%x (%u words)\n",
              pthread_mach_thread_np(pthread_self()),
              (unsigned long long)slot, b.fn32, w);
      fflush(stderr);
   }

   void *stkbuf = malloc(CB_LOWSTACK_SZ);     /* shim malloc -> low-4GB */
   if (!stkbuf) { return 0; }
   const uint64_t top =
      ((uint64_t)(uintptr_t)stkbuf + CB_LOWSTACK_SZ) & ~0xfULL;
   const uint32_t eax = _86x64_call_i386(b.fn32, w, words, top);
   free(stkbuf);
   if (cb_trace()) {
      fprintf(stderr, "[cbret] t=%x slot %llu fn 0x%x eax=0x%x\n",
              pthread_mach_thread_np(pthread_self()),
              (unsigned long long)slot, b.fn32, eax);
      fflush(stderr);
   }

   switch (sig->ret_kind) {
   case CBR_VOID:  return 0;
   case CBR_I32:
   case CBR_PTR:   return eax;
   case CBR_I32SX: return (uint64_t)(int64_t)(int32_t)eax;
   case CBR_OBJ:   return x64_objc_unwrap(eax); /* handle -> real; raw passes */
   default:        return eax;
   }
}
