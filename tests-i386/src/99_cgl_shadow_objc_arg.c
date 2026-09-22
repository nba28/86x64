/* 99_cgl_shadow_objc_arg.c — a CGL context the i386 side holds as an
 * i386-layout SHADOW must reach a native ObjC method as the REAL context.
 *
 * cgl_macro_shim.c hands translated code an i386-layout _CGLContextObject
 * (4-byte rend + 4-byte dispatch slots) so <OpenGL/CGLMacro.h> clients can
 * jump through ctx->disp. When that same value is passed to a NATIVE API typed
 * `^{_CGLContextObject=}`, the ObjC bridge unwraps it like any opaque CF-style
 * pointer — and before the fix nothing mapped the shadow back, so native code
 * read the 64-bit layout out of the shadow and called through two fused 4-byte
 * slots. MEASURED, Quinn: +[CIContext contextWithCGLContext:pixelFormat:options:]
 * faulted at rip = (libabiconv addr << 32 | libabiconv addr) during launch.
 *
 * Exit 42 = CoreImage accepted the context and returned a CIContext.
 */
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

typedef void *CGLPixelFormatObj;
typedef void *CGLContextObj;
extern int   CGLChoosePixelFormat(const int *attrs, CGLPixelFormatObj *pix, int *npix);
extern int   CGLCreateContext(CGLPixelFormatObj pix, CGLContextObj share, CGLContextObj *ctx);
extern int   CGLSetCurrentContext(CGLContextObj ctx);
extern void *objc_getClass(const char *name);
extern void *sel_registerName(const char *name);
extern void *objc_msgSend(void *self, void *sel, ...);

int main(void)
{
   setvbuf(stdout, NULL, _IONBF, 0);
   const int attrs[] = { 73 /* kCGLPFAAccelerated */, 0 };
   CGLPixelFormatObj pf = 0; int n = 0;
   int e1 = CGLChoosePixelFormat(attrs, &pf, &n);
   CGLContextObj ctx = 0;
   int e2 = pf ? CGLCreateContext(pf, 0, &ctx) : -1;
   printf("pixfmt err=%d ctx err=%d have_ctx=%d\n", e1, e2, ctx != 0);
   if (!ctx) { exit(1); }
   CGLSetCurrentContext(ctx);

   dlopen("/System/Library/Frameworks/QuartzCore.framework/QuartzCore", RTLD_NOW);
   void *cls = objc_getClass("CIContext");
   printf("CIContext class=%d\n", cls != 0);
   if (!cls) { exit(2); }
   void *ci = objc_msgSend(cls, sel_registerName("contextWithCGLContext:pixelFormat:options:"),
                           ctx, pf, (void *)0);
   printf("CIContext created=%d\n", ci != 0);
   exit(ci ? 42 : 3);
}
