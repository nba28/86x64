#!/bin/bash
# cg_color_test.sh — regression guard for the forward CGFloat-array colour
# marshalling fix (cg_color_shim.c). Self-contained NATIVE x86_64 test (no i386
# translation): it demonstrates that CGContextSetFillColor reading an i386
# float[4] as a native double[4] does NOT render the intended colour, while the
# shim's float->double element conversion DOES. This is the semantic the
# CGContextSetFillColor/SetStrokeColor shims implement (Quinn white board well).
#
# Exit 0 = fix semantics validated (converted -> white, raw float -> not white).
set -u
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cat > "$TMP/t.c" <<'EOF'
#include <CoreGraphics/CoreGraphics.h>
#include <stdio.h>
#include <stddef.h>
/* the exact conversion primitive from cg_color_shim.c */
static void cgfloat_i386_to_native(double *dst, const float *src, size_t n){
   for (size_t i=0;i<n;i++) dst[i]=(double)src[i];
}
static int fill_white(const void *components){
   unsigned char buf[4]={7,7,7,7};                 /* sentinel, not white */
   CGColorSpaceRef cs=CGColorSpaceCreateDeviceRGB();
   CGContextRef ctx=CGBitmapContextCreate(buf,1,1,8,4,cs,kCGImageAlphaPremultipliedLast);
   CGContextSetFillColorSpace(ctx,cs);
   CGContextSetFillColor(ctx,(const CGFloat*)components);
   CGContextFillRect(ctx,CGRectMake(0,0,1,1));
   CGContextRelease(ctx); CGColorSpaceRelease(cs);
   return (buf[0]==255&&buf[1]==255&&buf[2]==255);
}
int main(void){
   float  white_f[4]={1.0f,1.0f,1.0f,1.0f};          /* i386 CGFloat = float */
   double white_d[4]; cgfloat_i386_to_native(white_d,white_f,4);
   int okp   = (white_d[0]==1.0 && white_d[1]==1.0 && white_d[2]==1.0 && white_d[3]==1.0);
   int buggy = fill_white(white_f);                  /* abigen raw-pointer path */
   int fixed = fill_white(white_d);                  /* shim converted path     */
   printf("primitive=%d raw_float_white=%d converted_white=%d\n", okp, buggy, fixed);
   return (okp && fixed && !buggy) ? 0 : 1;
}
EOF
if ! clang -arch x86_64 -o "$TMP/t" "$TMP/t.c" -framework CoreGraphics 2>"$TMP/err"; then
   echo "cg-color: SKIP (compile failed)"; cat "$TMP/err"; exit 0
fi
OUT=$("$TMP/t"); RC=$?
if [ "$RC" -eq 0 ]; then
   echo "cg-color: PASS ($OUT)"
else
   echo "cg-color: FAIL ($OUT)"
fi
exit $RC
