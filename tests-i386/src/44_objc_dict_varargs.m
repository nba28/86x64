/* 44_objc_dict_varargs.m — nil-terminated id-list variadic ObjC bridge.
 *
 * +[NSDictionary dictionaryWithObjectsAndKeys:] takes a nil-terminated
 * (value, key, value, key, ..., nil) list. The ObjC method type encoding does
 * NOT model the varargs (method_getNumberOfArguments stops at firstObject), so
 * the i386->x86_64 forward bridge (objc_shim.c fill_args_and_return varargs
 * continuation) must walk the i386 stack args, unwrap each proxy handle to a
 * native id, place them into the SysV GP regs then spill onto the plan stack,
 * AND emit the nil terminator — or Foundation reads uninitialized memory and
 * either retains garbage (SIGSEGV) or throws.
 *
 * Quinn 3.5.7 (Sparkle's SUUpdater) crashes here: nondeterministic SIGSEGV
 * (KERN_INVALID_ADDRESS at 0x18) or an uncaught NSException thrown from inside
 * +[NSDictionary dictionaryWithObjectsAndKeys:].
 *
 * This test covers BOTH the register-only case (small dict) and the stack-spill
 * case (>2 pairs => the varargs run past the 6 GP regs). All keys/values are
 * real objects (constant NSString keys, NSNumber values) so every list element
 * must be unwrapped, and the nil terminator must survive.
 */
#include <Foundation/Foundation.h>
#include <stdio.h>

int main(void) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int failures = 0;

    /* --- Case A: one pair (fits entirely in GP registers) --- */
    NSDictionary *d1 = [NSDictionary dictionaryWithObjectsAndKeys:
                          [NSNumber numberWithInt:11], @"a",
                          nil];
    if (d1 && [d1 count] == 1
        && [[d1 objectForKey:@"a"] intValue] == 11) puts("ok one_pair");
    else { puts("FAIL one_pair"); failures++; }

    /* --- Case B: four pairs (spills past the 6 GP regs onto the stack) --- */
    NSDictionary *d4 = [NSDictionary dictionaryWithObjectsAndKeys:
                          [NSNumber numberWithInt:1], @"k1",
                          [NSNumber numberWithInt:2], @"k2",
                          [NSNumber numberWithInt:3], @"k3",
                          [NSNumber numberWithInt:4], @"k4",
                          nil];
    if (d4 && [d4 count] == 4
        && [[d4 objectForKey:@"k1"] intValue] == 1
        && [[d4 objectForKey:@"k2"] intValue] == 2
        && [[d4 objectForKey:@"k3"] intValue] == 3
        && [[d4 objectForKey:@"k4"] intValue] == 4) puts("ok four_pairs");
    else { puts("FAIL four_pairs"); failures++; }

    /* --- Case C: a LONG list (20 pairs) that overflows the plan's GP regs AND
     * its stack-spill capacity. The bridge must carry the nil terminator all
     * the way out; with the old 32-qword cap the terminator was dropped and
     * Foundation walked off the end of the args -> crash (the Quinn wall). */
    NSDictionary *big = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithInt: 0], @"k00", [NSNumber numberWithInt: 1], @"k01",
        [NSNumber numberWithInt: 2], @"k02", [NSNumber numberWithInt: 3], @"k03",
        [NSNumber numberWithInt: 4], @"k04", [NSNumber numberWithInt: 5], @"k05",
        [NSNumber numberWithInt: 6], @"k06", [NSNumber numberWithInt: 7], @"k07",
        [NSNumber numberWithInt: 8], @"k08", [NSNumber numberWithInt: 9], @"k09",
        [NSNumber numberWithInt:10], @"k10", [NSNumber numberWithInt:11], @"k11",
        [NSNumber numberWithInt:12], @"k12", [NSNumber numberWithInt:13], @"k13",
        [NSNumber numberWithInt:14], @"k14", [NSNumber numberWithInt:15], @"k15",
        [NSNumber numberWithInt:16], @"k16", [NSNumber numberWithInt:17], @"k17",
        [NSNumber numberWithInt:18], @"k18", [NSNumber numberWithInt:19], @"k19",
        nil];
    int big_ok = (big && [big count] == 20);
    for (int i = 0; big_ok && i < 20; ++i) {
        /* build "kNN" without any printf-family call (avoids the vararg path) */
        char k[4]; k[0] = 'k'; k[1] = (char)('0' + i / 10); k[2] = (char)('0' + i % 10); k[3] = 0;
        NSString *key = [NSString stringWithUTF8String:k];
        if ([[big objectForKey:key] intValue] != i) big_ok = 0;
    }
    if (big_ok) puts("ok twenty_pairs");
    else { puts("FAIL twenty_pairs"); failures++; }

    /* --- Case D: arrayWithObjects: (the original nil-terminated id list) --- */
    NSArray *a = [NSArray arrayWithObjects:@"x", @"y", @"z", nil];
    if (a && [a count] == 3
        && [[a objectAtIndex:0] isEqualToString:@"x"]
        && [[a objectAtIndex:2] isEqualToString:@"z"]) puts("ok array_objects");
    else { puts("FAIL array_objects"); failures++; }

    [pool release];
    exit(failures);
}
