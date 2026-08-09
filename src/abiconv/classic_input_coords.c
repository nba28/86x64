// classic_input_coords.c — the shared substrate for classic INPUT geometry and
// mouse-button state.
//
// One job: answer two questions that several classic input entry points all
// need, and that were previously answered by stubs in three different files:
//
//   1. where is the current window's content origin, in global coordinates?
//      (GetMouse, GlobalToLocal and LocalToGlobal all need it)
//   2. is a mouse button physically down right now?
//      (StillDown and WaitMouseUp need it)
//
// WHY THIS EXISTS. Classic Mac OS draws a hard line between GLOBAL (screen)
// and LOCAL (current GrafPort) coordinates:
//
//   GetGlobalMouse(&p)  -> screen coordinates
//   GetMouse(&p)        -> coordinates LOCAL TO THE CURRENT PORT
//   GlobalToLocal(&p)   -> subtract the port origin
//   LocalToGlobal(&p)   -> add it back
//
// Our previous implementations returned the GLOBAL position from BOTH mouse
// calls and made both conversions identity no-ops. That is correct only while
// the port origin is (0,0) — true for the offscreen GWorld case those stubs
// were written for, and FALSE for a real on-screen window, which is inset by
// its title bar and by wherever the user put it. An app that hit-tests its own
// UI with GetMouse (every classic game menu does) then compares a screen point
// against window-local item rectangles and concludes the pointer is outside
// every item. Symptom: the cursor moves perfectly and NOTHING is clickable.
//
// Likewise StillDown() and WaitMouseUp() returned a constant 0 ("button is not
// down"), swept in with a block of genuinely-dead List Manager no-ops. They are
// not dead surface: the classic click idiom is
//     if (Button()) { while (StillDown()) { track } }   // ... then act
// and with StillDown permanently false a press-track-release collapses.
//
// SCOPE / HONEST LIMIT. Classic GetMouse is defined against the CURRENT
// GRAFPORT, which is not necessarily the front window. We have no live
// port->window registry, so we use the front (active, non-floating) window's
// content region as the port origin. For a single-window app — every full
// screen classic game, and the case that matters here — those coincide. When
// there is no such window, or the Window Manager cannot be reached, we fall
// back to an origin of (0,0), which is exactly the previous behaviour: local
// == global. So this is strictly an improvement on the old stub and degrades
// to it rather than to something new.
//
// Kill switch M64_NO_CLASSIC_INPUT_FIX=1 forces the old behaviour (origin
// always (0,0), button state always 0) so the defect can be reproduced on
// demand and the two-arm guards have a real OFF arm.

#include <stdint.h>
#include <stdlib.h>
#include <dlfcn.h>

// CoreGraphics event-source query, declared locally to keep this file
// header-light (it resolves from the x86_64 shared cache like the rest of the
// CG surface we call).
extern int CGEventSourceButtonState(int32_t stateID, uint32_t button);

#define CI_SOURCE_COMBINED_SESSION 0   // kCGEventSourceStateCombinedSessionState
#define CI_MOUSE_BUTTON_LEFT       0   // kCGMouseButtonLeft
#define CI_WINDOW_CONTENT_RGN      33  // kWindowContentRgn

typedef struct { int16_t top, left, bottom, right; } CIRect;
typedef void *CIWindowRef;

typedef CIWindowRef (*ci_frontwindow_fn)(void);
typedef int32_t     (*ci_getbounds_fn)(CIWindowRef, uint16_t, CIRect *);

int ci_input_fix_disabled(void) {
   static int t = -1;
   if (t < 0) { t = getenv("M64_NO_CLASSIC_INPUT_FIX") ? 1 : 0; }
   return t;
}

// The content origin of the window a classic app would be drawing into, in
// global coordinates. Returns 1 and writes *ox/*oy on success; returns 0 and
// leaves them untouched when there is no usable window (callers then treat the
// origin as (0,0), i.e. local == global, the documented fallback above).
//
// ActiveNonFloatingWindow is preferred over FrontWindow: a floating palette
// must not become the coordinate reference. Both are resolved lazily because
// neither is guaranteed present, and a missing Window Manager must degrade,
// never crash.
int ci_content_origin(int16_t *ox, int16_t *oy) {
   if (ci_input_fix_disabled()) { return 0; }

   static int resolved = 0;
   static ci_frontwindow_fn active_win = 0;
   static ci_frontwindow_fn front_win  = 0;
   static ci_getbounds_fn   get_bounds = 0;
   if (!resolved) {
      resolved    = 1;
      active_win  = (ci_frontwindow_fn)dlsym(RTLD_DEFAULT, "ActiveNonFloatingWindow");
      front_win   = (ci_frontwindow_fn)dlsym(RTLD_DEFAULT, "FrontWindow");
      get_bounds  = (ci_getbounds_fn)dlsym(RTLD_DEFAULT, "GetWindowBounds");
   }
   if (!get_bounds) { return 0; }

   CIWindowRef w = 0;
   if (active_win) { w = active_win(); }
   if (!w && front_win) { w = front_win(); }
   if (!w) { return 0; }

   CIRect r;
   r.top = r.left = r.bottom = r.right = 0;
   if (get_bounds(w, CI_WINDOW_CONTENT_RGN, &r) != 0) { return 0; }

   // A degenerate rect means the window exists but has no measurable content
   // region; treat that as "unknown" rather than trusting a zero origin.
   if (r.right <= r.left || r.bottom <= r.top) { return 0; }

   if (ox) { *ox = r.left; }
   if (oy) { *oy = r.top; }
   return 1;
}

// Is a mouse button physically down right now?
//
// Uses the COMBINED SESSION state rather than the HID state on purpose: the
// combined state reflects synthesised CGEvents as well as real hardware, so an
// automated click driven by CGEventPost is visible here. Querying only the HID
// layer would make every scripted GUI run read as "button never pressed" — the
// exact failure mode that makes an automated pass silently prove nothing.
int ci_button_is_down(void) {
   if (ci_input_fix_disabled()) { return 0; }
   return CGEventSourceButtonState(CI_SOURCE_COMBINED_SESSION,
                                   CI_MOUSE_BUTTON_LEFT) ? 1 : 0;
}
