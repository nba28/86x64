/* shimdb curated implementations: private AppKit classes removed from modern
 * macOS that translated iLife frameworks subclass or instantiate. A curator
 * writes the real (minimal) behaviour here once; shimgen links the class into
 * the shim only for an app that actually binds _OBJC_CLASS_$_<name>.
 */
#import <Cocoa/Cocoa.h>

/* NSFlippableView: a flipped-coordinate NSView (top-left origin). iLifeKit
 * subclasses it for layout; the one behaviour that matters is -isFlipped. */
@interface NSFlippableView : NSView
@end
@implementation NSFlippableView
- (BOOL)isFlipped { return YES; }
@end
