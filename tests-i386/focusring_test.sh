#!/bin/bash
#
# Regression guard for the LEGACY FOCUS-RING contract under translation
# (audit finding: NO compat needed — this guard pins the two invariants that
# verdict rests on, so a future macOS/AppKit or bridge change that breaks
# either re-surfaces the issue loudly instead of as silent visual corruption).
#
# Invariant A (OS): NSSetFocusRingStyle(NSFocusRingOnly) + NSRectFill still
#   works headlessly on modern AppKit (bitmap/cacheDisplay path): the interior
#   of the filled shape is NOT painted (ring only), a ring IS painted, and —
#   critically — the CG focus-ring style draws EXEMPT from the current clip:
#   with CGContextClipToRect([self bounds]) live (proven live by a normal fill
#   that cannot escape it), the ring still escapes the view bounds. This is
#   the exact 10.6 escape mechanism, and it is why the landed reverse-bridge
#   draw-clip compat (_86x64_reverse_prep, clip-to-bounds on legacy drawRect:)
#   can NOT amputate legacy exterior focus rings. If a future AppKit makes
#   the ring honor the clip, translated legacy controls' focus rings would be
#   silently clipped off and a ring-escape compat becomes necessary.
#
# Invariant B (bridge): the built libabiconv exports the REAL marshalling
#   thunk __NSSetFocusRingStyle (i386 stack uint32 -> %edi -> native call).
#   NSSetFocusRingStyle is imported by 7/9 Apps32 targets (Quinn, iMovie,
#   iPhoto, iWeb, Keynote, Numbers, Pages). A stale shimdb auto-stub for it
#   exists in generated text (return-0 no-op); if that state ever deployed,
#   the legacy NSFocusRingOnly+fill idiom would degenerate into a SOLID FILL
#   painted over the focused control (the style is what suppresses the fill).
set -u
LIBABICONV="${1:?usage: focusring_test.sh <path-to-libabiconv.dylib>}"

fail() { echo "FAIL focus-ring: $1"; exit 1; }
[ -f "$LIBABICONV" ] || fail "libabiconv not found at $LIBABICONV"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/t.m" <<'EOF'
#import <AppKit/AppKit.h>
#import <dlfcn.h>

@interface FRView : NSView
@end
@implementation FRView
- (void)drawRect:(NSRect)r {
    [[NSColor whiteColor] set];
    NSRectFill([self bounds]);
    CGContextRef cg = [NSGraphicsContext currentContext].CGContext;
    /* the exact primitive the draw-clip compat applies to legacy drawRect: */
    CGContextSaveGState(cg);
    CGContextClipToRect(cg, NSRectToCGRect([self bounds]));
    /* clip-liveness control: a NORMAL fill of bounds+20 must NOT escape */
    [[NSColor redColor] set];
    NSRectFill(NSInsetRect([self bounds], -20, -20));
    /* the legacy focus idiom, shape hugging the bounds (worst case) */
    [NSGraphicsContext saveGraphicsState];
    NSSetFocusRingStyle(NSFocusRingOnly);
    NSRectFill([self bounds]);
    [NSGraphicsContext restoreGraphicsState];
    CGContextRestoreGState(cg);
}
@end

@interface FRParent : NSView
@end
@implementation FRParent
- (void)drawRect:(NSRect)r {
    [[NSColor colorWithCalibratedWhite:0.8 alpha:1.0] set];
    NSRectFill([self bounds]);
}
@end

static unsigned char *px(NSBitmapImageRep *rep, int x, int yview) {
    long H = rep.pixelsHigh, bpr = rep.bytesPerRow;
    return rep.bitmapData + (H - 1 - yview) * bpr + x * 4;
}

int main(int argc, char **argv) {
    @autoreleasepool {
        /* Invariant B: real marshalling thunk exported by libabiconv */
        void *h = dlopen(argv[1], RTLD_LAZY);
        if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 2; }
        if (!dlsym(h, "__NSSetFocusRingStyle")) {
            fprintf(stderr, "libabiconv lacks the __NSSetFocusRingStyle "
                            "marshalling thunk\n");
            return 1;
        }

        [NSApplication sharedApplication];
        /* parent 160x120 gray; ring view at (40,40) 80x40 */
        FRParent *parent = [[FRParent alloc] initWithFrame:NSMakeRect(0,0,160,120)];
        FRView *v = [[FRView alloc] initWithFrame:NSMakeRect(40,40,80,40)];
        [parent addSubview:v];
        NSWindow *w = [[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,160,120)
                                                  styleMask:0
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];
        [[w contentView] addSubview:parent];

        NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes:NULL pixelsWide:160 pixelsHigh:120
            bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
            colorSpaceName:NSCalibratedRGBColorSpace bytesPerRow:0 bitsPerPixel:0];
        [rep setSize:NSMakeSize(160,120)];
        [parent cacheDisplayInRect:parent.bounds toBitmapImageRep:rep];

        int fill_leak = 0, ring_outside = 0, interior_bad = 0;
        for (int y = 0; y < 120; ++y) {
            for (int x = 0; x < 160; ++x) {
                int inview = (x >= 40 && x < 120 && y >= 40 && y < 80);
                unsigned char *p = px(rep, x, y);
                if (inview) {
                    int lx = x - 40, ly = y - 40;
                    /* deep interior: red (clip-control fill), NOT white/ring */
                    if (lx > 8 && lx < 71 && ly > 8 && ly < 31)
                        if (!(p[0] > 200 && p[1] < 80 && p[2] < 80)) interior_bad++;
                } else if (x >= 34 && x < 126 && y >= 34 && y < 86) {
                    /* band just outside the view */
                    if (p[0] > 200 && p[1] < 80 && p[2] < 80) fill_leak++;
                    else if (abs(p[0]-204)>10 || abs(p[1]-204)>10 || abs(p[2]-204)>10)
                        ring_outside++;
                }
            }
        }
        fprintf(stderr, "fill_leak=%d ring_outside=%d interior_bad=%d\n",
                fill_leak, ring_outside, interior_bad);
        if (fill_leak > 0) {
            fprintf(stderr, "clip-liveness control BROKE (normal fill escaped "
                            "the bounds clip) — measurement invalid\n");
            return 1;
        }
        if (ring_outside < 100) {
            fprintf(stderr, "focus ring no longer escapes the bounds clip — "
                            "the draw-clip compat now amputates legacy focus "
                            "rings; a ring-escape compat is needed\n");
            return 1;
        }
        if (interior_bad > 0) {
            fprintf(stderr, "NSFocusRingOnly painted the shape interior — "
                            "legacy focus idiom now fills over control content\n");
            return 1;
        }
        fprintf(stderr, "focus-ring contract intact: ring drawn, escapes the "
                        "compat clip, interior untouched, thunk present\n");
        return 0;
    }
}
EOF

# -pagezero_size 0x1000: libabiconv's initializers reserve their low-4GB proxy
# arena at load (see snapshot_compat_test.sh).
clang -arch x86_64 -fobjc-arc -Wno-deprecated-declarations \
      -Wl,-pagezero_size,0x1000 \
      -framework AppKit -o "$TMP/t" "$TMP/t.m" || fail "compile"

OUT=$("$TMP/t" "$LIBABICONV" 2>&1); RC=$?
echo "$OUT" | sed 's/^/  /'
[ $RC -eq 0 ] || fail "guard exited $RC"

echo "focus-ring: legacy NSSetFocusRingStyle contract + bridge thunk intact; OK"
exit 0
