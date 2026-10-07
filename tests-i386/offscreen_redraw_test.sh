#!/bin/bash
# offscreen_redraw_test.sh — FULL-PATH canary for the legacy -[NSView
# cacheDisplayInRect:toBitmapImageRep:] RE-RENDER under translation (the
# offscreen snapshot path feeding the legacy immediate-display overlay,
# print/PDF export and drag images).
#
# Born from the 2026-07-18 Quinn suspicion that this re-render LOSES image/
# gradient draws (board snapshot all-white, sidebar snapshot all-dark) while
# the same draws render on screen. This guard REFUTED that headlessly — every
# draw family lands, in every hosting shape — and stays as the regression
# canary pinning the whole contract (it also end-to-end covers the
# cg_geoarray_shim NSRectFillList leg, whose CG-side siblings are guarded by
# 99_cg_rect_arrays).
#
# FULL PATH (the difference from tests 85/89/90, which prove the same
# primitives in a CLEAN i386-created CGBitmapContext): a REAL classic-ObjC1
# view class registered onto native NSView by libabiconv
# (reverse_register_image), whose reverse-dispatched drawRect: draws the
# Quinn draw families into the context handed to it by the re-render:
#   1. solid fill        SetFillColorSpace+SetFillColor+FillRect  (the control)
#   2. sprite image      CGContextDrawImage of a cropped CGImage  (board cells)
#   3. axial shading     CGFunctionCreate(i386 evaluate)+CGShadingCreateAxial+
#                        DrawShading                              (sidebar gloss)
#   4. mask-clipped fill CGImageMaskCreate+ClipToMask+FillRect    (LCD digits)
#   5. NSRectFillList    [NSColor set] + AppKit C rect-list fill  (iWeb idiom)
# with everything but the background gated on getRectsBeingDrawn:count:
# (bp_getrects) exactly like Quinn's cell loop, across FIVE hosting scenarios:
# windowless / windowed / layer-backed / self-built 2x rep / NESTED capture
# from inside an active draw pass (the overlay idiom). The i386 main()
# triggers the exact legacy-origin snapshot path
# (bitmapImageRepForCachingDisplayInRect: + cacheDisplayInRect:toBitmapImageRep:)
# and pixel-checks each rep via RAW bitmapData (NOT -colorAtX:y:, whose
# readback mis-marshals — see 90_nsstring_drawattr).
#
# Exit protocol of the translated program: 8 independent bits per scenario,
# ANDed; 255 == every family landed in every scenario.
# i386 linking needs the Snow Leopard ld64-95 wrapper: modern ld dropped -arch i386
# (same resolution as the Makefile's LD). Override with LD=... in the environment.
. "$(dirname "$0")/../src/86x64/paths.sh"   # M64_* local paths
LD="${LD:-$M64_I386_LD}"; [ -x "$LD" ] || LD=ld

set -u
cd "$(dirname "$0")"

PROJ_ROOT="$(cd .. && pwd)"
MT="${1:-$PROJ_ROOT/build/src/macho-tool/macho-tool}"
LIBABICONV="$PROJ_ROOT/build/src/abiconv/libabiconv.dylib"
LIBWRAPPER="$PROJ_ROOT/build/src/86x64/libwrapper.a"
LIBINTERPOSE="$PROJ_ROOT/build/src/86x64/libinterpose.dylib"
PIPELINE="$PROJ_ROOT/src/86x64/86x64.sh"
SYSROOT=/tmp/i386-sysroot
HOST_SDK="$(xcrun --show-sdk-path)"

if [ ! -f "$SYSROOT/usr/lib/libSystem.dylib" ] && [ ! -f "$SYSROOT/usr/lib/libSystem.B.dylib" ]; then
   echo "SKIP offscreen-redraw (no i386 sysroot at $SYSROOT; run 'make sysroot')"
   exit 0
fi
if [ ! -f "$SYSROOT/usr/lib/libobjc.dylib" ] && [ ! -f "$SYSROOT/usr/lib/libobjc.A.dylib" ]; then
   echo "SKIP offscreen-redraw (no staged i386 libobjc; run 'make sysroot-objc')"
   exit 0
