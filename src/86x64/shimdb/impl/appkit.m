/* shimdb curated implementations: private AppKit classes that translated iLife
 * frameworks subclass or instantiate. A curator writes the real (minimal)
 * behaviour here once; shimgen links the class into the shim only for an app
 * that actually binds _OBJC_CLASS_$_<name>.
 *
 * IMPORTANT: NSFlippableView is once again LIVE in modern AppKit as a PRIVATE
 * class (objc_getClass("NSFlippableView") != nil) — only its _OBJC_CLASS_$_
 * symbol is absent from the export trie. Defining a class literally named
 * "NSFlippableView" here would register a DUPLICATE and corrupt the runtime
 * ("Class NSFlippableView is implemented in both ... mysterious crashes"). So we
 * give the curated class a UNIQUE objc name and ALIAS the _OBJC_CLASS_$_/
 * _OBJC_METACLASS_$_ symbols to it: the dyld bind still resolves and an iLife
 * subclass still inherits a flipped NSView, but the live host class is never
 * shadowed. (Same technique shimgen applies to auto-stub classes it finds live.)
 */
#import <Cocoa/Cocoa.h>

/* a flipped-coordinate NSView (top-left origin); the one behaviour that
 * matters to iLifeKit subclasses is -isFlipped. */
@interface NSFlippableView_86x64fwd : NSView
@end
@implementation NSFlippableView_86x64fwd
- (BOOL)isFlipped { return YES; }
@end

__asm__(
"  .globl _OBJC_CLASS_$_NSFlippableView\n"
"  .set _OBJC_CLASS_$_NSFlippableView, _OBJC_CLASS_$_NSFlippableView_86x64fwd\n"
"  .globl _OBJC_METACLASS_$_NSFlippableView\n"
"  .set _OBJC_METACLASS_$_NSFlippableView, _OBJC_METACLASS_$_NSFlippableView_86x64fwd\n"
);
