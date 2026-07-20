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

/* A plain legacy-standing-in window subclass (no chrome override) — a stand-in
 * for a generic legacy Cocoa window like Quinn's toolbar window. The native
 * guard flips accept_subclass(1) so window_is_legacy() treats any non-NS*
 * subclass as legacy (it can't forge a reverse-bridge IMP). */
@interface LegacyWin : NSWindow @end
@implementation LegacyWin @end

/* An NSToolbar delegate that vends ONE custom-view item (borderless icon button)
 * — mimics a legacy app whose toolbar items come up as bare icon blobs. */
@interface TbDelegate : NSObject <NSToolbarDelegate> @end
@implementation TbDelegate
- (NSToolbarItem *)toolbar:(NSToolbar *)tb itemForItemIdentifier:(NSToolbarItemIdentifier)ident
    willBeInsertedIntoToolbar:(BOOL)flag {
    NSToolbarItem *it = [[NSToolbarItem alloc] initWithItemIdentifier:ident];
    [it setLabel:@"Abort"];
    NSButton *b = [[NSButton alloc] initWithFrame:NSMakeRect(0,0,32,32)];
    /* the WRONG translated state: bordered + a bezel + no title (the failed-metal
     * PolishedMetalButtonCell renders a black box behind the bare icon). */
    [b setBordered:YES];
    [b setBezelStyle:NSBezelStyleTexturedRounded];
    [b setTitle:@""];                   /* icon-only custom button (like Quinn) */
    [b setImage:[NSImage imageWithSize:NSMakeSize(32,32) flipped:NO
                 drawingHandler:^BOOL(NSRect r){ return YES; }]];
    [it setView:b];
    return it;
}
- (NSArray<NSToolbarItemIdentifier> *)toolbarDefaultItemIdentifiers:(NSToolbar *)tb { return @[@"abort"]; }
- (NSArray<NSToolbarItemIdentifier> *)toolbarAllowedItemIdentifiers:(NSToolbar *)tb { return @[@"abort"]; }
@end

