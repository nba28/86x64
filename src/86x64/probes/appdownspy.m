// appdownspy.m — PvZ windowed: clicks ignored (2026-09-29). DYLD_INSERT spy on
// the NATIVE SendEventToEventTarget libabiconv's carbon_event_appdown.c calls.
// Per mouse DOWN/UP: which target (dispatcher / application / other), the
// status, the window under the point and how deep the hit HIView sits (the
// appdown forwarder only re-sends a DOWN whose hit view is root or content).
// build: clang -arch x86_64 -dynamiclib -framework Carbon appdownspy.m -o /tmp/appdownspy.dylib
// Also logs every mouse DOWN/UP the app pulls (ReceiveNextEvent), how long the
// dispatcher held a DOWN, and the button state when it returned.
// run:   DYLD_INSERT_LIBRARIES=/tmp/appdownspy.dylib <app binary> 2>/tmp/appdown.log
#import <Carbon/Carbon.h>
#include <stdio.h>
/* Exported by HIToolbox, hidden from the 64-bit headers. */
typedef struct OpaqueControlRef *HIViewRef_;
extern short     FindWindow(Point, WindowRef *);
extern HIViewRef_ HIViewGetRoot(WindowRef);
extern OSStatus  HIViewGetViewForMouseEvent(HIViewRef_, EventRef, HIViewRef_ *);
extern HIViewRef_ HIViewGetSuperview(HIViewRef_);
extern WindowRef ActiveNonFloatingWindow(void);
extern WindowRef FrontWindow(void);
extern WindowRef FrontNonFloatingWindow(void);
extern OSStatus GetWindowBounds(WindowRef, WindowRegionCode, Rect *);
extern Boolean IsWindowVisible(WindowRef);
static void origin(WindowRef w, char *buf, size_t n) {
   Rect r = {0};
   if (!w || GetWindowBounds(w, kWindowContentRgn, &r) != noErr) { snprintf(buf, n, "-"); return; }
   snprintf(buf, n, "%p@(%d,%d %dx%d)%s", (void *)w, r.left, r.top, r.right - r.left, r.bottom - r.top,
            IsWindowVisible(w) ? "" : "hidden");
}

static __thread int g_in_down;
static void fourcc(UInt32 c, char *o) { o[0]=(char)(c>>24); o[1]=(char)(c>>16); o[2]=(char)(c>>8); o[3]=(char)c; o[4]=0; }

static OSStatus my_send(EventRef e, EventTargetRef t) {
   const UInt32 cl = GetEventClass(e), ki = GetEventKind(e);
   const int log = cl == kEventClassMouse && (ki == kEventMouseDown || ki == kEventMouseUp);
   const double t0 = GetCurrentEventTime();
   const int watch = log && ki == kEventMouseDown && t == GetApplicationEventTarget();
   if (watch) { g_in_down++; fprintf(stderr, "[appdown] >> app handler for DOWN begins\n"); }
   OSStatus r = SendEventToEventTarget(e, t);
   if (watch) { g_in_down--; fprintf(stderr, "[appdown] << app handler done, status=%d\n", (int)r); }
   if (!log) return r;
   const double held_ms = (GetCurrentEventTime() - t0) * 1000.0;
   const UInt32 btn_after = GetCurrentEventButtonState();
   const char *tn = t == GetEventDispatcherTarget() ? "dispatcher"
                  : t == GetApplicationEventTarget() ? "application" : "other";
   HIPoint p = {0}; GetEventParameter(e, kEventParamMouseLocation, typeHIPoint, NULL, sizeof p, NULL, &p);
   WindowRef w = NULL; Point q = { (short)p.y, (short)p.x };
   short part = FindWindow(q, &w);
   int depth = -1; const char *cls = "-";
   if (w) {
      HIViewRef_ root = HIViewGetRoot(w), hit = NULL;
      if (root && HIViewGetViewForMouseEvent(root, e, &hit) == noErr && hit) {
         depth = 0; for (HIViewRef_ v = hit; v && v != root; v = HIViewGetSuperview(v)) depth++;
         CFStringRef c = HIObjectCopyClassID((HIObjectRef)hit);
         static char buf[128]; buf[0] = 0;
         if (c) { CFStringGetCString(c, buf, sizeof buf, kCFStringEncodingUTF8); CFRelease(c); cls = buf; }
      }
   }
   char clicked[96], active[96], front[96];
   origin(w, clicked, sizeof clicked);
   origin(ActiveNonFloatingWindow(), active, sizeof active);
   origin(FrontWindow(), front, sizeof front);
   if (t == GetEventDispatcherTarget() && ki == kEventMouseDown)
      fprintf(stderr, "[appdown]   clicked=%s active=%s front=%s\n", clicked, active, front);
   fprintf(stderr, "[appdown] %s -> %s status=%d at(%.0f,%.0f) part=%d win=%p hit-depth=%d class=%s held=%.0fms button-after=%u\n",
           ki == kEventMouseDown ? "DOWN" : "UP  ", tn, (int)r, p.x, p.y, part, w, depth, cls, held_ms, (unsigned)btn_after);
   return r;
}

