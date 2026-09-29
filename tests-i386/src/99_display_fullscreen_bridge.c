/* 99_display_fullscreen_bridge — guard for the classic FULLSCREEN DISPLAY-MODE
 * idiom bridge (src/abiconv/cg_display_fullscreen_shim.c + cg_display_tramp.asm).
 *
 * Every 32-bit-era fullscreen Mac game does the same five lines:
 *
 *      CGCaptureAllDisplays();
 *      mode = CGDisplayBestModeForParameters(dpy, 32, w, h, &exact);
 *      CGDisplaySwitchToMode(dpy, mode);
 *      ... render ...
 *      CGDisplaySwitchToMode(dpy, saved);  CGReleaseAllDisplays();
 *
 * PERFORMING that on modern macOS kills the process: CoreGraphics posts a
 * display-reconfiguration datagram, AppKit's observer setFrame:s every window
 * in the process, and -[NSCGSWindow _createContext] cannot be created for an
 * NSCarbonWindow -> SIGILL in NSCGSPanic.  (Measured in
 * src/86x64/probes/displayswitch.m, including the negative results: hiding the
 * window, using the modern CGDisplaySetDisplayMode, and resizing the window all
 * panic too; only a MOVE-only SetWindowBounds survives.)
 *
 * So the bridge never reconfigures a display.  What this fixture pins:
 *
 *   best        CGDisplayBestModeForParameters answers with EXACTLY the
 *               requested geometry and exactMatch=1.  This is load-bearing, not
 *               cosmetic: natively, asking a modern Mac for 640x480 returns a
 *               real mode of a completely different size with exactMatch=0
 *               (measured: 1920x1200), and a game that trusts the returned
 *               dictionary would size its viewport to a resolution nothing in
 *               the process renders at.
 *   switch      CGDisplaySwitchToMode succeeds ...
 *   current     ... and CGDisplayCurrentMode / CGDisplayBounds /
 *   bounds      CGDisplayPixelsWide/High / CGDisplayBitsPerPixel then all agree
 *   pixels      with what the app asked for.  Coherence is what keeps the
 *               app's viewport and mouse mapping right.
 *   captured    the capture family is tracked (CGDisplayIsCaptured says yes)
 *               without a real capture, which would shield the screen over the
 *               app's own window.
 *   kept        the window that owns the GL drawable is left EXACTLY where and
 *               as big as the app made it: its context is presented on the
 *               fullscreen surface instead (cgl_fullscreen_shim.m; guard
 *               agl-fullscreen-present). A RESIZE is the fatal operation.
 *   gl2         and the AGL drawable is still LIVE afterwards.  A no-crash run
 *               with a dead drawable is not success.
 *   release     CGReleaseAllDisplays drops the capture AND the virtual mode
 *               (releasing a captured display is documented to restore it), so
 *               CGDisplayCurrentMode reports the physical mode again.
 *
 * argv[1] == "query" runs ONLY the two safe queries and stops.  That is how the
 * A/B guard drives the OFF (kill-switch) side: with the bridge disarmed the
 * switch would really reconfigure the tester's display, which is both destructive and
 * the very crash under test.  See display_fullscreen_bridge_test.sh.
 *
 * Neither Carbon/HIToolbox nor AGL is in the i386 sysroot and CoreGraphics is
 * reached through libabiconv's ___CG* shims, so the whole surface is an
 * undefined dynamic_lookup import resolved at translate time by static-interpose.
 *
 * NOTE: no crt0 — main must end in exit(), never return.
 */

extern int printf(const char *, ...);
extern void exit(int status);
extern int strcmp(const char *, const char *);

typedef struct { short top, left, bottom, right; } Rect;
typedef void *WindowRef, *CGrafPtr, *AGLPixelFormat, *AGLContext, *CFDictionaryRef;
typedef void *CFStringRef, *CFNumberRef, *CFTypeRef;
typedef unsigned char GLboolean;
typedef struct { float x, y, w, h; } CGRect32;   /* i386 CGFloat == float */

extern unsigned int   CGMainDisplayID(void);
extern CFDictionaryRef CGDisplayBestModeForParameters(unsigned int d, unsigned int bpp,
                                                      unsigned int w, unsigned int h,
                                                      int *exactMatch);
extern CFDictionaryRef CGDisplayCurrentMode(unsigned int d);
extern int            CGDisplaySwitchToMode(unsigned int d, CFDictionaryRef m);
extern CGRect32       CGDisplayBounds(unsigned int d);
extern unsigned int   CGDisplayPixelsWide(unsigned int d);
extern unsigned int   CGDisplayPixelsHigh(unsigned int d);
extern unsigned int   CGDisplayBitsPerPixel(unsigned int d);
extern int            CGCaptureAllDisplays(void);
extern int            CGReleaseAllDisplays(void);
extern int            CGDisplayIsCaptured(unsigned int d);

extern CFTypeRef   CFDictionaryGetValue(CFDictionaryRef d, const void *key);
extern CFStringRef CFStringCreateWithCString(void *alloc, const char *s, unsigned int enc);
extern int         CFNumberGetValue(CFNumberRef n, int type, void *out);
extern void        CFRelease(CFTypeRef t);

extern int      CreateNewWindow(unsigned int cls, unsigned int attrs,
                                const Rect *bounds, WindowRef *outWindow);
