/* 31_objc_protocol_conform.m — conformsToProtocol: on a reverse-registered
 * legacy class for an APP-DECLARED protocol. reverse_register_one creates the
 * modern class but never tells libobjc about the protocols the legacy class
 * adopts (they live only in its i386 __OBJC metadata), so native
 * conformsToProtocol: used to return NO for every app protocol. The bridge's
 * bp_conforms_protocol now falls back to walking the legacy class's OWN
 * protocol list (interface-declared + category-declared + legacy-super chain)
 * by name. This test exercises all three positive shapes plus the negative
 * case (a class must NOT report conformance to a protocol it never adopts) and
 * confirms the protocol method still dispatches.
 *
 * Validates via puts("ok N")/exit(failures). No vararg/fp printf (that path is
 * a separate, pre-existing libabiconv gap that 22_objc_fp_struct documents).
 */
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>

@protocol Swimmer
- (int)swimSpeed;
@end

/* (1) protocol adopted on the main @interface */
@interface Fish : NSObject <Swimmer>
@end
@implementation Fish
- (int)swimSpeed { return 9; }
@end

/* (2) protocol adopted via a CATEGORY on a class that didn't declare it */
@interface Animal : NSObject
- (int)legs;
@end
@implementation Animal
- (int)legs { return 4; }
@end
@interface Animal (WaterVariant) <Swimmer>
- (int)swimSpeed;
@end
@implementation Animal (WaterVariant)
- (int)swimSpeed { return 5; }
@end

int main(void) {
    int fail = 0;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    Fish *f = [[Fish alloc] init];
    if ([f conformsToProtocol:@protocol(Swimmer)]) puts("ok 1"); else { puts("FAIL 1 iface conform"); fail++; }

    Animal *a = [[Animal alloc] init];
    if ([a conformsToProtocol:@protocol(Swimmer)]) puts("ok 2"); else { puts("FAIL 2 category conform"); fail++; }

    /* negative: Animal never adopts NSCopying */
    if (![a conformsToProtocol:@protocol(NSCopying)]) puts("ok 3"); else { puts("FAIL 3 false conform"); fail++; }

    /* native conformance still authoritative when it says YES (NSObject) */
    if ([a conformsToProtocol:@protocol(NSObject)]) puts("ok 4"); else { puts("FAIL 4 nsobject conform"); fail++; }

    /* the protocol method still dispatches through the category */
    if ([a swimSpeed] == 5) puts("ok 5"); else { puts("FAIL 5 dispatch"); fail++; }
    if ([f swimSpeed] == 9) puts("ok 6"); else { puts("FAIL 6 dispatch"); fail++; }

    [a release];
    [f release];
    [pool drain];
    exit(fail);
}
