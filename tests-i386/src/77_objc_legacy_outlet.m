/* 77_objc_legacy_outlet.m — nib-outlet connection to a reverse-registered
 * legacy class's OWN object ivar.
 *
 * A reverse-registered legacy (fragile-ABI, ObjC1) class was allocated with
 * objc_allocateClassPair(super, name, 0) — NO ivars on the modern class. So the
 * modern runtime could not find the class's own object ivars: AppKit's nib
 * outlet connector (-[NSNibOutletConnector establishConnection] ->
 * object_setInstanceVariable -> class_getInstanceVariable == NULL) logged
 * "missing setter or instance variable" and left EVERY outlet nil (Quinn's board
 * fills the window, the stats sidebar & image wells never wire up; any classic
 * nib app). objc_shim.c now class_addIvar's each legacy class's own object ivars
 * so class_getInstanceVariable/KVC/object_setInstanceVariable find them, and
 * mirrors the connected NATIVE object into the i386 SHADOW (wrapped handle) at
 * the classic i386 offset the translated IMP reads.
 *
 * Here OutletHost declares one object ivar `myOutlet` (i386 offset 4, right
 * after the 4-byte isa). We connect it the way the nib connector does — through
 * class_getInstanceVariable (via KVC setValue:forKey:, which finds the raw ivar
 * when there is no setter) — then a legacy IMP reads it from the shadow and
 * messages it, and the native side reads it back from the real object.
 *
 * puts("ok N") on success (no vararg/fp printf on the happy path — see
 * 22_objc_fp_struct); exit(failures).
 */
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>

@interface OutletHost : NSObject {
    id myOutlet;                 /* the "outlet": object ivar @ i386 offset 4 */
}
- (int)readOutletLen;            /* fragile-ABI direct read of myOutlet, msg it */
@end
@implementation OutletHost
- (int)readOutletLen {
    id o = *(id *)((char *)(void *)self + 4);   /* self == i386 shadow here */
    return o ? (int)[o length] : -1;
}
@end

int main(void) {
    int fail = 0;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    OutletHost *h = [[OutletHost alloc] init];
    NSString *target = @"hello";                 /* length 5 */

    /* Connect the outlet exactly as the nib connector does: no setMyOutlet:
     * setter exists, so this resolves through class_getInstanceVariable on the
     * registered ivar. Before the fix that ivar did not exist -> raise / nil. */
    [h setValue:target forKey:@"myOutlet"];
    puts("ok 1");

    /* the legacy IMP reads the connected outlet from its i386 shadow slot */
    int len = [h readOutletLen];
    if (len == 5) puts("ok 2");
    else { printf("FAIL 2 outlet len=%d (want 5)\n", len); fail++; }

    /* native side reads the same outlet back off the real object */
    if ([[h valueForKey:@"myOutlet"] isEqualToString:@"hello"]) puts("ok 3");
    else { puts("FAIL 3 valueForKey mismatch"); fail++; }

    [h release];
    [pool drain];
    exit(fail);
}
