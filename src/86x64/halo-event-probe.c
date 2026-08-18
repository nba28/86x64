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
   n_install++;
   char c[5], k[5];
   for (ItemCount i = 0; i < n && i < 12; i++) {
      fourcc(c, types[i].eventClass);
      EP("[ev] InstallEventHandler  target=%p upp=%p  type[%lu] class='%s' kind=%u\n",
         (void *)target, (void *)h, (unsigned long)i, c, (unsigned)types[i].eventKind);
      (void)k;
   }
   if (g_nh < EP_MAX_H) { g_h[g_nh].upp = h; g_h[g_nh].tgt = target; g_nh++; }
   OSStatus (*orig)(EventTargetRef, EventHandlerUPP, ItemCount,
                    const EventTypeSpec *, void *, EventHandlerRef *) =
      dlsym(RTLD_NEXT, "InstallEventHandler");
   OSStatus r = orig ? orig(target, h, n, types, ud, out) : -1;
   EP("[ev] InstallEventHandler -> %d  (%lu total)\n", (int)r, n_install);
   return r;
}

static OSStatus ep_ReceiveNextEvent(ItemCount n, const EventTypeSpec *types,
                                    EventTimeout to, Boolean pull,
                                    EventRef *out) {
   OSStatus (*orig)(ItemCount, const EventTypeSpec *, EventTimeout, Boolean,
                    EventRef *) = dlsym(RTLD_NEXT, "ReceiveNextEvent");
   OSStatus r = orig ? orig(n, types, to, pull, out) : -1;
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
   return r;
}

static OSStatus ep_SendEventToEventTarget(EventRef e, EventTargetRef t) {
   const UInt32 cl = e ? GetEventClass(e) : 0, ki = e ? GetEventKind(e) : 0;
   const int interesting = (cl == kEventClassMouse &&
                            (ki == kEventMouseDown || ki == kEventMouseUp));
   OSStatus (*orig)(EventRef, EventTargetRef) =
      dlsym(RTLD_NEXT, "SendEventToEventTarget");
   OSStatus r = orig ? orig(e, t) : -1;
   n_send++;
   if (interesting) {
      char c[5]; fourcc(c, cl);
      EP("[ev] SendEventToEventTarget class='%s' kind=%u target=%p -> %d%s\n",
         c, (unsigned)ki, (void *)t, (int)r,
         r == eventNotHandledErr ? "  (eventNotHandledErr - NOBODY CLAIMED IT)" : "");
   }
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
   OSStatus (*orig)(EventHandlerCallRef, EventRef) =
      dlsym(RTLD_NEXT, "CallNextEventHandler");
   return orig ? orig(ref, e) : -1;
}

static OSStatus ep_GetEventParameter(EventRef e, EventParamName name,
                                     EventParamType want, EventParamType *got,
                                     ByteCount sz, ByteCount *outsz, void *buf) {
   OSStatus (*orig)(EventRef, EventParamName, EventParamType, EventParamType *,
                    ByteCount, ByteCount *, void *) =
      dlsym(RTLD_NEXT, "GetEventParameter");
   OSStatus r = orig ? orig(e, name, want, got, sz, outsz, buf) : -1;
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
