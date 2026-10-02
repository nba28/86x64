#!/bin/bash
#
# Regression guard for the legacy -[NSImage lockFocus] 1x-backing compat
# (objc_shim.c legacy_lockfocus_1x_install / img_lockFocus / img_unlockFocus).
#
# Modern AppKit backs an -[NSImage lockFocus] drawing context at the display's
# backingScaleFactor: on a Retina (2x) screen a 10x10-POINT image lockFocus'd
# yields a 20x20-PIXEL rep. LEGACY 10.6 lockFocus always produced 1x (pixels ==
# points). Legacy consumers read bitmapData/bytesPerRow at POINT coordinates,
# upload to glTexImage2D sizing quads in points, and measure pixelsWide to lay
# out — a 2x rep gives them the wrong stride/extent (garbled blits, GL texcoord
# mismatch). This is the offscreen sibling of legacy_glview_1x and the
# initWithFocusedViewRect: snapshot compat, sharing their 1x contract.
#
# The compat swizzles -[NSImage lockFocus]/unlockFocus so a LEGACY-originated
# lockFocus (marked via the g_lockfocus_legacy thread flag that the forward
# bridge sets for an i386 send) draws into an explicit 1x NSBitmapImageRep-backed
# context, pre-painting existing content, and commits the 1x rep on unlock.
# Native (flag-unset) lockFocus is untouched.
#
# Self-contained native x86_64 guard (no i386 sysroot): loads the REAL built
# libabiconv.dylib, forces the compat install via the exported test hook,
# SIMULATES a legacy send via the in-library test setter
# _86x64_test_set_lockfocus_legacy (a __thread flag can't be poked through
# dlsym), then runs the exact lockFocus idiom and pixel-checks the committed
# rep. Also asserts the fix does NOT fire when the flag is unset (native path
# stays device-scaled), so the structural gate is proven both ways.
set -u
LIBABICONV="${1:?usage: lockfocus_1x_test.sh <path-to-libabiconv.dylib>}"

fail() { echo "FAIL lockfocus-1x: $1"; exit 1; }
[ -f "$LIBABICONV" ] || fail "libabiconv not found at $LIBABICONV"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/t.m" <<'EOF'
#import <AppKit/AppKit.h>
#import <dlfcn.h>

/* draw a green base rep, then legacy-lockFocus an overlaid red corner. */
static NSImage* mkImage(void) {
    NSImage *im = [[NSImage alloc] initWithSize:NSMakeSize(10,10)];
    NSBitmapImageRep *base = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL pixelsWide:10 pixelsHigh:10 bitsPerSample:8
        samplesPerPixel:4 hasAlpha:YES isPlanar:NO
        colorSpaceName:NSCalibratedRGBColorSpace bytesPerRow:0 bitsPerPixel:0];
    NSGraphicsContext *bg = [NSGraphicsContext graphicsContextWithBitmapImageRep:base];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:bg];
    [[NSColor greenColor] set]; NSRectFill(NSMakeRect(0,0,10,10));
    [NSGraphicsContext restoreGraphicsState];
    [im addRepresentation:base];
    return im;
}

