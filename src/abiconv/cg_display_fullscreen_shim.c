/*
 * cg_display_fullscreen_shim.c — ONE job: the classic FULLSCREEN DISPLAY-MODE
 * idiom.  Bridge "reconfigure the physical display" to "present the app's
 * existing window", and keep every mode query coherent with what the app
 * believes it got.
 *
 * THE IDIOM (universal in 32-bit-era Mac games; never an app name):
 *
 *     CGCaptureAllDisplays();
 *     mode = CGDisplayBestModeForParameters(dpy, 32, w, h, &exact);
 *     CGDisplaySwitchToMode(dpy, mode);
 *     ... render into the window created for w x h ...
 *     CGDisplaySwitchToMode(dpy, savedMode);  CGReleaseAllDisplays();
 *
 * WHY IT MUST NOT BE PERFORMED (measured, src/86x64/probes/displayswitch.m):
 *   Modern AppKit cannot create a CGS context for an NSCarbonWindow.  Every code
 *   path that forces one dies with SIGILL in NSCGSPanic:
 *       -[NSCGSWindow _createContext] <- _createRootLayerAndContextIfNeeded...
 *                                     <- _updateLayer <- setSize:/setFrame:
 *   A display-mode switch reaches it because CoreGraphics posts a
 *   reconfiguration datagram and AppKit's observer setFrame:s EVERY window in
 *   the process.  Bisected, all on the live machine, native x86_64:
 *       no window / never-shown window / window disposed across it -> survives
 *       visible Carbon window ................................... PANICS
 *       hidden across the switch ................................ PANICS
 *       switched with the modern CGDisplaySetDisplayMode ........ PANICS
 *       SetWindowBounds that RESIZES, no display API at all ..... PANICS
 *       SetWindowBounds that only MOVES (size unchanged) ........ survives
 *   So neither "hide it", nor "use the modern API", nor "resize the window to
 *   the screen" is available; disposing is not either, because the surviving
 *   window is the app's own GL render target (measured with probes/windowspy.m:
 *   exactly one window at the switch, class=NSCarbonWindow, 800x622 = Halo's
 *   game window).
 *
 * WHAT THIS FILE DOES INSTEAD:
 *   1. CGDisplaySwitchToMode NEVER reconfigures a display.  It records the
 *      requested mode as the display's VIRTUAL mode and returns success.
 *   2. Every mode query — CGDisplayCurrentMode, CGDisplayBounds,
 *      CGDisplayPixelsWide/High, CGDisplayBitsPerPixel — answers from the
 *      virtual mode while one is installed.  Coherence is not cosmetic: a game
 *      computes its viewport, its aspect ratio and its mouse mapping from these,
 *      so a truthful answer here would be WORSE than the lie the app asked for.
 *   3. CGDisplayBestModeForParameters(AndRefreshRate) SYNTHESISES a mode
 *      dictionary for exactly the requested geometry and reports exactMatch=1.
 *      This is required, not cosmetic: on this machine native
 *      CGDisplayBestModeForParameters(dpy,32,800,600) returns 1920x1200 with
 *      exactMatch=0 — modern Macs have no small modes at all — so an app that
 *      trusts the returned dictionary would size its viewport to a resolution
 *      nothing in the process is rendering at.
 *   4. The capture family is tracked, not performed.  A real
 *      CGCaptureAllDisplays raises a shielding window over everything; with no
 *      mode switch behind it the app's own window is left UNDER the shield and
 *      the user sees black.  Capture only ever existed to get exclusive use of
 *      the screen, which on modern macOS is what presenting the window does.
 *      CGDisplayIsCaptured answers from the tracked state so the app's own
 *      bookkeeping stays consistent, and CGReleaseAllDisplays/CGDisplayRelease
 *      also drop the virtual mode — releasing a captured display is documented
 *      to restore its original mode, and apps rely on exactly that at exit.
 *   5. PRESENTATION: the window that owns the GL drawable (identified
 *      structurally through agl_drawable_shim.c's ctx->drawable association —
 *      never by title, class or app name) is MOVED to the display's origin so
 *      that virtual-display coordinates and global screen coordinates coincide,
 *      which is what makes CGWarpMouseCursorPosition and every "centre of the
 *      screen" computation land where the app means.  The move is performed
 *      ONLY when it needs no resize, because a resize is the fatal operation;
 *      and the bounds we report afterwards are read back from where the window
 *      ACTUALLY ended up, so a refused or constrained move degrades into a
 *      truthful origin rather than a wrong one.
 *
 * DELIBERATELY NOT DONE: no attempt to scale the drawable to fill the screen.
 * That needs a window resize (fatal) or AGL_SURFACE_BACKING_SIZE, and neither
 * is measured.  The app therefore renders at exactly the resolution it asked
 * for, in a window of that size — real functionality, honestly reported.
 *
 * KILL SWITCH: M64_NO_DISPLAY_FULLSCREEN_BRIDGE=1 disarms the whole file — every
 * entry point forwards to the native CoreGraphics function, i.e. exactly the
 * pre-fix abigen-bridge behaviour.  Used by the A/B guard (tests-i386,
 * display-fullscreen-bridge).
 *
 * ABI: reached from translated i386 code through the ___CG* trampolines in
 * cg_display_tramp.asm (rdi -> &i386 args[0], result in eax; CGDisplayBounds
 * uses the SRET form because an i386 CGRect is four 4-byte CGFloats returned
 * through a hidden first slot the callee pops).  The symbols are listed in
 * custom.syms so neither abigen pass emits a second definition.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <os/lock.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

/* objc_shim.c proxy arena + the full CF-ref resolver abigen's bridges use. */
extern uint32_t x64_objc_wrap(uint64_t real);
extern uint64_t _86x64_unwrap_obj_arg(uint32_t h);

