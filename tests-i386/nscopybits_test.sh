#!/bin/bash
# nscopybits_test.sh — repro + guard for the legacy -[NSView gState] + NSCopyBits
# view pixel-blit restore (objc_shim.c legacy_gstate_capture_install +
# nscopybits_shim.c).
#
# Root: the pre-10.10 offscreen view-capture idiom copies a source view's pixels
# with NSCopyBits([sourceView gState], srcRect, destPoint) into a focused dest
# (Quinn's -[NSView(QuinnExtensions) imageFromRect:], which builds the board
# REFLECTION source image). On modern macOS -[NSView gState] returns 0, so
# NSCopyBits(0, ...) copies NOTHING -> the reflected pieces vanish (the static
# gradient reflects via a direct draw, so only the pieces are missing).
#
# FIX: -[NSView gState] hands a LEGACY caller a non-zero TOKEN mapped to the view;
# the NSCopyBits shim resolves the token and renders the source view's srcRect
# into the current focus context at destPoint. STRUCTURAL trigger (token), never
# app-specific.
#
# This guard replays the EXACT idiom under translation: a legacy source NSView
# subclass draws a RED "piece" on black; the test focuses a dest bitmap context,
# calls NSCopyBits([src gState], srcRect, destPoint), and reads the dest pixels
# back. Exit protocol: bit0 the dest received the RED piece (fix works), bit1 the
# black background also copied (full rect, not just an edge). 3 == fixed. Without
# the fix, gState==0 -> NSCopyBits copies nothing -> the dest stays its initial
# color -> bit0 clear (RED absent) == RED.
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
   echo "SKIP nscopybits (no i386 sysroot at $SYSROOT; run 'make sysroot')"; exit 0
fi
if [ ! -f "$SYSROOT/usr/lib/libobjc.dylib" ] && [ ! -f "$SYSROOT/usr/lib/libobjc.A.dylib" ]; then
   echo "SKIP nscopybits (no staged i386 libobjc; run 'make sysroot-objc')"; exit 0
fi
for f in "$MT" "$LIBABICONV" "$LIBWRAPPER" "$LIBINTERPOSE"; do
   [ -e "$f" ] || { echo "SKIP nscopybits (missing $f; build first)"; exit 0; }
done

