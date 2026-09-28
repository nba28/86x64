/*
 * Legacy Foundation NSMapTable / NSHashTable C-API bridge.
 *
 * A 32-bit i386 app that uses the old collection C API does two things the
 * translator can't faithfully reproduce on x86_64:
 *
 *   1. It reads the global callback structs (`NSObjectMapValueCallBacks`,
 *      `NSIntegerMapKeyCallBacks`, ...) field-by-field with 4-byte `movl`s and
 *      copies them onto the stack to pass BY VALUE to NSCreateMapTable. On
 *      x86_64 those globals live at 64-bit Foundation addresses, so the i386
 *      `movl slot,%reg` that loads the struct's ADDRESS truncates the pointer
 *      and the subsequent deref faults (iPhoto's 11th blocker:
 *      `movl (%rdx),%eax` with rdx = low32 of 0x7ff84f1bd878).
 *
 *   2. It then calls NSCreateMapTable / NSMapInsert / NSMapGet / ... with i386
 *      cdecl (all args as 4-byte stack slots, structs expanded inline). The
 *      real Foundation entry points expect the System V ABI, and the values it
 *      stores are 32-bit proxy HANDLES, not real ids.
 *
 * Fix (mirrors the function-shim + data-shadow architecture):
 *
 *   - We export low-4GB SHADOW structs `___NS<set>CallBacks` (see the globals
 *     at the bottom). static-interpose redirects the binary's non-lazy bind for
 *     each `_NS<set>CallBacks` to our shadow, so the i386 reads hit a valid
 *     low-4GB address. Each shadow is just a tagged i386-layout struct
 *     {MTBC_MAGIC, kind, ...}; the kind tells us which real Foundation callback
 *     set the app asked for.
 *
 *   - We export `___NS<fn>` trampolines (maptable_tramp.asm) that marshal the
 *     i386 cdecl frame and call the C impls below. Each impl is backed by a
 *     REAL x86_64 NSMapTable/NSHashTable created with the matching REAL
 *     Foundation callback set (recognised from the tag), so Foundation performs
 *     correct hashing/retain/release. Values cross the handle<->id boundary
 *     uniformly: unwrap on the way in (a non-handle passes through), wrap on the
 *     way out iff the real value is a genuine 64-bit pointer (>=4GB). i386 can
 *     never produce a >=4GB scalar, so any high value is a real object we stored.
 *
 * Only OBJECT/pointer storage with the standard callback sets is supported;
 * custom user callbacks fall back to pointer-identity semantics.
 */

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#import <Foundation/Foundation.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

/* proxy arena (objc_shim.c) — handle<->real, process-shared across copies */
extern uint32_t x64_objc_wrap(uint64_t real);
extern uint64_t x64_objc_unwrap(uint32_t h);
extern uint64_t _86x64_unwrap_obj_arg(uint32_t a);  /* full object resolver */

#define MTBC_MAGIC 0x6d746263u   /* 'mtbc' — tags our shadow callback structs */

/* callback-set kinds, stored in shadow word[1] */
enum {
   CB_NONE = 0,
   /* map keys */
   CB_MAP_KEY_INTEGER, CB_MAP_KEY_OBJECT, CB_MAP_KEY_NONOWNED, CB_MAP_KEY_INT,
   CB_MAP_KEY_NONRETAINED, CB_MAP_KEY_OWNEDPTR,
   /* map values */
   CB_MAP_VAL_INTEGER, CB_MAP_VAL_OBJECT, CB_MAP_VAL_NONOWNED,
   CB_MAP_VAL_NONRETAINED, CB_MAP_VAL_INT, CB_MAP_VAL_OWNEDPTR,
   /* hash */
   CB_HASH_NONOWNED, CB_HASH_OBJECT,
};

static int trace_on(void) {
   static int t = -1;
   if (t < 0) { t = getenv("MAPTABLE_TRACE") ? 1 : 0; }
   return t;
}

