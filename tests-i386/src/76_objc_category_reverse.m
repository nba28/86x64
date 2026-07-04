/* 76_objc_category_reverse.m — a fragile-ObjC1 CATEGORY method on a
 * reverse-registered legacy class must survive the __OBJC slide + registration
 * and be dispatchable from the NATIVE runtime.
 *
 * iPhoto #7: libobjc's map_images old-ABI reader (runs BEFORE slide_objc's
 * add-image callback) uniques method-name selectors by writing the 8-byte
 * native SEL into the classic 4-byte legacy_objc_method.name slot; the 8-byte
 * write also clobbers the adjacent .types slot. reverse_add_methods then reads
 * the clobbered name, fails legacy_cstr_ok, and SKIPS the method -> the category
 * accessor -[ArchiveAlbum projectUUID] was intermittently (~13%, heap/ASLR-
 * layout dependent) registered as an "unrecognized selector", aborting iPhoto's
 * album-list enumeration. objc_slide.c now file-repairs the classic __category
 * structs + method-list name/types slots (repair_method_lists_from_file /
 * repair_refs_from_file), in the same phase as the __message_refs/__cls_refs
 * repair, before the legacy-class indexing consumes the lists.
 *
 * The live clobber is layout-dependent, so a single run can't force it; this is
 * a documented PIN of the category-registration path: a legacy category method
 * (its own __cat_inst_meth list, separate from the class's __inst_meth) must be
 * seen by the native runtime and dispatch correctly through the native
 * objc_msgSend / reverse bridge.
 */
#include <Foundation/Foundation.h>
#include <stdio.h>

@interface CatBase : NSObject
- (NSString *)baseTag;
@end
@implementation CatBase
- (NSString *)baseTag { return @"base"; }
@end

/* method under test lives in a CATEGORY -> a separate __cat_inst_meth list */
@interface CatBase (Extra)
- (NSString *)catTag;
@end
@implementation CatBase (Extra)
- (NSString *)catTag { return @"catok"; }
@end

int main(void) {
    int failures = 0;
    CatBase *o = [CatBase new];

    /* the NATIVE runtime must see the category method on the reverse-registered
     * class (respondsToSelector: is a native NSObject method) */
    if ([o respondsToSelector:@selector(catTag)]) puts("ok responds_cat");
    else { puts("FAIL responds_cat"); failures++; }

    /* the base (non-category) method still resolves */
    if ([[o baseTag] isEqualToString:@"base"]) puts("ok base");
    else { puts("FAIL base"); failures++; }

    /* dispatch the category method through the NATIVE objc_msgSend path
     * (performSelector: is native) -> reverse bridge -> i386 IMP -> object ret */
    NSString *r = [o performSelector:@selector(catTag)];
    if (r && [r isEqualToString:@"catok"]) puts("ok cat_dispatch");
    else { puts("FAIL cat_dispatch"); failures++; }

    exit(failures);
}
