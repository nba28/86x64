/*
 * 10_objc_legacy_classmethod — exercises legacy class-METHOD dispatch: a class
 * the program DEFINES itself (compiled -fobjc-runtime=macosx-fragile, so it
 * lives only in legacy __OBJC metadata and is invisible to the modern x86_64
 * libobjc). objc_getClass("Calc") returns nil, so the bridge must locate the
 * translated class-method IMP in the legacy metadata and resume the i386
 * dispatch straight into it (objc_shim.c legacy_class_method_imp + the
 * objc_msgSend.asm %%legacy path).
 *
 * Three cases, increasing in stress:
 *   +addBaseTo:        pure arithmetic IMP, one int arg, int return.
 *   +tripleViaSelf:    IMP that sends ANOTHER class message to self ([self ...])
 *                      — verifies self survives the round trip and the nested
 *                      bridge re-entry works.
 *   +sumWith:and:      two int args.
 *
 * Validates via EXIT CODE (0 = all correct), like 09 — keeps the result off
 * the printf varargs path.
 */
#import <Foundation/Foundation.h>

extern void _exit(int);

@interface Calc : NSObject
+ (int)addBaseTo:(int)x;
+ (int)doubleIt:(int)x;
+ (int)tripleViaSelf:(int)x;
+ (int)sumWith:(int)a and:(int)b;
@end

@implementation Calc
+ (int)addBaseTo:(int)x { return x + 100; }
+ (int)doubleIt:(int)x { return x * 2; }
+ (int)tripleViaSelf:(int)x { return [self doubleIt:x] + x; }
+ (int)sumWith:(int)a and:(int)b { return a + b; }
@end

int main(void) {
    int r1 = [Calc addBaseTo:23];        /* 123 */
    int r2 = [Calc doubleIt:21];         /* 42  */
    int r3 = [Calc tripleViaSelf:9];     /* 27  */
    int r4 = [Calc sumWith:40 and:2];    /* 42  */

    _exit((r1 == 123 && r2 == 42 && r3 == 27 && r4 == 42) ? 0 : 1);
}
