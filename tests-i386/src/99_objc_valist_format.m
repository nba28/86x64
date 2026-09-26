/* 99_objc_valist_format.m — the va_list forms of the format methods.
 *
 * -[NSString initWithFormat:locale:arguments:] takes a va_list. An i386 va_list
 * is a plain pointer to 4-byte vararg slots; x86_64 expects a __va_list_tag, so
 * passing it through raw crashed inside CFStringCreateWithFormat... (Quinn,
 * joining a network game). The bridge now sends the variadic sibling
 * (initWithFormat:locale:) with the slots expanded by the format.
 *
 * Off arm: ABICONV_NO_VALIST_SIBLING=1 must fail (crash or wrong string).
 * Exit 42 = all three strings format correctly.
 */
#include <Foundation/Foundation.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static NSString *fmt_locale(NSString *f, ...) {
   va_list ap; va_start(ap, f);
   NSString *s = [[[NSString alloc] initWithFormat:f locale:nil arguments:ap] autorelease];
   va_end(ap);
   return s;
}
static NSString *fmt_plain(NSString *f, ...) {
   va_list ap; va_start(ap, f);
   NSString *s = [[[NSString alloc] initWithFormat:f arguments:ap] autorelease];
   va_end(ap);
   return s;
}

int main(void)
{
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   NSString *a = fmt_locale(@"%@ joined %d/%s", @"Alice", 7, "lan");
   NSString *b = fmt_plain(@"%d-%@-%x", 12, @"x", 255);
   NSString *c = fmt_locale(@"%.2f pts", 3.25);
   printf("a=%s\nb=%s\nc=%s\n", [a UTF8String], [b UTF8String], [c UTF8String]);
   int ok = !strcmp([a UTF8String], "Alice joined 7/lan") &&
            !strcmp([b UTF8String], "12-x-ff") &&
            !strcmp([c UTF8String], "3.25 pts");
   [pool release];
   exit(ok ? 42 : 1);
}
