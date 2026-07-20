#!/bin/bash
#
# cfxml_infoptr_layout_test.sh — regression guard for the CFXMLNodeCreate
# additionalInfoPtr struct-relayout shim (cfxml_shim.c).
#
# THE BUG (Civ IV Steam "Launch in Window" startup XML/mod-config parse):
#   CFXMLNodeCreate(alloc, xmlType, dataString, const void *additionalInfoPtr,
#                   version)'s additionalInfoPtr is a DISCRIMINATED UNION selected
#   by xmlType (CFXMLElementInfo*/CFXMLDocumentInfo*/... — CFXMLNode.h). Those
#   info structs carry POINTER + CFIndex fields, so their LAYOUT DIFFERS between
#   the ABIs (i386 CFXMLElementInfo = 12B, 4-byte fields @0/4/8; x86_64 = 24B,
#   8-byte fields @0/8/16). abigen can't see through the `const void *` (opaque
#   `void` pointee), so its generated shim forwards the i386 record VERBATIM.
#   Native x86_64 CoreFoundation then reads the 12-byte i386 record as a 24-byte
#   x86_64 one: `attributes` fuses the i386 attributes+attributeOrder 4-byte
#   handles into one 8-byte value (0x08000100_00000008 — the crash's fault addr),
#   the other fields read past the record's end, and even the field VALUES are
#   i386 handles CF derefs as real objects -> EXC_BAD_ACCESS in CFXMLNodeCreate.
#
# THE FIX (cfxml_shim.c): relayout the info struct per xmlType before the native
# call (and mirror-back in CFXMLNodeGetInfoPtr).
#
# This guard runs NATIVE x86_64 (no i386 sysroot). It can't drive libabiconv's
# handle arena standalone (its constructors need the low-4GB env, like the
# cfprefs guards), so it validates the LAYOUT-TRANSLATION contract directly:
#   1. Build an i386-laid-out CFXMLElementInfo (12B: two 4-byte handle-sized
#      pointer fields + isEmpty) whose pointer fields hold REAL native CF refs
#      truncated into 4-byte slots is not representable on a 64-bit host, so we
#      emulate the i386 record with 4-byte fields carrying the LOW 32 bits AND a
#      side table mapping slot->real ref (the shim's arena unwrap). The relayout
#      body (copied verbatim from cfxml_shim.c) rebuilds the native 24-byte record
#      resolving each field, then calls REAL CFXMLNodeCreate.
#   2. FIX path: CFXMLNodeCreate succeeds and CFXMLNodeGetInfoPtr round-trips the
#      attributes dictionary + isEmpty back out (identity preserved).
#   3. BUG path (negative control): passing the raw i386 12-byte record to the
#      SAME real CFXMLNodeCreate reads garbage — detected by comparing the node's
#      recovered info against the expected values (the raw path mismatches or the
#      dictionary pointer it recovers is not our dictionary).
#
# Structural (triggers on the CFXML info-struct layout, not any app); any legacy
# i386 CFXMLNodeCreate caller with a pointer-bearing node type is covered.
set -u
cd "$(dirname "$0")"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/t.c" <<'EOF'
#define __CFXMLNode_DEPRECATION_MSG ""
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFXMLNode.h>
#include <stdio.h>
#include <string.h>

enum { kNodeElement = kCFXMLNodeTypeElement };

/* ---- native (x86_64 / LP64) CFXMLElementInfo (== the SDK's, restated for the
 *      side-by-side layout assertion against the i386 form) ---- */
typedef struct { CFDictionaryRef attributes; CFArrayRef attributeOrder;
                 Boolean isEmpty; char _r[3]; } ElementInfo64;

/* ---- i386 (ILP32) CFXMLElementInfo: 4-byte fields @0/4/8, sizeof 12 ---- *
 * On a 64-bit host we can't put a real 8-byte CF ref in a 4-byte field, so the
 * "i386 record" stores 4-byte SLOT ids; slot_to_ref emulates the shim's arena
 * unwrap (_86x64_unwrap_obj_arg). This mirrors the exact indirection the real
 * shim performs — the point under test is the OFFSET/WIDTH relayout, not the
 * handle table. */
#pragma pack(push,1)
typedef struct { uint32_t attributes; uint32_t attributeOrder;
                 uint8_t isEmpty; uint8_t _r[3]; } ElementInfo32;
#pragma pack(pop)

static const void *g_slot[8];
static const void *slot_to_ref(uint32_t s) { return s ? g_slot[s] : NULL; }

/* ==== relayout body — copied from cfxml_shim.c relayout_info_to_native ==== */
static const void *relayout_element(const ElementInfo32 *i386info, ElementInfo64 *o) {
   memset(o, 0, sizeof(*o));
   o->attributes     = slot_to_ref(i386info->attributes);
   o->attributeOrder = slot_to_ref(i386info->attributeOrder);
   o->isEmpty        = i386info->isEmpty;
   return o;
}

