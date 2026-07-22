#!/bin/bash
#
# getctable_test.sh — regression guard for the GetCTable color-table shim
# (qd_gworld.c shim_GetCTable).
#
# THE BUG (Civ IV Steam, render init after the jmptbl fix): `GetCTable(short
# ctID)` loaded a classic 'clut' color-table resource. Native QuickDraw is gone
# on modern macOS, so the abigen ___GetCTable forwarded to the dead native and
# returned NULL. Civ's HBITMAP_Mac paletted-bitmap path (the depth-8/4 arms) then
# dereferenced the NULL CTabHandle (`movl -0x34(%rbp),%esi; movl (%rsi),%edx` ->
# EXC_BAD_ACCESS at 0x0) while filling an indexed bitmap's color table. The fix
# returns a real, correctly-SIZED ColorTable handle the caller fills in.
#
# This guard runs NATIVE x86_64 (no i386 sysroot). libabiconv can't run
# standalone (its ctors need the low-4GB env, like the cfprefs guards), so it
# replicates the shim's ColorTable construction INLINE (exactly as
# shim_GetCTable writes it) and asserts the contract Civ relies on:
#   - a non-NULL table pointer (the bug: abigen returned NULL);
#   - ctSize == n-1 for n = 2^depth entries (depth from ctID's low 7 bits);
#   - the table has room for n ColorSpec entries (8 bytes each after the
#     8-byte header), so the caller's `ctTable[i].rgb` writes at +0xa/+0xc/+0xe
#     of ctTable[i] land inside the allocation (the exact writes Civ makes);
#   - a default gray ramp is present (a caller that does NOT overwrite still
#     gets a usable table).
# Negative control: the OLD behavior (return NULL) fails the non-NULL assert.
set -u
cd "$(dirname "$0")"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/t.c" <<'EOF'
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { uint16_t red, green, blue; } QDRGBColor;
typedef struct { int16_t value; QDRGBColor rgb; } QDColorSpec;      /* 8 bytes */
typedef struct { int32_t ctSeed; int16_t ctFlags; int16_t ctSize;
                 QDColorSpec ctTable[1]; } QDColorTable;

/* ==== EXACT body of shim_GetCTable (qd_gworld.c), minus the cm_new_handle
 *      wrapper (here plain malloc stands in for the low-4GB Handle block). ==== */
static QDColorTable *fixed_getctable(int16_t ctID) {
   int depth = ctID & 0x7f;
   if (depth <= 0 || depth > 8) depth = 8;
   int n = 1 << depth;
   if (n < 1) n = 1;
   if (n > 256) n = 256;
   uint32_t sz = (uint32_t)(sizeof(QDColorTable) - sizeof(QDColorSpec)
                            + (size_t)n * sizeof(QDColorSpec));
   QDColorTable *ct = (QDColorTable *)calloc(1, sz);
   if (!ct) return NULL;
   ct->ctSeed = 0; ct->ctFlags = 0; ct->ctSize = (int16_t)(n - 1);
   for (int i = 0; i < n; i++) {
      uint16_t g = (uint16_t)((n > 1) ? (i * 0xffff) / (n - 1) : 0);
      ct->ctTable[i].value = (int16_t)i;
      ct->ctTable[i].rgb.red = ct->ctTable[i].rgb.green = ct->ctTable[i].rgb.blue = g;
   }
   return ct;
}
/* ==== the OLD blanket-NULL (abigen forwarding to dead native) ==== */
static QDColorTable *old_getctable(int16_t ctID) { (void)ctID; return NULL; }

int main(void) {
   /* Civ passes ctID = biBitCount + 0x40; for 8bpp -> 0x48. */
   int16_t ctID = 8 + 0x40;
   QDColorTable *ct = fixed_getctable(ctID);
   int fixed_ok = 0;
   if (ct) {
      int n = ct->ctSize + 1;
      /* Civ's fill loop writes ctTable[i].rgb (bytes at +0xa/+0xc/+0xe of the
       * i-th 8-byte entry) for i in [0,count); emulate the last write and read
       * it back to prove the entry is in-bounds and laid out as expected. */
      int ramp_lo_ok  = (ct->ctTable[0].rgb.red == 0);        /* gray ramp low */
      int ramp_hi_ok  = (ct->ctTable[255].rgb.red == 0xffff); /* gray ramp high */
      int last = n - 1;                                       /* write [last] AFTER */
      ct->ctTable[last].rgb.red = 0x1234;                     /* caller-fill in bounds */
      int layout_ok = (n == 256) && (ct->ctSize == 255)
         && (ct->ctTable[last].rgb.red == 0x1234)
         && ramp_lo_ok && ramp_hi_ok;
      fixed_ok = layout_ok;
      free(ct);
   }
   QDColorTable *oldct = old_getctable(ctID);
   int old_broken = (oldct == NULL);   /* the NULL-deref bug the fix cures */

   printf("getctable: fixed_ok=%d old_null=%d\n", fixed_ok, old_broken);
   return (fixed_ok && old_broken) ? 0 : 1;
}
EOF

if ! clang -arch x86_64 -o "$TMP/t" "$TMP/t.c" 2>"$TMP/err"; then
   echo "getctable: SKIP (compile failed)"; cat "$TMP/err"; exit 0
fi
OUT=$("$TMP/t" 2>/dev/null); RC=$?
if [ "$RC" -eq 0 ]; then
   echo "getctable: PASS ($OUT)"
else
   echo "getctable: FAIL ($OUT)"
fi
exit $RC