/* real 64-bit value -> 32-bit value the i386 caller can hold. A genuine object
 * pointer (>=4GB) becomes a proxy handle; anything that already fits passes
 * through unchanged (integers, small/zero pointers, NULL). */
static inline uint32_t bridge_out(uint64_t real) {
   if (real == 0) { return 0; }
   if (real >= 0x100000000ULL) { return x64_objc_wrap(real); }
   return (uint32_t)real;
}
/* i386 32-bit value -> real 64-bit. Uses the canonical forward-bridge resolver
 * so that EVERY i386 object representation is mapped to its real x86_64 id:
 * arena proxy handles, reverse-bridge R/S SHADOWS (i386-layout, ~0xc8xxxxxx —
 * x64_objc_unwrap alone left these untouched, so Foundation's OBJECT key/value
 * callback would objc_retain the raw i386-layout shadow and fault on its fused
 * 4-byte isa), paired/raw legacy objects, real x86 objects, else passthrough.
 * Non-object keys/values (small integers) match nothing and pass through, so
 * this is safe for keys and values of every callback kind. */
static inline const void *bridge_in(uint32_t v) {
   return (const void *)(uintptr_t)_86x64_unwrap_obj_arg(v);
}

/* Fetch a real Foundation callback global by name (returns &struct or NULL). */
static const void *real_cb(const char *name) {
   return dlsym(RTLD_DEFAULT, name);
}

static const NSMapTableKeyCallBacks *real_map_key_cb(int kind) {
   const char *n;
   switch (kind) {
   case CB_MAP_KEY_OBJECT:      n = "NSObjectMapKeyCallBacks"; break;
   case CB_MAP_KEY_NONOWNED:    n = "NSNonOwnedPointerMapKeyCallBacks"; break;
   case CB_MAP_KEY_INT:         n = "NSIntMapKeyCallBacks"; break;
   case CB_MAP_KEY_NONRETAINED: n = "NSNonRetainedObjectMapKeyCallBacks"; break;
   case CB_MAP_KEY_OWNEDPTR:    n = "NSOwnedPointerMapKeyCallBacks"; break;
   case CB_MAP_KEY_INTEGER:
   default:                     n = "NSIntegerMapKeyCallBacks"; break;
   }
   const void *p = real_cb(n);
   if (!p) { p = real_cb("NSIntegerMapKeyCallBacks"); }
   return (const NSMapTableKeyCallBacks *)p;
}
static const NSMapTableValueCallBacks *real_map_value_cb(int kind) {
   const char *n;
   switch (kind) {
   case CB_MAP_VAL_OBJECT:       n = "NSObjectMapValueCallBacks"; break;
   case CB_MAP_VAL_NONRETAINED:  n = "NSNonRetainedObjectMapValueCallBacks"; break;
   case CB_MAP_VAL_INTEGER:      n = "NSIntegerMapValueCallBacks"; break;
   case CB_MAP_VAL_INT:          n = "NSIntMapValueCallBacks"; break;
   case CB_MAP_VAL_OWNEDPTR:     n = "NSOwnedPointerMapValueCallBacks"; break;
   case CB_MAP_VAL_NONOWNED:
   default:                      n = "NSNonOwnedPointerMapValueCallBacks"; break;
   }
   const void *p = real_cb(n);
   if (!p) { p = real_cb("NSNonOwnedPointerMapValueCallBacks"); }
   return (const NSMapTableValueCallBacks *)p;
}
static const NSHashTableCallBacks *real_hash_cb(int kind) {
   const char *n;
   switch (kind) {
   case CB_HASH_OBJECT: n = "NSObjectHashCallBacks"; break;
   case CB_HASH_NONOWNED:
   default:             n = "NSNonOwnedPointerHashCallBacks"; break;
   }
   const void *p = real_cb(n);
   if (!p) { p = real_cb("NSNonOwnedPointerHashCallBacks"); }
   return (const NSHashTableCallBacks *)p;
}

