/*
 * carbon_event_appdown.c — deliver a mouse-DOWN that the dispatcher consumed to
 * the APPLICATION event target, where classic apps install their mouse handler.
 *
 * THE CONTRACT A CLASSIC APP RELIES ON. A Carbon app pumps its own loop:
 *   ReceiveNextEvent(..., pullEvent=true) -> SendEventToEventTarget(e, dispatcher)
 * and installs its handlers on GetApplicationEventTarget(). The dispatcher
 * routes a mouse-DOWN to the window under the pointer; when every handler there
 * DECLINES (eventNotHandledErr), the event is supposed to propagate up the
 * containment hierarchy — window -> application — and reach the app's handler.
 *
 * WHAT ACTUALLY HAPPENS ON MODERN macOS (measured on Halo CE, 2026-08-19):
 *   * the app sends every mouse-DOWN to the DISPATCHER; the send returns noErr;
 *   * the front window's handlers receive all of them and DECLINE — 11 declines
 *     and ZERO claims across a run;
 *   * the APPLICATION target never sees a single DOWN;
 *   * mouse-UP, MOVED and DRAGGED all reach that same APPLICATION target fine,
 *     and the app and window both genuinely activate.
 * The dispatcher performs its own default window processing for the declined
 * DOWN and treats it as handled, so it never propagates. Everything the app
 * drives from a click is then dead while the rest of its input works — which is
 * exactly what a menu that highlights but will not activate looks like.
 *
 * A NATIVELY-INSTALLED CONTROL HANDLER, added by the diagnostic probe on the
 * same target with the same event kinds, ALSO received zero DOWNs. So this is
 * not our marshalling and not our InstallEventHandler: it is the platform's
 * routing, and it has to be compensated for rather than fixed at the source.
 *
 * THE RULE, and why it is not simply "always forward": a DOWN the dispatcher
 * legitimately consumed — window chrome, a live control, a tracking loop — must
 * NOT be delivered twice.
 *
 * ⚠MY FIRST RULE WAS WRONG, AND IT CRASHED THE APP. It forwarded any DOWN that
 * "the APPLICATION TARGET NEVER SAW", using that as a proxy for "nobody handled
 * it". Those are not the same thing: a click consumed by a REAL CONTROL — a
 * button in a settings dialog — also never reaches the application target. So
 * every dialog click was forwarded to the app's own handler, which for a game is
 * its in-game input handler, and Halo acted on a phantom click while a modal
 * dialog was up. It resized a window to bounds AppKit could not realise and
 * NSCGSPanic'd (SIGILL) before the main menu — a crash my change introduced.
 *
 * The proxy has to be the actual question: DID A VIEW OWN THIS CLICK? Ask the
 * toolbox directly. HIViewGetViewForMouseEvent hit-tests the event against the
 * window's view hierarchy; if it names a view other than the content root, a
 * real control owns the click and the app-level handler must never see it. Only
 * a click that lands on bare window content — which is all a game window has —
 * is forwarded. The sentinel check is kept on top of that, so a system where
 * propagation works is still left completely alone.
 *
 * When the hit test cannot be resolved we DO NOT forward. The failure modes are
 * not symmetric: not forwarding reproduces the old "click does nothing", which
 * is inert and recoverable, while forwarding wrongly injects a phantom click
 * into a live app — and that is what took the app down.
 *
 * Universal: triggers on the shape (a classic app dispatching mouse events to
 * the dispatcher while holding its handlers on the application target), not on
 * any app identity.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <Carbon/Carbon.h>

#include "carbon_shim.h"

#define AD_RING 16   /* EventRefs seen by the sentinel, most recent first */

static pthread_mutex_t ad_lk = PTHREAD_MUTEX_INITIALIZER;
static const void     *ad_seen[AD_RING];
static int             ad_seen_i;
static EventHandlerRef ad_sentinel;
static int             ad_installed;

static void ad_note(const void *e) {
   pthread_mutex_lock(&ad_lk);
   ad_seen[ad_seen_i++ % AD_RING] = e;
   pthread_mutex_unlock(&ad_lk);
}
static int ad_was_seen(const void *e) {
   int found = 0;
   pthread_mutex_lock(&ad_lk);
   for (int i = 0; i < AD_RING; i++) {
      if (ad_seen[i] == e) { found = 1; break; }
   }
   pthread_mutex_unlock(&ad_lk);
   return found;
}

/* Pure observer: records that this EventRef reached the application target and
 * always declines, so the app's own handlers still run exactly as before. */
static OSStatus ad_sentinel_proc(EventHandlerCallRef ref, EventRef e, void *ud) {
   (void)ref; (void)ud;
   ad_note((const void *)e);
   return eventNotHandledErr;
}

/* Installed lazily, and AFTER the app has installed its own handlers, so that
 * Carbon (which calls the most recently installed handler first) runs the
 * sentinel BEFORE them. Order matters: were the app's handler to claim the
 * event first, the sentinel would never see it and we would forward a DOWN that
 * had in fact been delivered — a double click from one press. */