/* agl_drawable_shim.c: which WindowRef currently owns a GL drawable, and through
 * which native context. THE structural identification of the render target. */
extern int agl_gl_render_target(void **win_out, void **ctx_out);

/* The deprecated CoreGraphics surface, declared by hand: the modern SDK still
 * ships the prototypes but behind availability macros, and this file IS the
 * compatibility layer for them. CGDisplayBitsPerPixel is gone from the modern
 * headers entirely, so it is never called natively — it is answered from the
 * mode dictionary instead. */
extern CFDictionaryRef CGDisplayBestModeForParameters(CGDirectDisplayID, size_t,
                                                      size_t, size_t, boolean_t *);
extern CFDictionaryRef CGDisplayBestModeForParametersAndRefreshRate(
   CGDirectDisplayID, size_t, size_t, size_t, double, boolean_t *);
extern CFDictionaryRef CGDisplayCurrentMode(CGDirectDisplayID);
extern CGError CGDisplaySwitchToMode(CGDirectDisplayID, CFDictionaryRef);
extern CGError CGCaptureAllDisplays(void);
extern CGError CGCaptureAllDisplaysWithOptions(uint32_t);
extern CGError CGDisplayCapture(CGDirectDisplayID);
extern CGError CGDisplayCaptureWithOptions(CGDirectDisplayID, uint32_t);
extern CGError CGReleaseAllDisplays(void);
extern CGError CGDisplayRelease(CGDirectDisplayID);
extern boolean_t CGDisplayIsCaptured(CGDirectDisplayID);

/* Carbon Rect + the window-geometry pair. No 64-bit SDK stub (same reason
 * carbon_window_shim.c dlsyms CreateNewWindow) but alive in HIToolbox. */
typedef struct { int16_t top, left, bottom, right; } CarbonRect;
typedef int32_t (*win_getbounds_fn)(void *, uint32_t, CarbonRect *);
typedef int32_t (*win_setbounds_fn)(void *, uint32_t, const CarbonRect *);
#define kWinContentRgn 33u

/* THE kill switch. Read once; when set, every entry point below forwards. */
static int bridge_off(void)
{
   static int off = -1;
   if (off < 0) off = getenv("M64_NO_DISPLAY_FULLSCREEN_BRIDGE") ? 1 : 0;
   return off;
}

/* ---- virtual-mode + capture state ---------------------------------------
 * A process drives a handful of displays; a linear scan under one lock is both
 * simplest and correct (same shape as agl_drawable_shim.c's bind table). */
#define MAX_DISP 8
static struct {
   uint32_t        id;
   int             used;
   int             captured;
   int             installed;         /* a virtual mode is in force */
   int             w, h, bpp;
   double          refresh;
   CFDictionaryRef mode;              /* the dict handed back by CurrentMode */
   double          bx, by;            /* virtual bounds origin (global CG coords) */
} g_disp[MAX_DISP];
static int g_all_captured;
static os_unfair_lock g_lk = OS_UNFAIR_LOCK_INIT;

