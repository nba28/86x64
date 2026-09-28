/* 99_objc_seltypes_kind.m — a legacy class's encoding for a selector must not
 * change the arg KIND of a NATIVE method with the same name.
 *
 * The seltypes registry (objc_shim.c) is keyed by selector only: it records the
 * i386 encoding of every selector a legacy class declares, so the forward
 * bridge can recover CGFloat/NSInteger widths a native 'd'/'q' leaves
 * ambiguous. iPhoto declares `addObject:(void *)` on one of its classes, so
 * -[__NSArrayM addObject:] was marshalled as `^v`: the object arg skipped
 * unwrap and the raw i386 object reached CoreFoundation -> SIGSEGV.
 *
 * Here Bag declares -addObject:(void *), then i386 code adds a constant string
 * and a legacy Bag instance to a real NSMutableArray. ON: the native `@` wins,
 * both are unwrapped -> "ok 1", "ok 2". OFF (M64_NO_SELTYPES_KIND_GUARD=1): the
 * Bag's raw i386 shadow is stored and Foundation messages it -> SIGSEGV (the
 * iPhoto crash). "ok 0" proves the legacy method still dispatches with its
 * own `^v` encoding; "ok 3" that the registry still refines a CGFloat width.
 */
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <stdio.h>
#include <stdlib.h>

@interface Bag : NSObject { void *last; }
- (void)addObject:(void *)p;
- (void *)last;
- (void)rotateByRadians:(CGFloat)r;
@end
@implementation Bag
- (void)addObject:(void *)p { last = p; }
- (void *)last { return last; }
- (void)rotateByRadians:(CGFloat)r { }
@end

int main(void) {
    int fail = 0;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    /* by name: SL ld64 cannot resolve a class ref to a class in the same .o */
    Bag *b = [[objc_getClass("Bag") alloc] init];
    [b addObject:(void *)0x1234];
    if ([b last] == (void *)0x1234) puts("ok 0");
    else { puts("FAIL 0 legacy addObject:"); fail++; }

    NSMutableArray *a = [[NSMutableArray alloc] init];
    [a addObject:@"seltypes"];
    if ([a count] == 1 && [[a objectAtIndex:0] isEqualToString:@"seltypes"])
        puts("ok 1");
    else { puts("FAIL 1 native addObject:"); fail++; }

    /* a legacy instance (iPhoto's AVSection shape): native code must hold the
     * real object, so a native-side identity search finds it. */
    [a addObject:b];
    if ([a indexOfObjectIdenticalTo:b] == 1) puts("ok 2");
    else { puts("FAIL 2 legacy object stored raw"); fail++; }

    /* positive control: the registry's real job is untouched. Bag's i386
     * `f` refines the native CGFloat 'd' of -[NSAffineTransform
     * rotateByRadians:] (not in the cgfloat mask) -> one 4-byte slot, widened.
     * 90 degrees maps (1,0) to (0,1). */
    NSAffineTransform *t = [NSAffineTransform transform];
    [t rotateByRadians:1.5707963f];
    NSPoint p = [t transformPoint:NSMakePoint(1.0, 0.0)];
    if ((int)(p.x + 0.5) == 0 && (int)(p.y + 0.5) == 1) puts("ok 3");
    else { puts("FAIL 3 CGFloat refinement lost"); fail++; }

    [a release];
    [b release];
    [pool drain];
    exit(fail);
}
