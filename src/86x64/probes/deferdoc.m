/* deferdoc.m — validate the WHOLE chain on kDocumentWindowClass, in Halo's order:
 *   create 800x600 -> [show HELD] -> attach AGL to an UNSHOWN window
 *   -> resize to 640x480 (pre-show, must be free) -> swap -> show
 * If this survives and ends 640x480 with live GL, the deferred-show design is
 * sound for the class Halo actually renders correctly in. */
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#include <dlfcn.h>
#include <stdio.h>

extern OSStatus CreateNewWindow(WindowClass, WindowAttributes, const Rect *, WindowRef *);
extern void     ShowWindow(WindowRef);
extern OSStatus SetWindowBounds(WindowRef, WindowRegionCode, const Rect *);
extern OSStatus GetWindowBounds(WindowRef, WindowRegionCode, Rect *);
typedef unsigned char GLboolean;

int main(void) {
   [NSApplication sharedApplication];
   void *agl = dlopen("/System/Library/Frameworks/AGL.framework/AGL", RTLD_LAZY);
   void *(*choose)(void *, int, const int *) = dlsym(agl, "aglChoosePixelFormat");
   void *(*create)(void *, void *)           = dlsym(agl, "aglCreateContext");
   GLboolean (*setwin)(void *, WindowRef)    = dlsym(agl, "aglSetWindowRef");
   GLboolean (*setcur)(void *)               = dlsym(agl, "aglSetCurrentContext");
   GLboolean (*updctx)(void *)               = dlsym(agl, "aglUpdateContext");
   GLboolean (*swap)(void *)                 = dlsym(agl, "aglSwapBuffers");
   const unsigned char *(*glStr)(unsigned)   = dlsym(RTLD_DEFAULT, "glGetString");
   void (*glClearF)(unsigned)                = dlsym(RTLD_DEFAULT, "glClear");

   Rect r = { 100, 100, 100 + 600, 100 + 800 };
   WindowRef w = NULL;
   OSStatus st = CreateNewWindow(kDocumentWindowClass,
                                 (1u<<19)|kWindowStandardDocumentAttributes, &r, &w);
   printf("1. CreateNewWindow(kDocument, 800x600) st=%d\n", (int)st);
   if (!w) return 2;

   printf("2. ShowWindow HELD (not called)\n");

   int attrs[] = { 4, 5, 12, 24, 0 };
   void *pf = choose(NULL, 0, attrs);
   void *ctx = pf ? create(pf, NULL) : NULL;
   printf("3. AGL attach to an UNSHOWN kDocument window: %d (ctx=%p)\n",
          ctx ? (int)setwin(ctx, w) : -1, ctx);
   if (!ctx) return 3;
   setcur(ctx);
   printf("   GL_RENDERER=%s\n", glStr ? glStr(0x1F01) : (const unsigned char *)"?");

   Rect t = { 0, 0, 480, 640 };
   st = SetWindowBounds(w, kWindowContentRgn, &t);
   printf("4. pre-show resize to 640x480 st=%d  (fatal if it were shown)\n", (int)st);

   if (updctx) updctx(ctx);
   if (glClearF) glClearF(0x4000);
   if (swap) swap(ctx);
   printf("5. frame swapped -> this is the trigger that releases the show\n");

   ShowWindow(w);
   if (updctx) updctx(ctx);
   if (glClearF) glClearF(0x4000);
   if (swap) swap(ctx);

   Rect got = { 0, 0, 0, 0 };
   GetWindowBounds(w, kWindowContentRgn, &got);
   printf("6. RESULT shown, %dx%d (want 640x480), GL live=%s\n",
          got.right - got.left, got.bottom - got.top,
          (glStr && glStr(0x1F01)) ? "yes" : "no");
   return 0;
}
