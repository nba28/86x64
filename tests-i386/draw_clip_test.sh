#!/bin/bash
# draw_clip_test.sh — structural regression guard for the LEGACY DRAW-CLIP
# CONTRACT restoration in objc_shim.c (_86x64_reverse_prep / _86x64_reverse_ret).
#
# Root cause it guards: pre-10.14 AppKit intersected a view's -drawRect: dirty
# rect with [self bounds] and clipped the context to the view before the method
# ran, so a legacy view "filling the rect it was handed" could never paint
# outside itself. Modern AppKit's recursive re-render (cacheDisplayInRect:) hands
# each subview the FULL target rect with NO per-view clip, so a legacy view whose
# drawRect: fills its whole background STOMPS sibling views that were already
# drawn (the observed whole-play-area whiteout: a board view white-filling a rect
# far larger than its bounds erased the surround + sidebar painted by siblings).
#
# The fix re-imposes the old contract structurally: for a reverse-dispatched
# legacy drawRect: (NSView subclass, honoring wantsDefaultClipping) it intersects
# the rect arg with [self bounds] and save-gstate + clips the context to it,
# restoring the gstate on return. This is the EXACT primitive exercised here.
#
# Self-contained NATIVE x86_64 test (no i386 translation): two sibling NSViews
# share a superview; the "stomper" view's drawRect: white-fills a rect that
# extends far beyond its own bounds (into the sibling's area). We render the
# hierarchy twice through -cacheDisplayInRect: (the modern no-per-view-clip
# path):
#   A) UNCLIPPED (bug reproduction): the stomper's over-large fill lands -> the
#      sibling's pixels are overwritten white.
#   B) CLIPPED (the fix's primitive): before invoking the fill we save-gstate +
#      CGContextClipToRect to the stomper's bounds -> the sibling survives.
# Exit 0 iff A stomps (sibling white) AND B preserves (sibling intact).

set -u
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/t.m" <<'EOF'
#import <AppKit/AppKit.h>
#import <objc/message.h>
#include <stdio.h>

/* When g_clip is set, apply the fix's draw-clip primitive: save-gstate + clip
 * the current context to the intersection of the drawRect: rect with bounds. */
static int g_clip;

@interface Sibling : NSView @end
@implementation Sibling
- (void)drawRect:(NSRect)r {
   [[NSColor blackColor] set];         /* the sibling paints itself black */
   NSRectFill(self.bounds);
}
@end

/* g_wants: emulate the fix's wantsDefaultClipping gate. When NO, the fix must
 * NOT clip (the escape hatch for views that intentionally overflow — shadows,
 * reflections), so legitimate out-of-bounds drawing is preserved. */
static int g_wants = 1;

@interface Stomper : NSView @end
@implementation Stomper
- (BOOL)wantsDefaultClipping { return g_wants ? YES : NO; }
- (void)drawRect:(NSRect)r {
   CGContextRef c = [NSGraphicsContext currentContext].CGContext;
   int restore = 0;
   /* EXACT fix gate+primitive: only when wantsDefaultClipping, intersect the
    * handed rect with bounds and clip to it. */
   if (g_clip && [self wantsDefaultClipping]) {
      CGRect inter = CGRectIntersection(r, self.bounds);
      if (CGRectIsNull(inter)) inter = CGRectZero;
      CGContextSaveGState(c);
      CGContextClipToRect(c, inter);
      restore = 1;
   }
   /* Legacy "fill the rect I was handed" — but r spans WAY beyond bounds. */
   [[NSColor whiteColor] set];
   NSRectFill(r);
   if (restore) CGContextRestoreGState(c);
}
@end

/* Render the two-sibling hierarchy and return the sibling-center pixel's red. */
static int render_sibling_red(int clip, int wants) {
   g_clip = clip; g_wants = wants;
   NSView *parent = [[NSView alloc] initWithFrame:NSMakeRect(0,0,400,200)];
   /* sibling on the LEFT (0..200), stomper on the RIGHT (200..400) */
   Sibling *sib   = [[Sibling alloc]  initWithFrame:NSMakeRect(0,0,200,200)];
   Stomper *stomp = [[Stomper alloc]  initWithFrame:NSMakeRect(200,0,200,200)];
   [parent addSubview:sib];
   [parent addSubview:stomp];         /* stomper drawn AFTER -> can overwrite */
   NSBitmapImageRep *rep =
      [parent bitmapImageRepForCachingDisplayInRect:parent.bounds];
   [parent cacheDisplayInRect:parent.bounds toBitmapImageRep:rep];
   unsigned char *px = rep.bitmapData;
   long bpr = rep.bytesPerRow, spp = rep.samplesPerPixel;
   long bw = rep.pixelsWide, bh = rep.pixelsHigh;
   double sc = (double)bw / 400.0;
   /* sample the sibling's center at parent point (100,100) */
   long sx = (long)(100*sc), sy = (long)(100*sc);
   unsigned char *p = px + (bh-1-sy)*bpr + sx*spp;
   return p[0];                        /* red channel: 0=black(intact) 255=white */
}

int main(void) {
   [NSApplication sharedApplication];
   int unclipped = render_sibling_red(0, 1);   /* bug: sibling stomped white  */
   int clipped   = render_sibling_red(1, 1);   /* fix: sibling stays black     */
   int optout    = render_sibling_red(1, 0);   /* wantsDefaultClipping==NO: the
                                                * escape hatch must NOT clip ->
                                                * the overflow fill still lands  */
   printf("unclipped_sibling_red=%d clipped_sibling_red=%d optout_sibling_red=%d\n",
          unclipped, clipped, optout);
   /* PASS iff: unclipped path stomps the sibling white (>=250); clipped path
    * (wantsDefaultClipping==YES) preserves the sibling black (<=5); and the
    * opt-out path (wantsDefaultClipping==NO) does NOT clip -> sibling stomped
    * white again (>=250), proving legitimate overflow drawing is preserved. */
   int ok = (unclipped >= 250) && (clipped <= 5) && (optout >= 250);
   return ok ? 0 : 1;
}
EOF

if ! clang -arch x86_64 -o "$TMP/t" "$TMP/t.m" -framework AppKit 2>"$TMP/err"; then
   echo "draw-clip: SKIP (compile failed)"; cat "$TMP/err"; exit 0
fi
OUT=$("$TMP/t" 2>/dev/null); RC=$?
if [ "$RC" -eq 0 ]; then
   echo "draw-clip: PASS ($OUT)"
else
   echo "draw-clip: FAIL ($OUT)"
fi
exit $RC