/* Caller holds g_lk. */
static int slot_for(uint32_t id)
{
   int free_slot = -1;
   for (int i = 0; i < MAX_DISP; i++) {
      if (g_disp[i].used && g_disp[i].id == id) return i;
      if (!g_disp[i].used && free_slot < 0) free_slot = i;
   }
   if (free_slot < 0) return -1;
   g_disp[free_slot].used = 1;
   g_disp[free_slot].id = id;
   return free_slot;
}

/* ---- mode-dictionary helpers -------------------------------------------- */
static int dict_int(CFDictionaryRef d, CFStringRef key, int dflt)
{
   CFNumberRef n = d ? (CFNumberRef)CFDictionaryGetValue(d, key) : NULL;
   int v = dflt;
   if (n && CFGetTypeID(n) == CFNumberGetTypeID()) CFNumberGetValue(n, kCFNumberIntType, &v);
   return v;
}
static double dict_dbl(CFDictionaryRef d, CFStringRef key, double dflt)
{
   CFNumberRef n = d ? (CFNumberRef)CFDictionaryGetValue(d, key) : NULL;
   double v = dflt;
   if (n && CFGetTypeID(n) == CFNumberGetTypeID()) CFNumberGetValue(n, kCFNumberDoubleType, &v);
   return v;
}
static void dict_set_int(CFMutableDictionaryRef d, CFStringRef key, int v)
{
   CFNumberRef n = CFNumberCreate(NULL, kCFNumberIntType, &v);
   if (n) { CFDictionarySetValue(d, key, n); CFRelease(n); }
}
static void dict_set_dbl(CFMutableDictionaryRef d, CFStringRef key, double v)
{
   CFNumberRef n = CFNumberCreate(NULL, kCFNumberDoubleType, &v);
   if (n) { CFDictionarySetValue(d, key, n); CFRelease(n); }
}

/* The classic mode-dictionary keys are plain CFStrings ("Width", "Height", ...);
 * kCGDisplayWidth & co. are the same strings and are on their way out of the
 * SDK, so spell them literally rather than depend on the exported constants. */
#define K_W    CFSTR("Width")
#define K_H    CFSTR("Height")
#define K_BPP  CFSTR("BitsPerPixel")
#define K_RR   CFSTR("RefreshRate")
#define K_PW   CFSTR("kCGDisplayPixelsWide")
#define K_PH   CFSTR("kCGDisplayPixelsHigh")
#define K_BPR  CFSTR("kCGDisplayBytesPerRow")
#define K_RES  CFSTR("kCGDisplayResolution")

/* Synthesise "the display is now w x h". Built by COPYING the display's real
 * current-mode dictionary and overriding only the geometry, so every other key
 * a 2006 app might read (IOFlags, UsableForDesktopGUI, BitsPerSample, the mode
 * ID, ...) is present and plausible. Cached and retained forever: the cache is
 * bounded, and the app must never be handed a dict we might free under it. */
#define MAX_SYNTH 32
static struct { int used; uint32_t id; int w, h, bpp; double rr; CFDictionaryRef d; }
   g_synth[MAX_SYNTH];

static CFDictionaryRef synth_mode(uint32_t id, int w, int h, int bpp, double rr)
{
   for (int i = 0; i < MAX_SYNTH; i++)
      if (g_synth[i].used && g_synth[i].id == id && g_synth[i].w == w &&
          g_synth[i].h == h && g_synth[i].bpp == bpp && g_synth[i].rr == rr)
         return g_synth[i].d;

   CFDictionaryRef cur = CGDisplayCurrentMode(id);
   CFMutableDictionaryRef m =
      cur ? CFDictionaryCreateMutableCopy(NULL, 0, cur)
          : CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);
   if (!m) return NULL;
   dict_set_int(m, K_W, w);
   dict_set_int(m, K_H, h);
   dict_set_int(m, K_BPP, bpp);
   dict_set_dbl(m, K_RR, rr);
   dict_set_int(m, K_PW, w);
   dict_set_int(m, K_PH, h);
   dict_set_int(m, K_BPR, w * ((bpp + 7) / 8));
   dict_set_int(m, K_RES, 1);

   for (int i = 0; i < MAX_SYNTH; i++)
      if (!g_synth[i].used) {
         g_synth[i].used = 1; g_synth[i].id = id; g_synth[i].w = w;
         g_synth[i].h = h; g_synth[i].bpp = bpp; g_synth[i].rr = rr;
         g_synth[i].d = m;
         return m;
      }
   /* Cache full (an app enumerating hundreds of modes): still correct, just no
    * longer cached — the dict stays retained so the app can hold it safely. */
   return m;
}

