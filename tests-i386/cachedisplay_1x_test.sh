#!/bin/bash
#
# Regression guard for the legacy -[NSView cacheDisplay] 1x-rep compat
# (objc_shim.c legacy_cachedisplay_1x_install / view_bitmapRepForCaching) — the
# VIEW-side sibling of the -[NSImage lockFocus] 1x compat.
#
# The offscreen view-capture idiom bitmapImageRepForCachingDisplayInRect: (vend
# a rep) + cacheDisplayInRect:toBitmapImageRep: (render into it) vends a rep
# sized at the display backingScaleFactor: on a Retina (2x) screen a 10-POINT
# view yields a 20-PIXEL rep and cacheDisplay fills the full 2x extent. LEGACY
# 10.6 vended a 1x rep (pixels == points). A legacy consumer reads the rep's
# bitmapData/bytesPerRow at POINT coordinates or uploads to glTexImage2D sizing
# the quad in points -> a 2x rep gives it the wrong stride/extent.
#
# The compat swizzles -[NSView bitmapImageRepForCachingDisplayInRect:] so a
# LEGACY-originated capture (flagged by the forward bridge) returns a hand-built
# 1x rep; the app's cacheDisplayInRect: then renders into it at 1x. Native
# (flag-unset) captures get the original device-scaled rep untouched.
#
# Self-contained native x86_64 guard (no i386 sysroot): loads the REAL built
# libabiconv.dylib, forces the compat install via the exported test hook,
# SIMULATES a legacy send via _86x64_test_set_cachedisplay_legacy, runs the exact
# capture idiom and pixel-checks the rendered 1x rep. Proves the structural gate
# BOTH ways: legacy send -> 1x rep with correct content; native send -> 2x.
set -u
LIBABICONV="${1:?usage: cachedisplay_1x_test.sh <path-to-libabiconv.dylib>}"

fail() { echo "FAIL cachedisplay-1x: $1"; exit 1; }
[ -f "$LIBABICONV" ] || fail "libabiconv not found at $LIBABICONV"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/t.m" <<'EOF'
#import <AppKit/AppKit.h>
#import <dlfcn.h>

@interface CHV : NSView @end
@implementation CHV
- (void)drawRect:(NSRect)r {
    [[NSColor redColor] set];  NSRectFill([self bounds]);
    [[NSColor blueColor] set]; NSRectFill(NSMakeRect(0,0,2,2)); /* view bottom-left */
}
@end

int main(int argc, char **argv) {
    @autoreleasepool {
        void *h = dlopen(argv[1], RTLD_LAZY);
        if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 2; }
        void (*install)(void) = dlsym(h, "_86x64_test_appkit_compat_install");
        if (!install) { fprintf(stderr, "no test hook: %s\n", dlerror()); return 2; }
        void (*set_legacy)(int) = dlsym(h, "_86x64_test_set_cachedisplay_legacy");
        if (!set_legacy) { fprintf(stderr, "no set hook: %s\n", dlerror()); return 2; }

        [NSApplication sharedApplication];
        install();
        CGFloat scale = [[NSScreen mainScreen] backingScaleFactor];
        fprintf(stderr, "main screen backingScaleFactor = %.1f\n", scale);

        NSRect bounds = NSMakeRect(0,0,10,10);

        /* --- LEGACY path: flag set -> 1x rep --- */
        CHV *v = [[CHV alloc] initWithFrame:bounds];
        set_legacy(1);
        NSBitmapImageRep *rep = [v bitmapImageRepForCachingDisplayInRect:bounds];
        [v cacheDisplayInRect:bounds toBitmapImageRep:rep];
        if (rep.pixelsWide != 10 || rep.pixelsHigh != 10) {
            fprintf(stderr, "legacy: rep %ldx%ld != 1x 10x10 (not pinned)\n",
                    (long)rep.pixelsWide, (long)rep.pixelsHigh);
            return 1;
        }
        unsigned char *p = rep.bitmapData; long b = rep.bytesPerRow;
        unsigned char *mid = p + 5*b + 5*4;             /* red fill */
        if (!(mid[0] > 0xc0 && mid[1] < 0x60)) {
            fprintf(stderr, "legacy: mid px %02x%02x%02x not red (content wrong)\n",
                    mid[0], mid[1], mid[2]); return 1;
        }
        /* blue marker at view bottom-left = bitmap y=9 (row 0 = top) */
        unsigned char *corner = p + 9*b + 0*4;
        if (!(corner[2] > 0x80 && corner[0] < 0x60)) {
            fprintf(stderr, "legacy: corner px %02x%02x%02x not blue (orientation)\n",
                    corner[0], corner[1], corner[2]); return 1;
        }

        /* --- NATIVE path: flag unset -> device-scaled rep untouched --- */
        CHV *v2 = [[CHV alloc] initWithFrame:bounds];
        set_legacy(0);
        NSBitmapImageRep *nr = [v2 bitmapImageRepForCachingDisplayInRect:bounds];
        long npx = nr.pixelsWide;
        if (scale > 1.0 && npx == 10) {
            fprintf(stderr, "native: rep %ldx%ld PINNED to 1x with flag unset "
                    "(gate over-fired!)\n", npx, (long)nr.pixelsHigh); return 1;
        }
        fprintf(stderr, "native(flag=0): rep %ld px (device-scaled, expect ~%ld) OK\n",
                npx, (long)(10*scale + 0.5));

        fprintf(stderr, "cachedisplay-1x OK: legacy view capture pinned 1x 10x10 "
                "(red fill + blue marker correct); native gate not over-fired "
                "(scale=%.1f)\n", scale);
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

echo "cachedisplay-1x: legacy -[NSView cacheDisplay] 1x rep restored; OK"
exit 0
