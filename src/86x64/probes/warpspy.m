// warpspy.m — Halo windowed-mode cursor jumps + gray traffic lights (2026-09-29).
// DYLD_INSERT spy on the NATIVE CGWarpMouseCursorPosition libabiconv's shim
// calls. Per warp it logs: requested point, where the cursor actually is now
// (did the last warp land?), the Carbon active window + its content origin
// (what ci_content_origin maps with) and AppKit's key window. Hypothesis: key
// flips between Halo's hidden Carbon window and the CGLFSWindow surface, so
// warp and read-back use different origins.
// build: clang -arch x86_64 -dynamiclib -framework AppKit -framework Carbon warpspy.m -o /tmp/warpspy.dylib
// run:   DYLD_INSERT_LIBRARIES=/tmp/warpspy.dylib WARPSPY_LOG=/tmp/warpspy.log <Halo binary>
#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#include <dlfcn.h>
#include <stdio.h>

static FILE *out(void) {
   static FILE *f;
   if (!f) { const char *p = getenv("WARPSPY_LOG"); f = fopen(p ? p : "/tmp/warpspy.log", "w"); setvbuf(f, NULL, _IOLBF, 0); }
   return f;
}

static CGError my_warp(CGPoint p) {
   static int n;
   CGEventRef e = CGEventCreate(NULL);
   CGPoint now = CGEventGetLocation(e);
   CFRelease(e);
   void *(*anfw)(void) = dlsym(RTLD_DEFAULT, "ActiveNonFloatingWindow");
   OSStatus (*gwb)(void *, WindowRegionCode, Rect *) = dlsym(RTLD_DEFAULT, "GetWindowBounds");
   void *aw = anfw ? anfw() : NULL;
   Rect r = {0};
   if (aw && gwb) gwb(aw, kWindowContentRgn, &r);
   NSWindow *kw = NSApp.keyWindow;
   fprintf(out(), "%5d warp(%.1f,%.1f) cursor-now(%.1f,%.1f) active=%p content-origin(%d,%d) key=%s#%ld main=#%ld appActive=%d\n",
           n++, p.x, p.y, now.x, now.y, aw, r.left, r.top,
           kw ? object_getClassName(kw) : "nil", (long)kw.windowNumber,
           (long)NSApp.mainWindow.windowNumber, NSApp.isActive);
   return CGWarpMouseCursorPosition(p);
}

__attribute__((used, section("__DATA,__interpose")))
static struct { const void *repl, *orig; } interposers[] = {
   { (const void *)my_warp, (const void *)CGWarpMouseCursorPosition },
};