/* Mirror abigen's opaque-pointer return rule: a >4GB ref would truncate in the
 * i386 caller's eax, so wrap it; a genuinely low value passes through. */
static uint32_t ref_out(const void *p)
{
   uint64_t v = (uint64_t)(uintptr_t)p;
   return (v >> 32) ? x64_objc_wrap(v) : (uint32_t)v;
}

/* ---- presentation -------------------------------------------------------
 * Move the GL render target to the display's origin so the virtual display and
 * the real screen share a coordinate system. MOVE ONLY: a resize is the exact
 * operation that panics (measured), so a window whose content is not already
 * the requested size is left alone. Returns the origin the window ACTUALLY has
 * afterwards, which is what gets reported as the virtual display's origin. */
static void present_window(uint32_t id, int w, int h, double *ox, double *oy)
{
   CGRect phys = CGDisplayBounds(id);
   *ox = phys.origin.x;
   *oy = phys.origin.y;

   void *win = NULL, *ctx = NULL;
   if (!agl_gl_render_target(&win, &ctx) || !win) {
      return;
   }
   static win_getbounds_fn getb; static win_setbounds_fn setb;
   if (!getb) getb = (win_getbounds_fn)dlsym(RTLD_DEFAULT, "GetWindowBounds");
   if (!setb) setb = (win_setbounds_fn)dlsym(RTLD_DEFAULT, "SetWindowBounds");
   if (!getb) { return; }

   CarbonRect r = { 0, 0, 0, 0 };
   if (getb(win, kWinContentRgn, &r) != 0) { return; }
   const int cw = r.right - r.left, ch = r.bottom - r.top;

   if (cw == w && ch == h && setb) {
      CarbonRect t = { (int16_t)*oy, (int16_t)*ox,
                       (int16_t)(*oy + ch), (int16_t)(*ox + cw) };
      (void)setb(win, kWinContentRgn, &t);
      (void)getb(win, kWinContentRgn, &r);        /* believe the window, not us */
   }
   *ox = r.left;
   *oy = r.top;
}

/* ==== entry points ======================================================== */

/* CGError CGDisplaySwitchToMode(CGDirectDisplayID, CFDictionaryRef) */
uint32_t shim_CGDisplaySwitchToMode(uint32_t *a)
{
   const uint32_t id = a[0];
   if (bridge_off())
      return (uint32_t)CGDisplaySwitchToMode(id,
                (CFDictionaryRef)(uintptr_t)_86x64_unwrap_obj_arg(a[1]));

   CFDictionaryRef req = (CFDictionaryRef)(uintptr_t)_86x64_unwrap_obj_arg(a[1]);
   if (!req || CFGetTypeID(req) != CFDictionaryGetTypeID()) {
      return 0;                                   /* never reconfigure a display */
   }
   const int w   = dict_int(req, K_W, 0);
   const int h   = dict_int(req, K_H, 0);
   const int bpp = dict_int(req, K_BPP, 32);
   const double rr = dict_dbl(req, K_RR, 0.0);
   if (w <= 0 || h <= 0) {
      return 0;
   }

   /* Is this the app putting the display BACK? Compare against the physical
    * mode, which is never virtualised. */
   CFDictionaryRef phys = CGDisplayCurrentMode(id);
   const int restore = phys && dict_int(phys, K_W, -1) == w &&
                               dict_int(phys, K_H, -1) == h;

   os_unfair_lock_lock(&g_lk);
   int s = slot_for(id);
   if (s >= 0) {
      if (restore) {
         g_disp[s].installed = 0;
         g_disp[s].mode = NULL;
      } else {
         g_disp[s].installed = 1;
         g_disp[s].w = w; g_disp[s].h = h; g_disp[s].bpp = bpp;
         g_disp[s].refresh = rr;
      }
   }
   os_unfair_lock_unlock(&g_lk);

   if (restore) {
      return 0;
   }

   CFDictionaryRef m = synth_mode(id, w, h, bpp, rr > 0 ? rr : dict_dbl(phys, K_RR, 60.0));
   double ox = 0, oy = 0;
   present_window(id, w, h, &ox, &oy);

   os_unfair_lock_lock(&g_lk);
   s = slot_for(id);
   if (s >= 0) { g_disp[s].mode = m; g_disp[s].bx = ox; g_disp[s].by = oy; }
   os_unfair_lock_unlock(&g_lk);

   return 0;                                      /* kCGErrorSuccess */
}

