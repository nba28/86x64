/* snapshot.m — what does a view snapshot ACTUALLY capture?
 *
 * DYLD_INSERT_LIBRARIES probe (native x86_64): swizzles the two legacy
 * view-snapshot paths — -[NSBitmapImageRep initWithFocusedViewRect:] and
 * -[NSView cacheDisplayInRect:toBitmapImageRep:] — logs each call and saves
 * the first few captured bitmaps as PNGs to $SNAPSHOT_DIR (default /tmp), so
 * a missing sprite/gradient in a reflection or thumbnail can be seen directly.
 *
 * Build:
 *   clang -arch x86_64 -dynamiclib -framework AppKit -o /tmp/snapshot.dylib snapshot.m
 * SNAPSHOT_VIEW=<class>: every 5 s also re-render every view of that class
 * offscreen (cacheDisplayInRect:) and save it — compare with a screenshot to
 * tell "the view draws wrong" from "its drawing never reaches the screen".
 *
 * Use:
 *   SNAPSHOT_DIR=/tmp/snap DYLD_INSERT_LIBRARIES=/tmp/snapshot.dylib App.app/Contents/MacOS/App
 */
#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include <stdio.h>

static id   (*o_focused)(id, SEL, NSRect);
static void (*o_cache)(id, SEL, NSRect, id);
static int  g_n;
static void (*o_sndr)(id, SEL, NSRect);
static Class g_watch;

/* SNAPSHOT_VIEW also logs every invalidation of that class. */
static void n_sndr(id s, SEL c, NSRect r)
{
   if (g_watch && [s isKindOfClass:g_watch])
      fprintf(stderr, "[snapshot] setNeedsDisplayInRect: {%g,%g,%g,%g} on %s bounds.h=%g\n",
              r.origin.x, r.origin.y, r.size.width, r.size.height,
              class_getName([s class]), [s bounds].size.height);
   o_sndr(s, c, r);
}

static void save(NSBitmapImageRep *rep, const char *what)
{
   static int cap = -1;
   if (cap < 0) cap = getenv("SNAPSHOT_MAX") ? atoi(getenv("SNAPSHOT_MAX")) : 12;
   if (!rep || g_n >= cap) return;
   const char *dir = getenv("SNAPSHOT_DIR") ?: "/tmp";
   NSString *p = [NSString stringWithFormat:@"%s/snap_%04d_%s.png", dir, g_n++, what];
   [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:p atomically:NO];
   fprintf(stderr, "[snapshot] saved %s\n", [p UTF8String]);
}

static id n_focused(id s, SEL c, NSRect r)
{
   id rep = o_focused(s, c, r);
   fprintf(stderr, "[snapshot] initWithFocusedViewRect: {%g,%g,%g,%g} view=%s -> %p\n",
           r.origin.x, r.origin.y, r.size.width, r.size.height,
           [NSView focusView] ? class_getName([[NSView focusView] class]) : "-", rep);
   save(rep, "focused");
   return rep;
}

static void n_cache(id s, SEL c, NSRect r, id rep)
{
   o_cache(s, c, r, rep);
   fprintf(stderr, "[snapshot] cacheDisplayInRect: {%g,%g,%g,%g} view=%s layer=%d\n",
           r.origin.x, r.origin.y, r.size.width, r.size.height,
           class_getName([s class]), [s layer] != nil);
   save(rep, "cache");
}

static void find(NSView *v, Class k, NSMutableArray *out)
{
   if ([v isKindOfClass:k]) [out addObject:v];
   for (NSView *s in [v subviews]) find(s, k, out);
}

static void render_views(Class k)
{
   NSMutableArray *vs = [NSMutableArray array];
   for (NSWindow *w in [NSApp windows]) find([[w contentView] superview], k, vs);
   for (NSView *v in vs) {
      NSBitmapImageRep *rep = [v bitmapImageRepForCachingDisplayInRect:[v bounds]];
      [v cacheDisplayInRect:[v bounds] toBitmapImageRep:rep];
      fprintf(stderr, "[snapshot] SNAPSHOT_VIEW %s frame={%g,%g,%g,%g} hidden=%d alpha=%g layer=%p\n",
              class_getName(k), [v frame].origin.x, [v frame].origin.y,
              [v frame].size.width, [v frame].size.height, [v isHiddenOrHasHiddenAncestor],
              [v alphaValue], [v layer]);
      save(rep, "view");
   }
}

__attribute__((constructor)) static void init(void)
{
   const char *vc = getenv("SNAPSHOT_VIEW");
   if (vc) {
      [NSTimer scheduledTimerWithTimeInterval:5 repeats:YES block:^(NSTimer *t) {
         Class k = objc_getClass(vc);
         g_watch = k;
         if (k) render_views(k);
      }];
   }
   Method m = class_getInstanceMethod([NSBitmapImageRep class], @selector(initWithFocusedViewRect:));
   o_focused = (void *)method_setImplementation(m, (IMP)n_focused);
   m = class_getInstanceMethod([NSView class], @selector(cacheDisplayInRect:toBitmapImageRep:));
   o_cache = (void *)method_setImplementation(m, (IMP)n_cache);
   m = class_getInstanceMethod([NSView class], @selector(setNeedsDisplayInRect:));
   o_sndr = (void *)method_setImplementation(m, (IMP)n_sndr);
   fprintf(stderr, "[snapshot] armed\n");
}
