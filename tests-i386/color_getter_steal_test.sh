#!/bin/bash
# color-getter-steal: A/B guard for libinterpose's NSColor component-getter
# contract. A FOREIGN image (this program plays a pre-2010 framework such as
# ProKit) replaces the SYSTEM -getRed:green:blue:alpha: and -whiteComponent with
# implementations that raise, then asks a Display-P3 colour for its components,
# as modern AppKit does mid-layout. ON (libinterpose inserted): the replacement
# is refused and the system getters answer -> exit 42. OFF (not inserted): the
# stolen getter raises -> exit 1. A CONTROL override that a class declares for
# ITSELF must still install in both arms (exit 3 if refused).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
LIB="$HERE/../build/src/86x64/libinterpose.dylib"
T="${TMPDIR:-/tmp}/color_getter_steal.$$"; mkdir -p "$T"
cat > "$T/t.m" <<'SRC'
#import <AppKit/AppKit.h>
#import <objc/runtime.h>
static void raising(id s, SEL c, CGFloat *r, CGFloat *g, CGFloat *b, CGFloat *a) {
   [NSException raise:NSInvalidArgumentException format:@"legacy getter: colorspace too new"];
}
static CGFloat raising1(id s, SEL c) {
   [NSException raise:NSInvalidArgumentException format:@"legacy getter: colorspace too new"];
   return 0;
}
@interface MyColor : NSColor @end
@implementation MyColor
- (CGFloat)alphaComponent { return 1; }
- (CGFloat)whiteComponent { return 0.25; }
@end
static CGFloat mine(id s, SEL c) { return 0.75; }
int main(void) {
   @autoreleasepool {
      NSColor *c = [NSColor colorWithDisplayP3Red:0.2 green:0.4 blue:0.6 alpha:1];
      NSColor *w = [c colorUsingColorSpace:NSColorSpace.extendedGenericGamma22GrayColorSpace];
      method_setImplementation(class_getInstanceMethod(object_getClass(c), @selector(getRed:green:blue:alpha:)), (IMP)raising);
      method_setImplementation(class_getInstanceMethod(object_getClass(w), @selector(whiteComponent)), (IMP)raising1);
      /* control: a subclass's OWN override is foreign-over-foreign -> allowed */
      method_setImplementation(class_getInstanceMethod([MyColor class], @selector(whiteComponent)), (IMP)mine);
      if ([[MyColor new] whiteComponent] != 0.75) return 3;
      @try {
         CGFloat r, g, b, a;
         [c getRed:&r green:&g blue:&b alpha:&a];
         (void)[w whiteComponent];
      } @catch (NSException *e) { return 1; }
   }
   return 42;
}
SRC
clang -arch x86_64 -fobjc-arc -framework AppKit -o "$T/t" "$T/t.m" || { echo "color-getter-steal: FAIL (build)"; exit 1; }
DYLD_INSERT_LIBRARIES="$LIB" "$T/t"; on=$?
"$T/t"; off=$?
rm -rf "$T"
[ "$on/$off" = 42/1 ] && echo "color-getter-steal: PASS" || { echo "color-getter-steal: FAIL (on=$on off=$off)"; exit 1; }
