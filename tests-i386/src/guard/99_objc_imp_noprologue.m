/*
 * 99_objc_imp_noprologue — an ObjC1 method IMP with no symbol and no
 * `push %ebp; mov %esp,%ebp` prologue must still be relocated.
 *
 * iPhoto '11's main image is locals-stripped and many of its methods are
 * frame-pointer-less, so the code-entry gate (which wants an nlist or a
 * `55 89 e5` prologue at a data-resident code pointer) left ~1.7k IMPs in
 * __OBJC,__inst_meth as raw i386 addresses; the legacy class index then
 * dropped whole classes (+[PreferenceKeys defaultBackgroundColorData]
 * unrecognized). A method-list IMP slot is a function entry by construction.
 *
 * Built -fomit-frame-pointer and stripped -x by objc_imp_entry_test.sh, which
 * asserts BYTES: the translated __OBJC,__cls_meth IMP word must be a
 * translated __text address (ON) and stays the raw i386 one under
 * M64_NO_OBJC_IMP_ENTRY=1 (OFF). main never messages the class: a cls_refs
 * self-reference does not link with ld64-95 (the objc suite's link-only
 * failures), and the defect is in the translated metadata, not the send.
 */
#import <Foundation/Foundation.h>

extern void _exit(int);

@interface NoFrame : NSObject
+ (int)plus:(int)x;
@end

@implementation NoFrame
+ (int)plus:(int)x { return x + 19; }
@end

int main(void) {
    _exit(0);
}
