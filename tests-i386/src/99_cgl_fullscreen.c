/* cgl-fullscreen: the classic CGL fullscreen context (PvZ). Modern CGL has no
 * fullscreen drawables, so a native CGLSetFullScreen answers 10012
 * (kCGLBadDrawable) and the game has nowhere to render. The bridge
 * (cgl_fullscreen_shim.m) presents the context in a window of the display's
 * virtual size. No capture / mode switch here: the fixture must never touch
 * the user's display. ON exits 42; OFF (M64_NO_CGL_FULLSCREEN_BRIDGE=1) 7.
 * CGL is not in the i386 sysroot: declared by hand, bound via dynamic_lookup. */
typedef int CGLError;
typedef void *CGLPixelFormatObj, *CGLContextObj;
CGLError CGLChoosePixelFormat(const int *, CGLPixelFormatObj *, int *);
CGLError CGLCreateContext(CGLPixelFormatObj, CGLContextObj, CGLContextObj *);
CGLError CGLSetCurrentContext(CGLContextObj);
CGLError CGLSetFullScreen(CGLContextObj);
CGLError CGLFlushDrawable(CGLContextObj);
CGLError CGLClearDrawable(CGLContextObj);
void exit(int) __attribute__((noreturn));

int main(void) {
    const int attrs[] = { 5 /* kCGLPFADoubleBuffer */, 8 /* kCGLPFAColorSize */, 24, 0 };
    CGLPixelFormatObj pix = 0; CGLContextObj ctx = 0; int n = 0;
    if (CGLChoosePixelFormat(attrs, &pix, &n) || !pix) exit(2);
    if (CGLCreateContext(pix, 0, &ctx) || !ctx) exit(3);
    CGLError e = CGLSetFullScreen(ctx);
    if (e) exit(7);
    CGLSetCurrentContext(ctx);
    if (CGLFlushDrawable(ctx)) exit(4);
    if (CGLClearDrawable(ctx)) exit(5);
    exit(42);
}