mkdir -p build
cp -f "$LIBABICONV" build/libabiconv.dylib
fail() { echo "FAIL nscopybits ($1)"; exit 1; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/t.m" <<'EOF'
#include <stdio.h>
extern void exit(int);
typedef struct objc_object *id; typedef struct objc_selector *SEL;
extern id objc_getClass(const char*); extern SEL sel_registerName(const char*);
extern id objc_msgSend(id,SEL,...);
typedef float CGFloat;
typedef struct{CGFloat x,y;}NSPoint; typedef struct{CGFloat w,h;}NSSize;
typedef struct{NSPoint o;NSSize s;}NSRect;
extern void NSRectFill(NSRect);
extern void NSCopyBits(long,NSRect,NSPoint);   /* undefined dynamic_lookup -> ___NSCopyBits */
static id  C(const char*n){return objc_getClass(n);}
static SEL S(const char*n){return sel_registerName(n);}
static id nsstr(const char*u){return ((id(*)(id,SEL,const char*))objc_msgSend)(
   C("NSString"),S("stringWithUTF8String:"),u);}

__attribute__((objc_root_class)) @interface NSView {id isa;} @end
@interface SrcView : NSView @end
@implementation SrcView
- (signed char)isOpaque { return 1; }
- (void)drawRect:(NSRect)r {
   ((void(*)(id,SEL))objc_msgSend)(
      ((id(*)(id,SEL))objc_msgSend)(C("NSColor"),S("blackColor")),S("set"));
   NSRect bg={{0,0},{40,40}}; NSRectFill(bg);
   ((void(*)(id,SEL))objc_msgSend)(
      ((id(*)(id,SEL))objc_msgSend)(C("NSColor"),S("redColor")),S("set"));
   NSRect piece={{10,10},{20,20}}; NSRectFill(piece);   /* the "settled piece" */
}
@end

int main(void){
   ((id(*)(id,SEL))objc_msgSend)(C("NSApplication"),S("sharedApplication"));
   NSRect fr={{0,0},{40,40}};
   /* source view in a window (so it has a real backing to capture) */
   id w=((id(*)(id,SEL,NSRect,unsigned long,unsigned long,signed char))objc_msgSend)(
      ((id(*)(id,SEL))objc_msgSend)(C("NSWindow"),S("alloc")),
      S("initWithContentRect:styleMask:backing:defer:"),fr,0ul,2ul,(signed char)0);
   id v=((id(*)(id,SEL))objc_msgSend)(C("SrcView"),S("alloc"));
   v=((id(*)(id,SEL,NSRect))objc_msgSend)(v,S("initWithFrame:"),fr);
   ((void(*)(id,SEL,id))objc_msgSend)(
      ((id(*)(id,SEL))objc_msgSend)(w,S("contentView")),S("addSubview:"),v);

   /* DEST: an explicit bitmap-backed graphics context, focused as current — this
    * stands in for imageFromRect:'s [destImage lockFocus] (NSImage lockFocus
    * needs a window server, unavailable headless; the shim draws into whatever
    * the current context is). Start the dest GREEN so an unchanged pixel is
    * clearly "nothing copied". */
   id dst=((id(*)(id,SEL))objc_msgSend)(C("NSBitmapImageRep"),S("alloc"));
   dst=((id(*)(id,SEL,void*,long,long,long,long,long,long,id,long,long))objc_msgSend)(
      dst,S("initWithBitmapDataPlanes:pixelsWide:pixelsHigh:bitsPerSample:"
            "samplesPerPixel:hasAlpha:isPlanar:colorSpaceName:bytesPerRow:bitsPerPixel:"),
      (void*)0,40L,40L,8L,4L,1L,0L,nsstr("NSCalibratedRGBColorSpace"),0L,0L);
   id gc=((id(*)(id,SEL,id))objc_msgSend)(C("NSGraphicsContext"),
      S("graphicsContextWithBitmapImageRep:"),dst);
   ((void(*)(id,SEL))objc_msgSend)(C("NSGraphicsContext"),S("saveGraphicsState"));
   ((void(*)(id,SEL,id))objc_msgSend)(C("NSGraphicsContext"),S("setCurrentContext:"),gc);
   ((void(*)(id,SEL))objc_msgSend)(
      ((id(*)(id,SEL))objc_msgSend)(C("NSColor"),S("greenColor")),S("set"));
   NSRect all={{0,0},{40,40}}; NSRectFill(all);       /* dest starts green */

   /* the legacy idiom: gState of the source, NSCopyBits into the current (dest) */
   long gs=((long(*)(id,SEL))objc_msgSend)(v,S("gState"));
   NSRect sr={{0,0},{40,40}}; NSPoint dp={0,0};
   NSCopyBits(gs, sr, dp);

   id gcf=((id(*)(id,SEL))objc_msgSend)(C("NSGraphicsContext"),S("currentContext"));
   if(gcf)((void(*)(id,SEL))objc_msgSend)(gcf,S("flushGraphics"));
   ((void(*)(id,SEL))objc_msgSend)(C("NSGraphicsContext"),S("restoreGraphicsState"));

   const unsigned char* d=((const unsigned char*(*)(id,SEL))objc_msgSend)(dst,S("bitmapData"));
   long bpr=((long(*)(id,SEL))objc_msgSend)(dst,S("bytesPerRow"));
   if(!d){puts("no data");exit(0);}
   /* piece center (view 20,20) and a bg point (view 3,3). rep row 0 = top. */
   int red=0,black=0,total=0;
   for(int y=0;y<40;y++)for(int x=0;x<40;x++){
      const unsigned char*p=d+y*bpr+x*4; total++;
      if(p[0]>180&&p[1]<90&&p[2]<90)red++;
      if(p[0]<60&&p[1]<60&&p[2]<60)black++;
   }
   printf("red=%d black=%d total=%d\n",red,black,total);
   int bits=0;
   if(red>50)   bits|=1;   /* the red piece got copied (the fix)         */
   if(black>50) bits|=2;   /* the black bg also copied (full-rect blit)  */
   exit(bits);             /* 3 == fixed */
}
EOF

clang -arch i386 -isysroot "$HOST_SDK" -mmacosx-version-min=10.6 \
   -fobjc-runtime=macosx-fragile -c "$TMP/t.m" -o "$TMP/t.o" 2>"$TMP/cc.err" \
   || { cat "$TMP/cc.err"; fail compile; }
ld -arch i386 -macos_version_min 10.6 -no_pie -syslibroot "$SYSROOT" \
   -lSystem -lobjc -framework Foundation -framework CoreFoundation \
   -undefined dynamic_lookup -e _main -o "$TMP/t.i386" "$TMP/t.o" 2>"$TMP/ld.err" \
   || { cat "$TMP/ld.err"; fail link; }
bash "$PIPELINE" -m "$MT" -l "$LIBABICONV" -w "$LIBWRAPPER" -i "$LIBINTERPOSE" \
   -o build/nscopybits.x86_64 "$TMP/t.i386" >"$TMP/pipe.log" 2>&1 \
   || { tail -5 "$TMP/pipe.log"; fail translate; }
chmod +x build/nscopybits.x86_64

OUT="$(perl -e 'alarm 120; exec @ARGV' build/nscopybits.x86_64 2>"$TMP/run.err")"; RC=$?
if [ "$RC" -eq 3 ]; then
   echo "nscopybits: PASS ($OUT)"
   exit 0
fi
echo "nscopybits: FAIL (bits=$RC, want 3: bit0=piece-copied bit1=bg-copied)"
echo "$OUT" | sed 's/^/    /'
tail -3 "$TMP/run.err" | sed 's/^/    stderr: /'
exit 1
