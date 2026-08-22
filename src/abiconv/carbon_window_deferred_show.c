/*
 * carbon_window_deferred_show.c — ONE job: hold a Carbon window's FIRST show
 * until the app has actually drawn a frame into it, so that the sizing the app
 * does before that frame is still a PRE-SHOW resize.
 *
 * WHY. On this macOS a SIZE change to a kDocumentWindowClass window that has
 * been shown traps in NSCGSPanic; pre-show it is free, in any number
 * (probes/attrresize.m, probes/resizeways.m). Classic apps routinely size their
 * window after showing it, and Halo does exactly that to apply the resolution
 * chosen in its settings dialog:
 *
 *      CreateNewWindow(800x600) -> ShowWindow -> ... settings dialog ...
 *          -> SetWindowBounds(640x480)                        <-- traps
 *
 * Suppressing that resize keeps the process alive but leaves the app rendering
 * its chosen resolution into a window of a different size: a letterboxed
 * picture, and clicks off by the difference because the app hit-tests in the
 * geometry it believes it has.
 *
 * WHY NOT JUST USE A DIFFERENT WINDOW CLASS. Measured, and rejected: several
 * classes do survive a post-show resize, but kMovableModal(4) breaks Halo's 3D
 * backdrop even with the resize suppressed (so the CLASS alone breaks
 * rendering), and kFloating(5) traps anyway via a WindowServer-initiated
 * placement (-[NSWindow _oldPlaceWindow:fromServer:]) that no shim issued.
 * kDocumentWindowClass is the only class in which this app renders correctly,
 * so the window must keep it and be sized before it is shown instead.
 *
 * THE TRIGGER. The app's first aglSwapBuffers: "do not show a GL window before
 * it has a frame." That is a real semantic boundary rather than a timing guess,
 * and it is structurally after the app's setup -- after the settings dialog,
 * after the resolution is applied, and after the resize. An earlier attempt
 * used "one main-runloop turn" and failed precisely because the resize arrives
 * many turns later, after the dialog.
 *
 * SAFETY. If the frame never comes, the window must not stay invisible
 * forever: a timeout shows everything still pending. That degrades to today's
 * behaviour (window shown at the wrong size) rather than to a hung-looking app.
 *
 * This file deliberately makes NO AppKit or Carbon calls on the ShowWindow
 * path. It tracks first-show in its own table instead of asking whether an
 * NSCarbonWindow mirror exists yet -- a previous probe that queried the
 * window's state from inside a ShowWindow hook null-dereferenced on its first
 * call, and a hook this hot must not be able to fail that way.
 *
 * Universal: triggers on the window's own history and on GL activity, never on
 * an app name. Kill switch M64_NO_DEFERRED_SHOW=1; trace ABICONV_DEFERSHOW_TRACE=1.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <pthread.h>
#include <dispatch/dispatch.h>

#include "carbon_shim.h"

extern uint64_t x64_objc_unwrap(uint32_t h);

typedef void (*dsw_void_fn)(void *);
typedef unsigned char (*dsw_bool_fn)(void *);

/* If no frame ever arrives we must still become visible. Long enough that a
 * human reading a settings dialog cannot trip it. */
#define DSW_FALLBACK_SECONDS 45

/* OPT-IN (M64_DEFERRED_SHOW=1). Unset, every hook is a straight pass-through
 * and behaviour is byte-for-byte what it was. This is the same FAMILY as an
 * attempt that already failed once (a one-runloop-turn hold), so it earns its
 * default only after a run says it works. */
static int dsw_off(void) {
   static int t = -1;
   if (t < 0) t = getenv("M64_DEFERRED_SHOW") ? 0 : 1;
   return t;
}
static int dsw_trace(void) {
   static int t = -1;
   if (t < 0) t = getenv("ABICONV_DEFERSHOW_TRACE") ? 1 : 0;
   return t;
}

#define DSW_MAX 32
static void           *dsw_pending[DSW_MAX];   /* shows we are holding back   */
static void           *dsw_seen[DSW_MAX];      /* windows shown at least once */
static int             dsw_nseen;
static pthread_mutex_t dsw_lk = PTHREAD_MUTEX_INITIALIZER;

static dsw_void_fn dsw_real_show(void) {
   static dsw_void_fn f;
   if (!f) f = (dsw_void_fn)dlsym(RTLD_DEFAULT, "ShowWindow");
   return f;
}

