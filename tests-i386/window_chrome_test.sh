#!/bin/bash
#
# Regression guard for the legacy private-frame-view window CHROME compat
# (objc_shim.c legacy_window_chrome_install / chrome_install_overlay /
# chrome_overlay_drawRect).
#
# A pre-10.x Cocoa app that wanted a custom titlebar look swapped NSWindow's
# PRIVATE frame-view class (subclass NSFrameView, override the frame view's
# chrome draw, wired via -borderViewClass / +hackBorderViewClass:), so the frame
# view called back into the window's -drawWindowBorderInRect: / -drawWindowTitle
# overrides. On modern macOS NSFrameView is GONE (it's NSThemeFrame) and the
# titlebar is a separate layer-composited NSTitlebarView, so that swap no-ops and
# the window's chrome overrides are NEVER invoked -> the window shows the stock
# white system titlebar instead of its own look (Quinn's brushed-metal band).
#
# The compat detects — STRUCTURALLY, keyed on the window responding to the legacy
# -drawWindowBorderInRect: with a translated-i386 (reverse-bridge) IMP, never a
# class name — such an orphaned-chrome window and installs a native overlay view
# into its NSTitlebarView (above the stock background, below the traffic-light
# widgets/title) whose -drawRect: re-invokes the window's own chrome overrides,
# re-materializing the app's authentic titlebar.
#
# Self-contained native x86_64 guard (no i386 sysroot): loads the REAL built
# libabiconv.dylib, forces the compat install + overlay class via exported test
# hooks, builds a titled window whose NSWindow subclass overrides
# -drawWindowBorderInRect: (paints a distinctive metal color) + -drawWindowTitle,
# installs the overlay, offscreen-renders the frame view and pixel-checks that the
# titlebar band is NON-WHITE (the app's chrome painted) with the fix. Proves the
# structural gate BOTH ways: an orphaned-chrome window gets the overlay + paints
# its own band; a NATIVE window (no override) is never touched.
set -u
LIBABICONV="${1:?usage: window_chrome_test.sh <path-to-libabiconv.dylib>}"

fail() { echo "FAIL window-chrome: $1"; exit 1; }
[ -f "$LIBABICONV" ] || fail "libabiconv not found at $LIBABICONV"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/t.m" <<'EOF'
#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#import <dlfcn.h>

/* A window that mimics the legacy PolishedMetalWindow contract: it overrides the
 * private-frame chrome-draw selectors the old frame view used to invoke. */
@interface MetalWin : NSWindow @end
@implementation MetalWin
- (void)drawWindowBorderInRect:(NSRect)frameRect {
    /* the app draws its band at the TOP of the window frame */
    NSRect fb = [[[self contentView] superview] bounds];
    CGFloat bandH = 32.0;
    NSRect band = NSMakeRect(0, fb.size.height - bandH, fb.size.width, bandH);
    [[NSColor colorWithCalibratedRed:0.20 green:0.20 blue:0.22 alpha:1.0] set];
    NSRectFill(band);
}
- (void)drawWindowTitle {
    /* draw a marker pixel at the top-left of the frame band (frame coords) */
    NSRect fb = [[[self contentView] superview] bounds];
    [[NSColor colorWithCalibratedRed:0.90 green:0.10 blue:0.10 alpha:1.0] set];
    NSRectFill(NSMakeRect(2, fb.size.height - 6, 3, 3));
}
@end

static NSView* findv(NSView *v, const char *sub) {
    if (strstr(object_getClassName(v), sub)) return v;
    for (NSView *s in v.subviews) { NSView *r = findv(s, sub); if (r) return r; }
    return nil;
}

