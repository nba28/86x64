/* 45_objc_sort_callback.m — i386 C-function-pointer comparator across the
 * ObjC forward bridge (the Quinn 3.5.7 sort wall).
 *
 * -[NSMutableArray sortUsingFunction:context:] and the NSArray
 * sortedArrayUsingFunction:context: family take a BARE C function pointer
 *      NSInteger (*compare)(id obj1, id obj2, void *context)
 * plus an opaque context. The ObjC method type encoding marks the comparator
 * argument as `^?` (pointer-to-function) but cannot describe the comparator's
 * OWN signature, so the i386->x86_64 forward bridge must bind the i386 callback
 * to a native trampoline using the comparator's known shape: two OBJECT args
 * (handle-wrapped so a >4GB / tagged native object pointer survives into the
 * i386 callee, where it unwraps on the next message send) + a pointer context,
 * returning a SIGN-EXTENDED NSInteger.
 *
 * Pre-fix the bridge bound `^?` with a generic all-POINTER descriptor:
 *   - the two id args were TRUNCATED to 32 bits, so the i386 comparator's
 *     [a intValue] dereferenced a chopped object pointer -> SIGSEGV (exactly
 *     Quinn's -[NSMutableArray sortUsingFunction:context:] crash:
 *     EXC_BAD_ACCESS / KERN_INVALID_ADDRESS in CoreFoundation's
 *     sortRange:options:usingComparator:), and
 *   - the -1/0/1 result was ZERO-extended, so CoreFoundation read
 *     NSOrderedAscending (-1) as a huge positive long -> inverted/garbage order.
 *
 * Either failure mode (crash, or wrong order) suppresses the "ok ..." lines,
 * so this test reproduces pre-fix and passes post-fix.
 */
#include <Foundation/Foundation.h>
#include <stdio.h>

/* i386 cdecl comparator. Touches BOTH objects via a message send (forcing the
 * object pointers to be real, not truncated) and reads the context (forcing it
 * to round-trip i386->native->i386). Returns NSInteger -1/0/1 — the sign must
 * survive all the way to CoreFoundation. */
static NSInteger cmp_int(id a, id b, void *ctx) {
   int dir = ctx ? *(const int *)ctx : 1;      /* +1 ascending, -1 descending */
   int x = (int)[a intValue];
   int y = (int)[b intValue];
   int c = (x > y) - (x < y);
   return (NSInteger)(dir < 0 ? -c : c);
}

static int is_order(NSArray *arr, const int *want, int n) {
   if ((int)[arr count] != n) { return 0; }
   for (int i = 0; i < n; ++i) {
      if ((int)[[arr objectAtIndex:i] intValue] != want[i]) { return 0; }
   }
   return 1;
}

int main(void) {
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   int failures = 0;
   int asc = 1, desc = -1;

   int seed[6] = { 42, 7, 99, -3, 18, 0 };
   NSMutableArray *m = [NSMutableArray array];
   for (int i = 0; i < 6; ++i) {
      [m addObject:[NSNumber numberWithInt:seed[i]]];
   }

   /* A: ascending in-place sort via C fn-ptr + context */
   [m sortUsingFunction:cmp_int context:&asc];
   int wa[6] = { -3, 0, 7, 18, 42, 99 };
   if (is_order(m, wa, 6)) { puts("ok sort_ascending"); }
   else { puts("FAIL sort_ascending"); failures++; }

   /* B: descending — exercises the SIGN of the NSInteger return (a zero-
    * extended -1 would break the ordering) and the context value */
   [m sortUsingFunction:cmp_int context:&desc];
   int wd[6] = { 99, 42, 18, 7, 0, -3 };
   if (is_order(m, wd, 6)) { puts("ok sort_descending"); }
   else { puts("FAIL sort_descending"); failures++; }

   /* C: NSArray sortedArrayUsingFunction:context: (returns a new array) */
   NSArray *src = [NSArray arrayWithObjects:
                     [NSNumber numberWithInt:5], [NSNumber numberWithInt:1],
                     [NSNumber numberWithInt:9], [NSNumber numberWithInt:3], nil];
   NSArray *s = [src sortedArrayUsingFunction:cmp_int context:&asc];
   int ws[4] = { 1, 3, 5, 9 };
   if (is_order(s, ws, 4)) { puts("ok sorted_array_func"); }
   else { puts("FAIL sorted_array_func"); failures++; }

   [pool release];
   exit(failures);
}