/* CFDictionaryRef CGDisplayCurrentMode(CGDirectDisplayID) */
uint32_t shim_CGDisplayCurrentMode(uint32_t *a)
{
   const uint32_t id = a[0];
   if (!bridge_off()) {
      CFDictionaryRef m = NULL;
      os_unfair_lock_lock(&g_lk);
      for (int i = 0; i < MAX_DISP; i++)
         if (g_disp[i].used && g_disp[i].id == id && g_disp[i].installed)
            { m = g_disp[i].mode; break; }
      os_unfair_lock_unlock(&g_lk);
      if (m) { return ref_out(m); }
   }
   return ref_out(CGDisplayCurrentMode(id));
}

/* CFDictionaryRef CGDisplayBestModeForParameters(id, bpp, w, h, boolean_t *) */
static uint32_t best_mode(uint32_t id, int bpp, int w, int h, double rr,
                          uint32_t exact_ptr, int have_rr)
{
   boolean_t *exact = (boolean_t *)(uintptr_t)exact_ptr;
   if (bridge_off()) {
      CFDictionaryRef n = have_rr
         ? CGDisplayBestModeForParametersAndRefreshRate(id, bpp, w, h, rr, exact)
         : CGDisplayBestModeForParameters(id, bpp, w, h, exact);
      return ref_out(n);
   }
   if (w <= 0 || h <= 0) {                        /* Rule A: leave it DEFINED */
      if (exact) *exact = 0;
      return 0;
   }
   if (!have_rr || rr <= 0)
      rr = dict_dbl(CGDisplayCurrentMode(id), K_RR, 60.0);
   /* ★NATIVE-GEOMETRY POLICY (opt-in: M64_FULLSCREEN_NATIVE_MODE=1).
    *
    * Synthesising the app's REQUESTED geometry keeps the app coherent with
    * itself, but it makes the app then RESIZE its window to that geometry — and
    * a resize is unsurvivable here. MEASURED on this machine (probes/
    * displayswitch.m `carbonmove`, and a fresh bisection 2026-08-20):
    *     resize once, BEFORE the window is ever shown ......... SURVIVES
    *     resize twice before showing .......................... SIGILL
    *     resize after showing / hidden / orderOut / SizeWindow
    *       / structure region / resizing the Cocoa mirror ..... SIGILL
    * Only the FIRST bounds change is survivable; after it the window owns a CGS
    * context and NSCGSPanic is unconditional. Halo asks for five. So no policy
    * that lets the resizes through can work, and suppressing them leaves the app
    * rendering at a size the window does not have (measured: a misaligned menu).
    *
    * The remaining move is to stop the app WANTING a different size: answer with
    * the display's NATIVE geometry, which is the size its fullscreen window
    * already has, so the bounds it then asks for match what the window is and no
    * resize is ever requested. The app stays coherent — it renders at what it
    * believes the screen is, and that belief is now TRUE — and it gets the
    * native resolution rather than a 640x480 upscale.
    *
    * ⛔HARMFUL — DO NOT USE. MEASURED (2026-08-22): a run with this flag
    * set leaves the app's SAVED PREFERENCES corrupt, and every LATER launch —
    * with or without the flag — comes up with an unresponsive graphics-settings
    * dialog until com.macsoft.halo is deleted. The mechanism is obvious in
    * hindsight: the app PERSISTS the graphics configuration it believes it
    * selected, so lying about the mode does not just change this run, it writes
    * a mode the app cannot read back. That also retro-explains the "unclickable
    * dialog" I spent a long time blaming on a core-translator change: the first
    * native-mode run poisoned the prefs, and every run after it inherited them.
    *
    * A shim may lie about what the SYSTEM reports; it must not cause the app to
    * PERSIST a lie. Anything an app writes to disk outlives the process and the
    * experiment, which makes this a different and much worse class of change
    * than a per-run override. Kept only as a documented dead end so it is not
    * reinvented.
    *
    * Opt-in because it overrides the user's resolution choice, which is a real
    * behaviour change and not mine to make silently. */
   static int native_policy = -1;
   if (native_policy < 0) {
      native_policy = getenv("M64_FULLSCREEN_NATIVE_MODE") ? 1 : 0;
      if (native_policy) {
         fprintf(stderr, "[fullscreen] ⚠M64_FULLSCREEN_NATIVE_MODE is a KNOWN-BAD "
                         "dead end: it makes the app persist a graphics config it "
                         "cannot read back, so LATER launches break until prefs "
                         "are deleted.\n");
      }
   }
   if (native_policy) {
      CGRect nb = CGDisplayBounds(id);
      const int nw = (int)nb.size.width, nh = (int)nb.size.height;
      if (nw > 0 && nh > 0 && (nw != w || nh != h)) {
         w = nw; h = nh;
      }
   }
   CFDictionaryRef m = synth_mode(id, w, h, bpp > 0 ? bpp : 32, rr);
   if (exact) *exact = 1;
   return ref_out(m);
}

