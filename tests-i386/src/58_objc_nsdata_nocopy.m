/* 58_objc_nsdata_nocopy.m — NSData taking ownership of an i386 shim-malloc'd
 * buffer across the forward bridge.
 *
 * +[NSData dataWithBytesNoCopy:length:] (and the freeWhenDone:YES / init forms)
 * hand the NSData OWNERSHIP of the buffer: -dealloc calls free() on it. An i386
 * app's malloc comes from the low-4GB SHIM heap; Foundation's native free() does
 * not own that memory -> malloc_report ABORT when the (autoreleased) NSData
 * deallocs at pool drain. Quinn: -[QuinnHighscoreDB read] wraps decrypt_bytes's
 * malloc'd plaintext via dataWithBytesNoCopy:length: to feed
 * unarchiveObjectWithData: -> SIGABRT in -[NSConcreteData dealloc] on play.
 *
 * Fix (bp_nsdata_nocopy): redirect the NoCopy creators to their COPYING form so
 * Foundation owns a NATIVE buffer that native free() can release; the shim buffer
 * is left unfreed (the app gave up ownership via freeWhenDone:YES).
 *
 *   content:  the NSData's bytes equal the source buffer (the copy is faithful).
 *   no_abort: the pool drains without aborting. Pre-fix the second line never
 *             prints (abort in -dealloc); post-fix both print, exit 0.
 */
#include <Foundation/Foundation.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void) {
   int failures = 0;
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

   unsigned char *buf = (unsigned char *)malloc(32);   /* i386 low-4GB shim heap */
   for (int i = 0; i < 32; ++i) { buf[i] = (unsigned char)(i * 5 + 1); }

   NSData *d = [NSData dataWithBytesNoCopy:buf length:32];   /* freeWhenDone: YES */
   const unsigned char *p = (const unsigned char *)[d bytes];
   if (p && memcmp(p, buf, 32) == 0) { puts("ok nsdata_nocopy_content"); }
   else { puts("FAIL nsdata_nocopy_content"); failures++; }

   [pool release];   /* drains the autoreleased NSData; native free(buf) aborts pre-fix */
   puts("ok nsdata_nocopy_no_abort");
   exit(failures);
}
