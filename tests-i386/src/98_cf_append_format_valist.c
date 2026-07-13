/*
 * 98_cf_append_format_valist — the CFStringAppendFormatAndArguments append path
 * that BUILDS Civ IV's standard-alert title + message (the "blank alert"
 * follow-on to the fixed CreateStandardAlert crash).
 *
 * Civ's internal "show formatted standard alert" routine (disasm-confirmed at
 * translated 0x1000c2a0) builds BOTH the `error` (title) and `explanation`
 * (message) CFStringRefs it hands to CreateStandardAlert like this:
 *     CFMutableStringRef s = CFStringCreateMutable(NULL, 0);
 *     CFStringAppendFormatAndArguments(s, NULL, fmt, va_list);
 * If that append produces an EMPTY string, the alert renders with an icon and a
 * default OK button but NO title/message text — exactly what the tester saw. So this
 * guard asserts the append path DELIVERS CONTENT (not merely "doesn't crash"):
 * it reproduces the create-mutable -> append-format-with-va_list -> read-back
 * chain and checks the exact bytes.
 *
 * CFStringAppendFormatAndArguments takes an explicit va_list (abigen can't
 * marshal one -> hand-shim shim_CFStringAppendFormatAndArguments in objc_shim.c,
 * sibling of the CFStringCreateWithFormatAndArguments shim guarded by 53). The
 * mutable string round-trips as a wrapped proxy handle across both calls, so
 * this also exercises the wrap/unwrap identity of a mutable CF object through
 * two consecutive C-function shims (the alert-string handle Civ reuses).
 *
 * CoreFoundation isn't in the i386 sysroot, so every CF symbol is an undefined
 * dynamic_lookup import resolved at translate time by static-interpose ->
 * libabiconv's ___CF* shims (see the Makefile rule, like 53). Validated by both
 * stdout content and exit code (all scalar/%@ args — float varargs are the
 * separate known gap; not used here).
 */
#include <stdarg.h>

extern int  printf(const char *, ...);
extern void exit(int status);

typedef const void       *CFStringRef;
typedef void             *CFMutableStringRef;
typedef const void       *CFAllocatorRef;
typedef const void       *CFDictionaryRef;
typedef unsigned char     Boolean;
typedef long              CFIndex;
typedef unsigned int      CFStringEncoding;

#define kCFStringEncodingASCII 0x0600u

extern CFStringRef        CFStringCreateWithCString(CFAllocatorRef, const char *,
                                                    CFStringEncoding);
extern CFMutableStringRef CFStringCreateMutable(CFAllocatorRef, CFIndex);
extern void               CFStringAppendFormatAndArguments(CFMutableStringRef,
                                                           CFDictionaryRef,
                                                           CFStringRef, va_list);
extern CFIndex            CFStringGetLength(CFStringRef);
extern Boolean            CFStringGetCString(CFStringRef, char *, CFIndex,
                                             CFStringEncoding);

/* Civ's builder shape: variadic wrapper -> real i386 va_list -> append-format. */
static void build_into(CFMutableStringRef s, CFStringRef fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   CFStringAppendFormatAndArguments(s, (CFDictionaryRef)0, fmt, ap);
   va_end(ap);
}

int main(void)
{
   /* (1) the title: a %@-substituted message (a CFStringRef arg through the
    * va_list, unwrapped by the shim's fill_format_varargs %@ path) */
   CFStringRef who = CFStringCreateWithCString((CFAllocatorRef)0, "Civ",
                                               kCFStringEncodingASCII);
   CFStringRef fmt1 = CFStringCreateWithCString((CFAllocatorRef)0,
                                                "Cannot load %@ (err %d)",
                                                kCFStringEncodingASCII);
   if (!who || !fmt1) { exit(90); }
   CFMutableStringRef title = CFStringCreateMutable((CFAllocatorRef)0, 0);
   if (!title) { exit(91); }
   build_into(title, fmt1, who, 42);

   /* (2) the message: a plain localized string with NO % (the shim's
    * CFStringAppend fast-path) — must still deliver its bytes */
   CFStringRef fmt2 = CFStringCreateWithCString((CFAllocatorRef)0,
                                                "Please reinstall.",
                                                kCFStringEncodingASCII);
   if (!fmt2) { exit(92); }
   CFMutableStringRef msg = CFStringCreateMutable((CFAllocatorRef)0, 0);
   if (!msg) { exit(93); }
   build_into(msg, fmt2, 0);

   char tbuf[128] = {0}, mbuf[128] = {0};
   Boolean tok = CFStringGetCString(title, tbuf, sizeof tbuf, kCFStringEncodingASCII);
   Boolean mok = CFStringGetCString(msg,   mbuf, sizeof mbuf, kCFStringEncodingASCII);

   printf("title.len=%ld ok=%d \"%s\"\n", (long)CFStringGetLength(title),
          tok ? 1 : 0, tbuf);
   printf("msg.len=%ld ok=%d \"%s\"\n", (long)CFStringGetLength(msg),
          mok ? 1 : 0, mbuf);
   exit(0);
}