static inline NSMapTable  *as_map(uint32_t h)  { return (NSMapTable  *)(uintptr_t)x64_objc_unwrap(h); }
static inline NSHashTable *as_hash(uint32_t h) { return (NSHashTable *)(uintptr_t)x64_objc_unwrap(h); }

/* ---- map table -------------------------------------------------------- */

/* NSCreateMapTable(NSMapTableKeyCallBacks key, NSMapTableValueCallBacks value,
 *                  NSUInteger capacity)
 * i386 frame: key=6 words [0..5], value=3 words [6..8], capacity=word[9]. */
uint32_t shim_NSCreateMapTable(uint32_t *a) {
   uint32_t *kCB = &a[0];
   uint32_t *vCB = &a[6];
   NSUInteger cap = a[9];
   int kkind = (kCB[0] == MTBC_MAGIC) ? (int)kCB[1] : CB_MAP_KEY_INTEGER;
   int vkind = (vCB[0] == MTBC_MAGIC) ? (int)vCB[1] : CB_MAP_VAL_NONOWNED;

   NSMapTableKeyCallBacks   kcl;
   NSMapTableValueCallBacks vcl;
   const NSMapTableKeyCallBacks   *kc = real_map_key_cb(kkind);
   const NSMapTableValueCallBacks *vc = real_map_value_cb(vkind);
   if (kc) { kcl = *kc; } else { memset(&kcl, 0, sizeof kcl); }
   if (vc) { vcl = *vc; } else { memset(&vcl, 0, sizeof vcl); }

   NSMapTable *t = NSCreateMapTable(kcl, vcl, cap);
   uint32_t h = bridge_out((uint64_t)(uintptr_t)t);
   if (trace_on()) {
      fprintf(stderr, "[mt] create key=%d val=%d cap=%lu -> %p handle=0x%x\n",
              kkind, vkind, (unsigned long)cap, (void *)t, h);
      fflush(stderr);
   }
   return h;
}

uint32_t shim_NSMapGet(uint32_t *a) {
   void *v = NSMapGet(as_map(a[0]), bridge_in(a[1]));
   return bridge_out((uint64_t)(uintptr_t)v);
}
uint32_t shim_NSMapInsert(uint32_t *a) {
   if (trace_on()) {
      fprintf(stderr, "[mt] insert table=0x%x key=0x%x->%p val=0x%x->%p\n",
              a[0], a[1], bridge_in(a[1]), a[2], bridge_in(a[2]));
      fflush(stderr);
   }
   NSMapInsert(as_map(a[0]), bridge_in(a[1]), (void *)bridge_in(a[2]));
   return 0;
}
uint32_t shim_NSMapInsertIfAbsent(uint32_t *a) {
   void *prev = NSMapInsertIfAbsent(as_map(a[0]), bridge_in(a[1]),
                                    (void *)bridge_in(a[2]));
   return bridge_out((uint64_t)(uintptr_t)prev);
}
uint32_t shim_NSMapRemove(uint32_t *a) {
   NSMapRemove(as_map(a[0]), bridge_in(a[1]));
   return 0;
}
uint32_t shim_NSResetMapTable(uint32_t *a) {
   NSResetMapTable(as_map(a[0]));
   return 0;
}
uint32_t shim_NSCountMapTable(uint32_t *a) {
   return (uint32_t)NSCountMapTable(as_map(a[0]));
}
uint32_t shim_NSFreeMapTable(uint32_t *a) {
   NSFreeMapTable(as_map(a[0]));
   return 0;
}
uint32_t shim_NSAllMapTableKeys(uint32_t *a) {
   NSArray *keys = NSAllMapTableKeys(as_map(a[0]));
   return bridge_out((uint64_t)(uintptr_t)keys);
}

/* Enumeration. The i386 NSMapEnumerator is 3 words; we stash a proxy HANDLE to
 * a heap-allocated REAL NSMapEnumerator in word[0] (the handle is always low,
 * regardless of where the allocation lands). */
