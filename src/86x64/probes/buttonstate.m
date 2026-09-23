/* buttonstate.m — why is a button invisible? Log every button-cell DRAW.
 *
 * DYLD_INSERT_LIBRARIES probe (native x86_64). Swizzles
 * -[NSButtonCell drawWithFrame:inView:] and -drawBezelWithFrame:inView: and
 * prints, per draw: title, cell class, the frame it is given, whether its
 * window is key, highlighted/enabled state, the key equivalent (RETURN marks
 * the DEFAULT button) and the view's alpha. A button that is never drawn, or
 * drawn with a zero/offscreen frame, is a LAYOUT problem; one drawn normally
 * yet invisible is a COLOUR/alpha problem.
 *
 * (No timers and no new ObjC classes: under translation, a probe that schedules
 * work on the main queue from its constructor kills the app — measured.)
 *
 * Build:
 *   clang -arch x86_64 -dynamiclib -framework AppKit -o /tmp/buttonstate.dylib buttonstate.m
 */
#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include <stdio.h>

static void (*o_draw)(id, SEL, NSRect, id);
static void (*o_bezel)(id, SEL, NSRect, id);

/* Deliberately minimal: print the raw facts FIRST and flush, then the ObjC
 * accessors, each guarded — under translation a cell may be a legacy object
 * whose accessors re-enter the bridge. */
static void say(const char *what, id cell, NSRect f, NSView *v)
{
   fprintf(stderr, "[btn] %-6s cell=%p cls=%s frame=(%.1f,%.1f,%.1f,%.1f) view=%p\n",
           what, (void *)cell, cell ? class_getName(object_getClass(cell)) : "nil",
           f.origin.x, f.origin.y, f.size.width, f.size.height, (void *)v);
   fflush(stderr);
}

static void n_draw(id s, SEL c, NSRect f, id v)  { say("draw",  s, f, v); o_draw(s, c, f, v); }
static void n_bezel(id s, SEL c, NSRect f, id v) { say("bezel", s, f, v); o_bezel(s, c, f, v); }

static void hook(Class k, SEL sel, IMP repl, void *save)
{
   Method m = class_getInstanceMethod(k, sel);
   if (!m) { fprintf(stderr, "[btn] no %s\n", sel_getName(sel)); return; }
   *(IMP *)save = method_getImplementation(m);
   method_setImplementation(m, repl);
}

__attribute__((constructor))
static void arm(void)
{
   Class k = objc_getClass("NSButtonCell");
   if (!k) { fprintf(stderr, "[btn] no NSButtonCell\n"); return; }
   hook(k, @selector(drawWithFrame:inView:),      (IMP)n_draw,  &o_draw);
   hook(k, @selector(drawBezelWithFrame:inView:), (IMP)n_bezel, &o_bezel);
   fprintf(stderr, "[btn] armed\n");
   fflush(stderr);
}
