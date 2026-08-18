/*
 * halo-event-probe.c — DIAGNOSTIC ONLY. Trace the Carbon EVENT path, to find
 * where a main-menu click is lost.
 *
 * ⚠Delete with the other #46/menu probes when this closes. It touches nothing
 * in libabiconv: DYLD_INSERT_LIBRARIES + __DATA,__interpose replaces the real
 * Carbon entry points, which works because libabiconv's abigen bridges reach
 * Carbon through ordinary symbol stubs (the same trick halo-gl-texprobe.c uses
 * for OpenGL). Two files, and it is gone.
 *
 * WHY. Menu items HIGHLIGHT but do not respond to a click. The first reading —
 * that classic Button() polling was returning a stuck 0 — was MEASURED AND
 * FALSIFIED: Halo calls Button() exactly zero times. Its imports say the menu
 * runs on the Carbon Event Manager (InstallEventHandler / ReceiveNextEvent /
 * SendEventToEventTarget / GetEventParameter), so this traces that chain
 * instead of theorising about it.
 *
 * WHAT EACH OUTCOME MEANS — the chain breaks at exactly one link:
 *   no InstallEventHandler         the handler is never installed; look at why
 *                                  (NewEventHandlerUPP wrapping, or Halo
 *                                  bailing before it gets there)
 *   installed, no mouse events     ReceiveNextEvent is not producing them:
 *                                  the app is not pumping, or the events go
 *                                  somewhere else (window vs application
 *                                  target, or the fullscreen capture)
 *   mouse events, handler never    dispatch is dropping them — the target or
 *     invoked                      the event-type registration is wrong
 *   handler invoked, nothing       the click REACHES translated Halo code and
 *     happens on screen            is lost inside it; then the coordinates
 *                                  GetEventParameter hands back are the next
 *                                  thing to check, since highlight (GetMouse,
 *                                  port-aware) and click (event coords) come
 *                                  from DIFFERENT sources
 *
 * The mouse-location parameter is decoded and printed for exactly that last
 * reason: if the click coordinates disagree with where the pointer visibly is,
 * the hit-test cannot succeed no matter how well the plumbing works.
 *
 * ⚠HOW TO CALL THROUGH, AND HOW NOT TO. The first version of this file reached
 * the real Carbon function with dlsym(RTLD_NEXT, "InstallEventHandler"). That
 * returned THIS FILE'S OWN replacement, so every call re-entered itself: 10800
 * identical log lines, ZERO completions, stack overflow, Abort trap 6 — and it
 * looked exactly like "Halo installs 10800 handlers" until the missing return
 * lines gave it away. With __DATA,__interpose the correct call-through is the
 * ORDINARY NAMED CALL: dyld rewrites the bindings of OTHER images, not those of
 * the interposing image itself, so a direct `InstallEventHandler(...)` here
 * reaches the real one. That is the same pattern halo-gl-texprobe.c uses.
 * A re-entry seatbelt below makes a future mistake of this kind lose logging
 * instead of taking the app down.
 */
#include <Carbon/Carbon.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *g_log;
static pthread_mutex_t g_lk = PTHREAD_MUTEX_INITIALIZER;

__attribute__((constructor))
static void ep_init(void) {
   const char *p = getenv("HALO_EVENT_LOG");
   g_log = (p && *p) ? fopen(p, "w") : stderr;
   if (!g_log) { g_log = stderr; }
   setvbuf(g_log, NULL, _IOLBF, 0);
   fprintf(g_log, "[ev] probe loaded\n");
}

static void EP(const char *fmt, ...) {
   va_list ap; va_start(ap, fmt);
   pthread_mutex_lock(&g_lk);
   vfprintf(g_log ? g_log : stderr, fmt, ap);
   pthread_mutex_unlock(&g_lk);
   va_end(ap);
}

/* Re-entry seatbelt. Depth should never exceed 1; if it ever does, the
 * call-through is recursing and we bail out with a benign value rather than
 * overflowing the stack. Losing a probe is acceptable; crashing the target the
 * probe exists to observe is not. */
static __thread int g_depth;
static int g_recursed;

static int ep_enter(const char *who) {
   if (g_depth > 2) {
      if (!g_recursed) {
         g_recursed = 1;
         fprintf(stderr, "[ev] ⚠RECURSION in %s - the call-through is reaching "
                 "this probe instead of Carbon. Logging disabled; the app is "
                 "NOT being taken down.\n", who);
      }
      return 0;
   }
   g_depth++;
   return 1;
}
static void ep_leave(void) { if (g_depth > 0) { g_depth--; } }

