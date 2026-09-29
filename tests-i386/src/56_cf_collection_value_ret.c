/*
 * 56_cf_collection_value_ret — abigen RETURN-side wrap for CF-collection value
 * accessors that hand back a bare `const void *` (the Halo __CFGetTypeID wall).
 *
 * The CF collection getters
 *     const void *CFArrayGetValueAtIndex(CFArrayRef, CFIndex);
 *     const void *CFDictionaryGetValue(CFDictionaryRef, const void *key);
 * return the stored element as a raw `const void *`. When that element is a
 * native CF object it lives in the CF heap ABOVE 4GB. abigen's return classifier
 * wrapped typed `CFFooRef` / objc / char* returns, but a bare `const void *` /
 * `CFTypeRef` return fell through UNWRAPPED, so the native >4GB pointer was
 * handed back RAW in rax; the i386 caller reads eax = the truncated low-32, and
 * the next CFGetTypeID(truncated) derefs obj+8 (CFRuntimeBase _cfinfo) at a wild
 * low address -> EXC_BAD_ACCESS (Halo crashed in libabiconv `__CFGetTypeID`).
 *
 * FIX: the return classifier now applies the SAME conditional `_x64_objc_wrap`
 * (only a >4GB pointer is wrapped; a low data/context void* passes through) to
 * raw `const void*` / `CFTypeRef`-typedef returns. The wrapped >4GB ref becomes
 * a low-4GB proxy handle that the CF-arg unwrap (convert_cf_ptr) restores on the
 * next call — so CFGetTypeID sees the real object and returns the right id.
 *
 * Both collections are built with NULL callbacks so no kCFType*CallBacks data
 * symbol (an un-shadowed struct const) is needed; the stored values are real
 * heap CFMutableArrays (never tagged pointers), guaranteed to live >4GB, so the
 * truncation path WOULD fault without the fix. The arg-side void* unwrap (already
 * present: ignore.structs lists `void`/`const void`, routed through
 * _x64_objc_unwrap) restores the handle going INTO the collection; this test
 * exercises the symmetric RETURN side coming back OUT.
 *
 * The CALLBACK side is the same contract: CFArrayApplyFunction /
 * CFDictionaryApplyFunction hand each element to the i386 applier as a
 * `const void *` callback ARG, which the callback bridge used to truncate
 * (CBA_PTR) — Halo's HID element walk faulted in CFGetTypeID once IOKit
 * matching forwarded for real. Such an arg is now CBA_OBJ (conditional wrap).
 * And the OUT-PARAM side: CFDictionaryGetValueIfPresent's `const void **value`
 * was copied back truncated (the dict Halo got back faulted in its next call).
 *
 * CoreFoundation is not in the i386 sysroot, so the CF symbols are undefined
 * dynamic_lookup imports resolved at translate time by static-interpose ->
 * libabiconv's ___CF* shims (see the Makefile rule), exactly as a real binary's
 * CF binds. Validation is by EXIT CODE: each of the two getters contributes a
 * distinct bit; all-correct == 31 (1,2 = getters, 4,8 = appliers, 16 = out-param). Without the fix the truncated pointer faults
 * (or yields a wrong type id), clearing its bit.
 */
extern void exit(int status);

typedef const void *CFTypeRef;
typedef unsigned long CFTypeID;
typedef long CFIndex;
typedef const struct __CFAllocator    *CFAllocatorRef;
typedef struct __CFArray              *CFMutableArrayRef;
typedef const struct __CFArray        *CFArrayRef;
typedef struct __CFDictionary         *CFMutableDictionaryRef;
typedef const struct __CFDictionary   *CFDictionaryRef;

extern CFMutableArrayRef CFArrayCreateMutable(CFAllocatorRef, CFIndex, const void *callBacks);
extern void              CFArrayAppendValue(CFMutableArrayRef, const void *value);
extern const void       *CFArrayGetValueAtIndex(CFArrayRef, CFIndex);
extern CFTypeID          CFArrayGetTypeID(void);

