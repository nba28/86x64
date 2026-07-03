/* CFStringCreateWithFormat is VARIADIC, so abigen skips it and the bind
 * went raw into native CoreFoundation: the i386 caller passes varargs on
 * the cdecl STACK, native reads SysV registers -> garbage args and a crash
 * inside CFStringCreateWithFormatAndArguments (Halo, 2026-07-02). The hand
 * shim expands the format + i386 varargs via fill_format_varargs. This
 * exercises %s/%d GP conversion, %@ i386-constant-CFString resolution, and
 * the no-directive CFStringCreateCopy fast path. */
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
   exit(0);   /* the test wrapper enters _main via jmp — never return */
}
