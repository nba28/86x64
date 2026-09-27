/*
 * cgl_fullscreen_shim.m — ONE job: the classic CGL FULLSCREEN CONTEXT.
 *
 * THE IDIOM (32-bit-era games that bypass AppKit/AGL windows entirely):
 *     CGDisplayCapture(dpy); CGDisplaySwitchToMode(dpy, 800x600);
 *     CGLChoosePixelFormat({kCGLPFAFullScreen, kCGLPFADisplayMask, ...});
 *     CGLCreateContext(pix, NULL, &ctx); CGLSetFullScreen(ctx);
 *     ... CGLFlushDrawable(ctx) every frame ...
 *     CGLClearDrawable(ctx);
 * Modern CGL has no fullscreen drawables: CGLSetFullScreen answers 10012
 * (kCGLBadDrawable, "invalid fullscreen drawable") and the app has nowhere to
 * render. cg_display_fullscreen_shim.c already virtualises the mode switch and
 * the capture; this is the drawable half of the same bridge.
 *
 * WHAT IT DOES: CGLSetFullScreen(OnDisplay) presents the context in a
 * borderless window of the display's VIRTUAL size (what the app believes the
 * screen is). When the app has its own visible window of exactly that content
 * size (PvZ's Carbon window, which receives its mouse/keyboard events), the
 * surface is a mouse-transparent CHILD over that window's content: input
 * reaches the app and the pair moves together. Otherwise it sits at the
 * display's origin, so virtual-display and global screen
 * coordinates coincide exactly as cg_display_fullscreen_shim.c arranges for
 * window-owning apps. The context is attached through NSOpenGLContext
 * initWithCGLContextObj: — the same native context, so every CGLFlushDrawable
 * / GL call the app makes lands in the window. CGLClearDrawable detaches and
 * closes it.
 *
 * DELIBERATELY NOT DONE: no scaling to fill the physical screen (the mouse
 * mapping would have to scale with it; same scope as the display shim).
 *
 * KILL SWITCH: M64_NO_CGL_FULLSCREEN_BRIDGE=1 forwards to native CGL (the
 * pre-fix 10012). ABI: MTSHIM (rdi -> &i386 args[0]); symbols in custom.syms.
 */
#import <AppKit/AppKit.h>
#import <OpenGL/OpenGL.h>
#include <stdint.h>
#include <stdlib.h>

extern uint64_t cgl_macro_ctx_native(uint32_t h);
extern uint64_t x64_objc_unwrap(uint32_t h);
extern uint32_t shim_CGDisplayPixelsWide(uint32_t *a);
extern uint32_t shim_CGDisplayPixelsHigh(uint32_t *a);

static CGLContextObj ctx_in(uint32_t h)
{
   uint64_t n = cgl_macro_ctx_native(h);
   if (!n) n = x64_objc_unwrap(h);
   return (CGLContextObj)(uintptr_t)n;
}

static int bridge_off(void)
{
   static int v = -1;
   if (v < 0) v = getenv("M64_NO_CGL_FULLSCREEN_BRIDGE") != NULL;
   return v;
}

/* ponytail: one presented context at a time (the idiom never has two); make it
 * a table keyed by ctx if a target ever flips between fullscreen contexts. */
static CGLContextObj    g_ctx;
static NSOpenGLContext *g_ns;
static NSWindow        *g_win;

static void on_main(void (^b)(void))
{
   if ([NSThread isMainThread]) b(); else dispatch_sync(dispatch_get_main_queue(), b);
}

/* The app's own window for this fullscreen: visible, not ours, content exactly
 * the virtual mode size (PvZ keeps an 800x600 Carbon window for input). */
static NSWindow *app_window(CGFloat w, CGFloat h)
{
   NSWindow *best = nil;
   for (NSWindow *win in NSApp.windows) {
      if (win == g_win || !win.isVisible) { continue; }
      /* An NSCarbonWindow reports its whole frame as content (Carbon draws
       * the title bar itself: PvZ 800x622 for an 800x600 content area). */
      NSSize c = [win contentRectForFrameRect:win.frame].size;
      if (c.width == w && c.height >= h && c.height <= h + 40) {
         best = win; if (win.isKeyWindow) { break; }
      }
   }
   return best;
}

static CGDirectDisplayID g_dpy;
static void present(CGLContextObj ctx, CGDirectDisplayID dpy);

/* The app usually shows its window AFTER CGLSetFullScreen (PvZ: NSApp has
 * no windows yet), and a Carbon window posts no key/main notification. Poll
 * from the run loop the app pumps (ReceiveNextEvent runs it) until the host
 * appears. ponytail: 0.25 s x 240 tries; a window-created hook if a target
 * shows its window later than a minute. */
static void watch_for_host(void)
{
   static NSTimer *t;
   if (t) { return; }
   __block int tries = 0;
   t = [NSTimer timerWithTimeInterval:0.25 repeats:YES block:^(NSTimer *tm) {
      if (!g_ctx || g_win.parentWindow || ++tries > 240) { [tm invalidate]; t = nil; return; }
      present(g_ctx, g_dpy);
   }];
   [NSRunLoop.mainRunLoop addTimer:t forMode:NSRunLoopCommonModes];
}

