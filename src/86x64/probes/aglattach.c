// Decisive experiment for Halo's AGL wall.
//
// 32-bit-era Carbon+OpenGL code universally does:
//     aglSetDrawable(ctx, GetWindowPort(win));
// On 64-bit macOS GetWindowPort is GONE (verified: dlsym returns NULL even with
// Carbon loaded), so our carbon_ui_shim.c shims it to return 0 and Halo ends up
// calling aglSetDrawable(ctx, NULL) -> no drawable -> "unrecoverable error".
//
// But aglSetWindowRef(ctx, WindowRef) IS present on 64-bit (verified). If that
// works against a modern COMPOSITING window, then the universal fix is:
// recognise the window in aglSetDrawable and route to aglSetWindowRef.
//
// Everything is dlsym'd: the modern SDK no longer declares most of this.
#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>

typedef struct { short top, left, bottom, right; } MacRect;
typedef void *WindowRef, *AGLPixelFormat, *AGLContext;
typedef unsigned char GLboolean;
typedef int GLint;
typedef unsigned int GLenum;

#define kDocumentWindowClass          6u
#define kWindowCompositingAttribute   (1u << 19)
#define kWindowStandardDocumentAttrs  0x0000000fu

#define AGL_NONE          0
#define AGL_RGBA          4
#define AGL_DOUBLEBUFFER  5
#define AGL_DEPTH_SIZE    12
#define AGL_ACCELERATED   73

int main(void) {
    void *carbon = dlopen("/System/Library/Frameworks/Carbon.framework/Carbon", RTLD_LAZY);
    void *agl    = dlopen("/System/Library/Frameworks/AGL.framework/AGL", RTLD_LAZY);
    if (!carbon || !agl) { printf("dlopen failed carbon=%p agl=%p\n", carbon, agl); return 1; }

    int32_t (*CreateNewWindow)(uint32_t, uint32_t, const MacRect *, WindowRef *) =
        dlsym(RTLD_DEFAULT, "CreateNewWindow");
    AGLPixelFormat (*aglChoosePixelFormat)(const void *, GLint, const GLint *) =
        dlsym(RTLD_DEFAULT, "aglChoosePixelFormat");
    AGLContext (*aglCreateContext)(AGLPixelFormat, AGLContext) =
        dlsym(RTLD_DEFAULT, "aglCreateContext");
    GLboolean (*aglSetWindowRef)(AGLContext, WindowRef) =
        dlsym(RTLD_DEFAULT, "aglSetWindowRef");
    GLboolean (*aglSetDrawable)(AGLContext, void *) =
        dlsym(RTLD_DEFAULT, "aglSetDrawable");
    GLboolean (*aglSetCurrentContext)(AGLContext) =
        dlsym(RTLD_DEFAULT, "aglSetCurrentContext");
    GLenum (*aglGetError)(void) = dlsym(RTLD_DEFAULT, "aglGetError");
    const unsigned char *(*aglErrorString)(GLenum) = dlsym(RTLD_DEFAULT, "aglErrorString");
    void *(*GetWindowPort)(WindowRef) = dlsym(RTLD_DEFAULT, "GetWindowPort");

    printf("CreateNewWindow=%p aglChoosePixelFormat=%p aglCreateContext=%p\n",
           (void*)CreateNewWindow, (void*)aglChoosePixelFormat, (void*)aglCreateContext);
    printf("aglSetWindowRef=%p aglSetDrawable=%p GetWindowPort=%p  <-- GetWindowPort NULL is the whole problem\n\n",
           (void*)aglSetWindowRef, (void*)aglSetDrawable, (void*)GetWindowPort);
    if (!CreateNewWindow || !aglChoosePixelFormat || !aglCreateContext || !aglSetWindowRef) return 2;

    MacRect r = { 100, 100, 400, 500 };
    WindowRef win = NULL;
    int32_t st = CreateNewWindow(kDocumentWindowClass,
                                 kWindowStandardDocumentAttrs | kWindowCompositingAttribute,
                                 &r, &win);
    printf("CreateNewWindow(compositing) -> st=%d win=%p\n", (int)st, win);
    if (st != 0 || !win) return 3;

    GLint attrs[] = { AGL_RGBA, AGL_DOUBLEBUFFER, AGL_DEPTH_SIZE, 24, AGL_ACCELERATED, AGL_NONE };
    AGLPixelFormat pix = aglChoosePixelFormat(NULL, 0, attrs);
    printf("aglChoosePixelFormat -> %p\n", pix);
    if (!pix) { printf("  aglGetError=%u\n", aglGetError ? aglGetError() : 0); return 4; }

    AGLContext ctx = aglCreateContext(pix, NULL);
    printf("aglCreateContext -> %p\n", ctx);
    if (!ctx) { printf("  aglGetError=%u\n", aglGetError ? aglGetError() : 0); return 5; }

    // (a) what Halo effectively does today, post-shim: attach a NULL drawable
    if (aglSetDrawable) {
        GLboolean okNull = aglSetDrawable(ctx, NULL);
        GLenum e = aglGetError ? aglGetError() : 0;
        printf("\n[a] aglSetDrawable(ctx, NULL)   -> %d   err=%u (%s)   <-- today's behaviour\n",
               (int)okNull, (unsigned)e,
               (e && aglErrorString) ? (const char *)aglErrorString(e) : "-");
    }

    // (b) the proposed fix: route to the modern WindowRef entry point
    GLboolean okWin = aglSetWindowRef(ctx, win);
    GLenum e2 = aglGetError ? aglGetError() : 0;
    printf("[b] aglSetWindowRef(ctx, win)   -> %d   err=%u (%s)   <-- proposed fix\n",
           (int)okWin, (unsigned)e2,
           (e2 && aglErrorString) ? (const char *)aglErrorString(e2) : "-");

    if (okWin && aglSetCurrentContext) {
        GLboolean cur = aglSetCurrentContext(ctx);
        GLenum e3 = aglGetError ? aglGetError() : 0;
        printf("[c] aglSetCurrentContext(ctx)   -> %d   err=%u\n", (int)cur, (unsigned)e3);
        const unsigned char *(*glGetString)(GLenum) = dlsym(RTLD_DEFAULT, "glGetString");
        if (cur && glGetString) {
            const unsigned char *ren = glGetString(0x1F01 /*GL_RENDERER*/);
            const unsigned char *ver = glGetString(0x1F02 /*GL_VERSION*/);
            printf("[d] GL_RENDERER = %s\n[d] GL_VERSION  = %s\n",
                   ren ? (const char *)ren : "(null)", ver ? (const char *)ver : "(null)");
        }
    }
    printf("\nVERDICT: aglSetWindowRef %s a viable replacement for the dead GetWindowPort path.\n",
           okWin ? "IS" : "is NOT");
    return okWin ? 0 : 6;
}
