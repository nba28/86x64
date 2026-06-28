/* 47_objc_legacy_variadic.m — nil-terminated VARIADIC method on a reverse-
 * registered legacy class (the Quinn -[ATAnimationGroup addAnimations:a,b,nil]
 * class of bug).
 *
 * The ObjC type encoding never models `...`, so a legacy variadic method
 * encodes exactly like a 1-arg method (here `i12@0:4@8`). The forward AND
 * reverse bridges thus marshalled only the single DECLARED object and the
 * legacy IMP's va_arg walk read leftover lowstack words as bogus object
 * pointers — exactly Quinn's `[__NSCFArray addObject:0x00080000]` garbage
 * receiver that later faulted in CoreFoundation's sort.
 *
 * The fix detects the i386 `va_start` idiom STRUCTURALLY (the IMP takes the
 * address of the first stack slot past its declared args, `lea (8+framesize)
 * (%rbp),r32`, which a non-variadic cdecl method never does) and (a) the
 * forward bridge spills the whole nil-terminated id list into the native call,
 * (b) the reverse bridge re-lays it into the IMP's i386 cdecl frame. Detection
 * is published cross-copy (Class/SEL) so the forward bridge — which runs in the
 * CALLER's libabiconv copy, not the registering one — can see it.
 *
 * Collector is a legacy class (reverse-registered at image load); main messages
 * it from i386, so each call crosses forward->reverse. Without the fix the 2nd+
 * objects are garbage: countAndLen: sees count==1 (or crashes in -[s length]
 * on a bogus pointer). With it, the entire variadic list round-trips and the
 * count AND each object's identity ([s length]) are correct.
 */
#include <Foundation/Foundation.h>
#include <stdarg.h>
#include <stdio.h>

@interface Collector : NSObject
- (int)countAndLen:(NSString *)first, ... NS_REQUIRES_NIL_TERMINATION;
@end

@implementation Collector
/* Returns count*1000 + sum of lengths, so BOTH the arg count AND each object's
 * identity (its real length, via a message send back across the bridge) must
 * survive the variadic marshalling. */
- (int)countAndLen:(NSString *)first, ... {
    int count = 0, len = 0;
    va_list ap;
    va_start(ap, first);
    for (NSString *s = first; s != nil; s = va_arg(ap, NSString *)) {
        count++;
        len += (int)[s length];
    }
    va_end(ap);
    return count * 1000 + len;
}
@end

int main(void) {
    int failures = 0;
    Collector *c = [[Collector alloc] init];

    /* 3 strings, lengths 3+5+2 = 10 -> 3*1000+10 = 3010 */
    int r3 = [c countAndLen:@"abc", @"defgh", @"ij", nil];
    if (r3 == 3010) puts("ok three");
    else { printf("FAIL three (got %d)\n", r3); failures++; }

    /* only the declared arg, length 4 -> 1*1000+4 = 1004 */
    int r1 = [c countAndLen:@"wxyz", nil];
    if (r1 == 1004) puts("ok one");
    else { printf("FAIL one (got %d)\n", r1); failures++; }

    /* 5 single-char strings -> 5*1000+5 = 5005 (fills past the GP regs) */
    int r5 = [c countAndLen:@"a", @"b", @"c", @"d", @"e", nil];
    if (r5 == 5005) puts("ok five");
    else { printf("FAIL five (got %d)\n", r5); failures++; }

    exit(failures);
}
