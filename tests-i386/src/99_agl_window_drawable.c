/* 99_agl_window_drawable — guard for the Carbon+AGL DRAWABLE contract
 * (src/abiconv/qd_gworld.c window-backed ports + src/abiconv/agl_drawable_shim.c).
 *
 * Every 32-bit-era Carbon+OpenGL app attaches its GL context with the same
 * three lines:
 *
 *      CreateNewWindow(...&win);
 *      ctx = aglCreateContext(pix, NULL);
 *      aglSetDrawable(ctx, GetWindowPort(win));
 *
 * 64-bit macOS deleted GetWindowPort outright (dlsym returns NULL even with
 * Carbon loaded), so libabiconv used to shim it to 0 and the app ended up
 * calling aglSetDrawable(ctx, NULL) -> 0.  No drawable means no GL context at
 * all, and the app falls into an error path that 2006 hardware never ran.
 * Halo CE has 21 aglSetDrawable call sites and ZERO aglSetWindowRef.
 *
 * The fix gives the window a REAL port that records its WindowRef, and
 * aglSetDrawable recognises such a port structurally and routes the attach to
 * the surviving native aglSetWindowRef.  What is asserted here is therefore not
 * a status code but a LIVE GL CONTEXT: aglSetCurrentContext succeeds and
 * glGetIntegerv(GL_MAX_TEXTURE_SIZE) comes back with a real driver limit, which
 * only a context bound to a real surface can produce.
 *
 * The four properties this pins, in order:
 *   port          GetWindowPort returns non-NULL and is STABLE (two calls give
 *                 the same CGrafPtr — apps compare and cache it).
 *   roundtrip     GetWindowFromPort(port) == the WindowRef it came from.
 *   set           aglSetDrawable(ctx, port) succeeds ...
 *   renderer      ... and yields a context that can actually be made current
 *                 and answer a driver query (GL_MAX_TEXTURE_SIZE > 0).  With no
 *                 drawable the same call leaves the caller's variable untouched.
 *   getdrawable   aglGetDrawable(ctx) reports the port back.  Native AGL is
 *                 BLIND to an aglSetWindowRef attach (measured: it returns
 *                 NULL), and Carbon+AGL apps universally do
 *                     saved = aglGetDrawable(ctx); aglSetDrawable(ctx, NULL);
 *                     ...QuickDraw UI...           aglSetDrawable(ctx, saved);
 *                 so without this the first alert would detach GL forever.
 *   restore       that whole save/detach/restore cycle leaves GL live again.
 *
 * A/B: re-run with M64_NO_AGL_WINDOWREF=1 and the substrate is disarmed —
 * GetWindowPort returns NULL again, aglSetDrawable(ctx, NULL) fails, and there
 * is no renderer.  See agl_window_drawable_test.sh; without that half a passing
 * fixture would not prove the substrate is what fixed it.
 *
 * Neither Carbon/HIToolbox nor AGL is in the i386 sysroot, so the whole surface
 * is an undefined dynamic_lookup import resolved at translate time by
 * static-interpose to libabiconv's ___<sym> shims.  libabiconv dlopens
 * AGL.framework itself (it has no SDK stub to link against), which is what lets
 * its ___agl* bridges bind in a process that carries no AGL load command.

 * NOTE on the liveness probe: glGetString is deliberately NOT used.  It has a
 * pre-existing defect in its generated bridge unrelated to this fix — the enum
 * arrives corrupted, GL raises GL_INVALID_ENUM (1280) and returns NULL, while
 * glGetIntegerv through the same bridge machinery returns the correct value and
 * a native glGetString from inside libabiconv, at the same instant on the same
 * thread, returns the real renderer string.  See the AGL drawable journal.
 *
 * NOTE: no crt0 — main must end in exit(), never return.
 */

extern int printf(const char *, ...);
extern void exit(int status);