static void present(CGLContextObj ctx, CGDirectDisplayID dpy)
{
   g_dpy = dpy;
   uint32_t id = dpy;
   CGFloat w = shim_CGDisplayPixelsWide(&id), h = shim_CGDisplayPixelsHigh(&id);
   on_main(^{
      [NSApplication sharedApplication];
      NSWindow *host = app_window(w, h);
      if (getenv("ABICONV_CGLFS_TRACE")) {
         fprintf(stderr, "[cglfs] present %gx%g host=%p (%s) key=%p windows=%lu\n", w, h,
                 (void *)host, host ? NSStringFromRect(host.frame).UTF8String : "-",
                 (void *)NSApp.keyWindow, (unsigned long)NSApp.windows.count);
         for (NSWindow *x in NSApp.windows)
            fprintf(stderr, "[cglfs]   win %p vis=%d frame=%s content=%s\n", (void *)x, x.isVisible,
                    NSStringFromRect(x.frame).UTF8String,
                    NSStringFromSize([x contentRectForFrameRect:x.frame].size).UTF8String);
      }
      NSRect r;
      if (host) {
         /* Draw OVER the app's window, not instead of it: the app's window
          * keeps the mouse/keyboard (the overlay ignores the mouse), and as a
          * child the overlay follows it when the user moves it. A host left
          * partly off-screen is centred -- a MOVE, which is safe on a shown
          * Carbon window (carbon_window_resize_guard.c). */
         NSScreen *hs = host.screen ?: NSScreen.mainScreen;
         if (!NSContainsRect(hs.visibleFrame, host.frame)) { [host center]; }
         r = [host contentRectForFrameRect:host.frame];
         r.size.height = h;   /* the content area sits below any Carbon title bar */
      } else {
         NSScreen *scr = NSScreen.screens.firstObject;
         for (NSScreen *s in NSScreen.screens)
            if ([s.deviceDescription[@"NSScreenNumber"] unsignedIntValue] == dpy) scr = s;
         /* AppKit origin is bottom-left: top-left of the display, virtual size. */
         NSRect f = scr.frame;
         r = NSMakeRect(f.origin.x, NSMaxY(f) - h, w, h);
      }
      if (!g_win) {
         g_win = [[NSWindow alloc] initWithContentRect:r styleMask:NSWindowStyleMaskBorderless
                                               backing:NSBackingStoreBuffered defer:NO];
         g_win.releasedWhenClosed = NO;
         g_win.contentView.wantsBestResolutionOpenGLSurface = NO;
      }
      [g_win setFrame:r display:NO];
      if (host) {
         g_win.ignoresMouseEvents = YES;
         g_win.level = host.level;
         if (g_win.parentWindow != host) {
            [g_win.parentWindow removeChildWindow:g_win];
            [host addChildWindow:g_win ordered:NSWindowAbove];
         }
         [g_win orderFront:nil];
         [host makeKeyAndOrderFront:nil];
      } else {
         watch_for_host();
         g_win.ignoresMouseEvents = NO;
         g_win.level = NSMainMenuWindowLevel + 1;   /* over the menu bar, like a capture */
         [g_win orderFront:nil];
      }
      if (g_ctx != ctx) {
         [g_ns clearDrawable];
         g_ns = [[NSOpenGLContext alloc] initWithCGLContextObj:ctx];
         g_ctx = ctx;
      }
      g_ns.view = g_win.contentView;
      [g_ns update];
   });
}

/* CGLError CGLSetFullScreen(CGLContextObj) */
uint32_t shim_CGLSetFullScreen(uint32_t *a)
{
   CGLContextObj ctx = ctx_in(a[0]);
   if (bridge_off() || !ctx) return (uint32_t)CGLSetFullScreen(ctx);
   present(ctx, CGMainDisplayID());
   return kCGLNoError;
}

/* CGLError CGLSetFullScreenOnDisplay(CGLContextObj, GLuint display_mask) */
uint32_t shim_CGLSetFullScreenOnDisplay(uint32_t *a)
{
   CGLContextObj ctx = ctx_in(a[0]);
   if (bridge_off() || !ctx) return (uint32_t)CGLSetFullScreenOnDisplay(ctx, a[1]);
   CGDirectDisplayID dpy = CGOpenGLDisplayMaskToDisplayID(a[1]);
   present(ctx, dpy ? dpy : CGMainDisplayID());
   return kCGLNoError;
}

/* CGLError CGLClearDrawable(CGLContextObj) */
uint32_t shim_CGLClearDrawable(uint32_t *a)
{
   CGLContextObj ctx = ctx_in(a[0]);
   if (bridge_off() || !ctx || ctx != g_ctx) return (uint32_t)CGLClearDrawable(ctx);
   on_main(^{
      [g_ns clearDrawable];
      [g_win.parentWindow removeChildWindow:g_win];
      [g_win orderOut:nil];
      g_ns = nil; g_ctx = NULL;
   });
   return kCGLNoError;
}
