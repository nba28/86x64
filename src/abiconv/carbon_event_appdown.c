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
 * NOT be delivered twice. The trigger is therefore structural and precise: we
 * forward only a DOWN that the APPLICATION TARGET NEVER SAW. A sentinel handler
 * on that target records every EventRef that reaches it; after the dispatcher
 * returns, an EventRef the sentinel never recorded is one that was consumed on
 * the way, and only that one is re-sent. On a system where propagation works,
 * the sentinel sees the event and we do nothing at all.
 *
 * Universal: triggers on the shape (a classic app dispatching mouse events to
 * the dispatcher while holding its handlers on the application target), not on
 * any app identity. Kill switch M64_NO_APP_MOUSEDOWN_FIX=1 restores the
 * unmodified behaviour; guard 99_app_mousedown_route.
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
static unsigned long   ad_fwd, ad_skipped;

static int ad_disabled(void) {
   static int t = -1;
   if (t < 0) { t = getenv("M64_NO_APP_MOUSEDOWN_FIX") ? 1 : 0; }
   return t;
}
static int ad_trace(void) {
   static int t = -1;
   if (t < 0) { t = getenv("ABICONV_APPDOWN_TRACE") ? 1 : 0; }
   return t;
}

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

/* Opaque Carbon refs are minted by HIToolbox in the 64-bit heap — the event
 * targets observed on this app sit at 0x6000_0xxxxxxx — so a translated app
 * cannot be holding raw pointers in its 32-bit slots: it holds ARENA HANDLES,
 * and i386_ptr() on one yields garbage. x64_objc_unwrap resolves a handle to
 * the real pointer and passes a genuine low pointer through untouched, so it is
 * correct for both. (This bit me on the first cut of this file.) */
extern uint64_t x64_objc_unwrap(uint32_t h);

/* OSStatus SendEventToEventTarget(EventRef, EventTargetRef) */
uint32_t shim_SendEventToEventTarget(uint32_t *args) {
   EventRef       e = (EventRef)      (uintptr_t)x64_objc_unwrap(args[0]);
   EventTargetRef t = (EventTargetRef)(uintptr_t)x64_objc_unwrap(args[1]);

   if (ad_disabled()) { return (uint32_t)SendEventToEventTarget(e, t); }

   const UInt32 cl = e ? GetEventClass(e) : 0;
   const UInt32 ki = e ? GetEventKind(e)  : 0;
   const int    is_down = (cl == kEventClassMouse && ki == kEventMouseDown);

   if (is_down) { ad_ensure_sentinel(); }

   const OSStatus r = SendEventToEventTarget(e, t);

   if (is_down && t == GetEventDispatcherTarget()) {
      if (ad_was_seen((const void *)e)) {
         /* Propagation worked here; touching it would double-deliver. */
         ad_skipped++;
      } else {
         /* Re-send the SAME EventRef so location, modifiers and click count
          * survive intact rather than being reconstructed from parameters. */
         const OSStatus fr = SendEventToEventTarget(e, GetApplicationEventTarget());
         ad_fwd++;
         if (ad_trace()) {
            fprintf(stderr, "[appdown] dispatcher consumed a mouse-DOWN; "
                            "re-sent to APPLICATION -> %d\n", (int)fr);
         }
      }
   }
   return (uint32_t)r;
}

__attribute__((destructor))
static void ad_summary(void) {
   if (!ad_trace()) { return; }
   fprintf(stderr, "[appdown] forwarded=%lu already-delivered=%lu\n",
           ad_fwd, ad_skipped);
}
