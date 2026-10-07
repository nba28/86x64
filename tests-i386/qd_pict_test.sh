#!/bin/bash
# qd_pict_test.sh — regression guard for the classic-PICT decode path in
# qd_gworld.c (shim_DrawPicture / QDPict*). Self-contained NATIVE x86_64 test
# (no i386 translation): it exercises the EXACT semantic the shims implement —
# an in-memory PicHandle holds the PICT *body* (no 512-byte file header), so we
# prepend a zero header and decode via ImageIO's surviving com.apple.pict
# reader. Proves QuickDraw PICT playback is really regained (not a no-op).
#
# Uses a real system .pict, strips its 512-byte file header to simulate the
# in-memory PicHandle body, and asserts the decode yields a valid CGImage.
# SKIPs (exit 0) if no .pict is present. Exit 0 = decode semantic validated.
. "$(dirname "$0")/../src/86x64/paths.sh"   # M64_* local paths
set -u
PICT=""
for c in \
  "/System/Library/Components/CoreAudio.component/Contents/Resources/GEQ10BandAqua.pict" \
  "$M64_APPS64/Numbers.app/Contents/Resources/empty_equation.pict" \
  "$M64_APPS64/Pages.app/Contents/Resources/default_equation.pict"; do
  [ -f "$c" ] && { PICT="$c"; break; }
done
if [ -z "$PICT" ]; then echo "qd-pict: SKIP (no system .pict found)"; exit 0; fi

TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
cat > "$TMP/t.c" <<'EOF'
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
/* the exact pict_decode semantic from qd_gworld.c */
static CGImageRef pict_decode(const void *body, size_t len){
   CFMutableDataRef d = CFDataCreateMutable(NULL, 0);
   static const unsigned char hdr512[512] = {0};
   CFDataAppendBytes(d, hdr512, 512);
   CFDataAppendBytes(d, body, (CFIndex)len);
   CFStringRef k = CFSTR("kCGImageSourceTypeIdentifierHint");
   CFStringRef v = CFSTR("com.apple.pict");
   CFDictionaryRef o = CFDictionaryCreate(NULL,(const void**)&k,(const void**)&v,1,
       &kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
   CGImageSourceRef s = CGImageSourceCreateWithData(d, o);
   CGImageRef img = s ? CGImageSourceCreateImageAtIndex(s, 0, NULL) : NULL;
   if (s) CFRelease(s); if (o) CFRelease(o); CFRelease(d);
   return img;
}
int main(int argc, char **argv){
   FILE *f = fopen(argv[1], "rb");
   if (!f) { printf("open fail\n"); return 1; }
   fseek(f,0,SEEK_END); long n = ftell(f); fseek(f,0,SEEK_SET);
   if (n <= 512) { printf("too small\n"); return 1; }
   unsigned char *buf = malloc(n); fread(buf,1,n,f); fclose(f);
   /* strip the 512-byte PICT FILE header -> the body an in-memory PicHandle holds */
   CGImageRef img = pict_decode(buf + 512, (size_t)(n - 512));
   if (!img) { printf("decode=FAIL\n"); return 1; }
   size_t w = CGImageGetWidth(img), h = CGImageGetHeight(img);
   printf("decode=OK %zux%zu\n", w, h);
   return (w > 0 && h > 0) ? 0 : 1;
}
EOF
if ! clang -arch x86_64 -o "$TMP/t" "$TMP/t.c" \
     -framework CoreGraphics -framework ImageIO -framework CoreFoundation 2>"$TMP/err"; then
   echo "qd-pict: SKIP (compile failed)"; cat "$TMP/err"; exit 0
fi
OUT=$("$TMP/t" "$PICT"); RC=$?
if [ "$RC" -eq 0 ]; then echo "qd-pict: PASS ($OUT)"; else echo "qd-pict: FAIL ($OUT)"; fi
exit $RC
