/* 99_super_hint_native — a legacy override that calls [super sel] into a NATIVE
 * super must not leave a stale super-dispatch hint behind. objc_bridge_prep_super
 * recorded one unconditionally; only a LEGACY super consumes it, so the next
 * native dispatch of the same (receiver, sel) took it, looked sel up from the
 * native super upward, found no legacy method and returned nil without running
 * the override. Portal 2 launcher: -[NSValveApplication sendEvent:] ->
 * [super sendEvent:] for every event, so every other event AppKit dispatched
 * was dropped and trackpad clicks never reached the game.
 *
 * Mirrors the launcher: an NSApplication subclass whose sendEvent: calls super,
 * pumped with nextEventMatchingMask:0xffffffff + [NSApp sendEvent:] (window never
 * shown). Exit 42 = all 4 posted events reached the legacy override. Kill switch
 * M64_NO_SUPER_HINT_GATE=1 (run time): every other event is dropped. */
#import <AppKit/AppKit.h>
#include <stdio.h>
#include <stdlib.h>
static int g_app;
@interface ValveApp : NSApplication @end
@implementation ValveApp
- (void)sendEvent:(NSEvent *)e {
   if ([e type] == NSLeftMouseDown || [e type] == NSLeftMouseUp) g_app++;
   if ([e modifierFlags] & NSCommandKeyMask) { [[self keyWindow] sendEvent:e]; return; }
   [super sendEvent:e];
}
@end
int main(void) {
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   [ValveApp sharedApplication];
   NSWindow *w = [[NSWindow alloc] initWithContentRect:NSMakeRect(100,100,200,200)
                  styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
   for (int i = 0; i < 4; i++) {
      NSEvent *e = [NSEvent mouseEventWithType:(i & 1) ? NSLeftMouseUp : NSLeftMouseDown
                    location:NSMakePoint(50,50) modifierFlags:0 timestamp:0
                    windowNumber:[w windowNumber] context:nil eventNumber:i clickCount:1 pressure:1];
      [NSApp postEvent:e atStart:NO];
   }
   for (int k = 0; k < 4; k++) {
      NSEvent *e;
      while ((e = [NSApp nextEventMatchingMask:0xffffffff untilDate:[NSDate distantPast]
                   inMode:NSDefaultRunLoopMode dequeue:YES]) != nil) {
         [NSApp sendEvent:e];
      }
   }
   printf("legacy sendEvent: saw %d of 4\n", g_app);
   [pool release];
   exit(g_app == 4 ? 42 : 1);
}