int main(int argc, char **argv) {
    @autoreleasepool {
        void *h = dlopen(argv[1], RTLD_LAZY);
        if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 2; }
        void (*install)(void)       = dlsym(h, "_86x64_test_appkit_compat_install");
        Class (*ovcls)(void)        = dlsym(h, "_86x64_test_window_chrome_overlay_class");
        void (*inst_ov)(id)         = dlsym(h, "_86x64_test_window_chrome_install_overlay");
        int  (*has_chrome)(id)      = dlsym(h, "_86x64_test_window_has_legacy_chrome");
        void (*accept_responds)(int)= dlsym(h, "_86x64_test_window_chrome_accept_responds");
        void (*title_on_show)(id)   = dlsym(h, "_86x64_test_window_title_on_show");
        void (*toolbar_style)(id)   = dlsym(h, "_86x64_test_toolbar_style_on_show");
        int  (*win_is_legacy)(id)   = dlsym(h, "_86x64_test_window_is_legacy");
        void (*accept_subclass)(int)= dlsym(h, "_86x64_test_window_accept_subclass");
        if (!install || !ovcls || !inst_ov || !has_chrome || !accept_responds ||
            !title_on_show || !toolbar_style || !win_is_legacy || !accept_subclass) {
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

        /* ===== generic TITLE-on-show ==================================== */
        accept_subclass(1); /* native guard can't forge a reverse-bridge IMP */

        /* native window with blank title must NOT get one (gate must be 0) */
        NSWindow *nt = [[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,300,100)
            styleMask:mask backing:NSBackingStoreBuffered defer:NO];
        [nt setTitle:@""];
        if (win_is_legacy(nt)) { fprintf(stderr, "native win falsely legacy\n"); return 1; }
        title_on_show(nt);
        if ([[nt title] length] != 0) {
            fprintf(stderr, "native blank-title window wrongly retitled to '%s'\n",
                    [[nt title] UTF8String]); return 1; }
        fprintf(stderr, "native blank-title window: gate=0, title left empty OK\n");

        /* legacy window with blank title -> gets the app/process name */
        LegacyWin *lw = [[LegacyWin alloc] initWithContentRect:NSMakeRect(0,0,300,100)
            styleMask:mask backing:NSBackingStoreBuffered defer:NO];
        [lw setTitle:@""];
        if (!win_is_legacy(lw)) { fprintf(stderr, "legacy win NOT flagged\n"); return 1; }
        title_on_show(lw);
        if ([[lw title] length] == 0) {
            fprintf(stderr, "legacy blank-title window NOT retitled\n"); return 1; }
        fprintf(stderr, "legacy blank-title window: title set to '%s' (visibility=%ld) OK\n",
                [[lw title] UTF8String], (long)[lw titleVisibility]);

        /* legacy window that ALREADY has a title -> preserved; and the fix must
         * force the title-render preconditions (Titled bit + visible titlebar). */
        LegacyWin *lw2 = [[LegacyWin alloc] initWithContentRect:NSMakeRect(0,0,300,100)
            styleMask:mask backing:NSBackingStoreBuffered defer:NO];
        [lw2 setTitle:@"AppChosen"];
        [lw2 setTitleVisibility:NSWindowTitleHidden]; /* the WRONG hidden-title state */
        title_on_show(lw2);
        if (![[lw2 title] isEqualToString:@"AppChosen"]) {
            fprintf(stderr, "legacy pre-titled window clobbered to '%s'\n",
                    [[lw2 title] UTF8String]); return 1; }
        if ([lw2 titleVisibility] != NSWindowTitleVisible) {
            fprintf(stderr, "legacy title still hidden (=%ld)\n", (long)[lw2 titleVisibility]); return 1; }
        if (!([lw2 styleMask] & NSWindowStyleMaskTitled)) {
            fprintf(stderr, "legacy window styleMask lost Titled bit\n"); return 1; }
        fprintf(stderr, "legacy pre-titled window: title preserved + titlebar forced "
                "visible/Titled OK\n");

        /* legacy TEXTURED/metal window created WITHOUT the Titled bit but with a
         * title string (Quinn's PolishedMetalWindow case) -> fix must ADD Titled
         * so the stock chrome lays out + shows the title. */
        LegacyWin *lw3 = [[LegacyWin alloc] initWithContentRect:NSMakeRect(0,0,300,100)
            styleMask:(NSWindowStyleMaskClosable|NSWindowStyleMaskResizable)
            backing:NSBackingStoreBuffered defer:NO];
        [lw3 setTitle:@"Quinn"];
        title_on_show(lw3);
        if (!([lw3 styleMask] & NSWindowStyleMaskTitled)) {
            fprintf(stderr, "textured legacy window did NOT gain Titled bit\n"); return 1; }
        if (![[lw3 title] isEqualToString:@"Quinn"]) {
            fprintf(stderr, "textured legacy window title lost\n"); return 1; }
        fprintf(stderr, "legacy textured window: Titled bit added, title '%s' shows OK\n",
                [[lw3 title] UTF8String]);

        /* ===== generic TOOLBAR styling ================================== */
        TbDelegate *dlg = [TbDelegate new];
        NSToolbar *tbar = [[NSToolbar alloc] initWithIdentifier:@"QuinnMainToolbar"];
        [tbar setDelegate:dlg];
        [tbar setDisplayMode:NSToolbarDisplayModeIconOnly]; /* the WRONG modern default */
        [lw setToolbar:tbar];
        /* force items to materialize */
        (void)[tbar items];
        toolbar_style(lw);
        if ([lw.toolbar displayMode] != NSToolbarDisplayModeIconAndLabel) {
            fprintf(stderr, "toolbar displayMode not IconAndLabel (=%ld)\n",
                    (long)[lw.toolbar displayMode]); return 1; }
        if ([lw.toolbar sizeMode] != NSToolbarSizeModeRegular) {
            fprintf(stderr, "toolbar sizeMode not Regular (=%ld)\n",
                    (long)[lw.toolbar sizeMode]); return 1; }
        /* the custom item's button must now be FLAT/borderless (no bezel, no
         * black bg) with the item's label surfaced as the button title beneath
         * the icon (imagePosition=NSImageAbove). NOT bezeled. */
        int styled = 0, items_n = 0;
        for (NSToolbarItem *it in [lw.toolbar items]) {
            items_n++;
            NSView *v = [it view];
            if ([v isKindOfClass:[NSButton class]]) {
                NSButton *b = (NSButton *)v;
                BOOL flat  = ![b isBordered];
                BOOL labelled = [[b title] isEqualToString:[it label]] &&
                                [[it label] length] > 0;
                BOOL iconAbove = ([b imagePosition] == NSImageAbove);
                if (flat && labelled && iconAbove) styled++;
                fprintf(stderr, "    item '%s': bordered=%d bezel=%ld title='%s' imgPos=%ld\n",
                        [[it label] UTF8String], [b isBordered], (long)[b bezelStyle],
                        [[b title] UTF8String], (long)[b imagePosition]);
            }
        }
        if (items_n == 0 || styled != items_n) {
            fprintf(stderr, "toolbar item buttons not flat+labeled (%d/%d)\n", styled, items_n); return 1; }
        fprintf(stderr, "toolbar: legacy window toolbar -> IconAndLabel/Regular, "
                "%d/%d item buttons FLAT+labeled OK\n", styled, items_n);

        /* native window's toolbar must be left ALONE */
        NSToolbar *ntb = [[NSToolbar alloc] initWithIdentifier:@"native"];
        [ntb setDisplayMode:NSToolbarDisplayModeIconOnly];
        [nt setToolbar:ntb];
        toolbar_style(nt);
        if ([nt.toolbar displayMode] != NSToolbarDisplayModeIconOnly) {
            fprintf(stderr, "native toolbar wrongly restyled\n"); return 1; }
        fprintf(stderr, "native window toolbar: left untouched (IconOnly) OK\n");

        fprintf(stderr, "window-chrome+title+toolbar OK: legacy window got a visible "
                "title (Titled+visible) + IconAndLabel/Regular FLAT (borderless) "
                "labeled toolbar items; native windows untouched\n");
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