static void fourcc(char out[5], UInt32 v) {
   out[0] = (char)(v >> 24); out[1] = (char)(v >> 16);
   out[2] = (char)(v >> 8);  out[3] = (char)v; out[4] = 0;
   for (int i = 0; i < 4; i++) {
      if (out[i] < 32 || out[i] > 126) { out[i] = '.'; }
   }
}

/* Counters, so a flood of mouse-moved events cannot bury the one mouse-down. */
static unsigned long n_install, n_recv, n_send, n_getparam;
static unsigned long n_mousedown, n_mouseup, n_mousemoved, n_other;
static unsigned long n_handler_calls, n_handler_mousedown;

/* ---- the installed handler, wrapped so we can see if it is ever called ---- */
#define EP_MAX_H 32
static struct { EventHandlerUPP upp; EventTargetRef tgt; } g_h[EP_MAX_H];
static int g_nh;

static OSStatus ep_InstallEventHandler(EventTargetRef target, EventHandlerUPP h,
                                       ItemCount n, const EventTypeSpec *types,
                                       void *ud, EventHandlerRef *out) {
   if (!ep_enter("InstallEventHandler")) {
      return InstallEventHandler(target, h, n, types, ud, out);
   }
   n_install++;
   /* Capped: a genuine flood must stay readable, and an UNBOUNDED log is how
    * the recursion above disguised itself as a finding. */
   if (n_install <= 24) {
      char c[5];
      for (ItemCount i = 0; i < n && i < 12; i++) {
         fourcc(c, types[i].eventClass);
         EP("[ev] InstallEventHandler #%lu target=%p upp=%p type[%lu] "
            "class='%s' kind=%u\n", n_install, (void *)target, (void *)h,
            (unsigned long)i, c, (unsigned)types[i].eventKind);
      }
   } else if (n_install == 25) {
      EP("[ev] (further InstallEventHandler calls counted, not printed)\n");
   }
   if (g_nh < EP_MAX_H) { g_h[g_nh].upp = h; g_h[g_nh].tgt = target; g_nh++; }
   const OSStatus r = InstallEventHandler(target, h, n, types, ud, out);
   if (n_install <= 24) { EP("[ev] InstallEventHandler #%lu -> %d\n", n_install, (int)r); }
   ep_leave();
   return r;
}

static OSStatus ep_ReceiveNextEvent(ItemCount n, const EventTypeSpec *types,
                                    EventTimeout to, Boolean pull,
                                    EventRef *out) {
   if (!ep_enter("ReceiveNextEvent")) {
      return ReceiveNextEvent(n, types, to, pull, out);
   }
   const OSStatus r = ReceiveNextEvent(n, types, to, pull, out);
   n_recv++;
   if (r == noErr && out && *out) {
      const UInt32 cl = GetEventClass(*out), ki = GetEventKind(*out);
      if (cl == kEventClassMouse) {
         switch (ki) {
            case kEventMouseDown:   n_mousedown++;  break;
            case kEventMouseUp:     n_mouseup++;    break;
            case kEventMouseMoved:  n_mousemoved++; break;
            default: break;
         }
         if (ki == kEventMouseDown || ki == kEventMouseUp) {
            HIPoint pt = {0, 0};
            OSStatus g = GetEventParameter(*out, kEventParamMouseLocation,
                                           typeHIPoint, NULL, sizeof pt, NULL, &pt);
            EP("[ev] ReceiveNextEvent  MOUSE %s at (%.1f,%.1f) [param rc=%d]\n",
               ki == kEventMouseDown ? "DOWN" : "UP  ",
               (double)pt.x, (double)pt.y, (int)g);
         }
      } else {
         n_other++;
      }
   }
   ep_leave();
   return r;
}

