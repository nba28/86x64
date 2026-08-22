/* classagl.m — can a window class that SURVIVES a post-show resize also host
 * AGL and keep drawing after the resize? AGL via dlsym (no headers needed). */
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

extern OSStatus CreateNewWindow(WindowClass, WindowAttributes, const Rect *, WindowRef *);
extern void     ShowWindow(WindowRef);
extern OSStatus SetWindowBounds(WindowRef, WindowRegionCode, const Rect *);
extern OSStatus GetWindowBounds(WindowRef, WindowRegionCode, Rect *);

typedef void *AGLPixelFormat, *AGLContext;
typedef unsigned char GLboolean;

int main(int argc, char **argv) {
   const unsigned cls = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 0) : 4;
   [NSApplication sharedApplication];

   void *agl = dlopen("/System/Library/Frameworks/AGL.framework/AGL", RTLD_LAZY);
   printf("AGL framework: %s\n", agl ? "loaded" : dlerror());
   if (!agl) return 3;
   AGLPixelFormat (*choose)(void *, int, const int *) = dlsym(agl, "aglChoosePixelFormat");
   AGLContext (*create)(AGLPixelFormat, AGLContext)   = dlsym(agl, "aglCreateContext");
   GLboolean (*setwin)(AGLContext, WindowRef)         = dlsym(agl, "aglSetWindowRef");
   GLboolean (*setcur)(AGLContext)                    = dlsym(agl, "aglSetCurrentContext");
   GLboolean (*updctx)(AGLContext)                    = dlsym(agl, "aglUpdateContext");
   GLboolean (*swap)(AGLContext)                      = dlsym(agl, "aglSwapBuffers");
   const unsigned char *(*glGetStringF)(unsigned)     = dlsym(RTLD_DEFAULT, "glGetString");
   void (*glClearF)(unsigned)                         = dlsym(RTLD_DEFAULT, "glClear");
   void (*glViewportF)(int,int,int,int)               = dlsym(RTLD_DEFAULT, "glViewport");
   if (!choose || !create || !setwin) { printf("AGL symbols missing\n"); return 3; }

   Rect r = { 100, 100, 100 + 600, 100 + 800 };
   WindowRef w = NULL;
   if (CreateNewWindow(cls, 0x00080000, &r, &w) != noErr || !w) {
      printf("RESULT cls=%u CREATE_FAILED\n", cls); return 2;
   }
   int attrs[] = { 4 /*AGL_RGBA*/, 5 /*AGL_DOUBLEBUFFER*/, 12 /*AGL_DEPTH_SIZE*/, 24, 0 };
   AGLPixelFormat pf = choose(NULL, 0, attrs);
   AGLContext ctx = pf ? create(pf, NULL) : NULL;
   printf("pf=%p ctx=%p\n", pf, ctx);
   if (!ctx) { printf("RESULT cls=%u NO_CONTEXT\n", cls); return 3; }

   ShowWindow(w);
   printf("attach: %d\n", (int)setwin(ctx, w));
   setcur(ctx);
   if (glGetStringF) printf("GL_RENDERER=%s\n", glGetStringF(0x1F01));
   if (glViewportF) glViewportF(0, 0, 800, 600);
   if (glClearF) glClearF(0x4000);
   if (swap) swap(ctx);

   /* the whole point: resize AFTER show AND after GL is live */
   Rect t = { 0, 0, 480, 640 };
   OSStatus st = SetWindowBounds(w, kWindowContentRgn, &t);
   if (updctx) updctx(ctx);
   if (glViewportF) glViewportF(0, 0, 640, 480);
   if (glClearF) glClearF(0x4000);
   if (swap) swap(ctx);

   Rect got = { 0, 0, 0, 0 };
   GetWindowBounds(w, kWindowContentRgn, &got);
   printf("RESULT cls=%u SURVIVED resize st=%d now=%dx%d GL_STILL_LIVE=%s\n",
          cls, (int)st, got.right - got.left, got.bottom - got.top,
          (glGetStringF && glGetStringF(0x1F01)) ? "yes" : "no");
   return 0;
}