extern CFMutableDictionaryRef CFDictionaryCreateMutable(CFAllocatorRef, CFIndex,
                                                        const void *keyCB,
                                                        const void *valCB);
extern void              CFDictionarySetValue(CFMutableDictionaryRef,
                                              const void *key, const void *value);
extern const void       *CFDictionaryGetValue(CFDictionaryRef, const void *key);
extern unsigned char     CFDictionaryGetValueIfPresent(CFDictionaryRef,
                                                       const void *key,
                                                       const void **value);

extern CFTypeID          CFGetTypeID(CFTypeRef);

typedef struct { CFIndex location, length; } CFRange;
extern void CFArrayApplyFunction(CFArrayRef, CFRange,
                                 void (*)(const void *value, void *ctx), void *ctx);
extern void CFDictionaryApplyFunction(CFDictionaryRef,
                                      void (*)(const void *key, const void *value,
                                               void *ctx), void *ctx);

static CFTypeID g_arrTID;

/* ctx is the i386 side's own low pointer: it must come back unchanged */
static void arr_applier(const void *value, void *ctx) {
   if (CFGetTypeID((CFTypeRef)value) == g_arrTID) { ++*(int *)ctx; }
}

static void dict_applier(const void *key, const void *value, void *ctx) {
   if (CFGetTypeID((CFTypeRef)key) == g_arrTID &&
       CFGetTypeID((CFTypeRef)value) == g_arrTID) { ++*(int *)ctx; }
}

int main(void) {
   const CFTypeID arrTID = CFArrayGetTypeID();

   /* --- CFArrayGetValueAtIndex: raw const void* return --- */
   CFMutableArrayRef inner = CFArrayCreateMutable((CFAllocatorRef)0, 0, (const void *)0);
   CFMutableArrayRef outer = CFArrayCreateMutable((CFAllocatorRef)0, 0, (const void *)0);
   CFArrayAppendValue(outer, inner);
   const void *got = CFArrayGetValueAtIndex((CFArrayRef)outer, 0);
   int t1 = (CFGetTypeID((CFTypeRef)got) == arrTID) ? 1 : 0;

   /* --- CFDictionaryGetValue: raw const void* return (pointer-equal key) --- */
   CFMutableDictionaryRef dict =
      CFDictionaryCreateMutable((CFAllocatorRef)0, 0, (const void *)0, (const void *)0);
   CFMutableArrayRef keyObj = CFArrayCreateMutable((CFAllocatorRef)0, 0, (const void *)0);
   CFMutableArrayRef valObj = CFArrayCreateMutable((CFAllocatorRef)0, 0, (const void *)0);
   CFDictionarySetValue(dict, keyObj, valObj);
   const void *dv = CFDictionaryGetValue((CFDictionaryRef)dict, keyObj);
   int t2 = (CFGetTypeID((CFTypeRef)dv) == arrTID) ? 1 : 0;

   /* --- appliers: element handed to the i386 callback as a const void* arg --- */
   g_arrTID = arrTID;
   int n3 = 0, n4 = 0;
   CFRange all = { 0, 1 };
   CFArrayApplyFunction((CFArrayRef)outer, all, arr_applier, &n3);
   CFDictionaryApplyFunction((CFDictionaryRef)dict, dict_applier, &n4);
   int t3 = n3 == 1, t4 = n4 == 1;

   /* --- out-param: element written through a const void ** --- */
   const void *ov = 0;
   int t5 = CFDictionaryGetValueIfPresent((CFDictionaryRef)dict, keyObj, &ov) &&
            CFGetTypeID((CFTypeRef)ov) == arrTID;

   exit(t1 * 1 + t2 * 2 + t3 * 4 + t4 * 8 + t5 * 16);   /* 31 with the fix */
   return 0;
}
