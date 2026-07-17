#!/bin/bash
#
# Regression guard for the LAYER-BACKING DRAW-STATE INVARIANCE that the
# translator's legacy-AppKit drawing compats rest on (audit finding: NO
# layer-vs-non-layer divergence exists in the headless draw path on modern
# macOS — this guard pins that invariance as a canary).
#
# Measured ground truth (macOS 15-era, audit 2026-07):
#  - For a never-shown window, [view display]/displayIfNeeded invokes ZERO
#    drawRect: regardless of layer-backing (all headless capture flows through
#    the cacheDisplay recursive re-render — the premise of the snapshot compat).
#  - cacheDisplayInRect: hands every subview the identical in-draw contract
#    (full target rect arg, full-area clip, same CTM/antialias/interpolation)
#    and produces identical pixels whether the tree is plain or force-migrated
#    to layer-backing (wantsLayer=YES on the content view — the same mechanism
#    AppKit uses when a CA-requiring view joins a legacy tree).
#
# The guard asserts INVARIANCE (plain == layer-backed), not absolute values,
# so uniform OS evolution won't false-alarm; only a genuine layer-vs-not
# DIVERGENCE — the thing that would silently change what translated legacy
# view trees render when AppKit force-migrates them — trips it.
set -u

fail() { echo "FAIL layerback-invariance: $1"; exit 1; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/t.m" <<'EOF'
#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>

struct rec {
    int draw_calls_display;   /* drawRect: fired by [cv display] (headless) */
    int calls;                /* drawRect: fired by cacheDisplay */
    CGRect arg, clip;
    CGAffineTransform ctm;
    int aa;
    long interp;
    unsigned char samp[12];   /* parent / childL / childR RGB from cacheDisplay */
};
static int g_in_cache;
static struct rec *g_cur;

@interface LIParent : NSView
@end
@implementation LIParent
- (BOOL)isOpaque { return YES; }
- (void)drawRect:(NSRect)r {
    if (!g_in_cache) { g_cur->draw_calls_display++; }
    [[NSColor redColor] set];
    NSRectFill([self bounds]);
}
@end

@interface LIChild : NSView
@end
@implementation LIChild
- (BOOL)isOpaque { return NO; }
- (void)drawRect:(NSRect)r {
    if (!g_in_cache) { g_cur->draw_calls_display++; return; }
    g_cur->calls++;
    g_cur->arg = NSRectToCGRect(r);
    NSGraphicsContext *gc = [NSGraphicsContext currentContext];
    g_cur->clip = CGContextGetClipBoundingBox(gc.CGContext);
    g_cur->ctm = CGContextGetCTM(gc.CGContext);
    g_cur->aa = gc.shouldAntialias;
    g_cur->interp = gc.imageInterpolation;
    [[NSColor greenColor] set];
    NSRectFill(NSMakeRect(0, 0, 50, 50));
}
@end

static unsigned char *px(NSBitmapImageRep *rep, int x, int yview) {
    long H = rep.pixelsHigh, bpr = rep.bytesPerRow;
    return rep.bitmapData + (H - 1 - yview) * bpr + x * 4;
}

static void run_case(int layered, struct rec *R) {
    g_cur = R;
    NSWindow *w = [[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,200,100)
                                              styleMask:0
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    NSView *cv = [w contentView];
    LIParent *parent = [[LIParent alloc] initWithFrame:NSMakeRect(0,0,200,100)];
    [cv addSubview:parent];
    LIChild *child = [[LIChild alloc] initWithFrame:NSMakeRect(50,25,100,50)];
    [parent addSubview:child];
    if (layered) [cv setWantsLayer:YES];

    g_in_cache = 0;
    [cv display];                 /* never-shown window: headless display */

    g_in_cache = 1;
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL pixelsWide:200 pixelsHigh:100
        bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
        colorSpaceName:NSCalibratedRGBColorSpace bytesPerRow:0 bitsPerPixel:0];
    [rep setSize:NSMakeSize(200,100)];
    [parent cacheDisplayInRect:parent.bounds toBitmapImageRep:rep];
    g_in_cache = 0;

    memcpy(R->samp + 0, px(rep, 20, 50), 3);    /* parent-only: red */
    memcpy(R->samp + 3, px(rep, 70, 50), 3);    /* child left: green */
    memcpy(R->samp + 6, px(rep, 125, 50), 3);   /* child right: parent through */
}

int main(void) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        struct rec plain, layered;
        memset(&plain, 0, sizeof plain);
        memset(&layered, 0, sizeof layered);
        run_case(0, &plain);
        run_case(1, &layered);

        fprintf(stderr,
            "plain:   disp=%d cache=%d arg=[%.0f %.0f %.0f %.0f] "
            "clip=[%.0f %.0f %.0f %.0f] aa=%d interp=%ld samp=%02x%02x%02x/"
            "%02x%02x%02x/%02x%02x%02x\n",
            plain.draw_calls_display, plain.calls,
            plain.arg.origin.x, plain.arg.origin.y,
            plain.arg.size.width, plain.arg.size.height,
            plain.clip.origin.x, plain.clip.origin.y,
            plain.clip.size.width, plain.clip.size.height,
            plain.aa, plain.interp,
            plain.samp[0],plain.samp[1],plain.samp[2],
            plain.samp[3],plain.samp[4],plain.samp[5],
            plain.samp[6],plain.samp[7],plain.samp[8]);
        fprintf(stderr,
            "layered: disp=%d cache=%d arg=[%.0f %.0f %.0f %.0f] "
            "clip=[%.0f %.0f %.0f %.0f] aa=%d interp=%ld samp=%02x%02x%02x/"
            "%02x%02x%02x/%02x%02x%02x\n",
            layered.draw_calls_display, layered.calls,
            layered.arg.origin.x, layered.arg.origin.y,
            layered.arg.size.width, layered.arg.size.height,
            layered.clip.origin.x, layered.clip.origin.y,
            layered.clip.size.width, layered.clip.size.height,
            layered.aa, layered.interp,
            layered.samp[0],layered.samp[1],layered.samp[2],
            layered.samp[3],layered.samp[4],layered.samp[5],
            layered.samp[6],layered.samp[7],layered.samp[8]);

        if (plain.draw_calls_display != layered.draw_calls_display) {
            fprintf(stderr, "DIVERGENCE: headless [display] drawRect count "
                            "differs plain=%d layered=%d\n",
                    plain.draw_calls_display, layered.draw_calls_display);
            return 1;
        }
        if (plain.calls != layered.calls ||
            !CGRectEqualToRect(plain.arg, layered.arg) ||
            !CGRectEqualToRect(plain.clip, layered.clip) ||
            memcmp(&plain.ctm, &layered.ctm, sizeof plain.ctm) != 0 ||
            plain.aa != layered.aa || plain.interp != layered.interp) {
            fprintf(stderr, "DIVERGENCE: cacheDisplay in-draw contract differs "
                            "under forced layer-backing\n");
            return 1;
        }
        if (memcmp(plain.samp, layered.samp, sizeof plain.samp) != 0) {
            fprintf(stderr, "DIVERGENCE: cacheDisplay pixels differ under "
                            "forced layer-backing\n");
            return 1;
        }
        fprintf(stderr, "layer-backing invariance holds\n");
        return 0;
    }
}
EOF

clang -arch x86_64 -fobjc-arc -Wno-deprecated-declarations \
      -framework AppKit -framework QuartzCore -o "$TMP/t" "$TMP/t.m" \
    || fail "compile"

OUT=$("$TMP/t" 2>&1); RC=$?
echo "$OUT" | sed 's/^/  /'
[ $RC -eq 0 ] || fail "guard exited $RC"

echo "layerback-invariance: forced layer-backing does not alter the legacy draw contract; OK"
exit 0
