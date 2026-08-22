/*
 * carbon_window_deferred_show.c — ONE job: delay a Carbon window's FIRST show
 * to the next main-runloop spin, so that the sizing the app does immediately
 * after showing still counts as a PRE-SHOW resize.
 *
 * WHY THIS EXISTS. On modern macOS a SIZE change to a Carbon window that has
 * ever been shown is fatal (NSCGSPanic, SIGILL — see
 * carbon_window_resize_guard.c for the measured table). Pre-show size changes
 * are free, in any number. Classic apps routinely do:
 *
 *      CreateNewWindow -> SetWindowBounds(800x600) -> ShowWindow
 *                      -> SetWindowBounds(<chosen resolution>)   <-- FATAL
 *
 * and Halo does exactly that, measured live 2026-08-22:
 *      pre-show  800x600            PASSED THROUGH
 *      SHOW
 *      post-show 800x600 -> 640x480 SUPPRESSED (would NSCGSPanic)
 *
 * Merely suppressing that last resize keeps the process alive but leaves the
 * window at a size the app did not choose. The app then renders its own
 * 640x480 into an 800x600 window: OpenGL's origin is bottom-left, so the frame
 * sits in the bottom-left corner with black bands top and right — and every
 * mouse click is off by the 120pt difference, because the app hit-tests in the
 * geometry it believes it has. Both of the tester's symptoms ("window misaligned",
 * "most of my clicks had no effect") are that one mismatch.
 *
 * THE CURE. Nothing is suppressed and nothing is faked: the app's resize is
 * simply allowed to happen while it is still free. We hold the first show back
 * by one runloop turn, which is long enough for the app's own synchronous
 * setup to finish, and then show the window at the size it actually asked for.
 *
 * Verified end-to-end before shipping (probes/defershow.m replaying Halo's
 * exact sequence): with the show deferred the 640x480 resize returns noErr
 * with no mirror yet, the deferred show then fires, and the window ends up
 * 640x480 with no SIGILL.
 *
 * WHY A RUNLOOP TURN IS THE RIGHT TRIGGER. It needs no knowledge of which
 * event-loop entry point the app uses — RunApplicationEventLoop,
 * ReceiveNextEvent, WaitNextEvent and any modal loop all spin the main runloop,
 * so the show lands in every case. An app that never spins a runloop cannot
 * process input at all, so there is no case where the window stays invisible
 * but the app is otherwise live.
 *
 * SCOPE. Only the FIRST show of a given window is deferred; once a window has
 * a mirror, re-showing it is immediate (the deferral would buy nothing, and a
 * re-show after a hide is often done for a reason the app expects to be
 * synchronous). IsWindowVisible reports a pending window as visible, so the
 * app never sees a state that contradicts the show it just requested — a lie
 * about what the SYSTEM reports, never anything the app PERSISTS. HideWindow
 * on a still-pending window cancels the pending show rather than racing it.
 *
 * Universal: triggers on the window's own state, never on an app name.
 * Kill switch: M64_NO_DEFERRED_SHOW=1. Trace: ABICONV_DEFERSHOW_TRACE=1.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <pthread.h>
#include <dispatch/dispatch.h>

#include "carbon_shim.h"

typedef void (*dsw_void_fn)(void *);
typedef unsigned char (*dsw_bool_fn)(void *);

extern uint64_t x64_objc_unwrap(uint32_t h);
extern int      carbon_window_ever_shown(void *win);   /* the mirror oracle */

static int dsw_off(void) {
   static int t = -1;
   if (t < 0) { t = getenv("M64_NO_DEFERRED_SHOW") ? 1 : 0; }
   return t;
}
static int dsw_trace(void) {
   static int t = -1;
   if (t < 0) { t = getenv("ABICONV_DEFERSHOW_TRACE") ? 1 : 0; }
   return t;
}

/* Windows whose first show we are holding back. Tiny and short-lived: an entry
 * exists only between the app's ShowWindow and the next runloop turn. */
