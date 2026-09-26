/* 99_objc_outparam_wb.m — `(id *)` out-params into NATIVE methods.
 *
 * i386 passes &local (a 4-byte slot). Passed raw, the native method stores an
 * 8-byte pointer: the caller reads back a truncated garbage object and the
 * neighbouring 4 bytes are clobbered. Quinn's server scanned every protocol
 * command with -scanUpToCharactersFromSet:intoString:, got garbage, called
 * the message invalid and crashed logging it.
 *
 * Checks: the scanned string is right and the canary next to the slot is
 * intact; an NSError ** out-param from a failing call is a usable error; a
 * call that does not write leaves the slot as it was.
 * Exit 42 = all hold. ABICONV_NO_OUTPARAM_WB=1 turns the fix off.
 */
#include <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>

struct pair { NSString *out; unsigned canary; };

int main(void)
{
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   volatile struct pair p = { nil, 0x11223344u };
   NSScanner *sc = [NSScanner scannerWithString:@"REQ_SERVER_INFO,rest"];
   BOOL ok1 = [sc scanUpToCharactersFromSet:
                 [NSCharacterSet characterSetWithCharactersInString:@","]
                               intoString:(NSString **)&p.out];
   int scan_ok = ok1 && [p.out isEqualToString:@"REQ_SERVER_INFO"];
   printf("scan=%d canary=%08x\n", scan_ok, p.canary);

   NSError *err = nil;
   NSString *s = [NSString stringWithContentsOfFile:@"/nonexistent/x"
                                           encoding:NSUTF8StringEncoding
                                              error:&err];
   int err_ok = s == nil && err != nil && [err code] != 0;
   printf("error=%d\n", err_ok);

   NSError *keep = (NSError *)@"untouched";
   NSString *t = [NSString stringWithContentsOfFile:@"/etc/hosts"
                                           encoding:NSUTF8StringEncoding
                                              error:&keep];
   int keep_ok = t != nil && [(id)keep isEqual:@"untouched"];
   printf("untouched=%d\n", keep_ok);

   [pool release];
   exit(scan_ok && p.canary == 0x11223344u && err_ok && keep_ok ? 42 : 1);
}
