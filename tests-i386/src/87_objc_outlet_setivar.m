/* 87_objc_outlet_setivar.m — nib-outlet connection via object_setInstanceVariable
 * where the outlet's DECLARED TYPE is a SEPARATE custom legacy class, and the
 * connected TARGET is an instance of that legacy class (not a Foundation object).
 *
 * This mirrors iPhoto's actual, still-open blocker: AppKit's
 * -[NSNibOutletConnector establishConnection] connects
 *   ArchiveController.mLocationController  ->  a LocationController instance
 * When the owner does NOT respond to a set<Label>: setter, the connector falls
 * back to the RAW runtime call object_setInstanceVariable(owner, "mLocation…",
 * target) (verified by disassembly, objc_shim.c ~L6096). It then TESTS the
 * returned Ivar and, on NULL, logs "Failed to connect (mLocationController)
 * outlet … missing setter or instance variable" and leaves the outlet nil.
 * ArchiveController then can't find its archive -> iPhoto's dead-end
 * "Your photo library is missing" popup.
 *
 * Test 77_objc_legacy_outlet already covers the KVC (setValue:forKey:) path with
 * a Foundation-object outlet. This test exercises the OTHER connector branch:
 *   (a) the DIRECT object_setInstanceVariable call (not KVC), and
 *   (b) an outlet whose target is another reverse-registered LEGACY class.
 *
 * Assertions:
 *   ok 1  object_setInstanceVariable returns a NON-NULL Ivar (the exact gate the
 *         nib connector checks; NULL == the "missing setter…" iPhoto failure).
 *   ok 2  the owner's legacy IMP reads the connected controller from its i386
 *         SHADOW at the classic offset and can message it (round-trips the
 *         legacy target back through the forward bridge).
 *   ok 3  object_getInstanceVariable reads the same connected target back.
 *   ok 4  the NATIVE runtime (KVC valueForKey:) also sees the connected outlet
 *         on the real object (mirrored into R).
 *
 * puts("ok N") on success; exit(failures).
 */
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <stdio.h>
#include <stdlib.h>

/* the outlet's declared class — a separate custom legacy controller. */
@interface LocationController : NSObject {
    int cookie;                 /* i386 offset 4 */
}
- (int)cookie;
- (void)setCookie:(int)v;
@end
@implementation LocationController
- (int)cookie { return cookie; }
- (void)setCookie:(int)v { cookie = v; }
@end

/* the owner — mirrors ArchiveController. mLocationController is an object ivar
 * typed as the custom class above (i386 offset 4, right after the 4-byte isa);
 * NO setMLocationController: setter, so the nib connector uses the raw runtime. */
@interface ArchiveController : NSObject {
    LocationController *mLocationController;
}
- (int)archiveCookie;          /* fragile-ABI direct read of mLocationController */
@end
@implementation ArchiveController
- (int)archiveCookie {
    /* self == the i386 shadow inside a legacy IMP; read the outlet at off 4 */
    id lc = *(id *)((char *)(void *)self + 4);
    return lc ? (int)[lc cookie] : -1;
}
@end

int main(void) {
    int fail = 0;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    ArchiveController *owner = [[ArchiveController alloc] init];
    LocationController *loc = [[LocationController alloc] init];
    [loc setCookie:1234];

    /* Connect exactly as -[NSNibOutletConnector establishConnection] does when
     * the owner has no setter: a raw object_setInstanceVariable keyed by name.
     * The connector TESTS the returned Ivar; NULL is the iPhoto failure. */
    Ivar iv = object_setInstanceVariable(owner, "mLocationController", loc);
    if (iv != NULL) puts("ok 1");
    else { printf("FAIL 1 object_setInstanceVariable returned NULL "
                  "(== iPhoto 'missing setter or instance variable')\n"); fail++; }

    /* the owner's legacy IMP reads the connected controller from its shadow. */
    int c = [owner archiveCookie];
    if (c == 1234) puts("ok 2");
    else { printf("FAIL 2 archiveCookie=%d (want 1234)\n", c); fail++; }

    /* object_getInstanceVariable reads the same target back. */
    void *out = NULL;
    Ivar giv = object_getInstanceVariable(owner, "mLocationController", &out);
    if (giv != NULL && out != NULL && [(id)out cookie] == 1234) puts("ok 3");
    else { printf("FAIL 3 getInstanceVariable giv=%p out=%p\n", (void *)giv, out); fail++; }

    /* the native runtime sees the connected outlet on the real object too. */
    id v = [owner valueForKey:@"mLocationController"];
    if (v != nil && [v cookie] == 1234) puts("ok 4");
    else { printf("FAIL 4 native valueForKey: missing/mismatch\n"); fail++; }

    [loc release];
    [owner release];
    [pool drain];
    exit(fail);
}
