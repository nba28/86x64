/* 38_objc_inherited_ivar.m — inherited native-superclass ivar shadow-sync.
 *
 * A reverse-registered legacy class gets NO native ivars; its ivars live in a
 * per-instance i386-layout SHADOW. When a legacy class INHERITS from a native
 * class, the native superclass's ivars live ONLY in the real x86_64 object, so
 * a legacy IMP that reads/writes an inherited protected ivar at its hardcoded
 * i386 offset (exactly as a fragile-ABI app compiled against the 10.6 SDK does,
 * e.g. NSControl._cell @84) hits the zeroed shadow -> 0/nil -> garbage.
 *
 * objc_shim.c's sync_inherited_ivars/ivar_wb_flush keep the shadow's inherited
 * region coherent with the real object BOTH ways around each legacy dispatch.
 * Here the native superclass is NSError, whose 10.6 i386 ivar _code sits at
 * offset 8 and _domain at offset 12 (gen_native_ivar_map.py extracted these
 * from the SL i386 Foundation; the modern offsets — _code@16, _domain@24 — and
 * the NSInteger 4->8 widening are resolved at runtime).
 *
 * READ  : native -initWithDomain:code:userInfo: sets the real object's _code;
 *         a legacy IMP reads it from the shadow at the i386 offset.
 * WRITE : a legacy IMP writes _code in the shadow; the native -code accessor
 *         (and a later legacy read) observe it, while an untouched inherited
 *         ivar (_domain) is NOT clobbered (dirty-tracking).
 *
 * puts("ok N")/exit(failures), no vararg/fp printf on the success path (that is
 * a separate libabiconv gap; see 22_objc_fp_struct).
 */
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>

/* legacy subclass of a NATIVE class; declares NO own ivars (it reaches the
 * inherited ones by hardcoded i386 offset, like the real apps). */
@interface InhError : NSError
- (int)legacyCode;             /* read inherited _code at i386 off 8 */
- (int)legacyDomainLen;        /* read inherited _domain at i386 off 12, msg it */
- (void)legacyPokeCode:(int)v; /* write inherited _code at i386 off 8 */
@end
@implementation InhError
- (int)legacyCode {
    return *(volatile int *)((char *)(void *)self + 8);
}
- (int)legacyDomainLen {
    id d = *(id *)((char *)(void *)self + 12);
    return d ? (int)[d length] : -1;
}
- (void)legacyPokeCode:(int)v {
    *(volatile int *)((char *)(void *)self + 8) = v;
}
@end

int main(void) {
    int fail = 0;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    InhError *e = [[InhError alloc] initWithDomain:@"d" code:42 userInfo:nil];

    /* READ: the legacy IMP reads the inherited _code the native init wrote. */
    if ([e legacyCode] == 42) puts("ok 1");
    else { printf("FAIL 1 read _code got %d\n", [e legacyCode]); fail++; }

    /* native side stays consistent */
    if ([e code] == 42) puts("ok 2");
    else { printf("FAIL 2 native code=%ld\n", (long)[e code]); fail++; }

    /* READ object ivar: the inherited _domain syncs as a bridge handle that
     * still messages back to the real @"d" (length 1). */
    if ([e legacyDomainLen] == 1) puts("ok 3");
    else { printf("FAIL 3 domain len=%d\n", [e legacyDomainLen]); fail++; }

    /* WRITE-BACK: the legacy IMP mutates the inherited _code in the shadow;
     * the native accessor must observe it on the real object. */
    [e legacyPokeCode:99];
    if ([e code] == 99) puts("ok 4");
    else { printf("FAIL 4 writeback native code=%ld\n", (long)[e code]); fail++; }

    /* a later legacy read sees its own write (round-trips through the real obj) */
    if ([e legacyCode] == 99) puts("ok 5");
    else { printf("FAIL 5 reread _code got %d\n", [e legacyCode]); fail++; }

    /* dirty-tracking: poking _code must NOT clobber the untouched _domain */
    if ([[e domain] isEqualToString:@"d"]) puts("ok 6");
    else { puts("FAIL 6 domain clobbered"); fail++; }

    [e release];
    [pool drain];
    exit(fail);
}
