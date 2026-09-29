/* aglpresent.m — can an AGL context attached to a SHOWN Carbon window be
 * re-presented in a native NSWindow (the cgl_fullscreen_shim.m presenter), and
 * does AGL steal the drawable back on aglUpdateContext / aglSwapBuffers?
 *
 * Each phase clears to a distinct colour, swaps with aglSwapBuffers, then reads
 * the centre pixel of BOTH windows from the WindowServer (screencapture -l),
 * so "which surface got the frame" is measured, not inferred.
 *
 *   clang -arch x86_64 -Wno-deprecated-declarations -framework Cocoa -framework Carbon \
 *         -framework OpenGL -o /tmp/aglpresent aglpresent.m && /tmp/aglpresent
 */
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#include <dlfcn.h>
#include <stdio.h>

extern OSStatus CreateNewWindow(WindowClass, WindowAttributes, const Rect *, WindowRef *);
extern void     ShowWindow(WindowRef);
extern CGWindowID HIWindowGetCGWindowID(WindowRef);

typedef void *AGLPixelFormat, *AGLContext;

static AGLContext g_ctx;
static GLboolean (*swapF)(AGLContext);

static void pump(double s)
{
   [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:s]];
}

static const char *pixel(CGWindowID wid)
{
   static char buf[4][64]; static int k; char *o = buf[k++ & 3];
   char path[64], cmd[128];
   snprintf(path, sizeof path, "/tmp/aglpresent-%u.png", wid);
   snprintf(cmd, sizeof cmd, "screencapture -x -o -l %u %s", wid, path);
   NSBitmapImageRep *rep = system(cmd) == 0
      ? [NSBitmapImageRep imageRepWithData:[NSData dataWithContentsOfFile:@(path)]] : nil;
   if (!rep) { snprintf(o, 64, "noimg"); return o; }
   NSColor *c = [rep colorAtX:rep.pixelsWide / 2 y:rep.pixelsHigh / 2];
   snprintf(o, 64, "%ldx%ld rgb(%.0f,%.0f,%.0f)", (long)rep.pixelsWide, (long)rep.pixelsHigh,
            c.redComponent * 255, c.greenComponent * 255, c.blueComponent * 255);
   return o;
}

static void frame(float r, float g, float b)
{
   glClearColor(r, g, b, 1); glClear(GL_COLOR_BUFFER_BIT); swapF(g_ctx);
}

int main(void)
{
   [NSApplication sharedApplication];
   [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
   void *agl = dlopen("/System/Library/Frameworks/AGL.framework/AGL", RTLD_LAZY);
   AGLPixelFormat (*choose)(void *, int, const int *) = dlsym(agl, "aglChoosePixelFormat");
   AGLContext (*create)(AGLPixelFormat, AGLContext)   = dlsym(agl, "aglCreateContext");
   GLboolean (*setwin)(AGLContext, WindowRef)         = dlsym(agl, "aglSetWindowRef");
   GLboolean (*setcur)(AGLContext)                    = dlsym(agl, "aglSetCurrentContext");
   GLboolean (*upd)(AGLContext)                       = dlsym(agl, "aglUpdateContext");
   GLboolean (*getcgl)(AGLContext, void **)           = dlsym(agl, "aglGetCGLContext");
   swapF = dlsym(agl, "aglSwapBuffers");

   Rect r = { 150, 150, 150 + 600, 150 + 800 };
   WindowRef cw = NULL;
   CreateNewWindow(kDocumentWindowClass, 0x00080000, &r, &cw);
   int attrs[] = { 4, 5, 12, 24, 0 };
   g_ctx = create(choose(NULL, 0, attrs), NULL);
   ShowWindow(cw);
   printf("attach carbon: %d\n", setwin(g_ctx, cw));
   setcur(g_ctx);
   void *cgl = NULL; getcgl(g_ctx, &cgl);
   printf("agl=%p cgl=%p same=%d\n", g_ctx, cgl, g_ctx == cgl);
   const CGWindowID cid = HIWindowGetCGWindowID(cw);

   frame(0, 1, 0); pump(0.3);
   printf("P0 green->carbon : carbon=%s\n", pixel(cid));

   NSWindow *nw = [[NSWindow alloc] initWithContentRect:NSMakeRect(900, 200, 800, 600)
      styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
   NSView *v = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)];
   v.wantsBestResolutionOpenGLSurface = NO;
   [nw.contentView addSubview:v];
   [nw makeKeyAndOrderFront:nil];
   NSOpenGLContext *ns = [[NSOpenGLContext alloc] initWithCGLContextObj:(CGLContextObj)cgl];
   const GLint bs[2] = { 640, 480 };
   CGLSetParameter(cgl, kCGLCPSurfaceBackingSize, bs);
   CGLEnable(cgl, kCGLCESurfaceBackingSize);
   ns.view = v; [ns update];
   const CGWindowID nid = (CGWindowID)nw.windowNumber;

   frame(1, 0, 0); pump(0.3);
   printf("P1 red after ns.view   : ns=%s carbon=%s\n", pixel(nid), pixel(cid));
   GLint vp[4]; glGetIntegerv(GL_VIEWPORT, vp);
   printf("   viewport %d,%d %dx%d\n", vp[0], vp[1], vp[2], vp[3]);

   upd(g_ctx);
   frame(0, 0, 1); pump(0.3);
   printf("P2 blue after aglUpdate: ns=%s carbon=%s\n", pixel(nid), pixel(cid));

   [ns update];
   frame(1, 1, 0); pump(0.3);
   printf("P3 yellow after ns upd : ns=%s carbon=%s\n", pixel(nid), pixel(cid));

   CGLFlushDrawable(cgl);
   frame(1, 0, 1); pump(0.3);
   printf("P4 magenta             : ns=%s carbon=%s\n", pixel(nid), pixel(cid));
   return 0;
}
