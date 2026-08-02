/* displayswitch.m — does a classic CGDisplaySwitchToMode panic modern AppKit?
 *
 * Halo CE dies with SIGILL in AppKit's NSCGSPanic, reached SYNCHRONOUSLY from our
 * ___CGDisplaySwitchToMode bridge:
 *
 *   libabiconv __CGDisplaySwitchToMode.l1 -> SkyLight displaySetMode
 *     -> SLSCompleteDisplayConfigurationWithOption -> the reconfiguration datagram
 *     -> AppKit _NSCGSDisplayConfigurationUpdateAndInvokeObservers
 *     -> -[NSApplication enumerateWindowsWithOptions:usingBlock:]
 *     -> -[NSCGSWindow setFrame:] -> _updateLayer
 *     -> _createRootLayerAndContextIfNeededUsingAsyncBehavior: -> _createContext
 *     -> NSCGSPanic (ud2)
 *
 * This probe reproduces the classic 2006 fullscreen idiom in a NATIVE x86_64 app
 * with an ordinary AppKit window, so the verdict separates two hypotheses:
 *
 *   (A) modern AppKit cannot survive ANY display reconfiguration raised from
 *       inside the process while it owns windows  -> the deprecated API is
 *       harmful-when-live and the translator must stop performing the switch;
 *   (B) it needs something specific to Halo/our substrate (a captured display, or
 *       a window WE minted) -> keep digging, do not neutralise the API.
 *
 * BISECTS the capture half separately, because the classic idiom is
 * `CGCaptureAllDisplays(); CGDisplaySwitchToMode(...)` and a captured display is
 * itself a reason CGS window-context creation could fail.
 *
 *   argv[1] = "plain"    switch only
 *             "capture"  CGCaptureAllDisplays() then switch
 *             "nowindow" switch with no window on screen (control)
 *             "agl"      attach a REAL AGL/GL context to the window with
 *                        aglSetWindowRef (what `6e9d284` makes Halo do) and then
 *                        switch — tests whether a GL-attached window is the one
 *                        AppKit cannot re-create a CGS context for
 *             "aglcap"   "agl" + CGCaptureAllDisplays (Halo's full idiom)
 *             "carbon"       a REAL Carbon CreateNewWindow window, then switch
 *             "carbonhidden" the same window NEVER shown, then switch
 *             "carbonmodern" the same window, switched with the LIVE successor
 *                            CGDisplaySetDisplayMode instead (does Rule B help?)
 *             "carbonhide"   the same window, HIDDEN across the switch and
 *                            re-shown after — the candidate fix
 *
 * ⚠It really changes the display mode. It restores the entry mode on every exit
 * path (including the panic path is NOT possible — so it prints the entry mode
 * first, and `restore` re-applies it if a run left the display switched).
 *
 * Build:
 *   clang -arch x86_64 -framework Cocoa -framework ApplicationServices \
 *         -o displayswitch displayswitch.m
 */

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>
#include <unistd.h>
#include <dlfcn.h>
#include <objc/runtime.h>

/* The deprecated mode-dictionary family Halo imports. Declared by hand: modern
 * SDK headers still ship them but behind availability macros that clang errors
 * on with -Werror; and this is exactly the surface under test. */
extern CFDictionaryRef CGDisplayBestModeForParameters(CGDirectDisplayID d,
                                                      size_t bpp, size_t w,
                                                      size_t h,
                                                      boolean_t *exactMatch);
extern CFDictionaryRef CGDisplayCurrentMode(CGDirectDisplayID d);
extern CGError CGDisplaySwitchToMode(CGDirectDisplayID d, CFDictionaryRef mode);

/* CreateNewWindow/ShowWindow are gone from the 64-bit Carbon HEADERS but alive in
 * HIToolbox (carbon_window_shim.c dlsyms CreateNewWindow for exactly this reason).
 * Declared by hand so the probe can mint the same window a translated app gets. */
typedef struct OpaqueWindowPtr *CarbonWindowRef;
extern OSStatus CreateNewWindow(UInt32 cls, UInt32 attrs, const Rect *bounds,
                                CarbonWindowRef *outWindow);
extern void ShowWindow(CarbonWindowRef w);
extern void HideWindow(CarbonWindowRef w);

