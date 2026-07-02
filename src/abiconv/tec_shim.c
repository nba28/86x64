// tec_shim.c — hand shims for the classic Text Encoding Converter (TEC) C API.
//
// The TEC survives in the modern x86_64 CarbonCore (deprecated), but abigen
// cannot emit marshalling shims for parts of its surface: e.g.
// ConvertFromUnicodeToText declares `const UniChar iUnicodeStr[]` /
// `ByteOffset oOffsetArray[]` incomplete-array parameters, which abigen's
// sizeof_type rejects ("incomplete array"). Halo (and other Carbon-era apps)
// call it for script-text conversion, so an unshimmed bind is an i386-frame
// over-pop crash. All twelve parameters are 4-byte i386 slots (opaque ref,
// ULong counts/flags, buffer pointers), and every pointer is a low-4GB i386
// address that is directly valid natively, so a hand forward is exact.
//
// MTSHIM convention: rdi -> &i386 args[0]; result in eax.

#include <stdint.h>
#include <dlfcn.h>

#define PTR(n) ((void *)(uintptr_t)args[(n)])

extern uint64_t _86x64_unwrap_obj_arg(uint32_t a);   // objc_shim.c: proxy handle -> real ref

// OSStatus ConvertFromUnicodeToText(UnicodeToTextInfo, ByteCount, const UniChar[],
//     OptionBits, ItemCount, const ByteOffset[], ItemCount*, ByteOffset[],
//     ByteCount, ByteCount*, ByteCount*, LogicalAddress)
// The UnicodeToTextInfo ref was minted by CreateUnicodeToTextInfo through an
// abigen shim, whose >4GB copy-back wraps the native ref into a proxy handle —
// so unwrap the first arg before handing it back to native code. Resolved via
// dlsym (deprecated 32-bit-era export the modern SDK .tbd may hide) with a
// paramErr fallback (its documented "bad converter" answer) if truly absent.
uint32_t shim_ConvertFromUnicodeToText(uint32_t *args) {
    typedef int32_t (*fn_t)(uint64_t, uint64_t, void *, uint64_t, uint64_t, void *,
                            void *, void *, uint64_t, void *, void *, void *);
    static fn_t fn; static int looked;
    if (!looked) { fn = (fn_t)dlsym(RTLD_DEFAULT, "ConvertFromUnicodeToText"); looked = 1; }
    if (!fn) return (uint32_t)-50;   // paramErr
    return (uint32_t)fn(_86x64_unwrap_obj_arg(args[0]),   // iUnicodeToTextInfo
                        args[1],                          // iUnicodeLen
                        PTR(2),                           // iUnicodeStr
                        args[3],                          // iControlFlags
                        args[4],                          // iOffsetCount
                        PTR(5),                           // iOffsetArray (may be NULL)
                        PTR(6),                           // oOffsetCount (may be NULL)
                        PTR(7),                           // oOffsetArray (may be NULL)
                        args[8],                          // iOutputBufLen
                        PTR(9),                           // oInputRead
                        PTR(10),                          // oOutputLen
                        PTR(11));                         // oOutputStr
}
