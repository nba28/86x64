/* 99_nsrange_notfound.m — NSNotFound inside a returned NSRange.
 *
 * -[NSString rangeOfString:…] returns {NSNotFound, 0} on a miss. Natively
 * NSNotFound is 0x7fffffffffffffff; narrowing the {_NSRange=QQ} to the i386
 * {II} by taking the low half gives 0xffffffff, so the i386 caller's
 * `loc == NSNotFound` (cmp eax,0x7fffffff) is never true. Quinn's key-config
 * check therefore reported "At least one key appears more than once" for every
 * configuration and refused to start a tournament.
 *
 * Exit 42 = a miss reads as NSNotFound and a hit keeps its location.
 */
#include <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   NSString *s = @"adswyx";
   NSRange miss = [s rangeOfString:@"q" options:0 range:NSMakeRange(1, 5)];
   NSRange hit  = [s rangeOfString:@"y" options:0 range:NSMakeRange(1, 5)];
   printf("miss.location=0x%lx hit.location=%lu\n",
          (unsigned long)miss.location, (unsigned long)hit.location);
   int ok = miss.location == NSNotFound && hit.location == 4;
   [pool release];
   exit(ok ? 42 : 1);
}
