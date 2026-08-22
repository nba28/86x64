/*
 * carbon_window_order_probe.c — ⚠DIAGNOSTIC ONLY. DELETE when the Halo window-
 * geometry task closes. Changes NO behaviour: every hook logs and then calls
 * straight through to the real function.
 *
 * WHY. A Carbon window cannot be resized once it has been shown (NSCGSPanic).
 * Halo shows its game window, runs its graphics-settings dialog, and only then
 * resizes to the chosen resolution -- so the resize must be suppressed, and the
 * app then renders its own resolution into a window of a different size (the
 * misaligned picture, and clicks off by the difference).
 *
 * The cure has to be "hold that window's first show until after its resize",
 * and the ONLY open question is where to put the hold point. A previous attempt
 * used "one main-runloop turn" and failed, because the resize arrives many
 * runloop turns later, after the dialog. This probe records the actual ordered
 * sequence -- per WindowRef -- of show / bounds / GL-attach / swap, so the hold
 * point can be CHOSEN rather than guessed again.
 *
 * Enable with ABICONV_WINORDER_TRACE=1 (off = these hooks are pure pass-through).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "carbon_shim.h"

extern uint64_t x64_objc_unwrap(uint32_t h);
extern int      carbon_window_ever_shown(void *win);

typedef void     (*wop_void_fn)(void *);
typedef int32_t  (*wop_getb_fn)(void *, uint32_t, void *);
typedef uint32_t (*wop_class_fn)(void *, uint32_t *);
typedef uint8_t  (*wop_agl_fn)(void *, void *);

static int wop_on(void) {
   static int t = -1;
   if (t < 0) { t = getenv("ABICONV_WINORDER_TRACE") ? 1 : 0; }
   return t;
}

/* A short stable label per WindowRef, so the log reads as a story. */
#define WOP_MAX 16
static void *wop_seen[WOP_MAX];
static int   wop_n;
static int wop_id(void *w) {
   for (int i = 0; i < wop_n; i++) if (wop_seen[i] == w) return i;
   if (wop_n < WOP_MAX) { wop_seen[wop_n] = w; return wop_n++; }
   return -1;
}

typedef struct { int16_t top, left, bottom, right; } WOPRect;

static void wop_desc(void *win, char *out, size_t n) {
   static wop_getb_fn  getb;
   static wop_class_fn getcls;
   static int resolved;
   if (!resolved) {
      resolved = 1;
      getb   = (wop_getb_fn)dlsym(RTLD_DEFAULT, "GetWindowBounds");
      getcls = (wop_class_fn)dlsym(RTLD_DEFAULT, "GetWindowClass");
   }
   WOPRect r = { 0, 0, 0, 0 };
   if (getb) getb(win, 33 /*kWindowContentRgn*/, &r);
   uint32_t cls = 0;
   if (getcls) getcls(win, &cls);
   snprintf(out, n, "win#%d[%p] %dx%d@%d,%d class=%u shown=%d",
            wop_id(win), win, r.right - r.left, r.bottom - r.top,
            r.left, r.top, cls, carbon_window_ever_shown(win));
}

static void wop_log(void *win, const char *what) {
   if (!wop_on()) return;
   char d[160];
   wop_desc(win, d, sizeof d);
   fprintf(stderr, "[winorder] %-22s %s\n", what, d);
}

/* void ShowWindow(WindowRef) — log, then show. No behaviour change. */
uint32_t shim_ShowWindow(uint32_t *args) {
   void *win = (void *)(uintptr_t)x64_objc_unwrap(args[0]);
   wop_log(win, "ShowWindow BEFORE");
   static wop_void_fn f;
   if (!f) f = (wop_void_fn)dlsym(RTLD_DEFAULT, "ShowWindow");
   if (f && win) f(win);
   wop_log(win, "ShowWindow AFTER");
   return 0;
}

/* Called by agl_drawable_shim.c when the app attaches GL to a window: the
 * natural candidate hold point ("the window is about to become useful"). */
void carbon_winorder_note_gl(void *win, void *ctx) {
   if (!wop_on()) return;
   if (win) {
      char d[160]; wop_desc(win, d, sizeof d);
      fprintf(stderr, "[winorder] %-22s ctx=%p %s\n", "GL ATTACH", ctx, d);
   } else {
      fprintf(stderr, "[winorder] %-22s ctx=%p (no WindowRef)\n", "GL ATTACH", ctx);
   }
}
