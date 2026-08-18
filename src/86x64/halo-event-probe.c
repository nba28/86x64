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
#include <signal.h>
#include <sys/time.h>

/* Removed from the modern Carbon headers (the function is still exported by
 * HIToolbox, which is why the translated app can bind to it). Declared here so
 * this file can interpose it. */
extern Boolean ConvertEventRefToEventRecord(EventRef inEvent,
                                            EventRecord *outEvent);
extern Boolean WaitNextEvent(EventMask eventMask, EventRecord *theEvent,
                             UInt32 sleep, RgnHandle mouseRgn);
/* Window Manager calls the modern headers no longer declare (still exported). */
extern short     FindWindow(Point thePoint, WindowRef *theWindow);
extern WindowRef FrontWindow(void);
extern WindowRef ActiveNonFloatingWindow(void);
extern OSStatus  GetWindowBounds(WindowRef w, WindowRegionCode r, Rect *b);
extern Boolean   IsWindowVisible(WindowRef w);
extern EventTargetRef GetUserFocusEventTarget(void);
extern EventTargetRef GetWindowEventTarget(WindowRef w);

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
static unsigned long n_mousedown, n_mouseup, n_mousemoved, n_other, n_key;
static unsigned long n_handler_calls, n_handler_mousedown;

/* ---- the installed handler, wrapped so we can see if it is ever called ----
 * SendEventToEventTarget returning noErr only says SOMEBODY handled the event;
 * it does not say WHO. And a handler that fully handles an event returns noErr
 * WITHOUT calling CallNextEventHandler, so a zero CallNextEventHandler count
 * proves nothing either way. The only way to know whether the click reaches
 * Halo is to sit in the middle of its handler, so we substitute our own UPP for
 * the first MOUSE-class handler it installs and forward faithfully. */
#define EP_MAX_H 32
static struct { EventHandlerUPP upp; EventTargetRef tgt; } g_h[EP_MAX_H];
static int g_nh;
static unsigned long n_halo_handler, n_halo_mousedown;

/*
 * WRAP EVERY MOUSE-CLASS HANDLER, not just Halo's.
 *
 * Measured: 33 mouse-downs arrive, all dispatch with noErr, and HALO'S OWN
 * handler is entered ZERO times. So somebody else consumes every click before
 * it reaches Halo's application-target handler. The candidate is visible in the
 * same log — install #10 is a SYSTEM handler (upp in the 0x7ff8... range, i.e.
 * native code, not translated) registered for 'mous' kind=1 plus a pile of
 * 'wind' kinds on a DIFFERENT target: the standard window handler. In Carbon a
 * mouse-down goes to the window under the pointer first, and the standard
 * handler processes it there.
 *
 * Wrapping only Halo's handler could show that it never ran; it could not show
 * WHO ran instead. So each mouse-class handler is wrapped, labelled HALO or
 * SYSTEM by whether its UPP is in the low-4GB translated range, and TIMED.
 * The timing matters: a standard handler that enters a control-TRACKING loop
 * blocks until the mouse is released and swallows the mouse-up inside itself —
 * which would explain the other oddity in the same run, 33 downs and ZERO ups
 * while Halo asks ReceiveNextEvent for EVERY event type.
 *
 * Context travels in userData: we install our own struct holding the original
 * UPP and the app's original userData, and hand the original back on the way
 * through, so the wrapped handler cannot tell the difference.
 */
struct wrapctx {
   EventHandlerUPP orig;
   void           *ud;
   int             is_halo;
   int             idx;
};
#define EP_MAX_W 16
static struct wrapctx g_w[EP_MAX_W];
static int g_nw;
static unsigned long n_sys_mousedown;