/* Perform every held-back show. Safe to call from any thread and any number of
 * times; ShowWindow itself is main-thread-only, hence the hop. */
static void dsw_flush_all(const char *why) {
   void *take[DSW_MAX];
   int n = 0;
   pthread_mutex_lock(&dsw_lk);
   for (int i = 0; i < DSW_MAX; i++)
      if (dsw_pending[i]) { take[n++] = dsw_pending[i]; dsw_pending[i] = NULL; }
   pthread_mutex_unlock(&dsw_lk);
   if (!n) return;

   if (dsw_trace())
      fprintf(stderr, "[defershow] flushing %d held show(s): %s\n", n, why);

   for (int i = 0; i < n; i++) {
      void *w = take[i];
      dispatch_async(dispatch_get_main_queue(), ^{
         dsw_void_fn f = dsw_real_show();
         if (f) f(w);
         if (dsw_trace()) fprintf(stderr, "[defershow] shown %p\n", w);
      });
   }
}

/* Called from the aglSwapBuffers shim: the app has a frame, so the window it
 * has been drawing into is finally worth showing. */
void carbon_deferred_show_on_frame(void) {
   if (dsw_off()) return;
   dsw_flush_all("first frame swapped");
}

/* void ShowWindow(WindowRef) */
uint32_t shim_ShowWindow(uint32_t *args) {
   void *win = (void *)(uintptr_t)x64_objc_unwrap(args[0]);
   dsw_void_fn show = dsw_real_show();
   if (!show || !win) return 0;
   if (dsw_off()) { show(win); return 0; }

   int first = 1, held = 0;
   pthread_mutex_lock(&dsw_lk);
   for (int i = 0; i < dsw_nseen; i++) if (dsw_seen[i] == win) { first = 0; break; }
   if (first) {
      if (dsw_nseen < DSW_MAX) dsw_seen[dsw_nseen++] = win;
      for (int i = 0; i < DSW_MAX; i++)
         if (!dsw_pending[i]) { dsw_pending[i] = win; held = 1; break; }
   }
   pthread_mutex_unlock(&dsw_lk);

   if (!held) { show(win); return 0; }    /* re-show, or table full: immediate */

   if (dsw_trace())
      fprintf(stderr, "[defershow] first ShowWindow(%p) HELD until the app "
                      "draws a frame\n", win);

   static int armed;
   if (!armed) {
      armed = 1;
      dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                   (int64_t)DSW_FALLBACK_SECONDS * NSEC_PER_SEC),
                     dispatch_get_main_queue(), ^{
         dsw_flush_all("fallback timeout — no frame arrived");
      });
   }
   return 0;
}

/* Boolean IsWindowVisible(WindowRef) — a held window must look shown, or the
 * app can observe a state contradicting the ShowWindow it just made. */
uint32_t shim_IsWindowVisible(uint32_t *args) {
   void *win = (void *)(uintptr_t)x64_objc_unwrap(args[0]);
   if (win) {
      int held = 0;
      pthread_mutex_lock(&dsw_lk);
      for (int i = 0; i < DSW_MAX; i++) if (dsw_pending[i] == win) { held = 1; break; }
      pthread_mutex_unlock(&dsw_lk);
      if (held) return 1;
   }
   static dsw_bool_fn f;
   if (!f) f = (dsw_bool_fn)dlsym(RTLD_DEFAULT, "IsWindowVisible");
   if (!f || !win) return 0;
   return f(win) ? 1u : 0u;
}

/* void HideWindow(WindowRef) — cancel a held show rather than let it resurrect
 * a window the app just hid. */
uint32_t shim_HideWindow(uint32_t *args) {
   void *win = (void *)(uintptr_t)x64_objc_unwrap(args[0]);
   int cancelled = 0;
   if (win) {
      pthread_mutex_lock(&dsw_lk);
      for (int i = 0; i < DSW_MAX; i++)
         if (dsw_pending[i] == win) { dsw_pending[i] = NULL; cancelled = 1; break; }
      pthread_mutex_unlock(&dsw_lk);
   }
   if (cancelled) {
      if (dsw_trace())
         fprintf(stderr, "[defershow] HideWindow(%p) cancelled a held show\n", win);
      return 0;                          /* never shown: nothing to hide */
   }
   static dsw_void_fn f;
   if (!f) f = (dsw_void_fn)dlsym(RTLD_DEFAULT, "HideWindow");
   if (f && win) f(win);
   return 0;
}
