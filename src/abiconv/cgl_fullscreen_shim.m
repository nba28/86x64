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
 * screen is) at the display's origin, so virtual-display and global screen
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

static void present(CGLContextObj ctx, CGDirectDisplayID dpy)
{
   uint32_t id = dpy;
   CGFloat w = shim_CGDisplayPixelsWide(&id), h = shim_CGDisplayPixelsHigh(&id);
   on_main(^{
      [NSApplication sharedApplication];
      NSScreen *scr = NSScreen.screens.firstObject;
      for (NSScreen *s in NSScreen.screens)
         if ([s.deviceDescription[@"NSScreenNumber"] unsignedIntValue] == dpy) scr = s;
      /* AppKit origin is bottom-left: top-left of the display, virtual size. */
      NSRect f = scr.frame;
      NSRect r = NSMakeRect(f.origin.x, NSMaxY(f) - h, w, h);
      if (!g_win) {
         g_win = [[NSWindow alloc] initWithContentRect:r styleMask:NSWindowStyleMaskBorderless
                                               backing:NSBackingStoreBuffered defer:NO];
         g_win.releasedWhenClosed = NO;
         g_win.level = NSMainMenuWindowLevel + 1;   /* over the menu bar, like a capture */
         g_win.contentView.wantsBestResolutionOpenGLSurface = NO;
      }
      [g_win setFrame:r display:NO];
      [g_win makeKeyAndOrderFront:nil];
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
      [g_win orderOut:nil];
      g_ns = nil; g_ctx = NULL;
   });
   return kCGLNoError;
}