uint32_t shim_NSEnumerateMapTable(uint32_t *a) {
   uint32_t sret = a[0];                 /* hidden &result (i386 ptr) */
   NSMapTable *t = as_map(a[1]);
   NSMapEnumerator *re = (NSMapEnumerator *)malloc(sizeof *re);
   if (re) { *re = NSEnumerateMapTable(t); }
   uint32_t *e = (uint32_t *)(uintptr_t)sret;
   e[0] = re ? x64_objc_wrap((uint64_t)(uintptr_t)re) : 0;
   e[1] = 0;
   e[2] = 0;
   return sret;
}
uint32_t shim_NSNextMapEnumeratorPair(uint32_t *a) {
   uint32_t *e = (uint32_t *)(uintptr_t)a[0];
   NSMapEnumerator *re = (NSMapEnumerator *)(uintptr_t)x64_objc_unwrap(e[0]);
   if (!re) { return 0; }
   void *k = NULL, *v = NULL;
   BOOL more = NSNextMapEnumeratorPair(re, &k, &v);
   if (more) {
      if (a[1]) { *(uint32_t *)(uintptr_t)a[1] = bridge_out((uint64_t)(uintptr_t)k); }
      if (a[2]) { *(uint32_t *)(uintptr_t)a[2] = bridge_out((uint64_t)(uintptr_t)v); }
   }
   return (uint32_t)more;
}
uint32_t shim_NSEndMapTableEnumeration(uint32_t *a) {
   uint32_t *e = (uint32_t *)(uintptr_t)a[0];
   NSMapEnumerator *re = (NSMapEnumerator *)(uintptr_t)x64_objc_unwrap(e[0]);
   if (re) { NSEndMapTableEnumeration(re); free(re); e[0] = 0; }
   return 0;
}

/* ---- hash table ------------------------------------------------------- */

/* NSCreateHashTable(NSHashTableCallBacks callBacks, NSUInteger capacity)
 * i386 frame: callBacks=5 words [0..4], capacity=word[5]. */
uint32_t shim_NSCreateHashTable(uint32_t *a) {
   uint32_t *cb = &a[0];
   NSUInteger cap = a[5];
   int kind = (cb[0] == MTBC_MAGIC) ? (int)cb[1] : CB_HASH_NONOWNED;
   NSHashTableCallBacks cl;
   const NSHashTableCallBacks *c = real_hash_cb(kind);
   if (c) { cl = *c; } else { memset(&cl, 0, sizeof cl); }
   NSHashTable *t = NSCreateHashTable(cl, cap);
   uint32_t h = bridge_out((uint64_t)(uintptr_t)t);
   if (trace_on()) {
      fprintf(stderr, "[ht] create kind=%d cap=%lu -> %p handle=0x%x\n",
              kind, (unsigned long)cap, (void *)t, h);
      fflush(stderr);
   }
   return h;
}
uint32_t shim_NSHashGet(uint32_t *a) {
   void *v = NSHashGet(as_hash(a[0]), bridge_in(a[1]));
   return bridge_out((uint64_t)(uintptr_t)v);
}
uint32_t shim_NSHashInsert(uint32_t *a) {
   NSHashInsert(as_hash(a[0]), bridge_in(a[1]));
   return 0;
}
uint32_t shim_NSHashRemove(uint32_t *a) {
   NSHashRemove(as_hash(a[0]), bridge_in(a[1]));
   return 0;
}
uint32_t shim_NSCountHashTable(uint32_t *a) {
   return (uint32_t)NSCountHashTable(as_hash(a[0]));
}
uint32_t shim_NSFreeHashTable(uint32_t *a) {
   NSFreeHashTable(as_hash(a[0]));
   return 0;
}
uint32_t shim_NSEnumerateHashTable(uint32_t *a) {
   uint32_t sret = a[0];
   NSHashTable *t = as_hash(a[1]);
   NSHashEnumerator *re = (NSHashEnumerator *)malloc(sizeof *re);
   if (re) { *re = NSEnumerateHashTable(t); }
   uint32_t *e = (uint32_t *)(uintptr_t)sret;
   e[0] = re ? x64_objc_wrap((uint64_t)(uintptr_t)re) : 0;
   e[1] = 0;
   e[2] = 0;
   return sret;
}
uint32_t shim_NSNextHashEnumeratorItem(uint32_t *a) {
   uint32_t *e = (uint32_t *)(uintptr_t)a[0];
   NSHashEnumerator *re = (NSHashEnumerator *)(uintptr_t)x64_objc_unwrap(e[0]);
   if (!re) { return 0; }
   void *it = NSNextHashEnumeratorItem(re);
   return bridge_out((uint64_t)(uintptr_t)it);
}
uint32_t shim_NSEndHashTableEnumeration(uint32_t *a) {
   uint32_t *e = (uint32_t *)(uintptr_t)a[0];
   NSHashEnumerator *re = (NSHashEnumerator *)(uintptr_t)x64_objc_unwrap(e[0]);
   if (re) { NSEndHashTableEnumeration(re); free(re); e[0] = 0; }
   return 0;
}

