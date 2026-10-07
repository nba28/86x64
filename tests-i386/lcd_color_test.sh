#!/bin/bash
# lcd_color_test.sh — repro + regression guard for the NSColor CGFloat-arg
# COLOR-FACTORY marshalling fix (objc_shim.c g_cgfloat_sels: the WHITE/HSB/
# component NSColor creators).
#
# Root: a CGFloat is a 4-byte float on i386 but the modern runtime encodes it
# 'd' (double, 8 bytes) — identical to a true double. When a translated app only
# CALLS +[NSColor colorWithCalibratedWhite:(CGFloat)white alpha:(CGFloat)alpha]
# (never declares it), the forward bridge fell back to the native 'd' and read
# 8 bytes (TWO i386 slots) per CGFloat -> both args became fused garbage doubles
# and every following arg misaligned -> a black/transparent NSColor. Quinn's
# -[LCDCell] builds its digit ON colour (glowing light), its OFF/ghost colour,
# and its light-gray label text via exactly this factory -> lit digits rendered
# DARK, the faint unlit-ghost segments collapsed to alpha~0 (invisible), and the
# NEXT/SCORE/LINES/LEVEL/LPM labels went invisible. The fix adds the white/HSB/
# alpha color creators to the cgfloat-mask registry so each marked arg is read as
# ONE 4-byte i386 float and cvtss2sd-widened.
#
# Translated i386 -> x86_64 through the REAL pipeline + libabiconv AppKit bridge
# (AppKit isn't in the i386 sysroot -> NSColor / NSGraphicsContext /
# NSBitmapImageRep are undefined dynamic_lookup imports static-interpose resolves
# to the bridge, exactly as for real Quinn). It draws the exact LCD idiom into an
# offscreen bitmap and reads back the pixels via RAW bitmapData. Exit protocol:
# 3 bits — bit0 lit-on-colour LIGHT, bit1 ghost-off-colour FAINT-but-present,
# bit2 a plain redColor control still correct — 7 == all correct.
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
   echo "SKIP lcd-color (no i386 sysroot at $SYSROOT; run 'make sysroot')"; exit 0
fi
if [ ! -f "$SYSROOT/usr/lib/libobjc.dylib" ] && [ ! -f "$SYSROOT/usr/lib/libobjc.A.dylib" ]; then
   echo "SKIP lcd-color (no staged i386 libobjc; run 'make sysroot-objc')"; exit 0
fi
for f in "$MT" "$LIBABICONV" "$LIBWRAPPER" "$LIBINTERPOSE"; do
   [ -e "$f" ] || { echo "SKIP lcd-color (missing $f; build first)"; exit 0; }
done

