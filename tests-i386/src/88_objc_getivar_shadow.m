/* 88_objc_getivar_shadow.m — object_getInstanceVariable from TRANSLATED code
 * must read a reverse-registered legacy class's OWN outlet ivar, including the
 * value connected purely through the modern synthesized ivar (KVC / nib).
 *
 * object_getInstanceVariable HAS a hand shim, but it used to call native
 * libobjc directly, which MISSES a legacy class's SHADOW ivars entirely (the
 * ivars live in the per-instance i386 shadow, not the modern class) and only
 * traced. It now routes through the legacy-aware interpose
 * (x64_object_getInstanceVariable): it reads the shadow first and, when the
 * value was connected only through R's modern synthesized ivar (the outlet was
 * set via KVC/nib and a freshly-created forward shadow hasn't been R->S mirrored
 * yet), falls back to R's modern slot — so a FORWARD read observes the connected
 * outlet either way. Universal: any fragile-ObjC1 app that introspects its own
 * outlet ivars in code (object_getInstanceVariable) after a nib/KVC connection.
 *
 * The outlet is connected via setValue:forKey: (the KVC / native path, which
 * writes R's synthesized ivar), then read back through the translated
 * object_getInstanceVariable call.
 *
 * ok 1  a non-NULL Ivar is returned (the ivar exists in the modern class).
 * ok 2  the out-value is the connected object (read back, messageable).
 * ok 3  reading a genuinely-unset outlet returns nil (not stale garbage).
 *
 * puts("ok N")/exit(failures).
 */
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <stdio.h>
#include <stdlib.h>

@interface Wired : NSObject {
    id firstOutlet;             /* i386 offset 4 — connected  */
    id secondOutlet;            /* i386 offset 8 — left unset  */
}
@end
@implementation Wired
@end

int main(void) {
    int fail = 0;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    Wired *w = [[Wired alloc] init];
    NSString *target = @"wired";                 /* length 5 */

    /* connect firstOutlet the KVC/native way (writes R's synthesized ivar). */
    [w setValue:target forKey:@"firstOutlet"];

    void *out = (void *)0xdead;
    Ivar iv = object_getInstanceVariable(w, "firstOutlet", &out);
    if (iv != NULL) puts("ok 1");
    else { puts("FAIL 1 firstOutlet Ivar NULL"); fail++; }

    if (out != NULL && out != (void *)0xdead && (int)[(id)out length] == 5) puts("ok 2");
    else { printf("FAIL 2 firstOutlet out=%p\n", out); fail++; }

    /* an unconnected outlet reads back nil (not stale). */
    void *out2 = (void *)0xbeef;
    object_getInstanceVariable(w, "secondOutlet", &out2);
    if (out2 == NULL) puts("ok 3");
    else { printf("FAIL 3 secondOutlet not nil: %p\n", out2); fail++; }

    [w release];
    [pool drain];
    exit(fail);
}
