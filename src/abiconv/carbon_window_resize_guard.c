/*
 * carbon_window_resize_guard.c — ONE job: keep a classic app's SetWindowBounds
 * from RESIZING a Carbon window that has ALREADY BEEN SHOWN, because on modern
 * macOS that operation is fatal.
 *
 * THE MEASURED RULE (probes/resizeways.m, re-measured 2026-08-22 after a bug in
 * the probe's own mode selection invalidated the first reading):
 *
 *                        | pre-show          | post-show
 *     ------------------ | ----------------- | ------------------
 *     size change        | SURVIVES (any n)  | NSCGSPanic (SIGILL)
 *     pure move          | survives          | survives
 *
 * The predicate is "has this window EVER been shown", not "is it visible now"
 * and NOT "is this the first bounds change": hiding the window first still
 * panics, while EIGHT consecutive pre-show resizes are all fine. The earlier
 * belief that "only the FIRST bounds change survives" was an artifact of the
 * probe showing the window before the modes under test ran.
 *
 * Modern AppKit cannot create a CGS context for an NSCarbonWindow, and a RESIZE
 * is what forces one:
 *     NSCarbonWindowHandleEvent -> handleCarbonBoundsChange -> _setFrameCommon
 *       -> _cgsSizeWindow -> [NSCGSWindow setSize:] -> _updateLayer
 *       -> _createRootLayerAndContextIfNeeded -> _createContext -> NSCGSPanic
 * NSCGSPanic is a deliberate trap, so the process dies with SIGILL and no
 * exception to catch. Two of the tester's 2026-08-22 crash reports are exactly this
 * stack.
 *
 * HOW WE TELL. The AppKit mirror IS the oracle: no NSCarbonWindow exists for a
 * WindowRef until its first show, and it PERSISTS across hide -- which is
 * precisely the fatal/safe split. It is causal rather than correlated, because
 * the panic path runs THROUGH that mirror: no mirror, no _setFrameCommon, no
 * _createContext. A window is matched to its mirror by comparing
 * HIWindowGetCGWindowID(win) against each NSWindow's -windowNumber (verified
 * equal, and verified to discriminate correctly between a shown and an unshown
 * window in the same process).
 *
 * WHAT THIS DOES
 *   never shown  -> pass the bounds change through UNCHANGED (it is free, and
 *                   this is where the app's real geometry gets established).
 *   ever shown   -> honour the requested ORIGIN, keep the CURRENT size, return
 *                   noErr so the app's own logic proceeds.
 *
 * ⚠The earlier version of this file suppressed EVERY size change, including the
 * pre-show ones that are perfectly safe -- which is why enabling it produced
 * The tester's "menu screen broken, misaligned": the app's chosen window size was
 * being discarded at the one moment it could legally have been applied.
 *
 * Suppressing a post-show resize is still a compromise, not a cure: the app
 * keeps rendering at the resolution it chose while the window keeps the size it
 * had. The alternative there is not "a correctly resized window", it is a dead
 * process -- a resize CANNOT be performed on a shown Carbon window on this OS.
 *
 * Universal: triggers on the operation and on the window's own state, never on
 * an app name.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <objc/runtime.h>
#include <objc/message.h>

#include "carbon_shim.h"

typedef struct { int16_t top, left, bottom, right; } WRGRect;
typedef int32_t (*wrg_bounds_fn)(void *, uint32_t, const WRGRect *);
typedef int32_t (*wrg_getb_fn)(void *, uint32_t, WRGRect *);

#define WRG_CONTENT_RGN 33   /* kWindowContentRgn */

/* DEFAULT ON. The old default-off existed because the guard threw away safe
 * pre-show resizes too and produced a misaligned menu; now that it only
 * suppresses the genuinely fatal post-show ones, leaving it off just means a
 * guaranteed SIGILL. M64_NO_WINDOW_RESIZE_GUARD=1 disables it (A/B kill
 * switch); M64_WINDOW_RESIZE_GUARD is still accepted and now redundant. */
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

/* ------------------------------------------------------------------------
 * The oracle: has this WindowRef ever been shown?
 *
 * An NSCarbonWindow mirror is minted at the window's FIRST show and survives
 * every later hide, so "a mirror exists" == "showing already happened" ==
 * "resizing this window traps". We read the AppKit side through the ObjC
 * runtime rather than linking it, and we read the NSApp GLOBAL rather than
 * calling +sharedApplication so that merely asking the question can never
 * instantiate an application object as a side effect.
 * ------------------------------------------------------------------------ */
typedef uint32_t WRGCGWindowID;
typedef WRGCGWindowID (*wrg_cgid_fn)(void *);

static int wrg_ever_shown(void *win) {
   static wrg_cgid_fn cgid_of;
   static int resolved;
   if (!resolved) {
      resolved = 1;
      cgid_of = (wrg_cgid_fn)dlsym(RTLD_DEFAULT, "HIWindowGetCGWindowID");
   }
   if (!cgid_of) return 1;            /* cannot tell -> assume the fatal case */
   const WRGCGWindowID want = cgid_of(win);
   if (!want) return 0;               /* no CGS window at all: nothing shown */

   id *nsapp_slot = (id *)dlsym(RTLD_DEFAULT, "NSApp");
   id app = nsapp_slot ? *nsapp_slot : NULL;
   if (!app) return 0;                /* no AppKit app yet -> no mirrors yet */

   id arr = ((id (*)(id, SEL))objc_msgSend)(app, sel_registerName("windows"));
   if (!arr) return 0;
   const long n = ((long (*)(id, SEL))objc_msgSend)(arr, sel_registerName("count"));
   SEL at = sel_registerName("objectAtIndex:");
   SEL num = sel_registerName("windowNumber");
   for (long i = 0; i < n; i++) {
      id w = ((id (*)(id, SEL, long))objc_msgSend)(arr, at, i);
      if (!w) continue;
      if ((WRGCGWindowID)((long (*)(id, SEL))objc_msgSend)(w, num) == want) return 1;
   }
   return 0;
}

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

   /* Never shown: the bounds change is free, and this is the ONLY moment the
    * app can legally establish its real geometry. Do not touch it. */
   if (!wrg_ever_shown(win)) {
      const int32_t st0 = setb(win, region, want);
      if (wrg_trace()) {
         fprintf(stderr, "[winresize] pre-show bounds -> %dx%d at %d,%d PASSED "
                         "THROUGH (window never shown), st=%d\n",
                 want->right - want->left, want->bottom - want->top,
                 want->left, want->top, (int)st0);
      }
      return (uint32_t)st0;
   }

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
      fprintf(stderr, "[winresize] POST-SHOW resize %dx%d -> %dx%d SUPPRESSED "
                      "(would NSCGSPanic); moved to %d,%d instead, st=%d\n",
              cw, ch, ww, wh, moved.left, moved.top, (int)st);
   }
   return 0;   /* report success: the app's own logic must continue */
}
