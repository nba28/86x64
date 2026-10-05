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
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <signal.h>

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

extern uint32_t qd_current_port(void);          // qd_gworld.c
extern void    *qd_port_window(uint32_t port_h);  // qd_gworld.c

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
int ci_port_content_origin(uint32_t port, int16_t *ox, int16_t *oy);
int ci_content_origin(int16_t *ox, int16_t *oy) {
   return ci_port_content_origin(qd_current_port(), ox, oy);
}

// The same, against an explicit port (QDGlobalToLocalPoint & co. name theirs).
int ci_port_content_origin(uint32_t port, int16_t *ox, int16_t *oy) {
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

   /* The CURRENT PORT's window first: GlobalToLocal, LocalToGlobal and GetMouse
    * are defined against the port the app SetPort()'d, not against whichever
    * window happens to be active -- an app can leave a hidden window active
    * (PvZ keeps its fullscreen-era one; ActiveNonFloatingWindow read NULL or
    * another window in probes/appdownspy.m runs). Active/front window only when the port
    * is not a window (an offscreen GWorld, or none set on this thread). */
   CIWindowRef w = (CIWindowRef)qd_port_window(port);
   if (!w && active_win) { w = active_win(); }
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

/*
 * Button() and GetCurrentEventButtonState() — THE OTHER HALF of the classic
 * click idiom.
 *
 * This file already fixed StillDown/WaitMouseUp, and its own header states the
 * idiom they belong to:
 *
 *     if (Button()) { while (StillDown()) { track } }   // ... then act
 *
 * but only the SECOND half was ever fixed. Button() and its Carbon successor
 * GetCurrentEventButtonState() were left as plain abigen ABI bridges straight
 * to HIToolbox — they were never hand-shimmed, unlike every other classic input
 * entry point (GetMouse, GetGlobalMouse, GetKeys, GlobalToLocal, LocalToGlobal,
 * StillDown, WaitMouseUp all are). And a permanently-false Button() defeats the
 * idiom just as completely as a permanently-false StillDown: the guard never
 * opens, the tracking loop never runs, nothing is ever clicked. The pointer
 * still moves and menu items still HIGHLIGHT, because highlighting is driven by
 * GetMouse polling, which works. That is exactly the reported symptom.
 *
 * The native calls are still asked FIRST and their answer is kept: this ORs in
 * the physical button state rather than replacing it, so a context where
 * HIToolbox answers correctly is completely unaffected. ABICONV_INPUT_TRACE=1
 * prints both answers side by side, so a run says which one actually carried
 * the click instead of leaving it to inference.
 *
 * HONEST LIMIT, inherited from ci_button_is_down and unchanged here: the
 * CombinedSession source reports the PHYSICAL button globally, not "a click
 * delivered to this app". For a full-screen game — the case this serves — those
 * coincide. It is the same trade StillDown already makes; this does not widen
 * it, and the kill switch M64_NO_CLASSIC_INPUT_FIX=1 disables both together.
 */
static int ci_trace(void) {
   static int t = -1;
   if (t < 0) { t = getenv("ABICONV_INPUT_TRACE") != NULL; }
   return t;
}

/* Bounded, legible trace: the first few calls (did the app poll at all?), then
 * only STATE CHANGES (each press and release, once). A button polled every
 * frame would otherwise bury the one line that matters under thousands of
 * identical ones, and a trace nobody can read is a trace nobody reads. */
/* ★Totals matter as much as the lines. The first run of this trace printed 5
 * calls and then only state CHANGES, which made a heavily-polled call look like
 * a handful of startup calls — and that mis-reading nearly retired
 * GetCurrentEventButtonState as "not the path". Counts are reported at exit so
 * "polled 40000 times, never once down" is distinguishable from "called 5
 * times at startup". */
static unsigned long ci_total[2], ci_downs[2];

static void ci_summary(void) {
   if (!ci_trace()) { return; }
   fprintf(stderr, "[input] ===== summary =====\n");
   fprintf(stderr, "[input] Button                     calls=%lu  ever-down=%lu\n",
           ci_total[0], ci_downs[0]);
   fprintf(stderr, "[input] GetCurrentEventButtonState calls=%lu  ever-down=%lu\n",
           ci_total[1], ci_downs[1]);
   if (ci_total[1] && !ci_downs[1]) {
      fprintf(stderr, "[input] ⚠polled but NEVER observed down — either the fix "
              "is disabled (M64_NO_CLASSIC_INPUT_FIX) or no click landed\n");
   }
}

/* ⚠CHAIN, do not replace. Two separate diagnostics both install a SIGTERM
 * handler; whichever registers last wins and the other's summary is silently
 * lost - which is exactly what happened to the event probe's counters on the
 * run before this one. Keep the previous handler and call it. */
static void (*ci_prev_term)(int);
static void (*ci_prev_int)(int);
static void ci_sig(int sig) {
   ci_summary();
   void (*prev)(int) = (sig == SIGINT) ? ci_prev_int : ci_prev_term;
   if (prev && prev != SIG_DFL && prev != SIG_IGN) { prev(sig); }
   signal(sig, SIG_DFL);
   raise(sig);
}

static void ci_report(const char *who, uint32_t nat, uint32_t cg) {
   if (!ci_trace()) { return; }
   static int reg;
   if (!reg) {
      reg = 1;
      atexit(ci_summary);
      ci_prev_term = signal(SIGTERM, ci_sig);
      ci_prev_int  = signal(SIGINT, ci_sig);
   }
   static unsigned long ncalls[2];
   static uint32_t last[2] = { 0xffffffffu, 0xffffffffu };
   const int slot = (who[0] == 'B') ? 0 : 1;
   const uint32_t now = nat | cg;
   const unsigned long n = ++ncalls[slot];
   ci_total[slot]++;
   if (now) { ci_downs[slot]++; }
   const int changed = (now != last[slot]);
   last[slot] = now;
   if (n > 5 && !changed) { return; }
   fprintf(stderr, "[input] %-26s call#%-6lu native=0x%x cg=0x%x -> 0x%x%s\n",
           who, n, nat, cg, now,
           (cg && !(nat & 1))
              ? "   <-- PHYSICAL BUTTON DOWN, HIToolbox did NOT report it"
              : "");
   if (n == 5) {
      fprintf(stderr, "[input]   (further %s calls reported only on CHANGE)\n", who);
   }
}

typedef unsigned char (*button_fn)(void);
typedef uint32_t (*gcebs_fn)(void);

uint32_t shim_Button(uint32_t *args) {
   (void)args;
   static button_fn nat = NULL;
   static int looked = 0;
   if (!looked) { looked = 1; nat = (button_fn)dlsym(RTLD_DEFAULT, "Button"); }
   const int n = nat ? (nat() ? 1 : 0) : 0;
   const int c = ci_button_is_down();
   ci_report("Button", (uint32_t)n, (uint32_t)c);
   return (uint32_t)(n | c);
}

uint32_t shim_GetCurrentEventButtonState(uint32_t *args) {
   (void)args;
   static gcebs_fn nat = NULL;
   static int looked = 0;
   if (!looked) {
      looked = 1;
      nat = (gcebs_fn)dlsym(RTLD_DEFAULT, "GetCurrentEventButtonState");
   }
   const uint32_t n = nat ? nat() : 0u;
   const uint32_t c = ci_button_is_down() ? 1u : 0u;   /* bit 0 = button 1 */
   ci_report("GetCurrentEventButtonState", n, c);
   return n | c;
}