static double ep_now(void) {
   struct timeval tv; gettimeofday(&tv, NULL);
   return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

static OSStatus ep_wrap(EventHandlerCallRef ref, EventRef e, void *ud) {
   struct wrapctx *w = (struct wrapctx *)ud;
   const UInt32 cl = e ? GetEventClass(e) : 0, ki = e ? GetEventKind(e) : 0;
   const int is_down = (cl == kEventClassMouse && ki == kEventMouseDown);
   char c[5]; fourcc(c, cl);
   if (is_down) {
      if (w->is_halo) { n_halo_handler++; n_halo_mousedown++; }
      else            { n_sys_mousedown++; }
      EP("[ev] >>> %s handler #%d entered on MOUSE DOWN\n",
         w->is_halo ? "HALO'S" : "SYSTEM", w->idx);
   } else if (w->is_halo) {
      n_halo_handler++;
   }
   const double t0 = ep_now();
   const OSStatus r = w->orig
      ? InvokeEventHandlerUPP(ref, e, w->ud, w->orig) : eventNotHandledErr;
   const double dt = ep_now() - t0;
   if (is_down) {
      EP("[ev] <<< %s handler #%d returned %d after %.3fs%s%s\n",
         w->is_halo ? "HALO'S" : "SYSTEM", w->idx, (int)r, dt,
         r == eventNotHandledErr ? "  (DECLINED - the event passes on)"
                                 : "  (CLAIMED - the event stops here)",
         dt > 0.15 ? "   <-- it BLOCKED: a tracking loop, which also eats the "
                     "mouse-UP" : "");
   }
   (void)c;
   return r;
}

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
   /* WHICH target is this? A handler installed on a target that is not in the
    * dispatch chain for the clicked window can never fire, however healthy the
    * rest of the plumbing looks. Named, not guessed. */
   if (n_install <= 24) {
      EventTargetRef app = GetApplicationEventTarget();
      EventTargetRef disp = GetEventDispatcherTarget();
      EventTargetRef focus = GetUserFocusEventTarget();
      WindowRef fw = FrontWindow();
      EventTargetRef win = fw ? GetWindowEventTarget(fw) : NULL;
      EP("[ev]   target %p is: %s%s%s%s\n", (void *)target,
         target == app   ? "APPLICATION " : "",
         target == disp  ? "DISPATCHER "  : "",
         target == focus ? "USERFOCUS "   : "",
         target == win   ? "FRONT-WINDOW " :
            (target != app && target != disp && target != focus)
               ? "(none of app/dispatcher/focus/front-window)" : "");
   }
   /* Wrap every MOUSE-class handler, whoever installs it, so the log shows who
    * actually consumes a click. Forwards faithfully; the app's own userData is
    * handed back unchanged. */
   EventHandlerUPP use = h;
   void *use_ud = ud;
   int mouse = 0;
   for (ItemCount i = 0; i < n; i++) {
      if (types[i].eventClass == kEventClassMouse) { mouse = 1; break; }
   }
   if (mouse && g_nw < EP_MAX_W) {
      struct wrapctx *w = &g_w[g_nw];
      w->orig = h;
      w->ud = ud;
      /* A translated Halo handler lives in the low 4GB; system handlers are up
       * in the shared cache. That is the discriminator, and it is structural. */
      w->is_halo = ((uintptr_t)h >> 32) == 0;
      w->idx = ++g_nw;
      use = NewEventHandlerUPP(ep_wrap);
      use_ud = w;
      EP("[ev] (wrapping MOUSE handler #%d: %s upp=%p)\n", w->idx,
         w->is_halo ? "HALO" : "SYSTEM", (void *)h);
   }
   const OSStatus r = InstallEventHandler(target, use, n, types, use_ud, out);
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
   /* The request matters as much as the result: a type list that asks only for
    * mouse-DOWN is why no mouse-UP is ever seen, and pullEvent decides whether
    * the event is consumed. Logged once - it is the same every iteration. */
   static int said;
   if (!said) {
      said = 1;
      EP("[ev] ReceiveNextEvent REQUEST: numTypes=%lu timeout=%.4f pullEvent=%d\n",
         (unsigned long)n, (double)to, (int)pull);
      for (ItemCount i = 0; i < n && i < 8; i++) {
         char c[5]; fourcc(c, types[i].eventClass);
         EP("[ev]   requested type[%lu] class='%s' kind=%u\n",
            (unsigned long)i, c, (unsigned)types[i].eventKind);
      }
      if (n == 0) { EP("[ev]   (numTypes=0 -> asking for EVERY event)\n"); }
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
            /* WHICH WINDOW does the OS think was clicked? A click attributed to
             * the wrong window (or to no window) is consumed by that window's
             * standard handler and never reaches the application target. The
             * part code names it: 3 = inContent, 4 = inDrag, 0 = inDesk. */
            if (ki == kEventMouseDown && n_mousedown <= 4) {
               Point qd; qd.h = (short)pt.x; qd.v = (short)pt.y;
               WindowRef hit = NULL;
               const short part = FindWindow(qd, &hit);
               WindowRef front = FrontWindow();
               WindowRef act = ActiveNonFloatingWindow();
               EP("[ev]   FindWindow -> part=%d window=%p | FrontWindow=%p | "
                  "ActiveNonFloating=%p\n", (int)part, (void *)hit,
                  (void *)front, (void *)act);
               if (hit) {
                  Rect b; GetWindowBounds(hit, kWindowContentRgn, &b);
                  EP("[ev]   hit window content = (%d,%d)-(%d,%d) visible=%d\n",
                     (int)b.left, (int)b.top, (int)b.right, (int)b.bottom,
                     (int)IsWindowVisible(hit));
               }
            }
         }
      } else if (cl == kEventClassKeyboard) {
         /* ★Report: pressing ENTER on a menu item does nothing either. If BOTH
          * mouse and keyboard activation fail while the highlight still moves,
          * the fault is unlikely to be in the mouse path specifically - so the
          * key events get the same scrutiny as the clicks. */
         n_key++;
         if (n_key <= 12) {
            UInt32 code = 0; char ch = 0;
            GetEventParameter(*out, kEventParamKeyCode, typeUInt32, NULL,
                              sizeof code, NULL, &code);
            GetEventParameter(*out, kEventParamKeyMacCharCodes, typeChar, NULL,
                              sizeof ch, NULL, &ch);
            EP("[ev] ReceiveNextEvent  KEY kind=%u code=%u char=0x%02x%s\n",
               (unsigned)ki, (unsigned)code, (unsigned char)ch,
               (code == 36 || code == 76) ? "   <-- RETURN/ENTER" : "");
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

/* Halo also imports WaitNextEvent and ConvertEventRefToEventRecord - the
 * CLASSIC event path. A game ported from OS 9 very plausibly handles its menu
 * from an EventRecord rather than from the Carbon handler, so that path is
 * traced too: if the conversion fails, or hands back the wrong `what`/`where`,
 * the click dies there and no amount of Carbon plumbing would show it. */
static unsigned long n_convert, n_convert_fail, n_wne;

static Boolean ep_ConvertEventRefToEventRecord(EventRef e, EventRecord *out) {
   if (!ep_enter("ConvertEventRefToEventRecord")) {
      return ConvertEventRefToEventRecord(e, out);
   }
   const UInt32 cl = e ? GetEventClass(e) : 0, ki = e ? GetEventKind(e) : 0;
   const Boolean r = ConvertEventRefToEventRecord(e, out);
   n_convert++;
   if (!r) { n_convert_fail++; }
   if (cl == kEventClassMouse && (ki == kEventMouseDown || ki == kEventMouseUp)) {
      char c[5]; fourcc(c, cl);
      if (r && out) {
         EP("[ev] ConvertEventRefToEventRecord('%s' kind=%u) -> TRUE  "
            "what=%u where=(%d,%d) mods=0x%x\n", c, (unsigned)ki,
            (unsigned)out->what, (int)out->where.h, (int)out->where.v,
            (unsigned)out->modifiers);
         if (out->what != 1 /*mouseDown*/ && ki == kEventMouseDown) {
            EP("[ev]   ⚠a Carbon mouseDown converted to classic what=%u, not 1\n",
               (unsigned)out->what);
         }
      } else {
         EP("[ev] ConvertEventRefToEventRecord('%s' kind=%u) -> FALSE  "
            "⚠the classic path never sees this click\n", c, (unsigned)ki);
      }
   }
   ep_leave();
   return r;
}

static Boolean ep_WaitNextEvent(EventMask mask, EventRecord *out,
                                UInt32 sleep, RgnHandle rgn) {
   if (!ep_enter("WaitNextEvent")) { return WaitNextEvent(mask, out, sleep, rgn); }
   const Boolean r = WaitNextEvent(mask, out, sleep, rgn);
   n_wne++;
   if (r && out && (out->what == 1 || out->what == 2)) {
      EP("[ev] WaitNextEvent -> classic what=%u where=(%d,%d) mask=0x%x\n",
         (unsigned)out->what, (int)out->where.h, (int)out->where.v,
         (unsigned)mask);
   } else if (n_wne == 1) {
      EP("[ev] WaitNextEvent is being called (mask=0x%x)\n", (unsigned)mask);
   }
   ep_leave();
   return r;
}

/* ---------------------------------------------------------------------------
 * MULTIPROCESSING SERVICES.
 *
 * ★Report: "every time I select a different main menu item, whether with keyboard
 * or mouse, Halo freezes for about a second (the moving backdrop stops)."
 *
 * A ~1s stall on a UI action that should be instant is the signature of a WAIT
 * TIMING OUT, and Halo imports the whole MP family: MPCreateTask, MPCreateQueue,
 * MPCreateSemaphore, MPCreateEvent, MPSetEvent, MPWaitForEvent, MPWaitOnQueue,
 * MPWaitOnSemaphore. That suggests ONE cause for BOTH symptoms: if worker tasks
 * never run (MPCreateTask failing) or their completion is never signalled, then
 * every operation that hands work to a worker and waits blocks until its
 * timeout — the highlight change stalls for the timeout and then continues, and
 * the ACTIVATION waits for a result that never arrives and quietly does nothing.
 *
 * Multiprocessing Services is ancient even by Carbon standards, so "does it
 * still work at all on this OS" is a real question and not a rhetorical one.
 * Every create is logged with its result and every wait is TIMED, because a
 * wait that returns the right code after a second is just as broken as one that
 * fails, and only the clock tells them apart.
 *
 * Duration is SInt32 milliseconds when positive and MICROSECONDS when negative
 * (kDurationMicrosecond = -1), so the raw value is printed as well as its
 * meaning — a sign or scale error there turns a 1 ms wait into a 1 s freeze,
 * which is precisely the reported symptom.
 */
/* Signatures come from CarbonCore/Multiprocessing.h, which is still present. */
static unsigned long n_mp_task, n_mp_task_fail, n_mp_wait, n_mp_slow;
static double        mp_slowest;

static void mp_dur(char *out, size_t n, Duration d) {
   if (d == 0)           snprintf(out, n, "0 (immediate)");
   else if (d == 0x7fffffff) snprintf(out, n, "forever");
   else if (d < 0)       snprintf(out, n, "%d us", -d);
   else                  snprintf(out, n, "%d ms", d);
}

/* One reporter for all the waits: log the first few, and ALWAYS log a wait that
 * blocked long enough for a human to see it. */
static void mp_report(const char *who, Duration timeout, OSStatus r, double dt) {
   n_mp_wait++;
   const int slow = dt > 0.10;
   if (slow) { n_mp_slow++; if (dt > mp_slowest) { mp_slowest = dt; } }
   if (n_mp_wait <= 6 || slow) {
      char d[32]; mp_dur(d, sizeof d, timeout);
      EP("[mp] %-18s timeout=%-14s -> %-6d after %.3fs%s\n",
         who, d, (int)r, dt,
         slow ? "   <-- BLOCKED (a stall a human would notice)" : "");
   }
}

/* ★ DOES THE WORKER ACTUALLY RUN?
 *
 * MPCreateTask returning noErr only proves the NATIVE side accepted the task;
 * it says nothing about whether the entry point is ever reached. And the entry
 * point here is not Halo's function: libabiconv wraps the i386 TaskProc in a
 * callback trampoline (x64_cb_wrap, sig 1) so that the native worker thread can
 * call translated code. That trampoline re-enters the translated callback on a
 * FRESH low-4GB stack, on a thread that has never executed translated code
 * before -- the single least-exercised path in the whole runtime.
 *
 * So interpose the entry point too: substitute our own proc, which records that
 * the worker thread reached us and then calls the trampoline. Three outcomes are
 * now distinguishable, and they have completely different causes:
 *   entered=0            -> the native task never starts the entry point;
 *   entered>0 returned=0 -> the worker entered translated code and never came
 *                           back (a fault or a hang INSIDE the translation);
 *   entered=returned>0   -> workers run fine and the stall is elsewhere.
 * Without this, all three look identical from the main thread: a wait that
 * times out.
 */
#define EP_MAXTASK 8
static TaskProc      task_orig[EP_MAXTASK];
static unsigned long n_task_enter[EP_MAXTASK], n_task_return[EP_MAXTASK];
static int           n_task_slot;

static OSStatus ep_task_common(int i, void *param) {
   n_task_enter[i]++;
   if (n_task_enter[i] <= 2) {
      EP("[mp] >>> WORKER %d ENTERED (thread=%p param=%p)\n", i,
         (void *)pthread_self(), param);
   }
   const OSStatus r = task_orig[i](param);
   n_task_return[i]++;
   if (n_task_return[i] <= 2) {
      EP("[mp] <<< worker %d returned %d\n", i, (int)r);
   }
   return r;
}
static OSStatus ep_task0(void *p) { return ep_task_common(0, p); }
static OSStatus ep_task1(void *p) { return ep_task_common(1, p); }
static OSStatus ep_task2(void *p) { return ep_task_common(2, p); }
static OSStatus ep_task3(void *p) { return ep_task_common(3, p); }
static OSStatus ep_task4(void *p) { return ep_task_common(4, p); }
static OSStatus ep_task5(void *p) { return ep_task_common(5, p); }
static OSStatus ep_task6(void *p) { return ep_task_common(6, p); }
static OSStatus ep_task7(void *p) { return ep_task_common(7, p); }
static const TaskProc ep_task_stub[EP_MAXTASK] = {
   ep_task0, ep_task1, ep_task2, ep_task3,
   ep_task4, ep_task5, ep_task6, ep_task7 };

static OSStatus ep_MPCreateTask(TaskProc e, void *p, ByteCount ss, MPQueueID nq,
                                void *t1, void *t2, MPTaskOptions o,
                                MPTaskID *task) {
   /* Substitute only while a slot is free; past that, pass through untouched
    * rather than silently dropping the app's entry point on the floor. */
   TaskProc use = e;
   int slot = -1;
   if (n_task_slot < EP_MAXTASK) {
      slot = n_task_slot++;
      task_orig[slot] = e;
      use = ep_task_stub[slot];
   }
   const OSStatus r = MPCreateTask(use, p, ss, nq, t1, t2, o, task);
   n_mp_task++;
   if (r != noErr) { n_mp_task_fail++; }
   EP("[mp] MPCreateTask entry=%p stack=%lu slot=%d -> %d%s\n", (void *)e,
      (unsigned long)ss, slot, (int)r,
      r == noErr ? "" : "   <-- FAILED: this worker will never run");
   return r;
}
static OSStatus ep_MPCreateEvent(MPEventID *ev) {
   const OSStatus r = MPCreateEvent(ev);
   EP("[mp] MPCreateEvent -> %d (id=%p)\n", (int)r, ev ? (void *)*ev : NULL);
   return r;
}
static OSStatus ep_MPCreateQueue(MPQueueID *q) {
   const OSStatus r = MPCreateQueue(q);
   EP("[mp] MPCreateQueue -> %d (id=%p)\n", (int)r, q ? (void *)*q : NULL);
   return r;
}
static OSStatus ep_MPCreateSemaphore(MPSemaphoreCount mx, MPSemaphoreCount in,
                                     MPSemaphoreID *sm) {
   const OSStatus r = MPCreateSemaphore(mx, in, sm);
   EP("[mp] MPCreateSemaphore(max=%u init=%u) -> %d\n", (unsigned)mx,
      (unsigned)in, (int)r);
   return r;
}
static OSStatus ep_MPWaitForEvent(MPEventID ev, MPEventFlags *fl, Duration to) {
   const double t0 = ep_now();
   const OSStatus r = MPWaitForEvent(ev, fl, to);
   mp_report("MPWaitForEvent", to, r, ep_now() - t0);
   return r;
}
static OSStatus ep_MPWaitOnQueue(MPQueueID q, void **a, void **b, void **c,
                                 Duration to) {
   const double t0 = ep_now();
   const OSStatus r = MPWaitOnQueue(q, a, b, c, to);
   mp_report("MPWaitOnQueue", to, r, ep_now() - t0);
   return r;
}
static OSStatus ep_MPWaitOnSemaphore(MPSemaphoreID sm, Duration to) {
   const double t0 = ep_now();
   const OSStatus r = MPWaitOnSemaphore(sm, to);
   mp_report("MPWaitOnSemaphore", to, r, ep_now() - t0);
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
   EP("[ev] KEYBOARD events received  : %lu\n", n_key);
   EP("[mp] MPCreateTask calls        : %lu  (%lu FAILED)\n",
      n_mp_task, n_mp_task_fail);
   for (int i = 0; i < n_task_slot; i++) {
      EP("[mp] worker %d                  : entered %lu, returned %lu%s\n", i,
         n_task_enter[i], n_task_return[i],
         n_task_enter[i] == 0
            ? "   <-- NEVER RAN: the task was created but its entry point was "
              "never reached"
            : (n_task_enter[i] > n_task_return[i]
                  ? "   <-- ENTERED AND DID NOT RETURN: it is stuck inside the "
                    "translated callback"
                  : ""));
   }
   EP("[mp] MP waits                  : %lu  (%lu blocked >0.10s, worst %.3fs)\n",
      n_mp_wait, n_mp_slow, mp_slowest);
   if (n_mp_slow) {
      EP("[mp] ⚠MP waits ARE stalling - that is the freeze, and a menu action "
         "that waits the same way would silently do nothing\n");
   }
   EP("[ev] app handler ran on a down : %lu\n", n_handler_mousedown);
   EP("[ev] HALO'S handler entered     : %lu  (on mouse-down: %lu)\n",
      n_halo_handler, n_halo_mousedown);
   EP("[ev] SYSTEM handler on mouse-down: %lu\n", n_sys_mousedown);
   EP("[ev] ConvertEventRefToEventRecord: %lu (%lu returned FALSE)\n",
      n_convert, n_convert_fail);
   EP("[ev] WaitNextEvent calls        : %lu\n", n_wne);
   if (g_recursed) {
      EP("[ev] ⚠THE PROBE RECURSED - treat every count above as unreliable.\n");
   }
}

/* The run ends with the app being KILLED, so atexit never fires and every
 * counter is lost - which is exactly what happened on the first good run.
 * Catch the terminating signals, print, then let the default action proceed. */
static void (*ep_prev[4])(int);
static void ep_sig(int sig) {
   EP("\n[ev] (terminated by signal %d)\n", sig);
   ep_atexit();
   void (*prev)(int) = (sig == SIGTERM) ? ep_prev[0]
                     : (sig == SIGINT)  ? ep_prev[1] : ep_prev[2];
   if (prev && prev != SIG_DFL && prev != SIG_IGN) { prev(sig); }
   signal(sig, SIG_DFL);
   raise(sig);
}

__attribute__((constructor))
static void ep_reg(void) {
   atexit(ep_atexit);
   ep_prev[0] = signal(SIGTERM, ep_sig);
   ep_prev[1] = signal(SIGINT, ep_sig);
   ep_prev[2] = signal(SIGHUP, ep_sig);
}

__attribute__((used)) static struct { const void *repl, *orig; }
ep_interposers[] __attribute__((section("__DATA,__interpose"))) = {
   { (const void *)ep_InstallEventHandler,    (const void *)InstallEventHandler },
   { (const void *)ep_ReceiveNextEvent,       (const void *)ReceiveNextEvent },
   { (const void *)ep_SendEventToEventTarget, (const void *)SendEventToEventTarget },
   { (const void *)ep_CallNextEventHandler,   (const void *)CallNextEventHandler },
   { (const void *)ep_GetEventParameter,      (const void *)GetEventParameter },
   { (const void *)ep_ConvertEventRefToEventRecord,
     (const void *)ConvertEventRefToEventRecord },
   { (const void *)ep_WaitNextEvent,          (const void *)WaitNextEvent },
   { (const void *)ep_MPCreateTask,           (const void *)MPCreateTask },
   { (const void *)ep_MPCreateEvent,          (const void *)MPCreateEvent },
   { (const void *)ep_MPCreateQueue,          (const void *)MPCreateQueue },
   { (const void *)ep_MPCreateSemaphore,      (const void *)MPCreateSemaphore },
   { (const void *)ep_MPWaitForEvent,         (const void *)MPWaitForEvent },
   { (const void *)ep_MPWaitOnQueue,          (const void *)MPWaitOnQueue },
   { (const void *)ep_MPWaitOnSemaphore,      (const void *)MPWaitOnSemaphore },
};
