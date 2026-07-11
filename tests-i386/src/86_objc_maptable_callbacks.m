/* 86_objc_maptable_callbacks.m — legacy NSMapTable / NSHashTable object-callback
 * data-shadow guard (the Numbers/Pages SFCompatibility + SFProofReader wall).
 *
 * A 32-bit app that uses the old Foundation collection C API reads the global
 * callback structs (NSObjectMapKeyCallBacks, NSObjectMapValueCallBacks,
 * NSObjectHashCallBacks, ...) field-by-field with 4-byte movl's and copies them
 * onto the stack to pass BY VALUE to NSCreateMapTable / NSCreateHashTable. On
 * x86_64 those globals live at 64-bit Foundation addresses, so the i386
 * `movl slot,%reg` that loads the struct's ADDRESS truncates the pointer and
 * the subsequent deref faults.
 *
 * The fix is a libabiconv low-4GB DATA SHADOW `___NS<set>CallBacks` for each
 * callback global: translate-time static-interpose redirects the binary's
 * non-lazy bind of `_NS<set>CallBacks` there ONLY IF the shadow is exported.
 * The shadow is a tagged i386-layout struct; the maptable_shim recognises the
 * tag and creates a REAL Foundation collection with the matching REAL callback
 * set, so hashing/retain/release/isEqual behave correctly.
 *
 * This guards the OBJECT callback family specifically:
 *   - NSObjectMapKeyCallBacks / NSObjectMapValueCallBacks (SFCompatibility
 *     -[CPEnumerationMap init]; already shadowed) — map with object key+value;
 *   - NSObjectHashCallBacks (SFProofReader; the shadow that was MISSING) — hash
 *     set of objects.
 *
 * Object semantics are the discriminator: keys/members are distinct NSString
 * INSTANCES that are -isEqual: but not pointer-identical. Real object callbacks
 * (hash = -hash, isEqual = -isEqual:) DEDUPE them; a zeroed/garbage callback
 * struct (what an auto function-stub or a truncated pointer yields) would use
 * pointer identity and NOT dedupe — or crash outright. If the shadow is
 * reverted, the i386 4-byte load of Foundation's >4GB struct truncates and the
 * copy-by-value faults (or the stub returns 0 -> wrong callbacks) and this test
 * FAILs / crashes.
 */
#import <Foundation/Foundation.h>
#include <stdio.h>

/* two distinct NSString instances that compare equal but are not the same
 * pointer, so only genuine object callbacks (-hash/-isEqual:) treat them as one
 * key/member. */
static NSString *mkstr(const char *s) {
    /* -initWithFormat: forces a fresh, non-interned instance (unlike @"lit"). */
    return [[NSString alloc] initWithFormat:@"%s", s];
}

int main(void) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int failures = 0;

    /* --- object-keyed / object-valued NSMapTable --- */
    NSMapTable *m = NSCreateMapTable(NSObjectMapKeyCallBacks,
                                     NSObjectMapValueCallBacks, 0);
    if (!m) { puts("FAIL map_create"); failures++; goto hash; }
    puts("ok map_create");

    NSString *k1 = mkstr("alpha");
    NSString *v1 = [NSString stringWithUTF8String:"one"];
    NSMapInsert(m, k1, v1);
    /* look up with a DIFFERENT instance equal to k1: object key callbacks must
     * find it (isEqual/hash), pointer callbacks would miss. */
    NSString *k1b = mkstr("alpha");
    id got = (id)NSMapGet(m, k1b);
    if (got && [got isEqualToString:@"one"]) puts("ok map_lookup_by_equal_key");
    else { puts("FAIL map_lookup_by_equal_key"); failures++; }

    /* re-insert an equal key: object semantics -> still ONE entry (dedup). */
    NSMapInsert(m, mkstr("alpha"), [NSString stringWithUTF8String:"two"]);
    NSMapInsert(m, mkstr("beta"),  [NSString stringWithUTF8String:"three"]);
    if (NSCountMapTable(m) == 2) puts("ok map_count_dedup");
    else {
        printf("FAIL map_count_dedup (count=%lu, want 2)\n",
               (unsigned long)NSCountMapTable(m));
        failures++;
    }
    /* the equal-key re-insert must have REPLACED the value. */
    id got2 = (id)NSMapGet(m, mkstr("alpha"));
    if (got2 && [got2 isEqualToString:@"two"]) puts("ok map_value_replaced");
    else { puts("FAIL map_value_replaced"); failures++; }
    NSFreeMapTable(m);

hash:;
    /* --- object NSHashTable (NSObjectHashCallBacks — the missing shadow) --- */
    NSHashTable *h = NSCreateHashTable(NSObjectHashCallBacks, 0);
    if (!h) { puts("FAIL hash_create"); failures++; goto done; }
    puts("ok hash_create");

    NSHashInsert(h, mkstr("x"));
    NSHashInsert(h, mkstr("y"));
    NSHashInsert(h, mkstr("x"));   /* equal to the first -> object dedup */
    if (NSCountHashTable(h) == 2) puts("ok hash_count_dedup");
    else {
        printf("FAIL hash_count_dedup (count=%lu, want 2)\n",
               (unsigned long)NSCountHashTable(h));
        failures++;
    }
    /* membership by an equal-but-distinct instance. */
    id hm = (id)NSHashGet(h, mkstr("y"));
    if (hm && [hm isEqualToString:@"y"]) puts("ok hash_member_by_equal");
    else { puts("FAIL hash_member_by_equal"); failures++; }
    NSFreeHashTable(h);

done:
    [pool release];
    exit(failures);
}
