#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "AppKit", s); }

@interface NSColorPanelColorWell : NSObject
@end
@implementation NSColorPanelColorWell

@end

@interface NSColorPickerColorSpacePopUp : NSObject
@end
@implementation NSColorPickerColorSpacePopUp

@end

@interface NSColorPickerPageableNameList : NSObject
@end
@implementation NSColorPickerPageableNameList

@end

@interface NSColorPickerPageableNameListScrollView : NSObject
@end
@implementation NSColorPickerPageableNameListScrollView

@end

@interface NSColorPickerUser : NSObject
@end
@implementation NSColorPickerUser

@end

@interface NSColorSwatchCell : NSObject
@end
@implementation NSColorSwatchCell

@end

@interface NSFlippableView : NSView
@end
@implementation NSFlippableView
- (BOOL)isFlipped { return YES; }
@end

@interface NSFontEffectsBox : NSObject
@end
@implementation NSFontEffectsBox

@end

@interface NSPageableTableView : NSObject
@end
@implementation NSPageableTableView

@end

@interface NSTextViewSharedData : NSObject
@end
@implementation NSTextViewSharedData

@end

@interface NSToolbarClippedItemsIndicator : NSObject
@end
@implementation NSToolbarClippedItemsIndicator

@end

@interface NSToolbarConfigPanel : NSObject
@end
@implementation NSToolbarConfigPanel

@end

@interface _NSColorPanelToolbar : NSObject
@end
@implementation _NSColorPanelToolbar

@end