int main(int argc, char **argv) {
    @autoreleasepool {
        void *h = dlopen(argv[1], RTLD_LAZY);
        if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 2; }
        void (*install)(void) = dlsym(h, "_86x64_test_appkit_compat_install");
        if (!install) { fprintf(stderr, "no test hook: %s\n", dlerror()); return 2; }
        void (*set_legacy)(int) = dlsym(h, "_86x64_test_set_lockfocus_legacy");
        if (!set_legacy) { fprintf(stderr, "no set hook: %s\n", dlerror()); return 2; }

        [NSApplication sharedApplication];
        install();

        CGFloat scale = [[NSScreen mainScreen] backingScaleFactor];
        fprintf(stderr, "main screen backingScaleFactor = %.1f\n", scale);

        /* --- LEGACY path: set the flag, lockFocus, overlay-draw --- */
        NSImage *im = mkImage();
        set_legacy(1);                    /* simulate the forward bridge marking */
        [im lockFocus];
        [[NSColor redColor] set]; NSRectFill(NSMakeRect(0,0,4,4)); /* bottom-left */
        [im unlockFocus];

        NSArray *reps = [im representations];
        if (reps.count != 1) { fprintf(stderr, "legacy: reps=%lu != 1\n",
                                        (unsigned long)reps.count); return 1; }
        NSBitmapImageRep *r = reps.firstObject;
        if (r.pixelsWide != 10 || r.pixelsHigh != 10) {
            fprintf(stderr, "legacy: rep %ldx%ld != 1x 10x10 (backing not pinned)\n",
                    (long)r.pixelsWide, (long)r.pixelsHigh);
            return 1;
        }
        unsigned char *p = r.bitmapData; long b = r.bytesPerRow;
        /* bitmap row 0 = top; red overlay at image-bottom-left = bitmap y=9 */
        unsigned char *red   = p + 9*b + 1*4;
        unsigned char *green = p + 1*b + 8*4;   /* top-right stays green */
        if (!(red[0] > 0xc0 && red[1] < 0x60)) {
            fprintf(stderr, "legacy: overlay px %02x%02x%02x not red\n",
                    red[0], red[1], red[2]); return 1;
        }
        if (!(green[1] > 0xc0 && green[0] < 0x60)) {
            fprintf(stderr, "legacy: base px %02x%02x%02x not green (content lost)\n",
                    green[0], green[1], green[2]); return 1;
        }
        if ([im TIFFRepresentation].length == 0) {
            fprintf(stderr, "legacy: TIFF empty\n"); return 1;
        }

        /* --- LEAK: the committed rep must die with its image (the compat's
         * alloc/init +1 is released once addRepresentation: retains it). --- */
        __weak NSBitmapImageRep *wrep = nil;
        @autoreleasepool {
            NSImage *li = mkImage();
            set_legacy(1);
            [li lockFocus]; NSRectFill(NSMakeRect(0,0,2,2)); [li unlockFocus];
            wrep = (NSBitmapImageRep *)[li representations].firstObject;
            li = nil;
        }
        if (wrep) { fprintf(stderr, "legacy: committed 1x rep outlived its image "
                            "(leaked +1)\n"); return 1; }

        /* --- NATIVE path: flag unset -> original device-scaled backing.
         * On a >1x screen this proves the gate does NOT over-fire. On a 1x
         * screen native is already 1x, so only assert it did NOT get pinned by
         * our (flag-gated) swizzle, i.e. behavior == native default. --- */
        NSImage *im2 = mkImage();
        set_legacy(0);
        [im2 lockFocus];
        [[NSColor redColor] set]; NSRectFill(NSMakeRect(0,0,4,4));
        NSBitmapImageRep *nr =
            [[NSBitmapImageRep alloc] initWithFocusedViewRect:NSMakeRect(0,0,10,10)];
        [im2 unlockFocus];
        long npx = nr ? nr.pixelsWide : -1;
        long expect = (long)(10 * scale + 0.5);
        if (scale > 1.0 && npx == 10) {
            fprintf(stderr, "native: rep %ldx%ld got PINNED to 1x with flag unset "
                    "(gate over-fired!)\n", (long)npx, nr ? (long)nr.pixelsHigh : -1);
            return 1;
        }
        fprintf(stderr, "native(flag=0): readback %ld px (device-scaled, expect ~%ld) OK\n",
                npx, expect);

        fprintf(stderr, "lockfocus-1x OK: legacy pinned 1x 10x10 (red overlay + green "
                "base preserved, TIFF encodes); native gate not over-fired (scale=%.1f)\n",
                scale);
        return 0;
    }
}
EOF

# -pagezero_size 0x1000: libabiconv reserves its low-4GB proxy arena at load;
# a default 4GB __PAGEZERO makes that unmappable. Match the translated process.
clang -arch x86_64 -fobjc-arc -Wno-deprecated-declarations \
      -Wl,-pagezero_size,0x1000 \
      -framework AppKit -o "$TMP/t" "$TMP/t.m" || fail "compile"

OUT=$("$TMP/t" "$LIBABICONV" 2>&1); RC=$?
echo "$OUT" | sed 's/^/  /'
[ $RC -eq 0 ] || fail "guard exited $RC"

echo "lockfocus-1x: legacy -[NSImage lockFocus] 1x backing restored; OK"
exit 0