static OSStatus ep_SendEventToEventTarget(EventRef e, EventTargetRef t) {
   const UInt32 cl = e ? GetEventClass(e) : 0, ki = e ? GetEventKind(e) : 0;
   const int interesting = (cl == kEventClassMouse &&
                            (ki == kEventMouseDown || ki == kEventMouseUp));
   if (!ep_enter("SendEventToEventTarget")) {
      return SendEventToEventTarget(e, t);
   }
   const OSStatus r = SendEventToEventTarget(e, t);
   n_send++;
   if (interesting) {
      char c[5]; fourcc(c, cl);
      EP("[ev] SendEventToEventTarget class='%s' kind=%u target=%p -> %d%s\n",
         c, (unsigned)ki, (void *)t, (int)r,
         r == eventNotHandledErr ? "  (eventNotHandledErr - NOBODY CLAIMED IT)" : "");
   }
   ep_leave();
   return r;
}

/* Wrapping CallNextEventHandler is how we see the app's own handler running:
 * a handler that never runs cannot call it, and one that does tells us the
 * dispatch reached translated code. */
static OSStatus ep_CallNextEventHandler(EventHandlerCallRef ref, EventRef e) {
   const UInt32 cl = e ? GetEventClass(e) : 0, ki = e ? GetEventKind(e) : 0;
   n_handler_calls++;
   if (cl == kEventClassMouse && ki == kEventMouseDown) {
      n_handler_mousedown++;
      EP("[ev] CallNextEventHandler on a MOUSE DOWN - the app's handler IS "
         "running (#%lu)\n", n_handler_mousedown);
   }
   return CallNextEventHandler(ref, e);
}

static OSStatus ep_GetEventParameter(EventRef e, EventParamName name,
                                     EventParamType want, EventParamType *got,
                                     ByteCount sz, ByteCount *outsz, void *buf) {
   if (!ep_enter("GetEventParameter")) {
      return GetEventParameter(e, name, want, got, sz, outsz, buf);
   }
   const OSStatus r = GetEventParameter(e, name, want, got, sz, outsz, buf);
   n_getparam++;
   /* Only the mouse-location reads, and only on a button event: this is the
    * number a hit-test depends on, and the reason to print it is that highlight
    * (GetMouse) and click (this) come from DIFFERENT sources. */
   if (e && name == kEventParamMouseLocation && r == noErr && buf) {
      const UInt32 ki = GetEventKind(e);
      if (GetEventClass(e) == kEventClassMouse &&
          (ki == kEventMouseDown || ki == kEventMouseUp)) {
         char t[5]; fourcc(t, want);
         if (want == typeHIPoint && sz >= sizeof(HIPoint)) {
            const HIPoint *p = (const HIPoint *)buf;
            EP("[ev]   GetEventParameter(MouseLocation,'%s') -> (%.1f,%.1f)\n",
               t, (double)p->x, (double)p->y);
         } else if (want == typeQDPoint && sz >= sizeof(Point)) {
            const Point *p = (const Point *)buf;
            EP("[ev]   GetEventParameter(MouseLocation,'%s') -> (%d,%d) QD\n",
               t, (int)p->h, (int)p->v);
         }
      }
   }
   ep_leave();
   return r;
}

static void ep_atexit(void) {
   EP("\n[ev] ===== summary =====\n");
   EP("[ev] InstallEventHandler calls : %lu\n", n_install);
   EP("[ev] ReceiveNextEvent calls    : %lu\n", n_recv);
   EP("[ev] SendEventToEventTarget    : %lu\n", n_send);
   EP("[ev] GetEventParameter calls   : %lu\n", n_getparam);
   EP("[ev] mouse DOWN received       : %lu\n", n_mousedown);
   EP("[ev] mouse UP   received       : %lu\n", n_mouseup);
   EP("[ev] mouse MOVED received      : %lu\n", n_mousemoved);
   EP("[ev] app handler ran on a down : %lu\n", n_handler_mousedown);
   if (g_recursed) {
      EP("[ev] ⚠THE PROBE RECURSED - treat every count above as unreliable.\n");
   }
}

__attribute__((constructor))
static void ep_reg(void) { atexit(ep_atexit); }

__attribute__((used)) static struct { const void *repl, *orig; }
ep_interposers[] __attribute__((section("__DATA,__interpose"))) = {
   { (const void *)ep_InstallEventHandler,    (const void *)InstallEventHandler },
   { (const void *)ep_ReceiveNextEvent,       (const void *)ReceiveNextEvent },
   { (const void *)ep_SendEventToEventTarget, (const void *)SendEventToEventTarget },
   { (const void *)ep_CallNextEventHandler,   (const void *)CallNextEventHandler },
   { (const void *)ep_GetEventParameter,      (const void *)GetEventParameter },
};
