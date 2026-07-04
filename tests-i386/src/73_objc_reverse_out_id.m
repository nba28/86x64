/* 73_objc_reverse_out_id.m — reverse-bridge native `out id*` out-param unwrap.
 *
 * When NATIVE code (here NSInvocation, mirroring AppKit's NSCell) calls a
 * reverse-registered LEGACY method that has an `out id *` out-parameter and
 * passes a NATIVE (>4GB) `id *`, the reverse bridge must give the i386 method a
 * low-4GB scratch slot and, on return, UNWRAP the arena handle the method wrote
 * into it back to the native object stored at the caller's *id. Without that,
 * the native out-pointer is truncated/mishandled and the caller sees a lost
 * value or a raw arena handle it later uses as an object (iPhoto:
 * -[AlbumViewCellFormatter getObjectValue:forString:errorDescription:] -> the
 * cell's native _contents held the handle -> objc_opt_isKindOfClass fault).
 *
 * ⚠SCOPE NOTE: this passes with AND without the fix. The fix is (correctly)
 * gated on a >4GB NATIVE out-pointer; in this Foundation-only i386 harness every
 * pointer — including &result passed via NSInvocation — lives in the low-4GB
 * i386 world, so the fix never fires and the pre-fix else-branch handles the
 * low-4GB out-ptr fine. Reproducing the real bug needs a NATIVE (>4GB) id* —
 * e.g. AppKit NSCell's native _contents ivar address — which this harness can't
 * create (no AppKit; test memory is all <4GB). Kept as a property/documentation
 * + non-regression test (it confirms the fix keeps the low-4GB round-trip and
 * the other 24 objc tests green); the definitive guard is the iPhoto N=15 boot.
 *
 * puts("ok N")/exit(failures), no vararg/fp printf on the success path.
 */
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>

/* a legacy formatter-style class with a getObjectValue: out-id* method */
@interface OutIdFmt : NSObject
- (BOOL)getObjectValue:(out id *)obj forString:(NSString *)s;
@end
@implementation OutIdFmt
- (BOOL)getObjectValue:(out id *)obj forString:(NSString *)s {
    if (obj) { *obj = [NSString stringWithFormat:@"parsed:%@", s]; }
    return YES;
}
@end

int main(void) {
    int fail = 0;
    @autoreleasepool {
        OutIdFmt *f = [[OutIdFmt alloc] init];
        SEL sel = @selector(getObjectValue:forString:);

        /* Invoke via NSInvocation so the call originates in NATIVE Foundation
         * with a NATIVE id* out-param (&result on the native stack, >4GB) —
         * exactly the shape that exercises the reverse-bridge out-id* path. */
        NSMethodSignature *sig = [f methodSignatureForSelector:sel];
        if (!sig) { puts("FAIL 0 no method signature"); exit(1); }
        NSInvocation *inv = [NSInvocation invocationWithMethodSignature:sig];
        [inv setTarget:f];
        [inv setSelector:sel];
        id result = nil;
        id *presult = &result;
        NSString *str = @"x";
        [inv setArgument:&presult atIndex:2];   /* the out id* */
        [inv setArgument:&str     atIndex:3];
        [inv invoke];

        /* WITH the fix: result is the real native NSString the i386 method made
         * (unwrapped from its arena handle). WITHOUT: the out-param write is
         * lost/garbage (nil or a raw handle) -> these checks fail. */
        if (result != nil && [result respondsToSelector:@selector(length)]) puts("ok 1");
        else { printf("FAIL 1 out id* not a native object (%p)\n", (void *)result); fail++; }

        if (result != nil && [result isKindOfClass:[NSString class]]) puts("ok 2");
        else { puts("FAIL 2 out id* isKindOfClass NSString failed"); fail++; }

        if (result != nil && [result hasPrefix:@"parsed:"]) puts("ok 3");
        else { puts("FAIL 3 out id* wrong value"); fail++; }
    }
    exit(fail);
}
