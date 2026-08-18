/*
 * carbon_window_resize_guard.c — ONE job: keep a classic app's SetWindowBounds
 * from RESIZING a Carbon window, because on modern macOS that operation is
 * fatal.
 *
 * THE MEASURED FACT (bisected on the live machine, native x86_64, recorded in
 * cg_display_fullscreen_shim.c and reproduced by probes/displayswitch.m):
 *
 *     no window / never-shown window / window disposed across it -> survives
 *     visible Carbon window ................................... PANICS
 *     hidden across the switch ................................ PANICS
 *     switched with the modern CGDisplaySetDisplayMode ........ PANICS
 *     SetWindowBounds that RESIZES, no display API at all ..... PANICS
 *     SetWindowBounds that only MOVES (size unchanged) ........ survives
 *
 * Modern AppKit cannot create a CGS context for an NSCarbonWindow, and a RESIZE
 * is precisely what forces one:
 *     NSCarbonWindowHandleEvent -> handleCarbonBoundsChange -> _setFrameCommon
 *       -> _cgsSizeWindow -> [NSCGSWindow setSize:] -> _updateLayer
 *       -> _createRootLayerAndContextIfNeeded -> _createContext -> NSCGSPanic
 * NSCGSPanic is a deliberate trap, so the process dies with SIGILL and no
 * exception to catch.
 *
 * WHY THIS EXISTS SEPARATELY. cg_display_fullscreen_shim.c already knows the
 * rule and is scrupulous about it — its present_window() only ever MOVES. But
 * that only governs the calls WE make. An app that calls SetWindowBounds itself
 * still reaches the real one through the ordinary bridge and still dies. Halo
 * does exactly that after its graphics-settings dialog: it imports
 * SetWindowBounds alongside CGCaptureAllDisplays / CGDisplaySwitchToMode /
 * aglSetFullScreen, and resizes its window to the chosen resolution.
 *
 * WHAT THIS DOES: honour the MOVE, drop the SIZE. The requested rectangle's
 * origin is applied; its width and height are replaced with the window's
 * current ones, and noErr is returned so the app's own logic proceeds. The
 * app's drawing is driven by its GL context and its own idea of the resolution
 * (kept coherent by the virtual display mode in cg_display_fullscreen_shim.c),
 * not by the Carbon window's pixel size.
 *
 * This is not a no-op on live surface: the alternative is not "a correctly
 * resized window", it is a dead process. A resize CANNOT be performed on this
 * OS — that is the measured bisection above, not a preference.
 *
 * Universal: triggers on the operation (a bounds change whose size differs),
 * never on an app name. Kill switch M64_NO_WINDOW_RESIZE_GUARD=1 restores the
 * real resize — the OFF arm, which genuinely reproduces the panic.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "carbon_shim.h"

typedef struct { int16_t top, left, bottom, right; } WRGRect;
typedef int32_t (*wrg_bounds_fn)(void *, uint32_t, const WRGRect *);
typedef int32_t (*wrg_getb_fn)(void *, uint32_t, WRGRect *);

#define WRG_CONTENT_RGN 33   /* kWindowContentRgn */

static int wrg_off(void) {
   static int t = -1;
   if (t < 0) { t = getenv("M64_NO_WINDOW_RESIZE_GUARD") ? 1 : 0; }
   return t;
}
static int wrg_trace(void) {
   static int t = -1;
   if (t < 0) { t = getenv("ABICONV_WINRESIZE_TRACE") ? 1 : 0; }
   return t;
}

extern uint64_t x64_objc_unwrap(uint32_t h);

/* OSStatus SetWindowBounds(WindowRef, WindowRegionCode, const Rect *) */
uint32_t shim_SetWindowBounds(uint32_t *args) {
   void          *win    = (void *)(uintptr_t)x64_objc_unwrap(args[0]);
   const uint32_t region = args[1];
   const WRGRect *want   = (const WRGRect *)i386_ptr(args[2]);

   static wrg_bounds_fn setb;
   static wrg_getb_fn   getb;
   if (!setb) { setb = (wrg_bounds_fn)dlsym(RTLD_DEFAULT, "SetWindowBounds"); }
   if (!getb) { getb = (wrg_getb_fn)  dlsym(RTLD_DEFAULT, "GetWindowBounds"); }
   if (!setb || !win || !want) { return (uint32_t)-50; /* paramErr */ }

   if (wrg_off()) { return (uint32_t)setb(win, region, want); }

   WRGRect cur = { 0, 0, 0, 0 };
   if (!getb || getb(win, region, &cur) != 0) {
      /* Cannot establish the current size, so cannot prove this is a pure move.
       * Refuse the operation rather than risk the panic: a window that did not
       * move is recoverable, a trapped process is not. */
      if (wrg_trace()) {
         fprintf(stderr, "[winresize] GetWindowBounds failed; bounds change "
                         "SUPPRESSED (cannot prove it is a pure move)\n");
      }
      return 0;
   }

   const int cw = cur.right - cur.left,  ch = cur.bottom - cur.top;
   const int ww = want->right - want->left, wh = want->bottom - want->top;

   if (cw == ww && ch == wh) {
      return (uint32_t)setb(win, region, want);      /* pure move: safe */
   }

   /* Keep the requested ORIGIN, keep the CURRENT size. */
   const WRGRect moved = { want->top, want->left,
                           (int16_t)(want->top  + ch),
                           (int16_t)(want->left + cw) };
   const int32_t st = setb(win, region, &moved);
   if (wrg_trace()) {
      fprintf(stderr, "[winresize] resize %dx%d -> %dx%d SUPPRESSED (would "
                      "NSCGSPanic); moved to %d,%d instead, st=%d\n",
              cw, ch, ww, wh, moved.left, moved.top, (int)st);
   }
   return 0;   /* report success: the app's own logic must continue */
}