/* ---- tagged callback-struct shadows ----------------------------------- */
/* Exported under ___NS<set>CallBacks (three underscores); static-interpose
 * redirects the binary's non-lazy bind for _NS<set>CallBacks here. Each is an
 * i386-layout struct: word[0]=MTBC_MAGIC, word[1]=kind. Sized to the i386 width
 * of its struct (key=6, value=3, hash=5 words) so the i386 field copies stay
 * in bounds. */
#define VAL_SHADOW(asmname, kind) \
   uint32_t asmname##_shadow[3] __asm__("___" #asmname) = { MTBC_MAGIC, (kind), 0 }
#define KEY_SHADOW(asmname, kind) \
   uint32_t asmname##_shadow[6] __asm__("___" #asmname) = { MTBC_MAGIC, (kind), 0, 0, 0, 0 }
#define HASH_SHADOW(asmname, kind) \
   uint32_t asmname##_shadow[5] __asm__("___" #asmname) = { MTBC_MAGIC, (kind), 0, 0, 0 }

VAL_SHADOW(NSObjectMapValueCallBacks,        CB_MAP_VAL_OBJECT);
VAL_SHADOW(NSNonOwnedPointerMapValueCallBacks, CB_MAP_VAL_NONOWNED);
VAL_SHADOW(NSNonRetainedObjectMapValueCallBacks, CB_MAP_VAL_NONRETAINED);
VAL_SHADOW(NSIntegerMapValueCallBacks,       CB_MAP_VAL_INTEGER);
VAL_SHADOW(NSIntMapValueCallBacks,           CB_MAP_VAL_INT);
VAL_SHADOW(NSOwnedPointerMapValueCallBacks,  CB_MAP_VAL_OWNEDPTR);

KEY_SHADOW(NSObjectMapKeyCallBacks,          CB_MAP_KEY_OBJECT);
KEY_SHADOW(NSIntegerMapKeyCallBacks,         CB_MAP_KEY_INTEGER);
KEY_SHADOW(NSNonOwnedPointerMapKeyCallBacks, CB_MAP_KEY_NONOWNED);
KEY_SHADOW(NSIntMapKeyCallBacks,             CB_MAP_KEY_INT);
KEY_SHADOW(NSNonRetainedObjectMapKeyCallBacks, CB_MAP_KEY_NONRETAINED);
KEY_SHADOW(NSOwnedPointerMapKeyCallBacks,    CB_MAP_KEY_OWNEDPTR);

HASH_SHADOW(NSNonOwnedPointerHashCallBacks,  CB_HASH_NONOWNED);
HASH_SHADOW(NSObjectHashCallBacks,           CB_HASH_OBJECT);

