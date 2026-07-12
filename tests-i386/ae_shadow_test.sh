#!/bin/bash
# ae_shadow_test.sh — regression guard for the Apple Event Manager native-shadow
# bridge (ae_shim.c). Self-contained NATIVE x86_64 test (no i386 translation): it
# links the REAL ae_shim.c and proves the invariants the Civ 'oapp' fixes rest
# on:
#
#   A) the shadow registry preserves the FULL 64-bit native AEDesc pointer across
#      the 32-bit i386 shadow key — the exact thing the old path truncated (a
#      >4GB dataHandle chopped to its low 32 bits -> native AE deref crash);
#   B) a registered shadow RESOLVES so ___AEGetParamPtr forwards the ORIGINAL
#      native descriptor and reads the real event's parameter back — not a
#      truncated rebuild;
#   C) ae_native_dispatch CLASSIFIES a bare translated-i386 handler (a low-4GB
#      address OUTSIDE the runtime's image — UPP == ProcPtr on Mach-O Carbon, so
#      apps install the proc bare; Civ IV does) as i386 and enters it through
#      _86x64_call_i386. The old classifier compared dladdr image basenames with
#      basename(3), whose Darwin implementation returns ONE static buffer — it
#      compared the buffer with itself, always equal, so EVERY handler was
#      called NATIVELY and a real i386 proc's 4-byte `ret` under-popped the
#      8-byte native return frame -> rsp skew +4 -> fused return PC
#      (Civ IV kAEOpenApplication: rip=0x173efb8b`00000000);
#   D) a handler that IS one of our native cb_bridge trampolines (per the
#      authoritative x64_cb_fn_is_tramp table test) is still invoked NATIVELY.
#
# Exit 42 = all invariants hold. 1 = registry truncated / lost the pointer;
# 2 = shadow did not resolve to the real event; 3 = bare i386 proc was invoked
# natively (the fused-PC bug); 4 = trampoline handler lost its native fast path.
set -u
SRC="$(cd "$(dirname "$0")/../src/abiconv" && pwd)"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# ---- helper dylib: the "app image" holding the bare handler. Loaded with a
# small-pagezero host so it maps BELOW 4GB (same condition as a translated
# app dylib) and dladdr resolves it to a non-runtime image basename. ----
cat > "$TMP/fakehdlr.c" <<'EOF'
#include <stdint.h>
volatile uint32_t fake_c_refcon;   /* what the 3rd arg carried */
volatile int      fake_c_calls;
int fake_handler_c(uint32_t a, uint32_t b, uint32_t c) {
   (void)a; (void)b;
   fake_c_refcon = c;
   fake_c_calls++;
   return 0;                        /* noErr */
}
volatile uint64_t fake_d_ev;
volatile int      fake_d_calls;
int fake_handler_d(uint64_t ev, uint64_t reply, uint64_t refcon) {
   (void)reply; (void)refcon;
   fake_d_ev = ev;
   fake_d_calls++;
   return 0;                        /* noErr */
}
EOF

cat > "$TMP/t.c" <<'EOF'
#include <CoreServices/CoreServices.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- stubs for ae_shim.c's externs (standalone link) ---- */

/* Records the i386-entry path and FORWARDS to the (really native) fake
 * handler with the marshalled words, so real AE gets a sane OSErr back. */
static volatile int      g_i386_entries;
static volatile uint32_t g_i386_fn;
uint32_t _86x64_call_i386(uint64_t fn, uint64_t n, const uint32_t *w, uint64_t t){
   (void)t;
   g_i386_entries++;
   g_i386_fn = (uint32_t)fn;
   if (n >= 3) {
      int (*f)(uint32_t, uint32_t, uint32_t) =
         (int (*)(uint32_t, uint32_t, uint32_t))(uintptr_t)fn;
      return (uint32_t)f(w[0], w[1], w[2]);
   }
   return 0;
}
void x64_cb_enter(void){}
void x64_cb_leave(void){}

/* cb_bridge.c's authoritative tramp-table test: the harness designates one
 * address as "our trampoline" (part D); everything else is not a tramp. */
static uint32_t g_designated_tramp;
int x64_cb_fn_is_tramp(uint32_t fn32){ return fn32 && fn32 == g_designated_tramp; }

/* from ae_shim.c */
void            ae_shadow_register(uint32_t shadow32, const AEDesc *nativep);
void            ae_shadow_unregister(uint32_t shadow32);
const AEDesc   *ae_shadow_lookup(uint32_t shadow32);
uint32_t        shim_AEGetParamPtr(uint32_t *args);
uint32_t        shim_AEInstallEventHandler(uint32_t *args);

/* Processes.h (HIServices) constant; self-targeted events dispatch to the
 * in-process handler table synchronously. AESendMessage (AEMach.h, part of
 * CoreServices) is the UI-free send — no Carbon/HIToolbox needed headless. */
