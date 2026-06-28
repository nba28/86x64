/* 42_objc_swizzle.m — method_exchangeImplementations on reverse-registered
 * legacy methods.
 *
 * Both legacy methods dispatch through the SINGLE shared _86x64_reverse_imp
 * trampoline, which re-derives the i386 IMP from (class,sel) at dispatch. So a
 * bare runtime exchange is a no-op (both Method slots hold the same trampoline)
 * AND, reached natively with our synthetic 12-byte i386_method32 handles
 * reinterpreted as 64-bit objc_method structs, it reads/writes the imp field
 * PAST the struct and corrupts the heap -> SIGSEGV.
 *
 * shim_method_exchangeImplementations swaps the (class,sel)->legacy-IMP entries
 * in g_rmeth so SEL-keyed dispatch honours the swizzle, and is involutive
 * (exchanging twice restores). Without the fix the program crashes / mis-routes.
 */
#include <Foundation/Foundation.h>
#include <objc/runtime.h>
#include <stdio.h>

@interface Swz : NSObject
@end
@implementation Swz
- (int)one { return 1; }
- (int)two { return 2; }
@end

int main(void) {
    int failures = 0;
    Swz *o = [[Swz alloc] init];

    if ([o one] == 1) puts("ok pre_one"); else { puts("FAIL pre_one"); failures++; }
    if ([o two] == 2) puts("ok pre_two"); else { puts("FAIL pre_two"); failures++; }

    Method m1 = class_getInstanceMethod([Swz class], @selector(one));
    Method m2 = class_getInstanceMethod([Swz class], @selector(two));
    method_exchangeImplementations(m1, m2);

    if ([o one] == 2) puts("ok swz_one"); else { puts("FAIL swz_one"); failures++; }
    if ([o two] == 1) puts("ok swz_two"); else { puts("FAIL swz_two"); failures++; }

    method_exchangeImplementations(m1, m2);   /* involutive: restore */

    if ([o one] == 1) puts("ok restore_one"); else { puts("FAIL restore_one"); failures++; }
    if ([o two] == 2) puts("ok restore_two"); else { puts("FAIL restore_two"); failures++; }

    exit(failures);
}