fi
for f in "$MT" "$LIBABICONV" "$LIBWRAPPER" "$LIBINTERPOSE"; do
   [ -e "$f" ] || { echo "SKIP offscreen-redraw (missing $f; build first)"; exit 0; }
done

mkdir -p build
cp -f "$LIBABICONV" build/libabiconv.dylib      # @loader_path dep of the output
fail() { echo "FAIL offscreen-redraw ($1)"; exit 1; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/t.m" <<'EOF'
#include <stdio.h>
extern void exit(int);

typedef struct objc_object *id;
typedef struct objc_selector *SEL;
extern id   objc_getClass(const char *);
extern SEL  sel_registerName(const char *);
extern id   objc_msgSend(id, SEL, ...);

typedef float CGFloat;                            /* i386: CGFloat == float */
typedef struct _NSPoint { CGFloat x, y; }           NSPoint;
typedef struct _NSSize  { CGFloat width, height; }  NSSize;
typedef struct _NSRect  { NSPoint origin; NSSize size; } NSRect;
typedef NSPoint CGPoint;
typedef NSRect  CGRect;

typedef void *CGColorSpaceRef;
typedef void *CGContextRef;
typedef void *CGImageRef;
typedef void *CGDataProviderRef;
typedef void *CGFunctionRef;
typedef void *CGShadingRef;

enum { kCGImageAlphaPremultipliedLast = 1 };

extern CGColorSpaceRef CGColorSpaceCreateDeviceRGB(void);
extern void CGColorSpaceRelease(CGColorSpaceRef);
extern CGContextRef CGBitmapContextCreate(void *, unsigned long, unsigned long,
        unsigned long, unsigned long, CGColorSpaceRef, unsigned int);
extern CGImageRef CGBitmapContextCreateImage(CGContextRef);
extern CGImageRef CGImageCreateWithImageInRect(CGImageRef, CGRect);
extern void CGContextRelease(CGContextRef);

extern void CGContextSetAlpha(CGContextRef, CGFloat);
extern void CGContextSetFillColorSpace(CGContextRef, CGColorSpaceRef);
extern void CGContextSetFillColor(CGContextRef, const CGFloat *);
extern void CGContextFillRect(CGContextRef, CGRect);
extern void CGContextDrawImage(CGContextRef, CGRect, CGImageRef);
extern void CGContextSaveGState(CGContextRef);
extern void CGContextRestoreGState(CGContextRef);
extern void CGContextClipToRect(CGContextRef, CGRect);
extern void CGContextClipToMask(CGContextRef, CGRect, CGImageRef);
extern void CGContextDrawShading(CGContextRef, CGShadingRef);
extern void CGShadingRelease(CGShadingRef);
extern void CGFunctionRelease(CGFunctionRef);

typedef void (*CGFunctionEvaluateCallback)(void *, const CGFloat *, CGFloat *);
typedef void (*CGFunctionReleaseInfoCallback)(void *);
typedef struct {
   unsigned int version;
   CGFunctionEvaluateCallback evaluate;
   CGFunctionReleaseInfoCallback releaseInfo;
} CGFunctionCallbacks;
extern CGFunctionRef CGFunctionCreate(void *, unsigned long, const CGFloat *,
        unsigned long, const CGFloat *, const CGFunctionCallbacks *);
extern CGShadingRef CGShadingCreateAxial(CGColorSpaceRef, CGPoint, CGPoint,
        CGFunctionRef, int, int);
extern CGDataProviderRef CGDataProviderCreateWithData(void *, const void *,
        unsigned long, void *);
extern void CGDataProviderRelease(CGDataProviderRef);
extern CGImageRef CGImageMaskCreate(unsigned long, unsigned long, unsigned long,
        unsigned long, unsigned long, CGDataProviderRef, const CGFloat *, int);

static id  C(const char *n) { return objc_getClass(n); }
static SEL S(const char *n) { return sel_registerName(n); }

/* AppKit C fill-list entry (undefined dynamic_lookup -> ___NSRectFillList) */
extern void NSRectFillList(const NSRect *rects, long count);

/* fixtures built in main BEFORE the snapshot — Quinn builds its sprites, digit
 * masks and gloss machinery at init/style-load, long before any re-render. */
static CGImageRef g_sprite;                 /* solid-green cropped sprite */
static CGImageRef g_mask;                   /* 8x8 all-paint image mask   */
static unsigned char g_spritebuf[8 * 8 * 4];
static unsigned char g_maskbuf[8 * 8];      /* 0 == paint through */
static int g_drawn;
static int g_nest;                          /* 1 = snapshot SELF from inside drawRect */
static int g_depth;
static int g_nested_bits = -1;              /* result of the nested capture */
static int check_rep(int mode, id rep);     /* fwd */

/* constant-blue gloss evaluate (the i386 callback CG samples via
 * cg_function_shim during DrawShading) */
static void shading_eval(void *info, const CGFloat *in, CGFloat *out) {
   out[0] = 0.0f; out[1] = 0.0f; out[2] = 1.0f; out[3] = 1.0f;
}
static const CGFloat g_domain[2] = {0.0f, 1.0f};
static const CGFloat g_range[8]  = {0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
static const CGFunctionCallbacks g_cb = {0u, shading_eval, 0};

struct grect { float x, y, w, h; };
/* Quinn's per-cell gate: draw only if the target rect intersects a dirty rect */
static int hit_rects(const struct grect *rects, long cnt, const void *cgr) {
   const float *r = (const float *)cgr;               /* CGRect as 4 floats */
   long i;
   if (!rects || cnt <= 0 || cnt >= 64) { return 0; }
   for (i = 0; i < cnt; i++) {
      if (r[0] < rects[i].x + rects[i].w && rects[i].x < r[0] + r[2] &&
          r[1] < rects[i].y + rects[i].h && rects[i].y < r[1] + r[3]) { return 1; }
   }
   return 0;
}

/* ---- the legacy view: classic ObjC1 class registered onto the REAL AppKit
 * NSView by libabiconv reverse_register_image (the Quinn shape). The local
 * @interface NSView is a fragile-ABI STAND-IN (the i386 sysroot has no AppKit
 * headers); only the superclass NAME reaches the runtime. ---- */
__attribute__((objc_root_class))
@interface NSView { id isa; }
@end
@interface OffscreenRedrawView : NSView
@end
@implementation OffscreenRedrawView
- (void)drawRect:(NSRect)r {
   g_drawn++;
   /* S4: the legacy-immediate-display overlay idiom — a snapshot taken from
    * INSIDE an active draw pass (re-entrant cacheDisplayInRect on self). */
   if (g_nest && g_depth == 0) {
      g_depth = 1;
      NSRect b = {{0.0f, 0.0f}, {64.0f, 64.0f}};
      id nrep = ((id(*)(id, SEL, NSRect))objc_msgSend)(
         (id)self, S("bitmapImageRepForCachingDisplayInRect:"), b);
      if (nrep) {
         ((void(*)(id, SEL, NSRect, id))objc_msgSend)(
            (id)self, S("cacheDisplayInRect:toBitmapImageRep:"), b, nrep);
         g_nested_bits = check_rep(4, nrep);
      }
      g_depth = 0;
   }
   id nsgc = ((id(*)(id, SEL))objc_msgSend)(C("NSGraphicsContext"), S("currentContext"));
   CGContextRef ctx = ((CGContextRef(*)(id, SEL))objc_msgSend)(nsgc, S("graphicsPort"));
   if (!ctx) { return; }
   CGContextSetAlpha(ctx, 1.0f);

   CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
   CGContextSetFillColorSpace(ctx, cs);

   /* 1) background fill — mid gray over the whole view (the control draw that
    * ALREADY lands in the broken snapshot: Quinn's white well / dark bg) */
   CGFloat gray[4] = {0.5f, 0.5f, 0.5f, 1.0f};
   CGContextSetFillColor(ctx, gray);
   CGRect bg = {{0.0f, 0.0f}, {64.0f, 64.0f}};
   CGContextFillRect(ctx, bg);

   /* Quinn-faithful DIRTY-RECT GATING: -[QuinnLocalBoardView drawRect:] runs
    * getRectsBeingDrawn:count: and draws cells/pieces ONLY inside the returned
    * rects (the classic 10.3+ redraw optimisation). Everything below is gated
    * exactly like that — if the list comes back empty/garbage in the offscreen
    * re-render, these draws vanish while the ungated bg fill above survives. */
   const struct grect { float x, y, w, h; } *rects = 0;
   long cnt = 0;
   ((void(*)(id, SEL, void *, void *))objc_msgSend)(
      (id)self, S("getRectsBeingDrawn:count:"), &rects, &cnt);
   printf("getrects depth=%d cnt=%ld", g_depth, cnt);
   if (rects && cnt > 0 && cnt < 64) {
      long i;
      for (i = 0; i < cnt; i++) {
         printf(" [%d,%d %dx%d]", (int)rects[i].x, (int)rects[i].y,
                (int)rects[i].w, (int)rects[i].h);
      }
   }
   printf("\n");

   /* 2) solid RED cell fill (FastDrawCells -1 cell) */
   CGRect rf = {{4.0f, 4.0f}, {12.0f, 12.0f}};
   if (hit_rects(rects, cnt, &rf)) {
      CGFloat red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
      CGContextSetFillColor(ctx, red);
      CGContextFillRect(ctx, rf);
   }

   /* 3) sprite CGContextDrawImage (FastDrawCells occupied cell) — GREEN */
   CGRect ri = {{20.0f, 4.0f}, {16.0f, 16.0f}};
   if (hit_rects(rects, cnt, &ri)) {
      CGContextDrawImage(ctx, ri, g_sprite);
   }

   /* 4) axial gloss shading (QuinnPlayerInfoCell box), built IN the draw like
    * Quinn (CGFunctionCreate + CGShadingCreateAxial + clip + DrawShading) — BLUE */
   CGRect rs = {{4.0f, 28.0f}, {16.0f, 16.0f}};
   if (hit_rects(rects, cnt, &rs)) {
      CGFunctionRef fn = CGFunctionCreate((void *)0, 1ul, g_domain, 4ul, g_range, &g_cb);
      CGPoint p0 = {4.0f, 36.0f}, p1 = {20.0f, 36.0f};
      CGShadingRef sh = CGShadingCreateAxial(cs, p0, p1, fn, 0, 0);
      CGContextSaveGState(ctx);
      CGContextClipToRect(ctx, rs);
      CGContextDrawShading(ctx, sh);
      CGContextRestoreGState(ctx);
      CGShadingRelease(sh);
      CGFunctionRelease(fn);
   }

   /* 5) mask-clipped fill (LCDCell 7-seg digit stroke) — MAGENTA */
   CGRect rm = {{28.0f, 28.0f}, {16.0f, 16.0f}};
   if (hit_rects(rects, cnt, &rm)) {
      CGContextSaveGState(ctx);
      CGContextClipToMask(ctx, rm, g_mask);
      CGFloat mag[4] = {1.0f, 0.0f, 1.0f, 1.0f};
      CGContextSetFillColor(ctx, mag);
      CGContextFillRect(ctx, rm);
      CGContextRestoreGState(ctx);
   }

   /* 6) NSRectFillList rect-LIST fill ([NSColor set] + AppKit C fill-list,
    * the iWeb idiom; exercises the CGFloat-geometry-ARRAY marshalling
    * cg_geoarray_shim through the REAL current-context path) — CYAN */
   {
      id cyan = ((id(*)(id, SEL, CGFloat, CGFloat, CGFloat, CGFloat))objc_msgSend)(
         C("NSColor"), S("colorWithCalibratedRed:green:blue:alpha:"),
         0.0f, 1.0f, 1.0f, 1.0f);
      ((void(*)(id, SEL))objc_msgSend)(cyan, S("set"));
      NSRect list[2] = {{{48.0f, 8.0f}, {8.0f, 8.0f}},
                        {{48.0f, 44.0f}, {8.0f, 8.0f}}};
      NSRectFillList(list, 2);
   }

   CGColorSpaceRelease(cs);
}
@end

/* sample the rep at VIEW point (x,y) (unflipped view: rep row 0 == view top),
 * scale-aware (handles both the legacy 1x rep and a native/self-built 2x rep). */
static const unsigned char *sample(const unsigned char *d, long bpr, long spp,
                                   long pw, long ph, int x, int y) {
   long sx = (x * pw) / 64, sy = (y * ph) / 64;
   return d + (ph - 1 - sy) * bpr + sx * spp;
}

static id nsstr(const char *utf8) {
   return ((id(*)(id, SEL, const char *))objc_msgSend)(
      C("NSString"), S("stringWithUTF8String:"), utf8);
}

/* One scenario = create a fresh legacy view (optionally hosted in a never-shown
 * borderless window / layer-backed / snapshotted into a 2x rep), run the FULL
 * cacheDisplayInRect re-render, pixel-check all five draw families.
 *   mode 0: windowless                      (minimal full path)
 *   mode 1: in window                       (real hosting)
 *   mode 2: in window + wantsLayer          (Quinn: AppKit-forced layer-backing)
 *   mode 3: in window + wantsLayer + 2x rep (the overlay/native Retina shape)
 * Returns the 7-bit result (127 == all landed). */
static int run_scenario(int mode) {
   int bits = 0;
   g_drawn = 0;

   NSRect frame = {{0.0f, 0.0f}, {64.0f, 64.0f}};
   id v = ((id(*)(id, SEL))objc_msgSend)(C("OffscreenRedrawView"), S("alloc"));
   v = ((id(*)(id, SEL, NSRect))objc_msgSend)(v, S("initWithFrame:"), frame);
   if (!v) { puts("no view"); return 0; }

   if (mode >= 1) {
      id w = ((id(*)(id, SEL))objc_msgSend)(C("NSWindow"), S("alloc"));
      w = ((id(*)(id, SEL, NSRect, unsigned long, unsigned long, signed char))objc_msgSend)(
         w, S("initWithContentRect:styleMask:backing:defer:"), frame, 0ul, 2ul, (signed char)1);
      if (!w) { puts("no window"); return 0; }
      id cv = ((id(*)(id, SEL))objc_msgSend)(w, S("contentView"));
      ((void(*)(id, SEL, id))objc_msgSend)(cv, S("addSubview:"), v);
   }
   if (mode >= 2) {
      ((void(*)(id, SEL, signed char))objc_msgSend)(v, S("setWantsLayer:"), (signed char)1);
   }

   id rep;
   if (mode >= 3) {
      /* self-built 2x rep (128px backing a 64pt rect) — what native AppKit
       * vends on Retina and what the overlay's native-origin capture uses */
      rep = ((id(*)(id, SEL))objc_msgSend)(C("NSBitmapImageRep"), S("alloc"));
      rep = ((id(*)(id, SEL, void *, long, long, long, long, long, long,
                    id, long, long))objc_msgSend)(
         rep,
         S("initWithBitmapDataPlanes:pixelsWide:pixelsHigh:bitsPerSample:"
           "samplesPerPixel:hasAlpha:isPlanar:colorSpaceName:bytesPerRow:bitsPerPixel:"),
         (void *)0, 128L, 128L, 8L, 4L, 1L, 0L,
         nsstr("NSCalibratedRGBColorSpace"), 0L, 0L);
      if (rep) {
         NSSize pts = {64.0f, 64.0f};
         ((void(*)(id, SEL, NSSize))objc_msgSend)(rep, S("setSize:"), pts);
      }
   } else {
      rep = ((id(*)(id, SEL, NSRect))objc_msgSend)(
         v, S("bitmapImageRepForCachingDisplayInRect:"), frame);
   }
   if (!rep) { puts("no rep"); return 0; }
   ((void(*)(id, SEL, NSRect, id))objc_msgSend)(
      v, S("cacheDisplayInRect:toBitmapImageRep:"), frame, rep);

   return check_rep(mode, rep);
}

/* pixel-check one snapshot rep; returns the 7-bit result */
static int check_rep(int mode, id rep) {
   int bits = 0;
   long bpr = ((long(*)(id, SEL))objc_msgSend)(rep, S("bytesPerRow"));
   long spp = ((long(*)(id, SEL))objc_msgSend)(rep, S("samplesPerPixel"));
   long pw  = ((long(*)(id, SEL))objc_msgSend)(rep, S("pixelsWide"));
   long ph  = ((long(*)(id, SEL))objc_msgSend)(rep, S("pixelsHigh"));
   const unsigned char *d =
      ((const unsigned char *(*)(id, SEL))objc_msgSend)(rep, S("bitmapData"));
   if (!d || bpr <= 0 || spp < 3 || pw <= 0 || ph <= 0) { puts("no bitmapData"); return 0; }
   bits |= 1;                                                     /* bit0 */
   if (g_drawn > 0) { bits |= 2; }                                /* bit1 */

   const unsigned char *p;
   p = sample(d, bpr, spp, pw, ph, 56, 56);                       /* background */
   printf("S%d bg    rgba=%d,%d,%d,%d\n", mode, p[0], p[1], p[2], spp > 3 ? p[3] : -1);
   if (p[0] > 70 && p[0] < 210 && p[1] > 70 && p[1] < 210 &&
       p[2] > 70 && p[2] < 210) { bits |= 4; }                    /* bit2 */

   p = sample(d, bpr, spp, pw, ph, 10, 10);                       /* red fill */
   printf("S%d fill  rgba=%d,%d,%d,%d\n", mode, p[0], p[1], p[2], spp > 3 ? p[3] : -1);
   if (p[0] > 180 && p[1] < 110 && p[2] < 110) { bits |= 8; }     /* bit3 */

   p = sample(d, bpr, spp, pw, ph, 28, 12);                       /* green image */
   printf("S%d image rgba=%d,%d,%d,%d\n", mode, p[0], p[1], p[2], spp > 3 ? p[3] : -1);
   if (p[1] > 150 && p[0] < 140 && p[2] < 140) { bits |= 16; }    /* bit4 */

   p = sample(d, bpr, spp, pw, ph, 12, 36);                       /* blue shading */
   printf("S%d shade rgba=%d,%d,%d,%d\n", mode, p[0], p[1], p[2], spp > 3 ? p[3] : -1);
   if (p[2] > 150 && p[0] < 140 && p[1] < 140) { bits |= 32; }    /* bit5 */

   p = sample(d, bpr, spp, pw, ph, 36, 36);                       /* magenta mask */
   printf("S%d mask  rgba=%d,%d,%d,%d\n", mode, p[0], p[1], p[2], spp > 3 ? p[3] : -1);
   if (p[0] > 180 && p[2] > 180 && p[1] < 110) { bits |= 64; }    /* bit6 */

   {                                                              /* cyan fill list */
      const unsigned char *q = sample(d, bpr, spp, pw, ph, 52, 48);
      p = sample(d, bpr, spp, pw, ph, 52, 12);
      printf("S%d list  rgba=%d,%d,%d + %d,%d,%d\n", mode,
             p[0], p[1], p[2], q[0], q[1], q[2]);
      if (p[1] > 150 && p[2] > 150 && p[0] < 140 &&
          q[1] > 150 && q[2] > 150 && q[0] < 140) { bits |= 128; }   /* bit7 */
   }

   printf("S%d bits=%d drawn=%d rep=%ldx%ld spp=%ld\n", mode, bits, g_drawn, pw, ph, spp);
   return bits;
}

int main(void) {
   ((id(*)(id, SEL))objc_msgSend)(C("NSApplication"), S("sharedApplication"));

   /* ---- fixtures (built ONCE, long before any snapshot — the Quinn shape;
    * every handle round-trips the arena exactly as Quinn's) ---- */
   {
      unsigned i;
      for (i = 0; i < sizeof g_spritebuf; i += 4) {
         g_spritebuf[i] = 0; g_spritebuf[i + 1] = 255;
         g_spritebuf[i + 2] = 0; g_spritebuf[i + 3] = 255;      /* green */
      }
      for (i = 0; i < sizeof g_maskbuf; i++) { g_maskbuf[i] = 0; } /* paint */

      CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
      CGContextRef mctx = CGBitmapContextCreate(g_spritebuf, 8, 8, 8, 8 * 4, cs,
                                                kCGImageAlphaPremultipliedLast);
      CGImageRef master = CGBitmapContextCreateImage(mctx);
      CGRect whole = {{0.0f, 0.0f}, {8.0f, 8.0f}};
      CGImageRef crop = CGImageCreateWithImageInRect(master, whole);
      g_sprite = crop ? crop : master;
      CGContextRelease(mctx);
      CGColorSpaceRelease(cs);

      CGDataProviderRef prov =
         CGDataProviderCreateWithData((void *)0, g_maskbuf, sizeof g_maskbuf, (void *)0);
      g_mask = CGImageMaskCreate(8, 8, 8, 8, 8, prov, (const CGFloat *)0, 0);
      CGDataProviderRelease(prov);
      if (!g_sprite || !g_mask) { puts("fixture build failed"); exit(0); }
   }

   int and_bits = 255;
   int m;
   for (m = 0; m <= 3; m++) {
      and_bits &= run_scenario(m);
   }
   /* S4: nested self-snapshot from inside an active draw pass (window +
    * layer-backed hosting, the overlay's structural condition) */
   g_nest = 1;
   int outer = run_scenario(2);
   g_nest = 0;
   printf("S4 nested_bits=%d outer_bits=%d\n", g_nested_bits, outer);
   and_bits &= outer;
   and_bits &= (g_nested_bits < 0) ? 0 : g_nested_bits;
   printf("AND bits=%d\n", and_bits);
   exit(and_bits);                                /* 255 == every scenario clean */
   return 0;
}
EOF

clang -arch i386 -isysroot "$HOST_SDK" -mmacosx-version-min=10.6 \
   -fobjc-runtime=macosx-fragile -c "$TMP/t.m" -o "$TMP/t.o" 2>"$TMP/cc.err" \
   || { cat "$TMP/cc.err"; fail compile; }

# AppKit/CG aren't in the i386 sysroot: the classic superclass ref
# (.objc_class_name_NSView) and every CG entry stay undefined dynamic_lookup
# imports the pipeline's static-interpose resolves to libabiconv (exactly the
# translated-Quinn shape: its .objc_class_name_* AppKit refs ride as never-fired
# lazy binds).
"$LD" -arch i386 -macos_version_min 10.6 -no_pie -syslibroot "$SYSROOT" \
   -lSystem -lobjc -framework Foundation -framework CoreFoundation \
   -undefined dynamic_lookup -e _main \
   -o "$TMP/t.i386" "$TMP/t.o" 2>"$TMP/ld.err" \
   || { cat "$TMP/ld.err"; fail link; }

bash "$PIPELINE" -m "$MT" -l "$LIBABICONV" -w "$LIBWRAPPER" -i "$LIBINTERPOSE" \
   -o build/offscreen_redraw.x86_64 "$TMP/t.i386" >"$TMP/pipe.log" 2>&1 \
   || { tail -5 "$TMP/pipe.log"; fail translate; }
chmod +x build/offscreen_redraw.x86_64

# 120s watchdog: a wedged AppKit boot must not hang the suite.
OUT="$(perl -e 'alarm 120; exec @ARGV' build/offscreen_redraw.x86_64 2>"$TMP/run.err")"
RC=$?

if [ "$RC" -eq 255 ]; then
   echo "offscreen-redraw: PASS (all draw families land in the cacheDisplay re-render)"
   exit 0
fi
echo "offscreen-redraw: FAIL (bits=$RC, want 255; bit2=bg bit3=fill bit4=image bit5=shading bit6=mask bit7=rectlist)"
echo "$OUT" | sed 's/^/    /'
tail -5 "$TMP/run.err" | sed 's/^/    stderr: /'
exit 1
