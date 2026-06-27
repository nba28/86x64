/* 37_objc_blocks.m — i386 BLOCKS crossing into the native ObjC/Block runtime.
 *
 * Translated create + translated invoke of a block already worked; the gap was
 * a block that CROSSES into native: [block copy] (-> native _Block_copy reads a
 * garbage descriptor/size off the i386 layout), or a block passed to a native
 * method that INVOKES it (enumerateObjectsUsingBlock:, sortUsingComparator:).
 * The block-marshalling bridge (objc_shim.c + block_tramp.asm) handles both:
 *   - [block copy] -> an i386-layout heap copy the translated code keeps
 *     invoking;
 *   - block as a @? arg -> a synthesized native Block_layout whose invoke
 *     trampolines back to the i386 invoke.
 * Without the bridge each of these SIGSEGVs (rip = fused garbage). */
#include <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

   /* (1) [block copy] then invoke the copy — the blk5 repro. The stack block
    *     captures `base`; copy must keep an i386-invokable layout. */
   int base = 100;
   int (^blk)(int) = [^(int x){ return base + x; } copy];
   printf("copy-call: %d\n", blk(3));               /* 103 */
   [blk release];

   /* (2) block passed to a native enumerator — native invokes it per element.
    *     The block captures a __block accumulator and sends a message to each
    *     (native) object it receives, exercising obj-arg wrapping. */
   NSArray *strs = [NSArray arrayWithObjects:@"a", @"bb", @"ccc", nil];
   __block int total = 0;
   [strs enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop){
      (void)idx; (void)stop;
      total += (int)[(NSString *)obj length];
   }];
   printf("enum-total: %d\n", total);               /* 1 + 2 + 3 = 6 */

   /* (3) block as a comparator — native calls it many times and reads its
    *     NSComparisonResult return (sign-extended through the bridge). */
   NSArray *nums = [NSArray arrayWithObjects:
                    [NSNumber numberWithInt:3],
                    [NSNumber numberWithInt:1],
                    [NSNumber numberWithInt:2], nil];
   NSArray *sorted = [nums sortedArrayUsingComparator:^NSComparisonResult(id a, id b){
      int ia = [(NSNumber *)a intValue], ib = [(NSNumber *)b intValue];
      if (ia < ib) { return NSOrderedAscending; }
      if (ia > ib) { return NSOrderedDescending; }
      return NSOrderedSame;
   }];
   printf("sorted: %d %d %d\n",
          [(NSNumber *)[sorted objectAtIndex:0] intValue],
          [(NSNumber *)[sorted objectAtIndex:1] intValue],
          [(NSNumber *)[sorted objectAtIndex:2] intValue]);   /* 1 2 3 */

   [pool drain];
   exit(0);
}
