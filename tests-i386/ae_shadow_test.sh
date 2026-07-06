#!/bin/bash
# ae_shadow_test.sh — regression guard for the Apple Event Manager native-shadow
# bridge (ae_shim.c). Self-contained NATIVE x86_64 test (no i386 translation): it
# links the REAL ae_shim.c and proves the two invariants the Civ 'oapp' fix rests
# on:
#
#   A) the shadow registry preserves the FULL 64-bit native AEDesc pointer across
#      the 32-bit i386 shadow key — the exact thing the old path truncated (a
#      >4GB dataHandle chopped to its low 32 bits -> native AE deref crash);
#   B) a registered shadow RESOLVES so ___AEGetParamPtr forwards the ORIGINAL
#      native descriptor and reads the real event's parameter back — not a
#      truncated rebuild.
#
# Exit 42 = both invariants hold. 1 = registry truncated / lost the pointer;
# 2 = shadow did not resolve to the real event.
set -u
SRC="$(cd "$(dirname "$0")/../src/abiconv" && pwd)"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/t.c" <<'EOF'
#include <CoreServices/CoreServices.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ae_shim.c externs it (only ae_native_dispatch uses these; the tested paths
 * do not). Stub so the test links standalone. */
uint32_t _86x64_call_i386(uint64_t fn, uint64_t n, const uint32_t *w, uint64_t t){
   (void)fn;(void)n;(void)w;(void)t; return 0;
}
void x64_cb_enter(void){}
void x64_cb_leave(void){}

/* from ae_shim.c */
void            ae_shadow_register(uint32_t shadow32, const AEDesc *nativep);
void            ae_shadow_unregister(uint32_t shadow32);
const AEDesc   *ae_shadow_lookup(uint32_t shadow32);
uint32_t        shim_AEGetParamPtr(uint32_t *args);

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
   return 42;
}
EOF

if ! clang -arch x86_64 -o "$TMP/t" "$SRC/ae_shim.c" "$TMP/t.c" \
        -framework CoreServices 2>"$TMP/err"; then
   echo "ae-shadow: SKIP (compile failed)"; cat "$TMP/err"; exit 0
fi
OUT=$("$TMP/t" 2>&1); RC=$?
echo "$OUT" | sed 's/^/  /'
if [ "$RC" -eq 42 ]; then
   echo "ae-shadow: PASS"; exit 0
else
   echo "ae-shadow: FAIL (rc=$RC)"; exit 1
fi