int main(int argc, char **argv) {
    @autoreleasepool {
        void *h = dlopen(argv[1], RTLD_LAZY);
        if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 2; }
        void (*install)(void)       = dlsym(h, "_86x64_test_appkit_compat_install");
        Class (*ovcls)(void)        = dlsym(h, "_86x64_test_window_chrome_overlay_class");
        void (*inst_ov)(id)         = dlsym(h, "_86x64_test_window_chrome_install_overlay");
        int  (*has_chrome)(id)      = dlsym(h, "_86x64_test_window_has_legacy_chrome");
        void (*accept_responds)(int)= dlsym(h, "_86x64_test_window_chrome_accept_responds");
        if (!install || !ovcls || !inst_ov || !has_chrome || !accept_responds) {
            fprintf(stderr, "missing test hooks\n"); return 2;
        }

        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
        install();
        Class oc = ovcls();
        if (!oc) { fprintf(stderr, "overlay class not built\n"); return 2; }

        NSUInteger mask = NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|
                          NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable;

        /* ---- NATIVE window: no chrome override -> gate must be 0, untouched ---- */
        NSWindow *nw = [[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,400,120)
            styleMask:mask backing:NSBackingStoreBuffered defer:NO];
        [nw setTitle:@"Native"];
        if (has_chrome(nw)) { fprintf(stderr, "native window falsely flagged\n"); return 1; }
        inst_ov(nw); /* should no-op (no titlebar override) */
        NSView *ntbv = findv([[nw contentView] superview], "NSTitlebarView");
        for (NSView *s in ntbv.subviews)
            if (object_getClass(s) == oc) { fprintf(stderr, "overlay wrongly on native win\n"); return 1; }
        fprintf(stderr, "native window: gate=0, no overlay (untouched) OK\n");

        /* ---- LEGACY-chrome window: override present -> overlay + paints band ---- */
        accept_responds(1); /* native guard can't forge a reverse-bridge IMP */
        MetalWin *mw = [[MetalWin alloc] initWithContentRect:NSMakeRect(0,0,400,120)
            styleMask:mask backing:NSBackingStoreBuffered defer:NO];
        [mw setTitle:@"Metal"];
        if (!has_chrome(mw)) { fprintf(stderr, "legacy-chrome window NOT flagged\n"); return 1; }
        [mw orderFront:nil];
        inst_ov(mw);
        NSView *frame = [[mw contentView] superview];
        NSView *tbv = findv(frame, "NSTitlebarView");
        int found = 0;
        for (NSView *s in tbv.subviews) if (object_getClass(s) == oc) found = 1;
        if (!found) { fprintf(stderr, "overlay NOT installed on legacy-chrome window\n"); return 1; }
        fprintf(stderr, "legacy-chrome window: gate=1, overlay installed OK\n");

        /* force a full frame render and offscreen-capture it */
        [frame display];
        NSRect fb = frame.bounds;
        NSBitmapImageRep *rep = [frame bitmapImageRepForCachingDisplayInRect:fb];
        [frame cacheDisplayInRect:fb toBitmapImageRep:rep];
        long W = rep.pixelsWide, H = rep.pixelsHigh;
        double sx = (double)W / fb.size.width;
        fprintf(stderr, "frame rep %ldx%ld scale=%.1f\n", W, H, sx);

        /* sample the titlebar band = TOP of the (unflipped) frame => bitmap row 0..
         * region. Use a y a few px below the very top (avoid rounded corners). */
        int ty = (int)(8 * sx);
        int metal = 0, total = 0;
        for (int fx = 120; fx < (int)fb.size.width - 40; fx += 40) {
            int px = (int)(fx * sx);
            NSColor *c = [[rep colorAtX:px y:ty]
                           colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
            double r = c.redComponent, g = c.greenComponent, b = c.blueComponent;
            total++;
            /* metal band is dark (~0.2..0.4 after visual-effect blend); WHITE stock
             * titlebar is ~0.9+. Accept clearly-non-white. */
            if (r < 0.75 && g < 0.75 && b < 0.75) metal++;
            fprintf(stderr, "  band fx=%d rgb=%.2f,%.2f,%.2f %s\n", fx, r, g, b,
                    (r<0.75&&g<0.75&&b<0.75)?"metal":"white");
        }
        if (metal < total) {
            fprintf(stderr, "titlebar band still WHITE (%d/%d metal) - chrome not "
                    "re-materialized\n", metal, total);
            return 1;
        }

        fprintf(stderr, "window-chrome OK: orphaned-chrome window's own "
                "drawWindowBorderInRect: re-materialized the titlebar band "
                "(%d/%d samples metal), native window untouched\n", metal, total);
        return 0;
    }
}
EOF

clang -arch x86_64 -fobjc-arc -Wno-deprecated-declarations \
      -Wl,-pagezero_size,0x1000 \
      -framework AppKit -o "$TMP/t" "$TMP/t.m" || fail "compile"

OUT=$("$TMP/t" "$LIBABICONV" 2>&1); RC=$?
echo "$OUT" | sed 's/^/  /'
[ $RC -eq 0 ] || fail "guard exited $RC"

echo "window-chrome: legacy private-frame chrome re-materialized via titlebar overlay; OK"
exit 0