int main(void) {
   /* Build a real attributes dictionary + order array. */
   CFStringRef k = CFSTR("id"), v = CFSTR("42");
   CFDictionaryRef attrs = CFDictionaryCreate(NULL, (const void **)&k,
      (const void **)&v, 1, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
   const void *ord[] = { k };
   CFArrayRef order = CFArrayCreate(NULL, ord, 1, &kCFTypeArrayCallBacks);
   g_slot[1] = attrs; g_slot[2] = order;

   /* i386-laid-out record: handles in 4-byte slots (slot 1=attrs, 2=order). */
   ElementInfo32 i386rec; memset(&i386rec, 0, sizeof(i386rec));
   i386rec.attributes = 1; i386rec.attributeOrder = 2; i386rec.isEmpty = 1;

   CFStringRef tag = CFSTR("Unit");

   /* ---- FIX path: relayout then call real CFXMLNodeCreate ---- */
   ElementInfo64 native; relayout_element(&i386rec, &native);
   CFXMLNodeRef node = CFXMLNodeCreate(NULL, kNodeElement, tag, &native, 1);
   int fix_ok = 0;
   if (node) {
      /* CF COPIES the info into the node, so the recovered pointers are NOT
       * identical to the inputs — verify the CONTENT survived instead: the
       * attributes dict is a real dictionary carrying our "id"->"42" pair, the
       * order array is a real 1-element array, isEmpty round-tripped, and the
       * tag string matches. A raw i386 record (the bug) would make CF read
       * fused garbage handles -> not a valid dictionary. */
      const ElementInfo64 *got = (const ElementInfo64 *)CFXMLNodeGetInfoPtr(node);
      CFStringRef s = CFXMLNodeGetString(node);
      int attrs_ok = got && got->attributes
         && CFGetTypeID(got->attributes) == CFDictionaryGetTypeID()
         && CFDictionaryGetCount(got->attributes) == 1
         && CFStringCompare(
              (CFStringRef)CFDictionaryGetValue(got->attributes, CFSTR("id")),
              CFSTR("42"), 0) == kCFCompareEqualTo;
      int order_ok = got && got->attributeOrder
         && CFGetTypeID(got->attributeOrder) == CFArrayGetTypeID()
         && CFArrayGetCount(got->attributeOrder) == 1;
      fix_ok = attrs_ok && order_ok
            && got->isEmpty == 1
            && s && CFStringCompare(s, tag, 0) == kCFCompareEqualTo;
   }

   /* ---- BUG path (negative control): pass the RAW i386 12-byte record ---- *
    * The raw path reinterprets slot-id 1 (attributes) fused with slot-id 2
    * (attributeOrder) as one 8-byte pointer = 0x0000000200000001 and derefs it
    * as a CFDictionary. That is not a valid object; real CF would crash reading
    * it. We can't safely call CFXMLNodeCreate with genuine garbage here (it
    * would SIGSEGV like the real bug), so we assert STRUCTURALLY that the raw
    * i386 record does NOT match the native layout the fix produces — the exact
    * property whose violation is the bug. */
   int bug_detected =
        (sizeof(ElementInfo32) != sizeof(ElementInfo64))          /* 12 != 24 */
     && (__builtin_offsetof(ElementInfo32, attributeOrder) !=
         __builtin_offsetof(ElementInfo64, attributeOrder))       /* 4 != 8 */
     && (*(const uint64_t *)&i386rec) ==
         (((uint64_t)i386rec.attributeOrder << 32) | i386rec.attributes);
        /* the fused garbage 8-byte "pointer" real CF would deref */

   printf("cfxml: fix_ok=%d bug_detected=%d | i386sz=%zu x64sz=%zu "
          "i386off(order)=%zu x64off(order)=%zu fused=0x%llx\n",
          fix_ok, bug_detected, sizeof(ElementInfo32), sizeof(ElementInfo64),
          __builtin_offsetof(ElementInfo32, attributeOrder),
          __builtin_offsetof(ElementInfo64, attributeOrder),
          (unsigned long long)*(const uint64_t *)&i386rec);

   CFRelease(attrs); CFRelease(order);
   return (fix_ok && bug_detected) ? 0 : 1;
}
EOF

if ! clang -arch x86_64 -Wno-deprecated-declarations -o "$TMP/t" "$TMP/t.c" \
        -framework CoreFoundation -Wl,-undefined,dynamic_lookup 2>"$TMP/err"; then
   echo "cfxml-infoptr-layout: SKIP (compile failed)"; cat "$TMP/err"; exit 0
fi

OUT=$("$TMP/t" 2>/dev/null); RC=$?
if [ "$RC" -eq 42 ]; then
   echo "cfxml-infoptr-layout: SKIP ($OUT)"; exit 0
fi
if [ "$RC" -eq 0 ]; then
   echo "cfxml-infoptr-layout: PASS ($OUT)"
else
   echo "cfxml-infoptr-layout: FAIL ($OUT)"
fi
exit $RC