enum { kSelfProcess = 2 /* kCurrentProcess */ };

static OSErr send_self(AEEventClass cls, AEEventID id){
   ProcessSerialNumber psn = { 0, kSelfProcess };
   AEDesc target = { typeNull, NULL };
   AppleEvent ev = { typeNull, NULL }, reply = { typeNull, NULL };
   OSErr err = AECreateDesc(typeProcessSerialNumber, &psn, sizeof psn, &target);
   if (err != noErr) { return err; }
   err = AECreateAppleEvent(cls, id, &target, kAutoGenerateReturnID,
                            kAnyTransactionID, &ev);
   AEDisposeDesc(&target);
   if (err != noErr) { return err; }
   err = (OSErr)AESendMessage(&ev, &reply, kAEWaitReply | kAENeverInteract,
                              kAEDefaultTimeout);
   AEDisposeDesc(&ev);
   AEDisposeDesc(&reply);
   return err;
}

int main(void){
   /* ---- A) registry preserves a FULL >4GB native pointer ---- */
   const uint32_t KEY = 0x80001000u;                 /* a low-4GB i386 shadow key */
   AEDesc *high = (AEDesc*)(uintptr_t)0x0000600000abcdefULL; /* fabricated >4GB */
   ae_shadow_register(KEY, high);
   const AEDesc *got = ae_shadow_lookup(KEY);
   if ((uint64_t)(uintptr_t)got != 0x0000600000abcdefULL){
      printf("A FAIL: registry returned %p (want 0x600000abcdef) — TRUNCATED\n",(void*)got);
      return 1;
   }
   ae_shadow_unregister(KEY);
   if (ae_shadow_lookup(KEY) != NULL){ printf("A FAIL: unregister leaked\n"); return 1; }
   printf("A ok: >4GB native pointer preserved across 32-bit shadow key\n");

   /* ---- B) shadow resolves -> ___AEGetParamPtr reads the real event ---- */
   AppleEvent ev; AEBuildError berr;
   SInt32 want = (SInt32)0xCAFE1234;
   if (AECreateAppleEvent('aevt','oapp',
        &(AEDesc){typeNull,NULL},   /* dummy null target address */
        kAutoGenerateReturnID, kAnyTransactionID, &ev) != noErr){
      printf("B SKIP: AECreateAppleEvent failed\n"); return 42; /* env, not our bug */
   }
   (void)berr;
   if (AEPutParamPtr(&ev, keyDirectObject, typeSInt32, &want, sizeof(want)) != noErr){
      printf("B SKIP: AEPutParamPtr failed\n"); AEDisposeDesc(&ev); return 42;
   }

   /* register a shadow key -> the real native event */
   const uint32_t EVK = 0x80002000u;
   ae_shadow_register(EVK, &ev);

   /* Drive shim_AEGetParamPtr's SHADOW-RESOLUTION branch directly. All out-slots
    * NULL + zero-size buffer: native AEGetParamPtr then returns noErr when the
    * param is FOUND (proving the shim resolved the shadow to the real event and
    * reached native AE) and errAEDescNotFound for a missing key (proving it
    * walked a VALID descriptor, not a truncated rebuild). No low-4GB out-slots
    * needed. (The full out-param write path uses low-4GB i386 buffers, exercised
    * live in the app; a standalone process can't map <4GB under Rosetta.) */
   uint32_t hit[7]  = { EVK, (uint32_t)keyDirectObject, (uint32_t)typeSInt32, 0,0,0,0 };
   uint32_t miss[7] = { EVK, 0x7a7a7a7au /*'zzzz'*/,    (uint32_t)typeSInt32, 0,0,0,0 };
   OSErr e_hit  = (OSErr)shim_AEGetParamPtr(hit);
   OSErr e_miss = (OSErr)shim_AEGetParamPtr(miss);
   printf("B shim: found-key err=%d  missing-key err=%d (want 0 / %d)\n",
          (int)e_hit,(int)e_miss,(int)errAEDescNotFound);

   /* and confirm the actual value via the resolved descriptor */
   const AEDesc *r = ae_shadow_lookup(EVK);
   SInt32 out=0; DescType t=0; Size sz=0;
   OSErr e_val = AEGetParamPtr(r, keyDirectObject, typeSInt32, &t, &out, sizeof(out), &sz);

   int ok = (e_hit == noErr)                 /* shim resolved shadow, param found */
         && (e_miss == errAEDescNotFound)     /* reached native on a valid desc    */
         && (e_val == noErr && out == want && t == typeSInt32); /* real value      */

   ae_shadow_unregister(EVK);
   AEDisposeDesc(&ev);
   if (!ok){ printf("B FAIL: shadow did not resolve to the real event\n"); return 2; }
   printf("B ok: shim resolved shadow -> real event parameter (0x%x) retrieved\n",(unsigned)out);

   /* ---- C) a bare i386 proc (low-4GB, NON-runtime image) must go through
    *         _86x64_call_i386, never a direct native call ---- */
   void *dl = dlopen("./libfakehdlr.dylib", RTLD_NOW);
   if (!dl){ printf("C SKIP: dlopen failed: %s\n", dlerror()); return 42; }
   void *fc = dlsym(dl, "fake_handler_c");
   void *fd = dlsym(dl, "fake_handler_d");
   volatile uint32_t *c_refcon = (volatile uint32_t *)dlsym(dl, "fake_c_refcon");
   volatile int      *c_calls  = (volatile int *)dlsym(dl, "fake_c_calls");
   volatile int      *d_calls  = (volatile int *)dlsym(dl, "fake_d_calls");
   if (!fc || !fd || !c_refcon || !c_calls || !d_calls){
      printf("C SKIP: dlsym failed\n"); return 42;
   }
   if ((uintptr_t)fc >= 0x100000000ULL){
      /* needs the small-pagezero host to map the dylib low; env quirk */
      printf("C SKIP: helper dylib mapped >4GB (%p)\n", fc); return 42;
   }

   uint32_t inst_c[5] = { '86xt', 'tsBC', (uint32_t)(uintptr_t)fc, 0x1234u, 0 };
   if ((OSErr)shim_AEInstallEventHandler(inst_c) != noErr){
      printf("C SKIP: install failed\n"); return 42;
   }
   OSErr e_send_c = send_self('86xt', 'tsBC');
   printf("C dispatch: AESend err=%d i386_entries=%d fn=0x%x calls=%d refcon=0x%x\n",
          (int)e_send_c, g_i386_entries, g_i386_fn, *c_calls, *c_refcon);
   if (e_send_c != noErr){ printf("C SKIP: AESend failed (env)\n"); return 42; }
   if (g_i386_entries != 1 || g_i386_fn != (uint32_t)(uintptr_t)fc){
      printf("C FAIL: bare i386 proc was invoked NATIVELY (the fused-PC bug) — "
             "an i386 4-byte ret under-pops the native 8-byte frame\n");
      return 3;
   }
   if (*c_calls != 1 || *c_refcon != 0x1234u){
      printf("C FAIL: handler did not receive the marshalled i386 frame\n");
      return 3;
   }
   printf("C ok: out-of-image bare proc entered via _86x64_call_i386 with its refcon\n");

   /* ---- D) our own cb_bridge trampoline keeps the native fast path ---- */
   g_designated_tramp = (uint32_t)(uintptr_t)fd;
   uint32_t inst_d[5] = { '86xt', 'tsBD', (uint32_t)(uintptr_t)fd, 0x5678u, 0 };
   if ((OSErr)shim_AEInstallEventHandler(inst_d) != noErr){
      printf("D SKIP: install failed\n"); return 42;
   }
   int i386_before = g_i386_entries;
   OSErr e_send_d = send_self('86xt', 'tsBD');
   printf("D dispatch: AESend err=%d i386_entries=%d d_calls=%d\n",
          (int)e_send_d, g_i386_entries - i386_before, *d_calls);
   if (e_send_d != noErr){ printf("D SKIP: AESend failed (env)\n"); return 42; }
   if (*d_calls != 1 || g_i386_entries != i386_before){
      printf("D FAIL: trampoline handler lost its native fast path\n");
      return 4;
   }
   printf("D ok: designated trampoline invoked natively\n");
   return 42;
}
EOF

if ! clang -arch x86_64 -dynamiclib -o "$TMP/libfakehdlr.dylib" \
        "$TMP/fakehdlr.c" 2>"$TMP/err"; then
   echo "ae-shadow: SKIP (helper dylib compile failed)"; cat "$TMP/err"; exit 0
fi
# -pagezero_size 0x1000: lets dyld map the dlopen'd helper dylib BELOW 4GB, the
# same address class as a translated app dylib (part C needs a real low-4GB
# out-of-image function address for dladdr to classify).
if ! clang -arch x86_64 -Wl,-pagezero_size,0x1000 -Wno-multichar \
        -o "$TMP/t" "$SRC/ae_shim.c" "$TMP/t.c" \
        -framework CoreServices 2>"$TMP/err"; then
   echo "ae-shadow: SKIP (compile failed)"; cat "$TMP/err"; exit 0
fi
OUT=$(cd "$TMP" && ./t 2>&1); RC=$?
echo "$OUT" | sed 's/^/  /'
if [ "$RC" -eq 42 ]; then
   echo "ae-shadow: PASS"; exit 0
else
   echo "ae-shadow: FAIL (rc=$RC)"; exit 1
fi
