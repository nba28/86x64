/*
 * ae_shim.c — Apple Event Manager native-shadow bridge (Civ IV 'oapp' wall).
 *
 * ROOT PROBLEM (the up/down asymmetry the pack fix a4ca917 could not reach):
 * when the native AE Manager dispatches an event (AEProcessAppleEvent ->
 * AEPredispatchHandler -> the app's installed handler), it hands the handler a
 * pointer to a NATIVE AEDesc: `{DescType descriptorType@0; AEDataStorage
 * dataHandle@+4}` where dataHandle is an 8-byte native Handle (pack(2), sizeof
 * 12). The event lives below 4 GB, so a raw pointer survives to i386 — but the
 * translated i386 handler reads the struct as an i386 AEDesc (4-byte dataHandle
 * at +4), taking only the LOW 32 bits of the native Handle. When the handler
 * then calls AEGetParamPtr(theAppleEvent, ...), the (abigen) shim faithfully
 * rebuilds a native AEDesc from those truncated fields and forwards the
 * chopped Handle to native AE, which dereferences garbage -> EXC_BAD_ACCESS in
 * AEGetParamDesc (nondeterministic: crashes only when the truncated address is
 * unmapped). This is the CALLBACK/down mirror of a4ca917 (pack, up) and
 * d6b0012 (Handle-value, up) — the AEDesc crosses the boundary in the
 * native->i386 direction here and no down-conversion existed.
 *
 * FIX (functionality-first, native-shadow): keep the native AEDesc; never let
 * i386 see (or rebuild) the 64-bit Handle.
 *   1. ___AEInstallEventHandler installs OUR native dispatcher (not the generic
 *      x64_cb_wrap trampoline — deliberately AE-specific so it does not collide
 *      with a301f9d's general cb_bridge pointer-to-struct work). It records the
 *      i386 handler UPP + i386 refcon in a binding table, keyed by an index
 *      passed to native AE as the SRefCon.
 *   2. On dispatch, ae_native_dispatch REGISTERS the REAL native AEDesc pointers
 *      (AE events live in the low-4GB heap, so a 4-byte i386 slot holds them
 *      intact — exactly what the abigen bridge already did) and then enters the
 *      registered handler. Civ wraps its handler via NewAEEventHandlerUPP, which
 *      abigen shims to x64_cb_wrap, so the "handler" it installs is ALREADY a
 *      native cb_bridge trampoline (it does its own low-4GB stack + i386
 *      re-entry). We therefore invoke it NATIVELY as a C function pointer, not
 *      via _86x64_call_i386 (running a native trampoline as i386 crashed: its
 *      native `ret` popped 8 bytes = ret32 fused with the ev word). If instead
 *      a bare i386 proc was registered (no UPP wrap), fall back to
 *      _86x64_call_i386. dladdr(handler) tells the two apart by image.
 *   3. ___AEGetParamPtr resolves the incoming i386 AEDesc pointer against the
 *      registry: a REGISTERED (delivered) descriptor -> forward the ORIGINAL
 *      native AEDesc (full 64-bit Handle) to native AEGetParamPtr, instead of
 *      rebuilding a native temp from the i386-truncated fields (the bug). A
 *      non-registered pointer (an app-CONSTRUCTED i386 descriptor, e.g. via
 *      AECreateDesc) falls through to the prior zero-extend behavior — Civ does
 *      not hit that on the incoming path, and preserving it avoids regression.
 *
 * Scope is deliberately the incoming/crash path only (Civ's AE surface is
 * AECreateAppleEvent/AECreateDesc/AEDisposeDesc/AEGetParamPtr/
 * AEInstallEventHandler/AEProcessAppleEvent/AESend; the outgoing builders stay
 * abigen-generated). Universal for any i386 Carbon app with an AE handler
 * (Halo/iPhoto 'oapp'/'odoc'). RESYNC-class (libabiconv-only). Exports wired
 * via ae_tramp.asm's AE_MTSHIM; excluded from abigen in custom.syms.
 */

