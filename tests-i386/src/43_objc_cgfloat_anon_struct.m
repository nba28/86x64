/* 43_objc_cgfloat_anon_struct.m — anonymous-tag CGFloat aggregate marshalling.
 *
 * NSAffineTransformStruct is typedef'd from an UNNAMED struct, so the modern
 * runtime encodes it {?=dddddd}: the '?' anonymous tag never matches objc_shim's
 * named CGFloat-struct table, so its fields were left as native 8-byte doubles
 * on BOTH sides. But each field is a CGFloat — 4-byte float on i386, 8-byte
 * double on x86_64. enc_body_all_fp now recognises the anonymous all-floating-
 * point body and applies CGFloat context, widening i386(4B float)->native(8B
 * double) on the way in and narrowing back on the way out.
 *
 * setTransformStruct: exercises the WIDEN path (i386->native by-value arg);
 * transformStruct exercises the NARROW path (native->i386 by-value return).
 * Without the fix the 6 fields are mis-sized (6 i386 slots read as 12) -> a
 * garbage matrix and wrong mapped point.
 */
#include <Foundation/Foundation.h>
#include <stdio.h>

int main(void) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int failures = 0;

    /* widen: install a known matrix by value, verify through a mapped point.
     * [[3 0][0 5]] + (7,11) maps (2,4) -> (3*2+7, 5*4+11) = (13, 31). */
    NSAffineTransform *t = [NSAffineTransform transform];
    NSAffineTransformStruct m;
    m.m11 = 3.0; m.m12 = 0.0; m.m21 = 0.0; m.m22 = 5.0; m.tX = 7.0; m.tY = 11.0;
    [t setTransformStruct:m];
    NSPoint p = [t transformPoint:NSMakePoint(2.0, 4.0)];
    if ((int)(p.x + 0.5) == 13) puts("ok w1"); else { puts("FAIL w1"); failures++; }
    if ((int)(p.y + 0.5) == 31) puts("ok w2"); else { puts("FAIL w2"); failures++; }

    /* narrow: read the struct back by value, verify each CGFloat field. */
    NSAffineTransformStruct r = [t transformStruct];
    if ((int)(r.m11 + 0.5) == 3)  puts("ok n1"); else { puts("FAIL n1"); failures++; }
    if ((int)(r.m22 + 0.5) == 5)  puts("ok n2"); else { puts("FAIL n2"); failures++; }
    if ((int)(r.tX + 0.5)  == 7)  puts("ok n3"); else { puts("FAIL n3"); failures++; }
    if ((int)(r.tY + 0.5)  == 11) puts("ok n4"); else { puts("FAIL n4"); failures++; }

    [pool drain];
    exit(failures);
}