/* ===========================================================================
 * Legacy "old-style" NSUserDefaults locale compat (generic, NOT app-specific).
 *
 * Pre-10.4 apps (iPhoto, etc.) read date/number formatting from the old
 * NSUserDefaults locale keys (NSDateTimeOrdering, NSMonthNameArray,
 * NSShortDateFormatString, ...). Modern macOS dropped them: every one reads
 * back nil, so the app computes nil/garbage and crashes.
 *
 * We DERIVE the values from the LIVE system locale (NSLocale/NSDateFormatter)
 * so this works for any app AND any locale — no hard-coded en_US table. The
 * modern locale exposes Unicode (CLDR) date patterns + symbol arrays; the
 * legacy keys want strftime-style ("%m/%d/%y") format strings, so we convert.
 *
 * Delivery is by SWIZZLING -[NSUserDefaults objectForKey:]: call the original,
 * and only when it returns nil for one of the legacy keys substitute the
 * derived value. (-registerDefaults: silently no-ops this early in these
 * processes; the swizzle is robust and a real user/app value still wins because
 * the original is consulted first.) Self-guarded.
 * =========================================================================== */

/* Convert a Unicode (CLDR) date pattern to a legacy strftime-style string. */
static NSString *mt_uni_to_strftime(NSString *pat) {
   if (!pat) { return @""; }
   NSMutableString *out = [NSMutableString string];
   NSUInteger n = [pat length];
   for (NSUInteger i = 0; i < n; ) {
      unichar c = [pat characterAtIndex:i];
      if (c == '\'') {                       /* quoted literal */
         i++;
         while (i < n) {
            unichar d = [pat characterAtIndex:i];
            if (d == '\'') {
               if (i + 1 < n && [pat characterAtIndex:i + 1] == '\'') {
                  [out appendString:@"'"]; i += 2; continue;
               }
               i++; break;
            }
            [out appendFormat:@"%C", d]; i++;
         }
         continue;
      }
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
         NSUInteger j = i;
         while (j < n && [pat characterAtIndex:j] == c) { j++; }
         NSUInteger w = j - i;
         const char *rep = "";
         switch (c) {
         case 'y': case 'Y': case 'u': rep = (w <= 2) ? "%y" : "%Y"; break;
         case 'M': case 'L': rep = (w >= 4) ? "%B" : (w == 3) ? "%b" : "%m"; break;
         case 'd':           rep = "%d"; break;
         case 'E': case 'c': rep = (w >= 4) ? "%A" : "%a"; break;
         case 'a':           rep = "%p"; break;
         case 'h': case 'K': rep = "%I"; break;
         case 'H': case 'k': rep = "%H"; break;
         case 'm':           rep = "%M"; break;
         case 's':           rep = "%S"; break;
         case 'z': case 'Z': case 'v': case 'V': rep = "%Z"; break;
         default:            rep = ""; break;   /* era/quarter/etc.: drop */
         }
         [out appendFormat:@"%s", rep];
         i = j; continue;
      }
      [out appendFormat:@"%C", c]; i++;        /* separators pass through */
   }
   return out;
}

/* Derive the legacy NSDateTimeOrdering (e.g. "MDYH") from the live patterns:
 * the order in which month/day/year/hour first appear. */
static NSString *mt_derive_ordering(NSString *datePat, NSString *timePat) {
   NSMutableString *o = [NSMutableString string];
   NSString *comb = [(datePat ?: @"") stringByAppendingString:(timePat ?: @"")];
   BOOL m = NO, d = NO, y = NO, h = NO;
   for (NSUInteger i = 0; i < [comb length]; i++) {
      unichar c = [comb characterAtIndex:i];
      if ((c == 'M' || c == 'L') && !m) { [o appendString:@"M"]; m = YES; }
      else if (c == 'd' && !d)          { [o appendString:@"D"]; d = YES; }
      else if ((c == 'y' || c == 'Y' || c == 'u') && !y) { [o appendString:@"Y"]; y = YES; }
      else if ((c == 'H' || c == 'h' || c == 'k' || c == 'K') && !h) { [o appendString:@"H"]; h = YES; }
   }
   if ([o length] == 0) { [o appendString:@"MDYH"]; }   /* sane fallback */
   return o;
}