#include <CoreServices/CoreServices.h>
#include <dlfcn.h>
#include <os/lock.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern void *x64_lowstack_get(size_t sz);          /* lowstack_pool.c */
extern void  x64_lowstack_put(void *p, size_t sz);

/* i386->translated re-entry primitive + cross-copy nesting bookkeeping, shared
 * with cb_bridge.c / cf_callback_shim.c (objc_reverse.asm / objc_shim.c). */
uint32_t _86x64_call_i386(uint64_t fn, uint64_t nwords,
                          const uint32_t *words, uint64_t lowstack_top);
void x64_cb_enter(void);
void x64_cb_leave(void);

/* Deep native call chains can originate from an AE handler; match cb_bridge. */
#define AE_LOWSTACK_SZ (4u * 1024u * 1024u)

/* i386-layout AEDesc the handler sees: {DescType type@0; uint32_t handle@+4}. */
typedef struct { uint32_t descriptorType; uint32_t dataHandle; } AEDesc32;

/* ------------------------------------------------------------------ *
 * Shadow registry: low-4GB shadow-AEDesc pointer -> the native AEDesc AE
 * delivered.  Resolution is by pointer identity of the shadow we handed i386,
 * so a handler that passes theAppleEvent/reply straight to AEGetParamPtr
 * recovers the ORIGINAL native descriptor (full 64-bit Handle), never a
 * 32-bit-truncated rebuild.  Small fixed table + lock: AE dispatch is
 * shallow/main-thread; register on entry, unregister on exit.
 * ------------------------------------------------------------------ */
#define AE_MAX_SHADOW 64
static struct { uint32_t shadow32; const AEDesc *nativep; } g_shadow[AE_MAX_SHADOW];
static os_unfair_lock g_shadow_lock = OS_UNFAIR_LOCK_INIT;

/* Non-static so the self-contained guard (ae_shadow_test) can prove the
 * registry preserves the FULL 64-bit native pointer across the 32-bit key. */
void ae_shadow_register(uint32_t shadow32, const AEDesc *nativep) {
   if (!shadow32) { return; }
   os_unfair_lock_lock(&g_shadow_lock);
   for (int i = 0; i < AE_MAX_SHADOW; i++) {
      if (!g_shadow[i].shadow32) {
         g_shadow[i].shadow32 = shadow32;
         g_shadow[i].nativep = nativep;
         break;
      }
   }
   os_unfair_lock_unlock(&g_shadow_lock);
}
void ae_shadow_unregister(uint32_t shadow32) {
   os_unfair_lock_lock(&g_shadow_lock);
   for (int i = 0; i < AE_MAX_SHADOW; i++) {
      if (g_shadow[i].shadow32 == shadow32) {
         g_shadow[i].shadow32 = 0;
         g_shadow[i].nativep = NULL;
         break;
      }
   }
   os_unfair_lock_unlock(&g_shadow_lock);
}
const AEDesc *ae_shadow_lookup(uint32_t shadow32) {
   const AEDesc *r = NULL;
   os_unfair_lock_lock(&g_shadow_lock);
   for (int i = 0; i < AE_MAX_SHADOW; i++) {
      if (g_shadow[i].shadow32 == shadow32) { r = g_shadow[i].nativep; break; }
   }
   os_unfair_lock_unlock(&g_shadow_lock);
   return r;
}

/* ------------------------------------------------------------------ *
 * Handler binding table: our single native dispatcher recovers the i386
 * handler UPP + i386 refcon from the binding index it receives as SRefCon.
 * ------------------------------------------------------------------ */
#define AE_MAX_BIND 64
static struct { uint32_t fn32; uint32_t refcon32; int used; } g_bind[AE_MAX_BIND];
static os_unfair_lock g_bind_lock = OS_UNFAIR_LOCK_INIT;

typedef OSErr (*ae_handler_fn)(const AppleEvent *, AppleEvent *, SRefCon);