uint32_t shim_CGDisplayBestModeForParameters(uint32_t *a)
{
   return best_mode(a[0], (int)a[1], (int)a[2], (int)a[3], 0.0, a[4], 0);
}

uint32_t shim_CGDisplayBestModeForParametersAndRefreshRate(uint32_t *a)
{
   double rr;
   memcpy(&rr, &a[4], sizeof rr);                 /* an i386 double = 2 slots */
   return best_mode(a[0], (int)a[1], (int)a[2], (int)a[3], rr, a[6], 1);
}

/* CGRect CGDisplayBounds(CGDirectDisplayID) — i386 struct return: a[0] is the
 * hidden result pointer (4 CGFloats = 4 floats) which the callee pops. */
uint32_t shim_CGDisplayBounds(uint32_t *a)
{
   float *out = (float *)(uintptr_t)a[0];
   const uint32_t id = a[1];
   CGRect r = CGDisplayBounds(id);
   if (!bridge_off()) {
      os_unfair_lock_lock(&g_lk);
      for (int i = 0; i < MAX_DISP; i++)
         if (g_disp[i].used && g_disp[i].id == id && g_disp[i].installed) {
            r = CGRectMake(g_disp[i].bx, g_disp[i].by, g_disp[i].w, g_disp[i].h);
            break;
         }
      os_unfair_lock_unlock(&g_lk);
   }
   if (out) {
      out[0] = (float)r.origin.x;   out[1] = (float)r.origin.y;
      out[2] = (float)r.size.width; out[3] = (float)r.size.height;
   }
   return a[0];
}

/* Caller must NOT hold g_lk. Returns 1 and the virtual geometry when installed. */
static int virt_geom(uint32_t id, int *w, int *h, int *bpp)
{
   int found = 0;
   if (bridge_off()) return 0;
   os_unfair_lock_lock(&g_lk);
   for (int i = 0; i < MAX_DISP; i++)
      if (g_disp[i].used && g_disp[i].id == id && g_disp[i].installed) {
         if (w) *w = g_disp[i].w;
         if (h) *h = g_disp[i].h;
         if (bpp) *bpp = g_disp[i].bpp;
         found = 1;
         break;
      }
   os_unfair_lock_unlock(&g_lk);
   return found;
}

uint32_t shim_CGDisplayPixelsWide(uint32_t *a)
{
   int w = 0;
   if (virt_geom(a[0], &w, NULL, NULL)) { return (uint32_t)w; }
   return (uint32_t)CGDisplayPixelsWide(a[0]);
}

uint32_t shim_CGDisplayPixelsHigh(uint32_t *a)
{
   int h = 0;
   if (virt_geom(a[0], NULL, &h, NULL)) { return (uint32_t)h; }
   return (uint32_t)CGDisplayPixelsHigh(a[0]);
}

/* CGDisplayBitsPerPixel is gone from the modern SDK entirely (the legacy abigen
 * pass used to forward it into nothing), so it is always answered from a mode
 * dictionary — the virtual one when installed, the real one otherwise. */
uint32_t shim_CGDisplayBitsPerPixel(uint32_t *a)
{
   int bpp = 0;
   if (virt_geom(a[0], NULL, NULL, &bpp)) return (uint32_t)bpp;
   return (uint32_t)dict_int(CGDisplayCurrentMode(a[0]), K_BPP, 32);
}

