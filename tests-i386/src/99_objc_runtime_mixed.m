/* 99_objc_runtime_mixed — ObjC1 runtime features that cross between a
 * translated (legacy, reverse-registered) method and a native one.
 *
 *   swizzle_*:  the textbook category swizzle — a legacy category method on a
 *               NATIVE class exchanged with that class's NATIVE method. After
 *               the exchange, sending the native selector must run the i386
 *               IMP (from i386 code AND from native code), and the i386 IMP's
 *               `[self x_description]` must reach the ORIGINAL native IMP.
 *               A second exchange restores both.
 *   forward_*:  a legacy class implementing methodSignatureForSelector: +
 *               forwardInvocation: receives messages it does not implement
 *               and forwards them to a legacy target and to a native object.
 *   nextlist_*: class_nextMethodList enumerates a legacy class's methods.
 */
#include <Foundation/Foundation.h>
#include <objc/objc-runtime.h>
#include <stdio.h>
#include <string.h>

/* The ObjC1 method-list API as the i386 10.6 SDK declared it (the modern SDK
 * hides it). */
struct m1_method { SEL method_name; char *method_types; IMP method_imp; };
struct m1_method_list { void *obsolete; int method_count; struct m1_method method_list[1]; };
extern struct m1_method_list *class_nextMethodList(Class, void **);

@interface Plain : NSObject
@end
@implementation Plain
@end

@interface NSObject (XDesc)
- (NSString *)x_description;
@end
@implementation NSObject (XDesc)
- (NSString *)x_description {
    return [@"X" stringByAppendingString:[self x_description]];
}
@end

@interface Target : NSObject
- (int)add:(int)a to:(int)b;
- (NSString *)greet:(NSString *)who;
@end
@implementation Target
- (int)add:(int)a to:(int)b { return a + b; }
- (NSString *)greet:(NSString *)who { return [@"hi " stringByAppendingString:who]; }
@end

@interface Fwd : NSObject {
    id target;
    id other;
}
@end
@implementation Fwd
- (id)init {
    if ((self = [super init])) {
        target = [[Target alloc] init];
        other = [@"hello" retain];
    }
    return self;
}
- (NSMethodSignature *)methodSignatureForSelector:(SEL)sel {
    NSMethodSignature *s = [target methodSignatureForSelector:sel];
    return s ? s : [other methodSignatureForSelector:sel];
}
- (void)forwardInvocation:(NSInvocation *)inv {
    if ([target respondsToSelector:[inv selector]]) [inv invokeWithTarget:target];
    else [inv invokeWithTarget:other];
}
@end

static int check(int ok, const char *name) {
    printf("%s %s\n", ok ? "ok" : "FAIL", name);
    return ok ? 0 : 1;
}

int main(void) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int failures = 0;

    /* ---- swizzle: legacy category method <-> native method ---- */
    Plain *p = [[Plain alloc] init];
    Method mn = class_getInstanceMethod([NSObject class], @selector(description));
    Method ml = class_getInstanceMethod([NSObject class], @selector(x_description));
    failures += check(mn && ml, "swizzle_methods_found");
    method_exchangeImplementations(mn, ml);
    NSString *d = [p description];                          /* i386 -> native */
    failures += check(d && [d hasPrefix:@"X<Plain"], "swizzle_i386_send");
    NSString *f = [NSString stringWithFormat:@"%@", p];     /* native -> desc */
    failures += check(f && [f hasPrefix:@"X<Plain"], "swizzle_native_send");
    method_exchangeImplementations(mn, ml);
    d = [p description];
    failures += check(d && [d hasPrefix:@"<Plain"], "swizzle_restore");

    /* ---- forwarding ---- */
    id fw = [[Fwd alloc] init];
    failures += check([fw add:2 to:3] == 5, "forward_legacy_int");
    NSString *g = [fw greet:@"bob"];
    failures += check(g && [g isEqualToString:@"hi bob"], "forward_legacy_obj");
    failures += check([fw length] == 5, "forward_native");

    /* ---- class_nextMethodList ---- */
    void *it = 0;
    struct m1_method_list *ml2;
    int n = 0, seen = 0;
    while ((ml2 = class_nextMethodList([Target class], &it))) {
        int i;
        for (i = 0; i < ml2->method_count; i++) {
            n++;
            if (strcmp(sel_getName(ml2->method_list[i].method_name), "add:to:") == 0) seen = 1;
        }
    }
    failures += check(n == 2 && seen, "nextlist_legacy");

    [pool release];
    exit(failures);
}
