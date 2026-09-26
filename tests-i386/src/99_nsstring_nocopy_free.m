/* 99_nsstring_nocopy_free.m — NSString taking ownership of an i386 malloc buffer.
 *
 * -initWithCStringNoCopy:length:freeWhenDone:YES (and the Bytes/Characters
 * siblings) make the string free() the buffer when it dies. The buffer comes
 * from the low-4GB shim heap, which native free() does not own -> malloc abort
 * at pool drain (Quinn, both peers, pressing Play in a network game). The
 * bridge now sends the copying init and frees the buffer on the shim heap.
 *
 * Off arm: ABICONV_NO_NSSTRING_NOCOPY=1 must abort. Exit 42 = strings correct
 * and the pool drained cleanly.
 */
#include <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   char *c = malloc(6); memcpy(c, "hello", 6);
   NSString *a = [[[NSString alloc] initWithCStringNoCopy:c length:5 freeWhenDone:YES] autorelease];
   char *u = malloc(4); memcpy(u, "play", 4);
   NSString *b = [[[NSString alloc] initWithBytesNoCopy:u length:4
                      encoding:NSUTF8StringEncoding freeWhenDone:YES] autorelease];
   unichar *w = malloc(3 * sizeof(unichar)); w[0] = 'l'; w[1] = 'a'; w[2] = 'n';
   NSString *d = [[[NSString alloc] initWithCharactersNoCopy:w length:3 freeWhenDone:YES] autorelease];
   printf("a=%s b=%s d=%s\n", [a UTF8String], [b UTF8String], [d UTF8String]);
   int ok = [a isEqualToString:@"hello"] && [b isEqualToString:@"play"] &&
            [d isEqualToString:@"lan"];
   [pool release];                     /* frees the three strings */
   printf("drained\n");
   exit(ok ? 42 : 1);
}
