/* resizeways.m — find ANY way to resize a Carbon window that does not NSCGSPanic.
 *
 * Control (`plain`) is known fatal: a visible NSCarbonWindow resized with
 * SetWindowBounds dies with SIGILL in NSCGSPanic <- _createContext. Each variant
 * runs in its OWN process because the panic is a trap, not an exception, so the
 * shell exit status IS the measurement: 0 = survived, 132 = SIGILL.
 */
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern OSStatus CreateNewWindow(WindowClass, WindowAttributes, const Rect *, WindowRef *);
extern void     ShowWindow(WindowRef);
extern void     HideWindow(WindowRef);
extern void     SizeWindow(WindowRef, short, short, Boolean);
extern OSStatus SetWindowBounds(WindowRef, WindowRegionCode, const Rect *);
extern OSStatus GetWindowBounds(WindowRef, WindowRegionCode, Rect *);

static NSWindow *cocoa_mirror(void) {
   for (NSWindow *w in [NSApp windows]) { return w; }
   return nil;
}

int main(int argc, char **argv) {
   const char *mode = argc > 1 ? argv[1] : "plain";
   [NSApplication sharedApplication];
   Rect r = { 200, 200, 200 + 622, 200 + 800 };
   WindowRef w = NULL;
   /* (1u<<19) = compositing: without it CreateNewWindow returns -5601 here. */
   OSStatus st = CreateNewWindow(kDocumentWindowClass,
                                 (1u << 19) | kWindowStandardDocumentAttributes,
                                 &r, &w);
   printf("[rw] CreateNewWindow st=%d w=%p mode=%s\n", (int)st, (void *)w, mode);
   if (!w) { return 3; }

   /* Modes that manage their OWN initial ShowWindow (they need to act on a
    * never-yet-shown window). Everything else is shown up front. */
   static const char *self_show[] = {
      "presize", "presize_twice", "presize_same", "move_twice", "presize_move",
      "presize_show_move", "coalesce", "nresize", "reshow", NULL
   };
   int preshow = 0;
   for (int i = 0; self_show[i]; i++) {
      if (!strcmp(mode, self_show[i])) { preshow = 1; break; }
   }
   if (!preshow) { ShowWindow(w); }

   NSWindow *mir = cocoa_mirror();
   printf("[rw] mirror=%s visible=%d\n",
          mir ? [[mir className] UTF8String] : "(none)",
          mir ? (int)[mir isVisible] : -1);

   Rect t = { 0, 0, 480, 640 };            /* the resize everyone dies on */

   if (!strcmp(mode, "plain")) {
      st = SetWindowBounds(w, kWindowContentRgn, &t);
   } else if (!strcmp(mode, "presize")) {
      st = SetWindowBounds(w, kWindowContentRgn, &t);   /* before ever shown */
      ShowWindow(w);
   } else if (!strcmp(mode, "hide")) {
      HideWindow(w);
      st = SetWindowBounds(w, kWindowContentRgn, &t);
      ShowWindow(w);
   } else if (!strcmp(mode, "sizewindow")) {
      SizeWindow(w, 640, 480, true);
      st = 0;
   } else if (!strcmp(mode, "orderout")) {
      [mir orderOut:nil];
      st = SetWindowBounds(w, kWindowContentRgn, &t);
      [mir orderFront:nil];
   } else if (!strcmp(mode, "structure")) {
      st = SetWindowBounds(w, kWindowStructureRgn, &t);
   } else if (!strcmp(mode, "cocoa")) {
      /* Resize the AppKit mirror instead of the Carbon window. */
      [mir setFrame:NSMakeRect(0, 0, 640, 480) display:NO];
      st = 0;
   } else if (!strcmp(mode, "reshow")) {
      /* Is it "currently visible" or "has EVER been shown"? presize survives,
       * so resize once before showing, then show, then resize AGAIN. */
      Rect t1 = { 0, 0, 500, 700 };
      st = SetWindowBounds(w, kWindowContentRgn, &t1);
      ShowWindow(w);
      st = SetWindowBounds(w, kWindowContentRgn, &t);
   } else if (!strcmp(mode, "presize_twice")) {
      Rect t1 = { 0, 0, 500, 700 };
      st = SetWindowBounds(w, kWindowContentRgn, &t1);
      st = SetWindowBounds(w, kWindowContentRgn, &t);   /* both before show */
      ShowWindow(w);
   } else if (!strcmp(mode, "presize_same")) {
      /* Two IDENTICAL resizes before show. If this survives, the predicate is
       * "one EFFECTIVE size change", not "one call". */
      st = SetWindowBounds(w, kWindowContentRgn, &t);
      st = SetWindowBounds(w, kWindowContentRgn, &t);
      ShowWindow(w);
   } else if (!strcmp(mode, "move_twice")) {
      /* Two pure MOVES before show (size never changes). If this survives, only
       * size changes are counted. */
      Rect m1 = { 100, 100, 100 + 622, 100 + 800 };
      Rect m2 = { 300, 300, 300 + 622, 300 + 800 };
      st = SetWindowBounds(w, kWindowContentRgn, &m1);
      st = SetWindowBounds(w, kWindowContentRgn, &m2);
      ShowWindow(w);
   } else if (!strcmp(mode, "presize_move")) {
      /* One resize, then a pure move, both before show. */
      Rect mv = { 300, 300, 300 + 480, 300 + 640 };
      st = SetWindowBounds(w, kWindowContentRgn, &t);
      st = SetWindowBounds(w, kWindowContentRgn, &mv);
      ShowWindow(w);
   } else if (!strcmp(mode, "presize_show_move")) {
      /* The candidate FIX shape: spend the one survivable resize before show,
       * then only ever MOVE afterwards. */
      Rect mv = { 300, 300, 300 + 480, 300 + 640 };
      st = SetWindowBounds(w, kWindowContentRgn, &t);
      ShowWindow(w);
      st = SetWindowBounds(w, kWindowContentRgn, &mv);
   } else if (!strcmp(mode, "coalesce")) {
      /* Halo asks for five bounds changes. Apply ONLY the last, before show. */
      Rect want[5] = { {0,0,600,800}, {0,0,480,640}, {0,0,768,1024},
                       {0,0,600,800}, {0,0,480,640} };
      Rect last = want[4];
      for (int i = 0; i < 5; i++) { last = want[i]; }   /* remember, apply none */
      st = SetWindowBounds(w, kWindowContentRgn, &last);
      ShowWindow(w);
   } else if (!strcmp(mode, "nresize")) {
      /* How many pre-show resizes survive? argv[2] = count. */
      int n = argc > 2 ? atoi(argv[2]) : 1;
      for (int i = 0; i < n; i++) {
         Rect ti = { 0, 0, (short)(480 + i * 8), (short)(640 + i * 8) };
         st = SetWindowBounds(w, kWindowContentRgn, &ti);
         printf("[rw] resize #%d -> %dx%d st=%d\n", i + 1,
                ti.right - ti.left, ti.bottom - ti.top, (int)st);
         fflush(stdout);
      }
      ShowWindow(w);
   } else {
      printf("[rw] unknown mode\n"); return 3;
   }

   Rect got = { 0, 0, 0, 0 };
   GetWindowBounds(w, kWindowContentRgn, &got);
   printf("[rw] SURVIVED st=%d content now %dx%d\n",
          (int)st, got.right - got.left, got.bottom - got.top);
   return 0;
}
