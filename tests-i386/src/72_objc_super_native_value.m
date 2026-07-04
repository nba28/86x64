/* 72_objc_super_native_value.m — a legacy [super sel] into a NATIVE super that
 * RETURNS A VALUE must return the native super's real value (not nil).
 *
 * CORRECTNESS/PROPERTY test for the reverse-bridge [super]-value handling: a
 * reverse-registered legacy class overriding a native method and calling
 * [super sel] must receive the native super's actual return, not 0/nil.
 *
 * ⚠SCOPE NOTE: this passes even WITHOUT the reverse_prep native-super fix,
 * because a plain [super value-method] into a native super here dispatches
 * NATIVELY (objc_msgSendSuper2 finds the native IMP) and never reaches
 * reverse_prep. The 48211bb regression it documents fires only when the super
 * lookup resolves to a reverse trampoline that has NO legacy IMP in the running
 * libabiconv copy — a condition tied to a MULTI-COPY deploy (iPhoto's 14
 * co-located copies: -[AlbumView numberOfRows] -> [super numberOfRows] resolved
 * to another copy's trampoline -> native-super path -> nil -> NSOutlineView
 * expandItem: _locationOfRow: index>numRows assertion, the invisible-sidebar
 * wall). tests-i386 links a SINGLE copy, so it can't reproduce that path; the
 * definitive guard for that fix is the iPhoto N=15 boot. Kept as a property/
 * documentation test (and it confirms the fix is non-regressive here).
 *
 * puts("ok N")/exit(failures), no vararg/fp printf on the success path.
 */
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>

/* legacy subclass of a NATIVE class (NSObject). Overrides value-returning
 * methods the native super defines, delegating to [super]. */
@interface SuperVal : NSObject
- (NSUInteger)hashViaSuper;    /* -> [super hash], native NSObject.hash */
- (Class)classViaSuper;        /* -> [super class], native NSObject.class */
@end
@implementation SuperVal
- (NSUInteger)hashViaSuper { return [super hash]; }
- (Class)classViaSuper { return [super class]; }
@end

int main(void) {
    int fail = 0;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    SuperVal *o = [[SuperVal alloc] init];

    /* [super hash] into native NSObject.hash must equal the object's own native
     * hash (a real nonzero value), never nil — 48211bb returned 0 here. */
    NSUInteger hs = [o hashViaSuper];
    NSUInteger hn = [o hash];
    if (hs != 0 && hs == hn) puts("ok 1");
    else { puts("FAIL 1 [super hash] wrong (nil?)"); fail++; }

    /* [super class] (object-returning) must be the real Class, not nil. */
    Class cs = [o classViaSuper];
    if (cs != nil && cs == [o class]) puts("ok 2");
    else { puts("FAIL 2 [super class] wrong (nil?)"); fail++; }

    [o release];
    [pool drain];
    exit(fail);
}
