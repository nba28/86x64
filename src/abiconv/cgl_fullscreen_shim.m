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
 * the capture (the app believes the screen IS its mode, e.g. 800x600); this is
 * the drawable half of the same bridge.
 *
 * WHAT IT DOES: CGLSetFullScreen(OnDisplay) opens a normal window and takes it
 * into NATIVE macOS fullscreen (its own Space, like any modern fullscreen app;
 * leaving fullscreen gives an ordinary resizable window). The context's surface
 * is letterboxed in it at the largest aspect-preserving scale and keeps the
 * VIRTUAL backing size (kCGLCPSurfaceBackingSize): the app renders exactly the
 * resolution it chose and the compositor scales it — sharp on Retina.
 *
 * INPUT: a Cocoa window keeps its own mouse/key events, and the app listens for
 * CARBON events on its application target. The window converts each one
 * (CreateEventWithCGEvent) and sends it there, with the location rewritten by
 * cglfs_map_global(): letterbox -> virtual point -> plus the content origin
 * the app's GlobalToLocal subtracts (ci_content_origin; PvZ keeps a hidden
 * 800x600 Carbon window and converts clicks with GlobalToLocal against it). GetGlobalMouse (osutil_shim.c) and pumped
 * mouse events (carbon_event_appdown.c) use the same mapping.
 *
 * WHY NOT DRAW INTO THE APP'S WINDOW: tried; a Carbon window cannot be resized
 * after show (NSCGSPanic), window managers resize it anyway (yabai float), and
 * the app's window is not what a fullscreen game expects to be in.
 *
 * KILL SWITCH: M64_NO_CGL_FULLSCREEN_BRIDGE=1 forwards to native CGL (the
 * pre-fix 10012). ABICONV_CGLFS_TRACE=1 logs placement and mapping.
 * ABI: MTSHIM (rdi -> &i386 args[0]); symbols in custom.syms.
 */
#import <AppKit/AppKit.h>
#import <OpenGL/OpenGL.h>
#import <Carbon/Carbon.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <objc/runtime.h>

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

static int trace(void)   /* 1: placement + forwarded events, 2: + every mapping */
{
   static int v = -1;
   if (v < 0) { const char *e = getenv("ABICONV_CGLFS_TRACE"); v = e ? atoi(e) ? atoi(e) : 1 : 0; }
   return v;
}

/* A borderless window refuses key status by default; a captured display took
 * the keyboard, so this one must too. */
@interface CGLFSWindow : NSWindow
@end

int cglfs_map_global(double *x, double *y);
extern int ci_content_origin(int16_t *ox, int16_t *oy);   /* classic_input_coords.c */

/* Hand a Cocoa input event to the app as the Carbon event it listens for. */
static int forward_to_app(NSEvent *ev)
{
   CGEventRef cg = ev.CGEvent;
   EventRef ce = NULL;
   if (!cg || CreateEventWithCGEvent(NULL, cg, kEventAttributeUserEvent, &ce) != noErr || !ce) {
      return 0;
   }
   if (GetEventClass(ce) == kEventClassMouse) {
      HIPoint p;
      if (GetEventParameter(ce, kEventParamMouseLocation, typeHIPoint, NULL, sizeof p,
                            NULL, &p) == noErr) {
         double x = p.x, y = p.y;
         cglfs_map_global(&x, &y);
         p.x = x; p.y = y;
         SetEventParameter(ce, kEventParamMouseLocation, typeHIPoint, sizeof p, &p);
      }
   }
   const OSStatus st = SendEventToEventTarget(ce, GetApplicationEventTarget());
   if (trace()) {
      fprintf(stderr, "[cglfs] forward NSEvent type %ld -> carbon %.4s/%u -> %d\n",
              (long)ev.type, (const char *)&(UInt32){ CFSwapInt32HostToBig(GetEventClass(ce)) },
              (unsigned)GetEventKind(ce), (int)st);
   }
   ReleaseEvent(ce);
   return 1;
}

@implementation CGLFSWindow
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
- (void)sendEvent:(NSEvent *)ev
{
   switch (ev.type) {
   case NSEventTypeKeyDown: case NSEventTypeKeyUp: case NSEventTypeFlagsChanged:
      /* Keep the system's fullscreen toggle (ctrl-cmd-F) working. */
      if ((ev.modifierFlags & (NSEventModifierFlagCommand | NSEventModifierFlagControl)) ==
             (NSEventModifierFlagCommand | NSEventModifierFlagControl)) { break; }
      forward_to_app(ev);
      return;
   case NSEventTypeLeftMouseDown: case NSEventTypeLeftMouseUp: case NSEventTypeLeftMouseDragged:
   case NSEventTypeRightMouseDown: case NSEventTypeRightMouseUp: case NSEventTypeRightMouseDragged:
   case NSEventTypeMouseMoved: case NSEventTypeScrollWheel:
      /* Clicks on the title bar (windowed) stay window management. */
      if (NSPointInRect(ev.locationInWindow, self.contentLayoutRect)) { forward_to_app(ev); return; }
      break;
   default: break;
   }
   [super sendEvent:ev];
}
@end

/* ponytail: one presented context at a time (the idiom never has two); make it
 * a table keyed by ctx if a target ever flips between fullscreen contexts. */
static CGLContextObj    g_ctx;
static NSOpenGLContext *g_ns;
static CGLFSWindow     *g_win;
static NSView          *g_view;
static CGFloat          g_vw, g_vh;          /* virtual mode size */
/* Mapping snapshot, readable from any thread: the view in Carbon global
 * coordinates (top-left origin, points) and its scale. */
