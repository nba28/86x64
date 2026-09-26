/* 99_cfstr_utf16_literal.m — UTF-16 CFString constants (flags 0x7d0).
 *
 * A literal with an embedded NUL or any non-ASCII character is compiled into
 * __ustring as UniChar[] + a 0 terminator. In an old (classic-reloc) image the
 * translated constant carries NO isa bind, so the bridge must recognize it by
 * its flags — and it recognized only the ASCII form (0x7c8). As a receiver the
 * UTF-16 constant resolved to no class and every message returned nil. Quinn's
 * protocol literals end in its NUL delimiter (@"REQ_SERVER_INFO\0"):
 * -dataUsingEncoding: returned nil, the client never sent a byte, and the
 * server start timed out ("some strange error").
 *
 * A modern link binds a real isa (which would hide the bug), so this builds
 * the isa-less records by hand, in both layouts the bridge accepts:
 *   i386   16 B {isa, flags, str, length}         (4-byte fields)
 *   native 32 B {isa, flags, str, length}         (8-byte fields, translated)
 *
 * Exit 42 = all three records answer -length (and -dataUsingEncoding:).
 */
#include <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static const uint16_t payload[] = { 'R', 'E', 'Q', 0, 0 };   /* "REQ\0" + terminator */
static uint32_t rec16[4] __attribute__((aligned(16))) = { 0, 0x7d0, 0, 4 };
static uint32_t rec32[8] __attribute__((aligned(16))) = { 0, 0, 0x7d0, 0, 0, 0, 4, 0 };
/* Quinn's last __ustring entry: "\x18\0" with NO terminator after it (the old
 * linker sized the section without it; the next section starts there). */
static const uint16_t unterminated[] = { 0x18, 0, 0x4c00 };
static uint32_t recnt[8] __attribute__((aligned(16))) = { 0, 0, 0x7d0, 0, 0, 0, 2, 0 };

static int check(const char *what, NSString *s)
{
   NSData *d = [s dataUsingEncoding:NSUTF8StringEncoding];
   printf("%s len=%u data=%u\n", what, (unsigned)[s length], (unsigned)[d length]);
   return [s length] == 4 && [d length] == 4;
}

int main(void)
{
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   rec16[2] = (uint32_t)(uintptr_t)payload;
   rec32[4] = (uint32_t)(uintptr_t)payload;
   recnt[4] = (uint32_t)(uintptr_t)unterminated;
   int ok = check("i386", (NSString *)(void *)rec16);
   ok &= check("native", (NSString *)(void *)rec32);
   NSString *nt = (NSString *)(void *)recnt;
   printf("unterminated len=%u\n", (unsigned)[nt length]);
   ok &= [nt length] == 2;
   [pool release];
   exit(ok ? 42 : 1);
}