mkdir -p build
cp -f "$LIBABICONV" build/libabiconv.dylib
fail() { echo "FAIL lcd-color ($1)"; exit 1; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/t.m" <<'EOF'
#include <stdio.h>
extern void exit(int);
typedef struct objc_object *id; typedef struct objc_selector *SEL;
extern id objc_getClass(const char*); extern SEL sel_registerName(const char*);
extern id objc_msgSend(id,SEL,...);
typedef float CGFloat;
typedef struct {CGFloat x,y;} NSPoint; typedef struct{CGFloat w,h;} NSSize;
typedef struct{NSPoint o;NSSize s;} NSRect; typedef NSRect CGRect;
typedef void* CGContextRef;
extern void CGContextFillRect(CGContextRef,CGRect);
static id  C(const char*n){return objc_getClass(n);}
static SEL S(const char*n){return sel_registerName(n);}

/* the EXACT -[LCDCell drawDigitElements:] colour idiom, drawn into the context
 * AppKit hands drawRect: (currentContext.graphicsPort). */
__attribute__((objc_root_class)) @interface NSView {id isa;} @end
@interface LCDColorView : NSView @end
@implementation LCDColorView
- (void)drawRect:(NSRect)r {
   CGContextRef ctx = ((CGContextRef(*)(id,SEL))objc_msgSend)(
      ((id(*)(id,SEL))objc_msgSend)(C("NSGraphicsContext"),S("currentContext")),
      S("graphicsPort"));
   /* cell 0 (LIT): on-colour = colorWithCalibratedWhite:0.9 alpha:1.0, then
    * colorWithAlphaComponent:1.0 (the opacity pass), set, fill */
   id on = ((id(*)(id,SEL,CGFloat,CGFloat))objc_msgSend)(
      C("NSColor"), S("colorWithCalibratedWhite:alpha:"), 0.9f, 1.0f);
   id lit = ((id(*)(id,SEL,CGFloat))objc_msgSend)(on, S("colorWithAlphaComponent:"), 1.0f);
   ((void(*)(id,SEL))objc_msgSend)(lit, S("set"));
   CGRect r0 = {{0,0},{5,16}}; CGContextFillRect(ctx, r0);
   /* cell 1 (GHOST): off-colour = colorWithCalibratedWhite:1.0 alpha:0.1, set, fill */
   id off = ((id(*)(id,SEL,CGFloat,CGFloat))objc_msgSend)(
      C("NSColor"), S("colorWithCalibratedWhite:alpha:"), 1.0f, 0.1f);
   ((void(*)(id,SEL))objc_msgSend)(off, S("set"));
   CGRect r1 = {{6,0},{5,16}}; CGContextFillRect(ctx, r1);
   /* cell 2 (CONTROL): redColor (no CGFloat args), set, fill */
   id red = ((id(*)(id,SEL))objc_msgSend)(C("NSColor"), S("redColor"));
   ((void(*)(id,SEL))objc_msgSend)(red, S("set"));
   CGRect r2 = {{12,0},{4,16}}; CGContextFillRect(ctx, r2);
}
@end

int main(void) {
   ((id(*)(id,SEL))objc_msgSend)(C("NSApplication"), S("sharedApplication"));
   NSRect fr = {{0,0},{16,16}};
   id v = ((id(*)(id,SEL))objc_msgSend)(C("LCDColorView"), S("alloc"));
   v = ((id(*)(id,SEL,NSRect))objc_msgSend)(v, S("initWithFrame:"), fr);
   id rep = ((id(*)(id,SEL,NSRect))objc_msgSend)(
      v, S("bitmapImageRepForCachingDisplayInRect:"), fr);
   if (!rep) { puts("no rep"); exit(0); }
   ((void(*)(id,SEL,NSRect,id))objc_msgSend)(
      v, S("cacheDisplayInRect:toBitmapImageRep:"), fr, rep);

   long bpr = ((long(*)(id,SEL))objc_msgSend)(rep, S("bytesPerRow"));
   long pw  = ((long(*)(id,SEL))objc_msgSend)(rep, S("pixelsWide"));
   long ph  = ((long(*)(id,SEL))objc_msgSend)(rep, S("pixelsHigh"));
   const unsigned char *d = ((const unsigned char*(*)(id,SEL))objc_msgSend)(rep, S("bitmapData"));
   if (!d || bpr <= 0) { puts("no data"); exit(0); }
   long yy = (8*ph)/16;
   const unsigned char *lit = d + (ph-1-yy)*bpr + ((2*pw)/16)*4;
   const unsigned char *gho = d + (ph-1-yy)*bpr + ((8*pw)/16)*4;
   const unsigned char *ctl = d + (ph-1-yy)*bpr + ((14*pw)/16)*4;
   printf("lit=%d,%d,%d,%d ghost=%d,%d,%d,%d ctl=%d,%d,%d,%d\n",
      lit[0],lit[1],lit[2],lit[3], gho[0],gho[1],gho[2],gho[3], ctl[0],ctl[1],ctl[2],ctl[3]);
   int bits = 0;
   if (lit[0] > 200 && lit[1] > 200 && lit[2] > 200) bits |= 1;      /* light on-colour */
   if (gho[0] > 2 && gho[0] < 120)                    bits |= 2;      /* faint ghost     */
   if (ctl[0] > 200 && ctl[1] < 90 && ctl[2] < 90)    bits |= 4;      /* red control     */
   exit(bits);                                                        /* 7 == all correct */
}
EOF

clang -arch i386 -isysroot "$HOST_SDK" -mmacosx-version-min=10.6 \
   -fobjc-runtime=macosx-fragile -c "$TMP/t.m" -o "$TMP/t.o" 2>"$TMP/cc.err" \
   || { cat "$TMP/cc.err"; fail compile; }
"$LD" -arch i386 -macos_version_min 10.6 -no_pie -syslibroot "$SYSROOT" \
   -lSystem -lobjc -framework Foundation -framework CoreFoundation \
   -undefined dynamic_lookup -e _main -o "$TMP/t.i386" "$TMP/t.o" 2>"$TMP/ld.err" \
   || { cat "$TMP/ld.err"; fail link; }
bash "$PIPELINE" -m "$MT" -l "$LIBABICONV" -w "$LIBWRAPPER" -i "$LIBINTERPOSE" \
   -o build/lcd_color.x86_64 "$TMP/t.i386" >"$TMP/pipe.log" 2>&1 \
   || { tail -5 "$TMP/pipe.log"; fail translate; }
chmod +x build/lcd_color.x86_64

OUT="$(perl -e 'alarm 120; exec @ARGV' build/lcd_color.x86_64 2>"$TMP/run.err")"
RC=$?
if [ "$RC" -eq 7 ]; then
   echo "lcd-color: PASS ($OUT)"; exit 0
fi
echo "lcd-color: FAIL (bits=$RC, want 7: bit0=lit-light bit1=ghost-faint bit2=red-control)"
echo "$OUT" | sed 's/^/    /'
tail -3 "$TMP/run.err" | sed 's/^/    stderr: /'
exit 1
