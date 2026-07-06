/* CFStringCreateWithFormat is VARIADIC, so abigen skips it and the bind
 * went raw into native CoreFoundation: the i386 caller passes varargs on
 * the cdecl STACK, native reads SysV registers -> garbage args and a crash
 * inside CFStringCreateWithFormatAndArguments (Halo, 2026-07-02). The hand
 * shim expands the format + i386 varargs via fill_format_varargs. This
 * exercises %s/%d GP conversion, %@ i386-constant-CFString resolution, and
 * the no-directive CFStringCreateCopy fast path.
 *
 * The SHORT format case is load-bearing (Halo "Msg%s" alert title,
 * 2026-07-06): a <=7-char format constant is realized by Foundation as an
 * objc TAGGED POINTER, which format_cstr's mem_readable gate rejected as
 * unreadable -> the shim fast-pathed and returned the format string
 * LITERALLY, never substituting %s. */
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
   char buf[128];
   CFStringRef who = CFSTR("world");
   CFStringRef s = CFStringCreateWithFormat(NULL, NULL,
       CFSTR("hello %s %d %@!"), "there", 42, who);
   if (s && CFStringGetCString(s, buf, sizeof buf, kCFStringEncodingUTF8)) {
      printf("%s\n", buf);
   } else {
      printf("FAIL formatted\n");
   }
   CFStringRef t = CFStringCreateWithFormat(NULL, NULL, CFSTR("plain"));
   if (t && CFStringGetCString(t, buf, sizeof buf, kCFStringEncodingUTF8)) {
      printf("%s\n", buf);
   } else {
      printf("FAIL plain\n");
   }
   /* short (<=7 char) format -> tagged-pointer CFString (the Halo "Msg%s"
    * title): %s must still substitute, not echo the format literally.
    * Use a synthetic 16-byte i386 CFConstantString record {isa,flags=0x7c8,
    * cstr,len} — this test binary's own CFSTR constants are translated to
    * the 32-byte native layout (real-obj passthrough in unwrap_obj_arg), but
    * Halo's records take the runtime i386_cfstr_to_real conversion, whose
    * CFStringCreateWithBytes MINTS a TAGGED pointer for short ASCII content;
    * only that path reproduces the bug. */
   static const char msgfmt[] = "Msg%s";
   static unsigned int fakecf[4];
   fakecf[0] = 0x80000001u;                    /* isa: wrapped-handle-shaped */
   fakecf[1] = 0x7c8u;                         /* CFSTR_FLAGS_ASCII8 */
   fakecf[2] = (unsigned int)(unsigned long)msgfmt;
   fakecf[3] = 5;                              /* strlen("Msg%s") */
   CFStringRef u = CFStringCreateWithFormat(NULL, NULL, (CFStringRef)fakecf,
                                            "InsufficientHDSpace");
   if (u && CFStringGetCString(u, buf, sizeof buf, kCFStringEncodingUTF8)) {
      printf("%s\n", buf);
   } else {
      printf("FAIL tagged-short\n");
   }
   exit(0);   /* the test wrapper enters _main via jmp — never return */
}