/* Is `fn` a NATIVE-ABI address (i.e. a cb_bridge trampoline from
 * NewAEEventHandlerUPP -> x64_cb_wrap), vs a bare translated i386 proc in the
 * app's dylib (UPP == ProcPtr on Mach-O Carbon, so apps legally install the
 * proc bare — Civ IV does)?  Decide by MECHANISM first: the authoritative
 * cb_tramp-table range test for THIS copy's trampolines.  Only then fall back
 * to the image-identity heuristic (a trampoline minted by ANOTHER of the
 * multi-copy libabiconv instances), comparing the dladdr image basenames.
 *
 * ⚠ Darwin basename(3) copies into ONE static internal buffer and returns it,
 * so the old `strcmp(basename(a), basename(b))` compared that buffer with
 * ITSELF — always equal — and EVERY handler (including bare translated i386
 * procs) classified "native".  The native call then let the handler's i386
 * 4-byte `ret` under-pop the 8-byte native return frame: rsp came back
 * skewed +4, and ae_native_dispatch's own epilogue `ret` fused the saved
 * rbp's zero high half with the low half of the OUTER (AE Manager) return
 * address -> rip = 0xXXXXXXXX`00000000 (Civ IV kAEOpenApplication:
 * rip=0x173efb8b`00000000, r11=ae_native_dispatch+362).  Compare path tails
 * with strrchr instead — no shared static storage. */
int x64_cb_fn_is_tramp(uint32_t fn32);   /* cb_bridge.c: tramp-table range */

static const char *ae_path_base(const char *p) {
   const char *s = strrchr(p, '/');
   return s ? s + 1 : p;
}

static int ae_fn_is_native_tramp(uint32_t fn32) {
   if (x64_cb_fn_is_tramp(fn32)) { return 1; }   /* THIS copy's trampoline */
   Dl_info self, target;
   if (!dladdr((void *)&ae_fn_is_native_tramp, &self) || !self.dli_fname) {
      return 0;
   }
   if (!dladdr((void *)(uintptr_t)fn32, &target) || !target.dli_fname) {
      return 0;
   }
   return strcmp(ae_path_base(self.dli_fname),
                 ae_path_base(target.dli_fname)) == 0;   /* another copy's */
}

/* The native handler installed with real AE for every app handler. */
static OSErr ae_native_dispatch(const AppleEvent *ev, AppleEvent *reply,
                                SRefCon refcon) {
   long idx = (long)refcon;
   if (idx < 0 || idx >= AE_MAX_BIND || !g_bind[idx].used) {
      return errAEHandlerNotFound;
   }
   uint32_t fn32 = g_bind[idx].fn32;
   uint32_t rc32 = g_bind[idx].refcon32;

   /* Register the REAL native AEDesc pointers (AE events live below 4GB, so a
    * 4-byte i386 slot holds them intact — same as the abigen bridge) so
    * ___AEGetParamPtr forwards the genuine descriptor instead of rebuilding
    * from truncated i386 fields. */
   uint32_t ev32 = (uint32_t)(uintptr_t)ev;
   uint32_t rep32 = (uint32_t)(uintptr_t)reply;
   ae_shadow_register(ev32, ev);
   ae_shadow_register(rep32, reply);

   int native = ae_fn_is_native_tramp(fn32);

   OSErr err;
   if (native) {
      /* Civ's handler is a cb_bridge trampoline: call it NATIVELY with the AE
       * refcon Civ installed (rc32). It does its own i386 re-entry + low stack
       * and marshals ev/reply -> the i386 handler exactly as we registered. */
      err = ((ae_handler_fn)(uintptr_t)fn32)(ev, reply,
                                             (SRefCon)(uintptr_t)rc32);
   } else {
      /* Bare i386 proc: enter it on a fresh low-4GB stack via the primitive. */
      void *stk = x64_lowstack_get(AE_LOWSTACK_SZ);
      if (stk) {
         uint64_t top = ((uint64_t)(uintptr_t)stk + AE_LOWSTACK_SZ) & ~0xfULL;
         uint32_t words[3] = { ev32, rep32, rc32 };
         x64_cb_enter();
         uint32_t eax = _86x64_call_i386((uint64_t)fn32, 3, words, top);
         x64_cb_leave();
         x64_lowstack_put(stk, AE_LOWSTACK_SZ);
         err = (OSErr)(int32_t)eax;
      } else {
         err = memFullErr;
      }
   }

   ae_shadow_unregister(ev32);
   ae_shadow_unregister(rep32);
   return err;
}

