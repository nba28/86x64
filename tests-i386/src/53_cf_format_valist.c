/*
 * 53_cf_format_valist — the va_list CF-formatting shim
 * (CFStringCreateWithFormatAndArguments), the Msg%s / Halo-wall family.
 *
 * CFStringCreateWithFormat is VARIADIC (hand-shimmed already); its sibling
 * CFStringCreateWithFormatAndArguments takes an explicit va_list. abigen cannot
 * marshal a va_list arg — an i386 va_list is a plain `char *` pointing straight
 * at the first vararg on the i386 stack (contiguous 4-byte slots), whereas an
 * x86_64 va_list is a 4-field __va_list_tag register-save-area struct — so
 * abigen's auto-shim deep-copied garbage bytes as a __va_list_tag and native CF
 * dereferenced a fused overflow_arg_area (the Portal-2-class SIGSEGV).
 *
 * FIX: a hand-shim reads the varargs through the i386 va_list pointer and drives
 * the SAME CF-format expansion the variadic CFStringCreateWithFormat shim uses
 * (positional %N$, %@ handle resolution, %s/%d), then makes the variadic native
 * call — no x86_64 va_list synthesized.
 *
 * This exercises it end-to-end: a variadic wrapper builds a real i386 va_list and
 * calls CFStringCreateWithFormatAndArguments; the result is read back with
 * CFStringGetCString and compared. A marshalling bug garbles the string or faults.
 * CoreFoundation is not in the i386 sysroot, so the CF symbols are undefined
 * dynamic_lookup imports resolved by static-interpose -> libabiconv's ___CF*
 * shims (see the Makefile rule). Validation by EXIT CODE (float printf varargs
 * are a separate gap; all scalars here): 55 == the formatted string is exact.
 */
#include <stdarg.h>
extern void exit(int status);
extern unsigned long strlen(const char *);
extern int strcmp(const char *, const char *);

typedef const void       *CFStringRef;
typedef const void       *CFAllocatorRef;
typedef const void       *CFDictionaryRef;
typedef unsigned char     Boolean;
typedef long              CFIndex;
typedef unsigned int      CFStringEncoding;

#define kCFStringEncodingASCII 0x0600u

extern CFStringRef CFStringCreateWithCString(CFAllocatorRef, const char *,
                                             CFStringEncoding);
extern CFStringRef CFStringCreateWithFormatAndArguments(CFAllocatorRef,
                                                        CFDictionaryRef,
                                                        CFStringRef, va_list);
extern Boolean CFStringGetCString(CFStringRef, char *, CFIndex, CFStringEncoding);

/* variadic wrapper -> builds a real i386 va_list and calls the va_list CF fn */
static CFStringRef fmt_va(CFStringRef fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   CFStringRef s = CFStringCreateWithFormatAndArguments(
      (CFAllocatorRef)0, (CFDictionaryRef)0, fmt, ap);
   va_end(ap);
   return s;
}

int main(void) {
   CFStringRef fmt =
      CFStringCreateWithCString((CFAllocatorRef)0, "n=%d s=%s x=%d", kCFStringEncodingASCII);
   if (!fmt) { exit(100); }

   CFStringRef r = fmt_va(fmt, 42, "hi", 7);   /* -> "n=42 s=hi x=7" */
   if (!r) { exit(101); }

   char buf[128];
   if (!CFStringGetCString(r, buf, (CFIndex)sizeof buf, kCFStringEncodingASCII)) {
      exit(102);
   }

   int ok = (strcmp(buf, "n=42 s=hi x=7") == 0);
   exit(ok ? 55 : 7);
}
