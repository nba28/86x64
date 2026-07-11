#!/bin/bash
#
# Regression guard for the legacy NSView-snapshot compat
# (objc_shim.c legacy_snapshot_compat_install /
#  -[NSBitmapImageRep initWithFocusedViewRect:] -> cacheDisplay bridge).
#
# The pre-10.6 view-snapshot idiom — render a view inside a never-shown
# borderless window, lockFocus, read the pixels back with
# initWithFocusedViewRect: — is DEAD on modern AppKit: [view display] on a
# never-ordered-in window doesn't invoke drawRect:, and the deprecated
# readback returns nil. Downstream, [NSImage TIFFRepresentation] logs
# "CGImageDestinationFinalize failed for output type 'public.tiff'" and GL
# texture uploads see NULL bitmapData. Quinn 3.5.7 builds ALL its transition
# content this way (NSView(QuinnExtensions) bitmapFromView/imageFromView ->
# GLImage board-flip textures, AnimatedContainerView menu switches,
# QuinnHighscoreAnimationView), so every capture-based animation composited
# EMPTY content.
#
# The compat swizzles initWithFocusedViewRect: to synthesize the capture with
# cacheDisplayInRect:toBitmapImageRep: on [NSView focusView], hand-building a
# 1x rep (pixels == points, the 10.6 contract legacy GL uploaders assume).
#
# Self-contained native x86_64 guard (no i386 sysroot): loads the REAL built
# libabiconv.dylib, forces the compat install via the exported test hook, then
# runs the exact legacy idiom and pixel-checks the captured rep.
set -u
LIBABICONV="${1:?usage: snapshot_compat_test.sh <path-to-libabiconv.dylib>}"

fail() { echo "FAIL snapshot-compat: $1"; exit 1; }
[ -f "$LIBABICONV" ] || fail "libabiconv not found at $LIBABICONV"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/t.m" <<'EOF'
#import <AppKit/AppKit.h>
#import <dlfcn.h>

static int g_drew = 0;

@interface SnapRedView : NSView
@end
@implementation SnapRedView
- (void)drawRect:(NSRect)r {
    g_drew++;
    [[NSColor redColor] set];
    NSRectFill([self bounds]);
    [[NSColor blueColor] set];
    NSRectFill(NSMakeRect(0, 0, 10, 10));     /* orientation marker */
}
@end

int main(int argc, char **argv) {
    @autoreleasepool {
        /* RTLD_LAZY: libabiconv carries flat-namespace binds to legacy Carbon
         * symbols that resolve inside a translated process (shim dylibs) but
         * not in this bare native host; nothing on the snapshot path calls
         * them, so defer. */
        void *h = dlopen(argv[1], RTLD_LAZY);
        if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 2; }
        void (*install)(void) = dlsym(h, "_86x64_test_appkit_compat_install");
        if (!install) { fprintf(stderr, "no test hook: %s\n", dlerror()); return 2; }

        [NSApplication sharedApplication];
        install();

        /* Quinn's -[NSView(QuinnExtensions) bitmapFromView], verbatim shape:
         * borderless never-shown buffered window, clear/non-opaque, addSubview,
         * display, lockFocus, initWithFocusedViewRect:, unlockFocus. */
        NSRect frame = NSMakeRect(0, 0, 200, 100);
        NSWindow *w = [[NSWindow alloc] initWithContentRect:frame
                                                  styleMask:0
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];
        [w setBackgroundColor:[NSColor clearColor]];
        [w setOpaque:NO];
        SnapRedView *v = [[SnapRedView alloc] initWithFrame:frame];
        [[w contentView] addSubview:v];
        [v setFrameOrigin:NSZeroPoint];
        [v display];
        [v lockFocus];
        NSBitmapImageRep *rep =
            [[NSBitmapImageRep alloc] initWithFocusedViewRect:frame];
        [v unlockFocus];

        if (!rep) { fprintf(stderr, "rep=nil (capture still dead)\n"); return 1; }
        if (rep.pixelsWide != 200 || rep.pixelsHigh != 100) {
            fprintf(stderr, "rep %ldx%ld != 1x contract 200x100\n",
                    (long)rep.pixelsWide, (long)rep.pixelsHigh);
            return 1;
        }
        unsigned char *p = rep.bitmapData;
        long bpr = rep.bytesPerRow;
        if (!p) { fprintf(stderr, "bitmapData NULL\n"); return 1; }
        unsigned char *mid = p + 50*bpr + 100*4;
        unsigned char *corner = p + (100-1-2)*bpr + 2*4;   /* view y=2 */
        if (!(mid[0] > 0xc0 && mid[1] < 0x60)) {
            fprintf(stderr, "mid pixel %02x%02x%02x not red\n",
                    mid[0], mid[1], mid[2]);
            return 1;
        }
        if (!(corner[2] > 0x80 && corner[0] < 0x60)) {
            fprintf(stderr, "corner pixel %02x%02x%02x not blue (orientation)\n",
                    corner[0], corner[1], corner[2]);
            return 1;
        }
        /* the downstream consumers Quinn feeds: rep TIFF + NSImage-wrap TIFF
         * (GLImage's [image bitmap] path -> the public.tiff finalize error) */
        if ([rep TIFFRepresentation].length == 0) {
            fprintf(stderr, "rep TIFF empty\n"); return 1;
        }
        NSImage *img = [[NSImage alloc] initWithSize:rep.size];
        [img addRepresentation:rep];
        if ([img TIFFRepresentation].length == 0) {
            fprintf(stderr, "NSImage TIFF empty\n"); return 1;
        }
        fprintf(stderr, "capture OK: 200x100 1x, red+marker, TIFF encodes, drew=%d\n",
                g_drew);
        return 0;
    }
}
EOF

# -pagezero_size 0x1000: libabiconv's initializers reserve their low-4GB proxy
# arena at load; a default x86_64 host's 4GB __PAGEZERO makes that unmappable
# ("objc_shim: no low-4GB region for proxy arena"). Translated processes get
# this from the wrapper exec; give the native guard the same address space.
clang -arch x86_64 -fobjc-arc -Wno-deprecated-declarations \
      -Wl,-pagezero_size,0x1000 \
      -framework AppKit -o "$TMP/t" "$TMP/t.m" || fail "compile"

OUT=$("$TMP/t" "$LIBABICONV" 2>&1); RC=$?
echo "$OUT" | sed 's/^/  /'
[ $RC -eq 0 ] || fail "guard exited $RC"

echo "snapshot-compat: legacy initWithFocusedViewRect: capture restored; OK"
exit 0