/* ------------------------------------------------------------------ *
 * ___AEInstallEventHandler (i386 args via ae_tramp.asm AE_MTSHIM).
 * i386: (AEEventClass, AEEventID, AEEventHandlerUPP handler, SRefCon refcon,
 *        Boolean isSysHandler)
 * ------------------------------------------------------------------ */
uint32_t shim_AEInstallEventHandler(uint32_t *args) {
   uint32_t theClass = args[0];
   uint32_t theID = args[1];
   uint32_t fn32 = args[2];
   uint32_t rc32 = args[3];
   uint8_t sys = (uint8_t)args[4];

   long idx = -1;
   os_unfair_lock_lock(&g_bind_lock);
   for (int i = 0; i < AE_MAX_BIND; i++) {
      if (!g_bind[i].used) {
         g_bind[i].used = 1;
         g_bind[i].fn32 = fn32;
         g_bind[i].refcon32 = rc32;
         idx = i;
         break;
      }
   }
   os_unfair_lock_unlock(&g_bind_lock);
   if (idx < 0) { return (uint32_t)(int32_t)memFullErr; }

   OSErr err = AEInstallEventHandler((AEEventClass)theClass, (AEEventID)theID,
                                     NewAEEventHandlerUPP(ae_native_dispatch),
                                     (SRefCon)(uintptr_t)idx, sys ? true : false);
   if (err != noErr) {
      os_unfair_lock_lock(&g_bind_lock);
      g_bind[idx].used = 0;
      os_unfair_lock_unlock(&g_bind_lock);
   }
   return (uint32_t)(int32_t)err;
}

/* ------------------------------------------------------------------ *
 * ___AEGetParamPtr (i386 args via ae_tramp.asm AE_MTSHIM).
 * i386: (AppleEvent* ev, AEKeyword key, DescType desiredType, DescType* type,
 *        void* dataPtr, Size maxSize, Size* actualSize)
 * Size is 4 bytes on i386, 8 on x86_64 — widen in, truncate back.
 * ------------------------------------------------------------------ */
uint32_t shim_AEGetParamPtr(uint32_t *args) {
   uint32_t ev32 = args[0];
   AEKeyword key = (AEKeyword)args[1];
   DescType wanted = (DescType)args[2];
   uint32_t typeCode32 = args[3];   /* DescType* (i386, low-4GB) */
   uint32_t dataPtr32 = args[4];    /* void*     (i386, low-4GB) */
   uint32_t maxSize = args[5];      /* Size (i386 4B)            */
   uint32_t actSize32 = args[6];    /* Size* (i386, low-4GB)     */

   const AEDesc *nativep = ae_shadow_lookup(ev32);
   AEDesc tmp;
   if (!nativep) {
      /* App-CONSTRUCTED i386 descriptor (not a delivered event): reconstruct
       * exactly as abigen did (zero-extend the i386 Handle). Civ does not reach
       * this on the incoming path; kept to avoid regressing that surface. */
      AEDesc32 *d = (AEDesc32 *)(uintptr_t)ev32;
      tmp.descriptorType = d ? d->descriptorType : typeNull;
      tmp.dataHandle = d ? (AEDataStorage)(uintptr_t)d->dataHandle : NULL;
      nativep = &tmp;
   }

   DescType actualType = 0;
   Size actualSize = 0;
   OSErr err = AEGetParamPtr(nativep, key, wanted,
                             typeCode32 ? &actualType : NULL,
                             (void *)(uintptr_t)dataPtr32, (Size)maxSize,
                             actSize32 ? &actualSize : NULL);
   if (typeCode32) { *(uint32_t *)(uintptr_t)typeCode32 = (uint32_t)actualType; }
   if (actSize32) { *(uint32_t *)(uintptr_t)actSize32 = (uint32_t)actualSize; }

   return (uint32_t)(int32_t)err;
}