#define DSW_MAX_PENDING 32
static void           *dsw_pending[DSW_MAX_PENDING];
static pthread_mutex_t dsw_lk = PTHREAD_MUTEX_INITIALIZER;

static int dsw_is_pending(void *w) {
   int found = 0;
   pthread_mutex_lock(&dsw_lk);
   for (int i = 0; i < DSW_MAX_PENDING; i++)
      if (dsw_pending[i] == w) { found = 1; break; }
   pthread_mutex_unlock(&dsw_lk);
   return found;
}
static int dsw_add_pending(void *w) {
   int ok = 0;
   pthread_mutex_lock(&dsw_lk);
   for (int i = 0; i < DSW_MAX_PENDING; i++)
      if (!dsw_pending[i]) { dsw_pending[i] = w; ok = 1; break; }
   pthread_mutex_unlock(&dsw_lk);
   return ok;
}
/* Returns 1 if it was still pending (i.e. the caller now owns the show). */
static int dsw_take_pending(void *w) {
   int had = 0;
   pthread_mutex_lock(&dsw_lk);
   for (int i = 0; i < DSW_MAX_PENDING; i++)
      if (dsw_pending[i] == w) { dsw_pending[i] = NULL; had = 1; break; }
   pthread_mutex_unlock(&dsw_lk);
   return had;
}

static dsw_void_fn dsw_real_show(void) {
   static dsw_void_fn f;
   if (!f) f = (dsw_void_fn)dlsym(RTLD_DEFAULT, "ShowWindow");
   return f;
}

/* void ShowWindow(WindowRef) */
uint32_t shim_ShowWindow(uint32_t *args) {
   void *win = (void *)(uintptr_t)x64_objc_unwrap(args[0]);
   dsw_void_fn show = dsw_real_show();
   if (!show || !win) { return 0; }

   /* Already shown once, deferral disabled, or the table is full: show now.
    * Falling back to the immediate show is always safe — it is exactly the
    * behaviour we had before this file existed. */
   if (dsw_off() || carbon_window_ever_shown(win) || dsw_is_pending(win)) {
      show(win);
      return 0;
   }
   if (!dsw_add_pending(win)) { show(win); return 0; }

   if (dsw_trace()) {
      fprintf(stderr, "[defershow] first ShowWindow(%p) HELD until the next "
                      "runloop turn (so the app's own resize stays free)\n", win);
   }
   dispatch_async(dispatch_get_main_queue(), ^{
      if (!dsw_take_pending(win)) { return; }   /* cancelled by HideWindow */
      dsw_void_fn f = dsw_real_show();
      if (f) f(win);
      if (dsw_trace()) {
         fprintf(stderr, "[defershow] deferred ShowWindow(%p) performed\n", win);
      }
   });
   return 0;
}

/* Boolean IsWindowVisible(WindowRef) — a pending window must look shown, or the
 * app can observe a state that contradicts the ShowWindow it just made. */
uint32_t shim_IsWindowVisible(uint32_t *args) {
   void *win = (void *)(uintptr_t)x64_objc_unwrap(args[0]);
   if (win && dsw_is_pending(win)) { return 1; }
   static dsw_bool_fn f;
   if (!f) f = (dsw_bool_fn)dlsym(RTLD_DEFAULT, "IsWindowVisible");
   if (!f || !win) { return 0; }
   return f(win) ? 1u : 0u;
}

/* void HideWindow(WindowRef) — cancel a show we are still holding rather than
 * letting the deferred block resurrect a window the app just hid. */
uint32_t shim_HideWindow(uint32_t *args) {
   void *win = (void *)(uintptr_t)x64_objc_unwrap(args[0]);
   if (win && dsw_take_pending(win)) {
      if (dsw_trace()) {
         fprintf(stderr, "[defershow] HideWindow(%p) cancelled the pending "
                         "show (window was never made visible)\n", win);
      }
      return 0;                     /* never shown, so nothing to hide */
   }
   static dsw_void_fn f;
   if (!f) f = (dsw_void_fn)dlsym(RTLD_DEFAULT, "HideWindow");
   if (f && win) f(win);
   return 0;
}