static void ad_ensure_sentinel(void) {
   if (ad_installed) { return; }
   ad_installed = 1;
   static const EventTypeSpec t[] = {
      { kEventClassMouse, kEventMouseDown },
   };
   InstallEventHandler(GetApplicationEventTarget(),
                       NewEventHandlerUPP(ad_sentinel_proc),
                       1, t, NULL, &ad_sentinel);
}

/* These are still present in HIToolbox but were dropped from the modern public
 * headers, so declare what we call rather than guessing at replacements. */
extern short     FindWindow(Point, WindowRef *);
extern HIViewRef HIViewGetRoot(WindowRef);
extern HIViewRef HIViewGetSuperview(HIViewRef);
extern OSStatus  HIViewGetViewForMouseEvent(HIViewRef, EventRef, HIViewRef *);

/* Did a real view/control own this click? Conservative by construction: any
 * uncertainty answers YES (a view owns it), which suppresses forwarding.
 *
 * Depth is the test, and it needs no header constant: a Carbon window nests
 * root -> content -> controls, so the root and its content child ARE the bare
 * window content — all a game window ever has — and anything DEEPER is a real
 * control that legitimately consumed the click. */
static int ad_view_owns_click(EventRef e) {
   WindowRef w = NULL;
   if (GetEventParameter(e, kEventParamWindowRef, typeWindowRef, NULL,
                         sizeof w, NULL, &w) != noErr || !w) {
      Point where;
      if (GetEventParameter(e, kEventParamMouseLocation, typeQDPoint, NULL,
                            sizeof where, NULL, &where) != noErr) {
         return 1;                      /* cannot tell -> do not forward */
      }
      if (FindWindow(where, &w) == 0 || !w) { return 1; }
   }
   HIViewRef root = HIViewGetRoot(w);
   if (!root) { return 1; }
   HIViewRef hit = NULL;
   if (HIViewGetViewForMouseEvent(root, e, &hit) != noErr || !hit) {
      return 0;   /* the toolbox found no view at all: bare content */
   }
   if (hit == root) { return 0; }
   return !(HIViewGetSuperview(hit) == root);
}

/* Opaque Carbon refs are minted by HIToolbox in the 64-bit heap — the event
 * targets observed on this app sit at 0x6000_0xxxxxxx — so a translated app
 * cannot be holding raw pointers in its 32-bit slots: it holds ARENA HANDLES,
 * and i386_ptr() on one yields garbage. x64_objc_unwrap resolves a handle to
 * the real pointer and passes a genuine low pointer through untouched, so it is
 * correct for both. (This bit me on the first cut of this file.) */
extern uint64_t x64_objc_unwrap(uint32_t h);
extern int cglfs_map_global(double *x, double *y);   /* cgl_fullscreen_shim.m */

/* OSStatus SendEventToEventTarget(EventRef, EventTargetRef) */
uint32_t shim_SendEventToEventTarget(uint32_t *args) {
   EventRef       e = (EventRef)      (uintptr_t)x64_objc_unwrap(args[0]);
   EventTargetRef t = (EventTargetRef)(uintptr_t)x64_objc_unwrap(args[1]);

   const UInt32 cl = e ? GetEventClass(e) : 0;
   const UInt32 ki = e ? GetEventKind(e)  : 0;
   const int    is_down = (cl == kEventClassMouse && ki == kEventMouseDown);

   if (is_down) { ad_ensure_sentinel(); }

   /* A fullscreen CGL surface (cgl_fullscreen_shim.m) is letterboxed and
    * scaled on the physical screen, while the app reasons in its virtual mode.
    * Rewrite the event's global location into the app's space once, here,
    * where every event it pumps passes, before anything routes on it. */
   if (cl == kEventClassMouse) {
      HIPoint p;
      if (GetEventParameter(e, kEventParamMouseLocation, typeHIPoint, NULL,
                            sizeof p, NULL, &p) == noErr) {
         double x = p.x, y = p.y;
         if (cglfs_map_global(&x, &y)) {
            p.x = x; p.y = y;
            SetEventParameter(e, kEventParamMouseLocation, typeHIPoint, sizeof p, &p);
         }
      }
   }

   const OSStatus r = SendEventToEventTarget(e, t);

   if (is_down && t == GetEventDispatcherTarget()) {
      /* Already seen: propagation worked here; touching it would double-deliver.
       * A view owns it: a real control took it — forwarding would be a phantom
       * click. Otherwise re-send the SAME EventRef so location, modifiers and
       * click count survive intact rather than being reconstructed. */
      if (!ad_was_seen((const void *)e) && !ad_view_owns_click(e)) {
         (void)SendEventToEventTarget(e, GetApplicationEventTarget());
      }
   }
   return (uint32_t)r;
}

