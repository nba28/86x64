/* 41_objc_legacy_load.m — fragile-ObjC1 +load on reverse-registered legacy
 * classes.
 *
 * The pre-ObjC2 runtime's call_load_methods() ran +load at image load,
 * superclass-first, after the class and its categories were attached.
 * objc_registerClassPair() (how we reverse-register legacy classes) does NOT
 * trigger +load, so a legacy class's +load never ran. reverse_register_image
 * now replays each registered class's own +load through the native->i386 bridge.
 *
 * Verifies, all BEFORE main: base +load ran, subclass +load ran, and the
 * superclass-first ordering contract. Without the fix every counter stays 0 and
 * the test FAILs. The classes are never messaged from main — registration (and
 * thus +load) is driven purely by the __OBJC metadata walk at image load.
 */
#include <Foundation/Foundation.h>
#include <stdio.h>

static int g_order     = 0;   /* monotonic: proves +load precedes main */
static int g_base_load = 0;
static int g_sub_load  = 0;

@interface LoadBase : NSObject
@end
@implementation LoadBase
+ (void)load { g_base_load = ++g_order; }
@end

@interface LoadSub : LoadBase
@end
@implementation LoadSub
+ (void)load { g_sub_load = ++g_order; }
@end

int main(void) {
    int failures = 0;
    int main_order = ++g_order;

    if (g_base_load > 0) puts("ok base_load");
    else { puts("FAIL base_load"); failures++; }

    if (g_sub_load > 0) puts("ok sub_load");
    else { puts("FAIL sub_load"); failures++; }

    if (g_base_load && g_sub_load && g_base_load < g_sub_load) puts("ok order");
    else { puts("FAIL order"); failures++; }

    if (g_base_load && g_sub_load
        && g_base_load < main_order && g_sub_load < main_order) puts("ok before_main");
    else { puts("FAIL before_main"); failures++; }

    exit(failures);
}