extern int      GetWindowBounds(WindowRef w, unsigned int region, Rect *b);
extern CGrafPtr GetWindowPort(WindowRef w);
extern void     DisposeWindow(WindowRef w);
extern AGLPixelFormat aglChoosePixelFormat(const void *gdevs, int ndev, const int *attrs);
extern AGLContext     aglCreateContext(AGLPixelFormat pix, AGLContext share);
extern GLboolean      aglSetDrawable(AGLContext ctx, CGrafPtr draw);
extern GLboolean      aglSetCurrentContext(AGLContext ctx);
extern void           aglDestroyPixelFormat(AGLPixelFormat pix);
extern void glGetIntegerv(unsigned int pname, int *params);

#define kDocumentWindowClass          6u
#define kWindowCompositingAttribute   (1u << 19)
#define kWindowStandardDocumentAttrs  0x0000000fu
#define kWindowContentRgn             33u

#define AGL_NONE 0
#define AGL_RGBA 4
#define AGL_DOUBLEBUFFER 5
#define AGL_DEPTH_SIZE 12
#define AGL_ACCELERATED 73
#define GL_MAX_TEXTURE_SIZE 0x0D33
#define kCFStringEncodingUTF8 0x08000100u
#define kCFNumberIntType 9

/* The resolution asked for. No modern Mac offers it, which is exactly the point:
 * natively CGDisplayBestModeForParameters answers with a real mode of a
 * different size and exactMatch=0, so the OFF arm of the A/B is unambiguous. */
#define REQ_W 640
#define REQ_H 480

static int mode_int(CFDictionaryRef m, const char *key, int dflt)
{
    if (!m) return dflt;
    CFStringRef k = CFStringCreateWithCString(0, key, kCFStringEncodingUTF8);
    if (!k) return dflt;
    CFNumberRef n = (CFNumberRef)CFDictionaryGetValue(m, k);
    int v = dflt;
    if (n) CFNumberGetValue(n, kCFNumberIntType, &v);
    CFRelease(k);
    return v;
}

static int mode_is(CFDictionaryRef m, int w, int h)
{
    return m && mode_int(m, "Width", -1) == w && mode_int(m, "Height", -1) == h;
}

static int live_gl(void)
{
    int max_tex = -1;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex);
    return max_tex > 0;
}

int main(int argc, char **argv)
{
    const unsigned int dpy = CGMainDisplayID();
    const int query_only = (argc > 1 && strcmp(argv[1], "query") == 0);

    /* The PHYSICAL mode, read before anything is virtualised. The last check of
     * the run compares against it again: if the bridge had really reconfigured
     * the display, the two would differ. */
    CFDictionaryRef phys = CGDisplayCurrentMode(dpy);
    const int phys_w = mode_int(phys, "Width", -1), phys_h = mode_int(phys, "Height", -1);

    int exact = -1;
    CFDictionaryRef best = CGDisplayBestModeForParameters(dpy, 32, REQ_W, REQ_H, &exact);
    printf("best_exact=%d\n", mode_is(best, REQ_W, REQ_H));
    printf("exact=%d\n", exact);

    if (query_only) exit(0);

    /* --- the app's render target: a Carbon window with a live AGL drawable --- */
    Rect r = { 100, 100, 100 + REQ_H, 100 + REQ_W };
    WindowRef win = 0;
    int st = CreateNewWindow(kDocumentWindowClass,
                             kWindowStandardDocumentAttrs | kWindowCompositingAttribute,
                             &r, &win);
    AGLContext ctx = 0;
    if (st == 0 && win) {
        int attrs[] = { AGL_RGBA, AGL_DOUBLEBUFFER, AGL_DEPTH_SIZE, 24, AGL_ACCELERATED, AGL_NONE };
        AGLPixelFormat pix = aglChoosePixelFormat(0, 0, attrs);
        ctx = pix ? aglCreateContext(pix, 0) : 0;
        if (pix) aglDestroyPixelFormat(pix);
        if (ctx) { aglSetDrawable(ctx, GetWindowPort(win)); aglSetCurrentContext(ctx); }
    }
    printf("gl1=%d\n", ctx ? live_gl() : 0);

    /* --- the classic idiom, verbatim --- */
    printf("capture=%d\n", CGCaptureAllDisplays());
    printf("captured=%d\n", CGDisplayIsCaptured(dpy) ? 1 : 0);
    printf("switch=%d\n", CGDisplaySwitchToMode(dpy, best));

    printf("current_ok=%d\n", mode_is(CGDisplayCurrentMode(dpy), REQ_W, REQ_H));
    CGRect32 b = CGDisplayBounds(dpy);
    printf("bounds_ok=%d\n", ((int)b.w == REQ_W && (int)b.h == REQ_H));
    printf("pixels_ok=%d\n", (CGDisplayPixelsWide(dpy) == REQ_W &&
                              CGDisplayPixelsHigh(dpy) == REQ_H));
    printf("bpp_ok=%d\n", CGDisplayBitsPerPixel(dpy) == 32);

    /* The window must be untouched: neither moved nor resized. */
    Rect after = { -1, -1, -1, -1 };
    if (win) GetWindowBounds(win, kWindowContentRgn, &after);
    printf("kept=%d\n", (win && after.left == r.left && after.top == r.top) ? 1 : 0);
    printf("size_kept=%d\n",
           (win && after.right - after.left == REQ_W && after.bottom - after.top == REQ_H) ? 1 : 0);
    printf("gl2=%d\n", ctx ? live_gl() : 0);

    printf("release=%d\n", CGReleaseAllDisplays());
    printf("captured2=%d\n", CGDisplayIsCaptured(dpy) ? 1 : 0);
    /* The physical display must be exactly where it started: the bridge never
     * reconfigured it, so this is the strongest single assertion in the file. */
    printf("restored=%d\n", mode_is(CGDisplayCurrentMode(dpy), phys_w, phys_h));

    if (win) DisposeWindow(win);
    exit(0);
}