static NSDictionary *mt_build_legacy_locale(void) {
   NSLocale *loc = [NSLocale currentLocale];
   NSDateFormatter *df = [[[NSDateFormatter alloc] init] autorelease];
   [df setLocale:loc];

   #define PAT(ds, ts) (^{ [df setDateStyle:(ds)]; [df setTimeStyle:(ts)]; \
                           return [df dateFormat]; }())
   NSString *shortDate = PAT(NSDateFormatterShortStyle,  NSDateFormatterNoStyle);
   NSString *fullDate  = PAT(NSDateFormatterFullStyle,   NSDateFormatterNoStyle);
   NSString *timeP     = PAT(NSDateFormatterNoStyle,     NSDateFormatterMediumStyle);
   NSString *medDT     = PAT(NSDateFormatterMediumStyle, NSDateFormatterMediumStyle);
   NSString *shortDT   = PAT(NSDateFormatterShortStyle,  NSDateFormatterShortStyle);
   #undef PAT

   NSString *sep   = [loc objectForKey:NSLocaleDecimalSeparator]  ?: @".";
   NSString *grp   = [loc objectForKey:NSLocaleGroupingSeparator] ?: @",";
   NSString *cursym= [loc objectForKey:NSLocaleCurrencySymbol]    ?: @"$";
   NSString *curcod= [loc objectForKey:NSLocaleCurrencyCode]      ?: @"USD";

   NSMutableDictionary *d = [NSMutableDictionary dictionary];
   #define SET(k, v) do { id _v = (v); if (_v) d[k] = _v; } while (0)
   SET(@"NSDateTimeOrdering",          mt_derive_ordering(shortDate, timeP));
   SET(@"NSAMPMDesignation",           (@[ [df AMSymbol] ?: @"AM",
                                           [df PMSymbol] ?: @"PM" ]));
   SET(@"NSMonthNameArray",            [df monthSymbols]);
   SET(@"NSShortMonthNameArray",       [df shortMonthSymbols]);
   SET(@"NSWeekDayNameArray",          [df weekdaySymbols]);
   SET(@"NSShortWeekDayNameArray",     [df shortWeekdaySymbols]);
   SET(@"NSTimeFormatString",          mt_uni_to_strftime(timeP));
   SET(@"NSDateFormatString",          mt_uni_to_strftime(fullDate));
   SET(@"NSShortDateFormatString",     mt_uni_to_strftime(shortDate));
   SET(@"NSTimeDateFormatString",      mt_uni_to_strftime(medDT));
   SET(@"NSShortTimeDateFormatString", mt_uni_to_strftime(shortDT));
   SET(@"NSDecimalSeparator",          sep);
   SET(@"NSThousandsSeparator",        grp);
   SET(@"NSCurrencySymbol",            cursym);
   SET(@"NSInternationalCurrencyString", curcod);
   /* Natural-language designations are en-centric and rarely consulted by the
    * crashing format path; provide harmless defaults so lookups don't return
    * nil. */
   SET(@"NSThisDayDesignations",       (@[@"today", @"now"]));
   SET(@"NSNextDayDesignations",       (@[@"tomorrow"]));
   SET(@"NSPriorDayDesignations",      (@[@"yesterday"]));
   SET(@"NSYearMonthWeekDesignations", (@[@"year", @"month", @"week"]));
   SET(@"NSEarlierTimeDesignations",   (@[@"prior", @"last", @"past", @"ago"]));
   SET(@"NSLaterTimeDesignations",     (@[@"next"]));
   #undef SET
   return d;
}

static NSDictionary *g_legacy_locale;                 /* retained, live-derived */
static id (*g_orig_objectForKey)(id, SEL, id);        /* original IMP */