/* ---- the capture family: tracked, never performed ------------------------
 * ★OBSERVED ON THE LIVE MACHINE: a real CGCaptureAllDisplays makes the WHOLE
 * display SLOWLY FADE TO BLACK and stay black for as long as the capture is
 * held — the user only got their screen back because the process DIED and the
 * kernel released it implicitly. So a bridge that neutralised only the mode
 * switch and let the capture through would leave the user staring at a black
 * screen with no crash to rescue them. Owning the capture half is therefore a
 * SAFETY requirement, not just a correctness one.
 *
 * The armed path never captures, so there is nothing to leak. The DISARMED
 * path (kill switch) does forward to the real thing, and that is exactly the
 * configuration an A/B runs — so register a one-shot atexit release the first
 * time a real capture is forwarded. It costs nothing and it means no arm of any
 * experiment can end with a black screen on a normal exit. */
static void release_on_exit(void) { CGReleaseAllDisplays(); }
static void arm_capture_safety_net(void)
{
   static int armed;
   if (!armed) { armed = 1; atexit(release_on_exit); }
}

static uint32_t capture_all(void)
{
   if (bridge_off()) { arm_capture_safety_net(); return (uint32_t)CGCaptureAllDisplays(); }
   os_unfair_lock_lock(&g_lk);
   g_all_captured = 1;
   for (int i = 0; i < MAX_DISP; i++) if (g_disp[i].used) g_disp[i].captured = 1;
   os_unfair_lock_unlock(&g_lk);
   return 0;
}

uint32_t shim_CGCaptureAllDisplays(uint32_t *a) { (void)a; return capture_all(); }

uint32_t shim_CGCaptureAllDisplaysWithOptions(uint32_t *a)
{
   if (bridge_off()) { arm_capture_safety_net(); return (uint32_t)CGCaptureAllDisplaysWithOptions(a[0]); }
   return capture_all();
}

uint32_t shim_CGDisplayCapture(uint32_t *a)
{
   if (bridge_off()) { arm_capture_safety_net(); return (uint32_t)CGDisplayCapture(a[0]); }
   os_unfair_lock_lock(&g_lk);
   int s = slot_for(a[0]);
   if (s >= 0) g_disp[s].captured = 1;
   os_unfair_lock_unlock(&g_lk);
   return 0;
}

uint32_t shim_CGDisplayCaptureWithOptions(uint32_t *a)
{
   if (bridge_off()) { arm_capture_safety_net(); return (uint32_t)CGDisplayCaptureWithOptions(a[0], a[1]); }
   return shim_CGDisplayCapture(a);
}

/* Releasing a captured display is documented to restore its original mode, and
 * apps rely on that at exit — so this drops the virtual mode too. */
uint32_t shim_CGReleaseAllDisplays(uint32_t *a)
{
   (void)a;
   if (bridge_off()) return (uint32_t)CGReleaseAllDisplays();
   os_unfair_lock_lock(&g_lk);
   g_all_captured = 0;
   for (int i = 0; i < MAX_DISP; i++)
      if (g_disp[i].used) { g_disp[i].captured = 0; g_disp[i].installed = 0;
                            g_disp[i].mode = NULL; }
   os_unfair_lock_unlock(&g_lk);
   return 0;
}

uint32_t shim_CGDisplayRelease(uint32_t *a)
{
   if (bridge_off()) return (uint32_t)CGDisplayRelease(a[0]);
   os_unfair_lock_lock(&g_lk);
   int s = slot_for(a[0]);
   if (s >= 0) { g_disp[s].captured = 0; g_disp[s].installed = 0; g_disp[s].mode = NULL; }
   os_unfair_lock_unlock(&g_lk);
   return 0;
}

uint32_t shim_CGDisplayIsCaptured(uint32_t *a)
{
   if (bridge_off()) return (uint32_t)CGDisplayIsCaptured(a[0]);
   uint32_t cap = 0;
   os_unfair_lock_lock(&g_lk);
   cap = g_all_captured ? 1 : 0;
   if (!cap)
      for (int i = 0; i < MAX_DISP; i++)
         if (g_disp[i].used && g_disp[i].id == a[0] && g_disp[i].captured) { cap = 1; break; }
   os_unfair_lock_unlock(&g_lk);
   return cap;
}
