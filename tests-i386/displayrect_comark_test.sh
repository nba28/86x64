#!/bin/bash
# displayrect_comark_test.sh — guard for the LEGACY immediate-displayRect
# compositor co-mark (objc_shim.c, legacy_displayrect_comark_install /
# ABICONV_QUINN_DISPLAYRECT_COMARK).
#
# Root: a legacy i386 app's animation framework drives per-frame redraw via an
# IMMEDIATE synchronous -[NSView displayRect:] / displayRectIgnoringOpacity:
# (Quinn's ATViewAnimation -> [board displayRect:(union of old+new piece cells)]
# each rotate frame). On 10.6 that drew straight to the window backing the
# WindowServer read; on modern macOS a LAYER-BACKED view's immediate displayRect:
# lands in the layer backing but the WindowServer may not recomposite that
# sub-rect -> stale old-piece pixels persist (the rotate TRAIL) + partial new
# draw (lost squares).
#
# FIX: a swizzle of displayRect:/displayRectIgnoringOpacity? that, for a
# LEGACY-originated call on a LAYER-BACKED view, runs the ORIGINAL immediate draw
# FIRST (synchrony preserved) and THEN issues ONE setNeedsDisplayInRect: for the
# same rect, scheduling AppKit's coalesced compositor-facing pass. STRUCTURAL gate
# (legacy-origin — set by the forward bridge for exactly this send — + immediate-
# displayRect selector + layer!=nil); never class/app specific.
#
# The on-SCREEN result (trail gone) needs a live WindowServer and is confirmed by
# a manual display A/B (the compositor cycle can't run headlessly). This guard
# proves the MECHANISM + its env gate headlessly by counting the setNeedsDisplay
# InRect: the view receives during ONE legacy-origin displayRect:. The view
# overrides setNeedsDisplayInRect: to count; a plain (COMARK OFF) displayRect:
# issues N internal marks, COMARK ON issues EXACTLY ONE MORE (our co-mark):
#   OFF -> baseline count
#   ON  -> baseline + 1   (the extra compositor-facing mark)
# Legacy-origin is intrinsic here — every send from the translated i386 program
# through the forward bridge sets the flag, so displayRect: naturally exercises
# the legacy path (the NATIVE-origin direction — flag 0, never co-marked — is
# covered by the swizzle's `if (!legacy) return` and the lockfocus/cachedisplay
# native guards' pattern).
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
   echo "SKIP displayrect-comark (no i386 sysroot at $SYSROOT; run 'make sysroot')"; exit 0
fi
if [ ! -f "$SYSROOT/usr/lib/libobjc.dylib" ] && [ ! -f "$SYSROOT/usr/lib/libobjc.A.dylib" ]; then
   echo "SKIP displayrect-comark (no staged i386 libobjc; run 'make sysroot-objc')"; exit 0
fi
for f in "$MT" "$LIBABICONV" "$LIBWRAPPER" "$LIBINTERPOSE"; do
   [ -e "$f" ] || { echo "SKIP displayrect-comark (missing $f; build first)"; exit 0; }
done

mkdir -p build
cp -f "$LIBABICONV" build/libabiconv.dylib
fail() { echo "FAIL displayrect-comark ($1)"; exit 1; }

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
static id  C(const char*n){return objc_getClass(n);}
static SEL S(const char*n){return sel_registerName(n);}

static int g_snd=0;
__attribute__((objc_root_class)) @interface NSView {id isa;} @end
@interface CMView : NSView @end
@implementation CMView
- (signed char)isOpaque { return 1; }
- (void)drawRect:(NSRect)r {}
/* count-only override (does NOT chain to super): measures how many
 * setNeedsDisplayInRect: the view receives during one displayRect:. */
- (void)setNeedsDisplayInRect:(NSRect)r { g_snd++; }
@end

int main(void){
   ((id(*)(id,SEL))objc_msgSend)(C("NSApplication"),S("sharedApplication"));
   NSRect fr={{0,0},{100,100}};
   id w=((id(*)(id,SEL,NSRect,unsigned long,unsigned long,signed char))objc_msgSend)(
      ((id(*)(id,SEL))objc_msgSend)(C("NSWindow"),S("alloc")),
      S("initWithContentRect:styleMask:backing:defer:"),fr,0ul,2ul,(signed char)0);
   id v=((id(*)(id,SEL))objc_msgSend)(C("CMView"),S("alloc"));
   v=((id(*)(id,SEL,NSRect))objc_msgSend)(v,S("initWithFrame:"),fr);
   ((void(*)(id,SEL,id))objc_msgSend)(
      ((id(*)(id,SEL))objc_msgSend)(w,S("contentView")),S("addSubview:"),v);
   ((void(*)(id,SEL,signed char))objc_msgSend)(v,S("setWantsLayer:"),(signed char)1);

   g_snd=0;
   NSRect dirty={{10,20},{30,40}};
   ((void(*)(id,SEL,NSRect))objc_msgSend)(v,S("displayRect:"),dirty);   /* legacy-origin */
   printf("snd=%d\n", g_snd);
   /* clamp to a small range so a runaway count can't masquerade as a valid delta */
   exit(g_snd > 63 ? 63 : g_snd);
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
   -o build/displayrect_comark.x86_64 "$TMP/t.i386" >"$TMP/pipe.log" 2>&1 \
   || { tail -5 "$TMP/pipe.log"; fail translate; }
chmod +x build/displayrect_comark.x86_64

perl -e 'alarm 120; exec @ARGV' build/displayrect_comark.x86_64 >/dev/null 2>&1; OFF=$?
ABICONV_QUINN_DISPLAYRECT_COMARK=1 perl -e 'alarm 120; exec @ARGV' build/displayrect_comark.x86_64 >/dev/null 2>&1; ON=$?

# Correct: COMARK ON issues EXACTLY ONE more setNeedsDisplayInRect: than OFF.
if [ "$OFF" -ge 1 ] && [ "$OFF" -lt 63 ] && [ "$ON" -eq $((OFF + 1)) ]; then
   echo "displayrect-comark: PASS (off snd=$OFF, on snd=$ON = off+1)"
   exit 0
fi
echo "displayrect-comark: FAIL (off snd=$OFF, on snd=$ON; want on == off+1, off in 1..62)"
exit 1