static volatile double  g_gx, g_gy, g_scale;
static volatile int     g_active;

static void on_main(void (^b)(void))
{
   if ([NSThread isMainThread]) b(); else dispatch_sync(dispatch_get_main_queue(), b);
}

/* Main thread only: refresh the mapping snapshot from the live geometry. */
static void relayout(void)
{
   if (!g_win) { return; }
   const NSRect b = g_win.contentView.bounds;
   const CGFloat k = MIN(b.size.width / g_vw, b.size.height / g_vh);
   const NSRect vr = NSMakeRect(floor((b.size.width - g_vw * k) / 2),
                                floor((b.size.height - g_vh * k) / 2), g_vw * k, g_vh * k);
   g_view.frame = vr;
   const NSRect sr = [g_win convertRectToScreen:[g_win.contentView convertRect:vr toView:nil]];
   const CGFloat top = NSMaxY(NSScreen.screens.firstObject.frame);   /* Carbon: y down */
   g_gx = sr.origin.x;
   g_gy = top - NSMaxY(sr);
   g_scale = k;
   [g_ns update];
   if (trace()) {
      fprintf(stderr, "[cglfs] layout scale %.3f view@%.0f,%.0f\n", k, g_gx, g_gy);
   }
}

static void present(CGLContextObj ctx, CGDirectDisplayID dpy)
{
   uint32_t id = dpy;
   const CGFloat w = shim_CGDisplayPixelsWide(&id), h = shim_CGDisplayPixelsHigh(&id);
   on_main(^{
      [NSApplication sharedApplication];
      g_vw = w; g_vh = h;
      if (!g_win) {
         NSScreen *scr = NSScreen.screens.firstObject;
         for (NSScreen *s in NSScreen.screens)
            if ([s.deviceDescription[@"NSScreenNumber"] unsignedIntValue] == dpy) scr = s;
         const NSRect vf = scr.visibleFrame;
         const CGFloat k = MIN(1.0, MIN(vf.size.width / w, (vf.size.height - 28) / h));
         g_win = [[CGLFSWindow alloc]
            initWithContentRect:NSMakeRect(0, 0, w * k, h * k)
                      styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                        backing:NSBackingStoreBuffered defer:NO];
         g_win.releasedWhenClosed = NO;
         g_win.backgroundColor = NSColor.blackColor;
         g_win.title = NSProcessInfo.processInfo.processName;
         g_win.acceptsMouseMovedEvents = YES;
         g_win.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;
         g_win.contentAspectRatio = NSMakeSize(w, h);
         [g_win center];
         g_view = [[NSView alloc] initWithFrame:NSZeroRect];
         g_view.wantsBestResolutionOpenGLSurface = NO;
         [g_win.contentView addSubview:g_view];
         [NSNotificationCenter.defaultCenter addObserverForName:NSWindowDidResizeNotification
            object:g_win queue:nil usingBlock:^(NSNotification *n) { (void)n; relayout(); }];
         [NSNotificationCenter.defaultCenter addObserverForName:NSWindowDidMoveNotification
            object:g_win queue:nil usingBlock:^(NSNotification *n) { (void)n; relayout(); }];
      }
      [g_win makeKeyAndOrderFront:nil];
      [NSApp activateIgnoringOtherApps:YES];
      if (g_ctx != ctx) {
         [g_ns clearDrawable];
         g_ns = [[NSOpenGLContext alloc] initWithCGLContextObj:ctx];
         g_ctx = ctx;
      }
      /* Render at the app's resolution; the compositor scales the surface. */
      const GLint bs[2] = { (GLint)w, (GLint)h };
      CGLSetParameter(ctx, kCGLCPSurfaceBackingSize, bs);
      CGLEnable(ctx, kCGLCESurfaceBackingSize);
      relayout();
      g_ns.view = g_view;
      [g_ns update];
      g_active = 1;
      /* The app asked for fullscreen: give it its own Space. */
      if (!(g_win.styleMask & NSWindowStyleMaskFullScreen)) { [g_win toggleFullScreen:nil]; }
   });
}

/* Global screen point (Carbon: main-screen top-left, y down) -> the app's
 * space. Returns 0 (point untouched) when no fullscreen surface is up. Safe
 * from any thread: reads the snapshot relayout() keeps. */
int cglfs_map_global(double *x, double *y)
{
   if (!g_active || g_scale <= 0) { return 0; }
   double vx = (*x - g_gx) / g_scale, vy = (*y - g_gy) / g_scale;
   vx = vx < 0 ? 0 : vx > g_vw - 1 ? g_vw - 1 : vx;
   vy = vy < 0 ? 0 : vy > g_vh - 1 ? g_vh - 1 : vy;
   /* Plus exactly the origin the app's GlobalToLocal will subtract (qd_shim.c),
    * so GlobalToLocal(mapped) is the virtual point whatever window that is. */
   int16_t ox = 0, oy = 0;
   ci_content_origin(&ox, &oy);
   if (trace() > 1) {
      fprintf(stderr, "[cglfs] map %.0f,%.0f -> virtual %.0f,%.0f + origin %d,%d\n",
              *x, *y, vx, vy, ox, oy);
   }
   *x = ox + vx; *y = oy + vy;
   return 1;
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
   g_active = 0;
   on_main(^{
      [g_ns clearDrawable];
      [g_win orderOut:nil];
      g_ns = nil; g_ctx = NULL;
   });
   return kCGLNoError;
}