static void dump_mode(const char *tag, CFDictionaryRef m)
{
   if (!m) { fprintf(stderr, "[probe] %s = NULL\n", tag); return; }
   int w = 0, h = 0, bpp = 0;
   CFNumberRef n;
   if ((n = CFDictionaryGetValue(m, CFSTR("Width"))))  CFNumberGetValue(n, kCFNumberIntType, &w);
   if ((n = CFDictionaryGetValue(m, CFSTR("Height")))) CFNumberGetValue(n, kCFNumberIntType, &h);
   if ((n = CFDictionaryGetValue(m, CFSTR("BitsPerPixel")))) CFNumberGetValue(n, kCFNumberIntType, &bpp);
   fprintf(stderr, "[probe] %s = %dx%d %dbpp\n", tag, w, h, bpp);
}

int main(int argc, char **argv)
{
   const char *mode = argc > 1 ? argv[1] : "plain";
   setvbuf(stderr, NULL, _IONBF, 0);

   @autoreleasepool {
      [NSApplication sharedApplication];
      [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

      NSWindow *win = nil;
      void *wref = NULL;              /* Carbon WindowRef when carbon* mode */
      if (strncmp(mode, "carbon", 6) == 0 || strncmp(mode, "agl", 3) == 0) {
         /* A REAL Carbon window, minted the way carbon_window_shim.c does for a
          * translated app: kWindowCompositingAttribute (bit 19) is mandatory on
          * 64-bit or NewWindowCommon returns -5601. On 64-bit HIToolbox this is
          * backed by an NSCarbonWindow, so it lands in [NSApp windows] and the
          * display-reconfiguration walk will visit it. */
         Rect b = { 200, 200, 200 + 622, 200 + 800 };  /* t,l,b,r */
         CarbonWindowRef w = NULL;
         OSStatus st = CreateNewWindow(kDocumentWindowClass,
                                       (1u << 19) | kWindowStandardDocumentAttributes,
                                       &b, &w);
         fprintf(stderr, "[probe] CreateNewWindow st=%d w=%p\n", (int)st, (void *)w);
         if (w) {
            if (strcmp(mode, "carbonhidden") != 0) ShowWindow(w);
            wref = w;
         }
      } else if (strcmp(mode, "nowindow") != 0) {
         win = [[NSWindow alloc]
                  initWithContentRect:NSMakeRect(200, 200, 800, 622)
                            styleMask:NSWindowStyleMaskTitled
                              backing:NSBackingStoreBuffered
                                defer:NO];
         [win setTitle:@"displayswitch probe"];
         [win makeKeyAndOrderFront:nil];
         fprintf(stderr, "[probe] window on screen: %d\n", (int)[win isVisible]);
      }
      for (NSWindow *w in [NSApp windows])
         fprintf(stderr, "[probe] NSApp window: class=%s visible=%d frame=%.0fx%.0f\n",
                 class_getName([w class]), (int)[w isVisible],
                 [w frame].size.width, [w frame].size.height);
      fprintf(stderr, "[probe] NSApp windows=%lu\n", (unsigned long)[[NSApp windows] count]);
      /* Let AppKit finish bringing the window up before reconfiguring. */
      [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.7]];

      CGDirectDisplayID disp = CGMainDisplayID();
      CFDictionaryRef entry = CGDisplayCurrentMode(disp);
      if (entry) CFRetain(entry);
      dump_mode("entry mode", entry);

      if (strcmp(mode, "restore") == 0) {
         fprintf(stderr, "[probe] restore-only: nothing to do, entry mode is current\n");
         return 0;
      }

      /* Attach a real hardware GL context to the window exactly the way
       * agl_drawable_shim.c does for a translated Carbon app. AGL has no SDK
       * stub since ~10.14, so it is dlopen'd from the shared cache. */
      if (strncmp(mode, "agl", 3) == 0 && wref) {
         void *aglh = dlopen("/System/Library/Frameworks/AGL.framework/AGL",
                             RTLD_LAZY | RTLD_GLOBAL);
         fprintf(stderr, "[probe] dlopen AGL -> %p\n", aglh);
         void *(*aglChoosePixelFormat)(const void *, int, const int *) =
            dlsym(aglh, "aglChoosePixelFormat");
         void *(*aglCreateContext)(void *, void *) = dlsym(aglh, "aglCreateContext");
         unsigned char (*aglSetWindowRef)(void *, void *) = dlsym(aglh, "aglSetWindowRef");
         unsigned char (*aglSetCurrentContext)(void *) = dlsym(aglh, "aglSetCurrentContext");
         const int attrs[] = { 4 /*RGBA*/, 5 /*DOUBLEBUFFER*/, 12 /*DEPTH_SIZE*/, 24,
                               73 /*ACCELERATED*/, 0 /*NONE*/ };
         void *pix = aglChoosePixelFormat ? aglChoosePixelFormat(0, 0, attrs) : NULL;
         void *ctx = pix && aglCreateContext ? aglCreateContext(pix, NULL) : NULL;
         int sw = (ctx && wref && aglSetWindowRef) ? aglSetWindowRef(ctx, wref) : -1;
         int cc = (ctx && aglSetCurrentContext) ? aglSetCurrentContext(ctx) : -1;
         fprintf(stderr, "[probe] pix=%p ctx=%p wref=%p aglSetWindowRef=%d "
                         "aglSetCurrentContext=%d\n", pix, ctx, wref, sw, cc);
         const unsigned char *(*p_glGetString)(unsigned int) =
            dlsym(RTLD_DEFAULT, "glGetString");
         if (cc == 1 && p_glGetString)
            fprintf(stderr, "[probe] GL_RENDERER=%s\n", (const char *)p_glGetString(0x1F01));
         [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.4]];
      }

      if (strstr(mode, "cap")) {
         fprintf(stderr, "[probe] calling CGCaptureAllDisplays ...\n");
         CGError ce = CGCaptureAllDisplays();
         fprintf(stderr, "[probe] CGCaptureAllDisplays -> %d (captured=%d) (SURVIVED)\n",
                 (int)ce, (int)CGDisplayIsCaptured(disp));
         [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.4]];
      }
      if (strstr(mode, "only")) {   /* capture-only: stop before the switch */
         CGReleaseAllDisplays();
         fprintf(stderr, "[probe] DONE, no panic (capture only)\n");
         return 0;
      }

      boolean_t exact = 0;
      CFDictionaryRef target = CGDisplayBestModeForParameters(disp, 32, 800, 600, &exact);
      dump_mode("target mode", target);
      fprintf(stderr, "[probe] exactMatch=%d\n", (int)exact);

      /* THE CANDIDATE FIX: a HIDDEN Carbon window does not panic (arm
       * "carbonhidden"), so hide every visible Carbon window across the
       * reconfiguration and re-show it afterwards. If this survives AND the mode
       * really changes, the switch keeps working instead of being neutralised. */
      if (strstr(mode, "hide") && wref) {
         HideWindow((CarbonWindowRef)wref);
         [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.2]];
         fprintf(stderr, "[probe] hid Carbon window; NSApp windows=%lu\n",
                 (unsigned long)[[NSApp windows] count]);
      }

      CGError e;
      if (strstr(mode, "modern")) {
         /* The LIVE successor: CGDisplaySetDisplayMode. If this panics too, the
          * panic is about display RECONFIGURATION per se and routing the dead API
          * to its successor (Rule B) cannot help. */
         CFArrayRef all = CGDisplayCopyAllDisplayModes(disp, NULL);
         CGDisplayModeRef best = NULL;
         for (CFIndex i = 0; all && i < CFArrayGetCount(all); ++i) {
            CGDisplayModeRef m = (CGDisplayModeRef)CFArrayGetValueAtIndex(all, i);
            if (CGDisplayModeGetWidth(m) == 1920 && CGDisplayModeGetHeight(m) == 1200)
               best = m;
         }
         fprintf(stderr, "[probe] calling CGDisplaySetDisplayMode(%p) ...\n", best);
         e = best ? CGDisplaySetDisplayMode(disp, best, NULL) : -1;
         fprintf(stderr, "[probe] CGDisplaySetDisplayMode -> %d  (SURVIVED)\n", (int)e);
      } else {
         fprintf(stderr, "[probe] calling CGDisplaySwitchToMode ...\n");
         e = CGDisplaySwitchToMode(disp, target);
         fprintf(stderr, "[probe] CGDisplaySwitchToMode -> %d  (SURVIVED)\n", (int)e);
      }

      [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.8]];
      dump_mode("after switch", CGDisplayCurrentMode(disp));
      if (strstr(mode, "hide") && wref) {
         ShowWindow((CarbonWindowRef)wref);
         [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.3]];
         fprintf(stderr, "[probe] re-showed Carbon window; NSApp windows=%lu\n",
                 (unsigned long)[[NSApp windows] count]);
      }

      fprintf(stderr, "[probe] restoring entry mode ...\n");
      CGError r = CGDisplaySwitchToMode(disp, entry);
      fprintf(stderr, "[probe] restore -> %d\n", (int)r);
      if (strstr(mode, "cap")) {
         CGReleaseAllDisplays();
         fprintf(stderr, "[probe] released displays\n");
      }
      [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.5]];
      dump_mode("final mode", CGDisplayCurrentMode(disp));
   }
   fprintf(stderr, "[probe] DONE, no panic\n");
   return 0;
}