typedef struct { short top, left, bottom, right; } Rect;
typedef void *WindowRef, *CGrafPtr, *AGLPixelFormat, *AGLContext;
typedef unsigned char GLboolean;

extern int      CreateNewWindow(unsigned int cls, unsigned int attrs,
                                const Rect *bounds, WindowRef *outWindow);
extern CGrafPtr GetWindowPort(WindowRef w);
extern WindowRef GetWindowFromPort(CGrafPtr p);
extern void     DisposeWindow(WindowRef w);

extern AGLPixelFormat aglChoosePixelFormat(const void *gdevs, int ndev, const int *attrs);
extern AGLContext     aglCreateContext(AGLPixelFormat pix, AGLContext share);
extern GLboolean      aglSetDrawable(AGLContext ctx, CGrafPtr draw);
extern CGrafPtr       aglGetDrawable(AGLContext ctx);
extern GLboolean      aglSetCurrentContext(AGLContext ctx);
extern void           aglDestroyPixelFormat(AGLPixelFormat pix);
extern void glGetIntegerv(unsigned int pname, int *params);

#define kDocumentWindowClass          6u
#define kWindowCompositingAttribute   (1u << 19)
#define kWindowStandardDocumentAttrs  0x0000000fu

#define AGL_NONE          0
#define AGL_RGBA          4
#define AGL_DOUBLEBUFFER  5
#define AGL_DEPTH_SIZE    12
#define AGL_ACCELERATED   73

#define GL_MAX_TEXTURE_SIZE 0x0D33

/* GL is live iff the current context can answer a driver query. A context with
 * no drawable never reaches the driver, so the sentinel survives untouched. */
static int live_renderer(void)
{
    int max_tex = -1;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex);
    return max_tex > 0;
}

int main(void)
{
    Rect r = { 100, 100, 400, 500 };
    WindowRef win = 0;

    int st = CreateNewWindow(kDocumentWindowClass,
                             kWindowStandardDocumentAttrs | kWindowCompositingAttribute,
                             &r, &win);
    printf("window=%d\n", (st == 0 && win) ? 1 : 0);
    if (st != 0 || !win) { printf("port=0\nstable=0\nroundtrip=0\nset=0\nrenderer=0\n"
                                  "getdrawable=0\nrestore=0\n"); exit(0); }

    /* --- the classic idiom, verbatim --- */
    CGrafPtr port  = GetWindowPort(win);
    CGrafPtr port2 = GetWindowPort(win);
    printf("port=%d\n",   port != 0);
    printf("stable=%d\n", (port != 0 && port == port2));
    printf("roundtrip=%d\n", (port != 0 && GetWindowFromPort(port) == win));

    int attrs[] = { AGL_RGBA, AGL_DOUBLEBUFFER, AGL_DEPTH_SIZE, 24, AGL_ACCELERATED, AGL_NONE };
    AGLPixelFormat pix = aglChoosePixelFormat(0, 0, attrs);
    AGLContext ctx = pix ? aglCreateContext(pix, 0) : 0;
    if (pix) aglDestroyPixelFormat(pix);
    if (!ctx) { printf("set=0\nrenderer=0\ngetdrawable=0\nrestore=0\n");
                DisposeWindow(win); exit(0); }

    GLboolean set = aglSetDrawable(ctx, port);
    printf("set=%d\n", set ? 1 : 0);
    printf("renderer=%d\n", (set && aglSetCurrentContext(ctx)) ? live_renderer() : 0);

    /* --- the save / detach / restore cycle around QuickDraw UI --- */
    CGrafPtr saved = aglGetDrawable(ctx);
    printf("getdrawable=%d\n", (saved != 0 && saved == port));
    aglSetDrawable(ctx, 0);                       /* detach */
    GLboolean back = aglSetDrawable(ctx, saved);  /* restore */
    printf("restore=%d\n", (back && aglSetCurrentContext(ctx)) ? live_renderer() : 0);

    DisposeWindow(win);
    exit(0);
}