/* What the app PULLS from the queue: does an UP ever reach its pump? */
static OSStatus my_recv(ItemCount n, const EventTypeSpec *l, EventTimeout to, Boolean pull, EventRef *out) {
   OSStatus r = ReceiveNextEvent(n, l, to, pull, out);
   if (r == noErr && out && *out && GetEventClass(*out) == kEventClassMouse) {
      UInt32 k = GetEventKind(*out);
      if (k == kEventMouseDown || k == kEventMouseUp)
         fprintf(stderr, "[appdown] pulled %s\n", k == kEventMouseDown ? "DOWN" : "UP");
   }
   return r;
}

/* Inside the app's DOWN handler: what it reads and which windows it asks about. */
static OSStatus my_gep(EventRef e, EventParamName n, EventParamType t, EventParamType *ot,
                       ByteCount sz, ByteCount *osz, void *d) {
   OSStatus r = GetEventParameter(e, n, t, ot, sz, osz, d);
   if (g_in_down) {
      char a[5], b[5]; fourcc(n, a); fourcc(t, b);
      fprintf(stderr, "[appdown]    GetEventParameter '%s' as '%s' -> %d", a, b, (int)r);
      if (r == noErr && t == typeWindowRef && d) fprintf(stderr, " = %p", *(void **)d);
      if (r == noErr && (t == typeQDPoint) && d) fprintf(stderr, " = (%d,%d)", ((Point *)d)->h, ((Point *)d)->v);
      if (r == noErr && (t == typeHIPoint) && d) fprintf(stderr, " = (%.0f,%.0f)", ((HIPoint *)d)->x, ((HIPoint *)d)->y);
      fprintf(stderr, "\n");
   }
   return r;
}
#define WQ(name, ret, args, call, fmt) \
   static ret my_##name args { ret r = call; if (g_in_down) fprintf(stderr, "[appdown]    " #name " -> " fmt "\n", r); return r; }
WQ(FrontWindow, WindowRef, (void), FrontWindow(), "%p")
WQ(FrontNonFloatingWindow, WindowRef, (void), FrontNonFloatingWindow(), "%p")
WQ(ActiveNonFloatingWindow, WindowRef, (void), ActiveNonFloatingWindow(), "%p")
extern Boolean IsWindowActive(WindowRef);
extern WindowRef GetUserFocusWindow(void);
static Boolean my_IsWindowActive(WindowRef w) { Boolean r = IsWindowActive(w); if (g_in_down) fprintf(stderr, "[appdown]    IsWindowActive(%p) -> %d\n", w, r); return r; }
WQ(GetUserFocusWindow, WindowRef, (void), GetUserFocusWindow(), "%p")
static short my_FindWindow(Point p, WindowRef *w) { short r = FindWindow(p, w); if (g_in_down) fprintf(stderr, "[appdown]    FindWindow(%d,%d) -> part %d win %p\n", p.h, p.v, r, w ? *w : 0); return r; }

/* Which events the app listens for, and where (window / application target). */
static OSStatus my_install(EventTargetRef t, EventHandlerUPP h, ItemCount n, const EventTypeSpec *l,
                           void *ud, EventHandlerRef *out) {
   const char *tn = t == GetApplicationEventTarget() ? "application"
                  : t == GetEventDispatcherTarget() ? "dispatcher" : "window/other";
   fprintf(stderr, "[appdown] InstallEventHandler on %s:", tn);
   for (ItemCount i = 0; i < n && l; i++) {
      UInt32 c = l[i].eventClass;
      fprintf(stderr, " %c%c%c%c/%u", (char)(c >> 24), (char)(c >> 16), (char)(c >> 8), (char)c, (unsigned)l[i].eventKind);
   }
   fprintf(stderr, "\n");
   return InstallEventHandler(t, h, n, l, ud, out);
}

__attribute__((used, section("__DATA,__interpose")))
static struct { const void *repl, *orig; } interposers[] = {
   { (const void *)my_send, (const void *)SendEventToEventTarget },
   { (const void *)my_recv, (const void *)ReceiveNextEvent },
   { (const void *)my_install, (const void *)InstallEventHandler },
   { (const void *)my_gep, (const void *)GetEventParameter },
   { (const void *)my_FrontWindow, (const void *)FrontWindow },
   { (const void *)my_FrontNonFloatingWindow, (const void *)FrontNonFloatingWindow },
   { (const void *)my_ActiveNonFloatingWindow, (const void *)ActiveNonFloatingWindow },
   { (const void *)my_IsWindowActive, (const void *)IsWindowActive },
   { (const void *)my_GetUserFocusWindow, (const void *)GetUserFocusWindow },
   { (const void *)my_FindWindow, (const void *)FindWindow },
};
