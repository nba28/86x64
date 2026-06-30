/* 57_objc_nsdata_bytes.m — -[NSData bytes] across the i386->x86_64 forward bridge.
 *
 * A native NSData's backing buffer lives at a >4GB address. -[NSData bytes]
 * returns it as `const void *` (encoding `r^v`). The bare-`^v` RETURN wrap
 * (commit 8aab6e4, needed for the graphicsPort/CGContextRef opaque-token family)
 * turns any >4GB `^v` return into a low-4GB ARENA HANDLE — correct for an opaque
 * token passed BACK to native, but WRONG for a raw data buffer the app
 * DEREFERENCES directly. Raw-C consumers then read the arena slot as data and
 * get garbage. Quinn's `ks_decrypt` reads the encrypted-highscore bytes straight
 * off `[data bytes]`: given the handle it produces a corrupt length-prefix ->
 * malloc(huge)=NULL -> memcpy(NULL) -> SIGSEGV at 0x0 in -[QuinnHighscoreDB read]
 * on play. (Even without the wrap the >4GB pointer truncates to eax -> equally
 * unreadable; the bug is fundamental to `bytes`.)
 *
 * Fix (bp_nsdata_bytes): intercept -[NSData bytes] and return a LOW-4GB COPY of
 * the bytes, cached on the receiver so repeated calls return the same pointer.
 *
 *   equal:  the returned pointer is readable and its bytes EQUAL the source.
 *           Pre-fix `p` is a handle -> the bytes mismatch (deterministic, no
 *           crash: the arena is mapped). Post-fix they match.
 *   stable: repeated [data bytes] returns the SAME low pointer (cache identity).
 */
#include <Foundation/Foundation.h>
#include <stdio.h>
#include <string.h>

int main(void) {
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   int failures = 0;

   unsigned char src[64];
   for (int i = 0; i < 64; ++i) { src[i] = (unsigned char)(i * 7 + 3); }
   NSData *d = [NSData dataWithBytes:src length:64];

   const unsigned char *p = (const unsigned char *)[d bytes];
   if (p != NULL && memcmp(p, src, 64) == 0) {
      puts("ok nsdata_bytes_lowcopy_equal");
   } else {
      puts("FAIL nsdata_bytes_lowcopy_equal");
      failures++;
   }

   const unsigned char *p2 = (const unsigned char *)[d bytes];
   if (p2 == p) {
      puts("ok nsdata_bytes_cached_stable");
   } else {
      puts("FAIL nsdata_bytes_cached_stable");
      failures++;
   }

   [pool release];
   exit(failures);
}