static id mt_compat_objectForKey(id self, SEL _cmd, id key) {
   id v = g_orig_objectForKey ? g_orig_objectForKey(self, _cmd, key) : nil;
   if (!v && key) {
      /* In these translated processes NSUserDefaults' own search-list lookup for
       * the app (persistent) domain comes back nil even though the value is
       * present one layer down in CFPreferences: -[NSUserDefaults objectForKey:]
       * returns nil for EVERY key while CFPreferencesCopyAppValue and
       * -dictionaryRepresentation both have it (the NSUserDefaults source cache
       * for the app domain is never populated). That left iPhoto unable to read
       * RootDirectory -> "Your photo library is missing". Fall back to the
       * CFPreferences app domain — exactly what objectForKey: should consult —
       * for the standard defaults. */
      if (self == [NSUserDefaults standardUserDefaults] &&
          [key isKindOfClass:[NSString class]]) {
         CFTypeRef cf = CFPreferencesCopyAppValue((CFStringRef)key,
                                                  kCFPreferencesCurrentApplication);
         if (cf) { return [(id)cf autorelease]; }  /* Copy is +1: balance it */
      }
      id sub = [g_legacy_locale objectForKey:key];
      if (sub) { return sub; }
   }
   return v;
}

void legacy_locale_compat_install(void) {
   static int done = 0;
   if (done) { return; }
   Class cUD = objc_getClass("NSUserDefaults");
   if (!cUD) { return; }                  /* Foundation not up yet: retry */

   /* Swizzle -[NSUserDefaults objectForKey:] in EXACTLY ONE libabiconv copy
    * process-wide. Every copy runs this installer; if more than one swizzles,
    * each chains mt_compat onto the previous copy's mt_compat, and a broken
    * link in that chain makes objectForKey: return nil for ALL keys (defaults
    * become unreadable -> iPhoto can't read RootDirectory -> "library missing").
    * The winner of the atomic claim captures the REAL Foundation IMP. */
   extern int objc_shared_claim_locale_ofk(void);   /* objc_shim.c */
   if (!objc_shared_claim_locale_ofk()) { done = 1; return; }
   done = 1;

   g_legacy_locale = [mt_build_legacy_locale() retain];

   Method m = class_getInstanceMethod(cUD, sel_registerName("objectForKey:"));
   if (m) {
      g_orig_objectForKey = (id (*)(id, SEL, id))
         method_setImplementation(m, (IMP)mt_compat_objectForKey);
   }
   if (getenv("OBJC_BRIDGE_TRACE")) {
      fprintf(stderr, "[compat] legacy locale objectForKey: swizzled "
              "(%lu keys, orig=%p)\n",
              (unsigned long)[g_legacy_locale count], (void *)g_orig_objectForKey);
   }
}

/* x64_safe_remove_rect — call [self sel:tag] guarded by @try/@catch.
 *
 * objc_shim.c's bp_track_tag (NSTrackingRectTag/NSToolTipTag remove side) needs
 * to swallow the modern-AppKit NSInternalInconsistencyException thrown when a
 * STALE/already-removed tag is passed to -[NSView removeTrackingRect:] /
 * -removeToolTipRect:. Old macOS (the i386 era) silently ignored an invalid
 * remove; modern AppKit throws and aborts the app (iPhoto startup: a gone
 * _NSTrackingAreaAKViewHelper tag from the "remove old before adding new" idiom).
 * objc_shim.c is plain C, so the @try/@catch lives here (an ObjC unit) and it
 * calls this. The throw unwinds only NATIVE frames (AppKit -> here), so no
 * reverse-IMP frame / rsp-stash entry is abandoned. Dead/invalid surface must
 * DEGRADE, not crash. Universal: any i386 app removing tracking/tooltip rects. */
void x64_safe_remove_rect(id self, SEL sel, long tag) {
   @try {
      ((void (*)(id, SEL, long))objc_msgSend)(self, sel, tag);
   } @catch (NSException *e) {
      (void)e;   /* legacy silent-invalid-remove leniency */
   }
}
