/* 99_nsdata_mutablebytes_wb.m — writes through -[NSMutableData mutableBytes].
 *
 * A native NSMutableData's buffer is >4GB; the i386 code writes through the
 * returned pointer. Mirrors AsyncSocket's read loop (Quinn's network code):
 * per byte, -increaseLengthBy:1, write at [buf mutableBytes]+n, check the
 * delimiter via -bytes, and finally build the message with -initWithData:.
 * Unbridged, the bytes never reached the NSData: the server saw "\0" and
 * rejected every command.
 *
 * Exit 42 = the data holds what was written. ABICONV_NO_MUTABLEBYTES_SHADOW=1
 * turns the fix off.
 */
#include <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   const char msg[] = "REQ_SERVER_INFO";          /* + the NUL delimiter */
   NSMutableData *buf = [NSMutableData dataWithLength:0];
   int saw_delim = 0;
   for (unsigned n = 0; n < sizeof msg; ++n) {
      [buf increaseLengthBy:1];
      ((char *)[buf mutableBytes])[n] = msg[n];
      if (((const char *)[buf bytes])[n] == 0) { saw_delim = 1; }
   }
   NSString *s = [[[NSString alloc] initWithData:buf encoding:NSUTF8StringEncoding]
                   autorelease];
   int ok = saw_delim && [buf length] == sizeof msg &&
            [s length] == sizeof msg && [s hasPrefix:@"REQ_SERVER_INFO"];
   printf("delim=%d len=%u str=%s\n", saw_delim, (unsigned)[s length],
          [s hasPrefix:@"REQ_SERVER_INFO"] ? "REQ_SERVER_INFO" : "(wrong)");
   [pool release];
   exit(ok ? 42 : 1);
}
