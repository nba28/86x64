/* 40_objc_cgfloat_args.m — forward CGFloat-arg marshalling into NATIVE methods.
 *
 * A CGFloat is a 4-byte float on i386 but an 8-byte double on x86_64, and the
 * modern runtime encodes BOTH a CGFloat and a true double as 'd'. When the
 * translated app only CALLS a system method, the forward bridge would otherwise
 * read 8 bytes (two i386 slots) for a value the i386 caller pushed as ONE 4-byte
 * float -> a fused garbage double AND a one-slot misalignment of every following
 * arg. The cgfloat-mask registry (objc_shim.c g_cgfloat_sels) marks which
 * explicit args are CGFloat so the bridge reads a single 4-byte slot and
 * cvtss2sd-widens it, at any position in any-arity method. This is the exact
 * mechanism that fixes the iWeb +[NSRulerView registerUnitWithName:...
 * unitToPointsConversionFactor:(CGFloat)...] wall (CGFloat BETWEEN object args).
 *
 * Uses NSAffineTransform (Foundation, already in the objc sysroot) whose
 * scale/translate methods take CGFloat args: build a transform with mis-marshal-
 * sensitive CGFloat args, then apply it to a point (transformPoint: round-trips
 * NSPoint, an _NSPoint=dd struct the bridge already widens/narrows) and check the
 * result. If a CGFloat consumed 2 slots, the matrix would be garbage and the
 * mapped point wrong. Float comparisons via scaled (int) casts (no printf %f).
 * Validates via puts("ok N")/puts("FAIL N"); exits with the failure count.
 */
#include <Foundation/Foundation.h>
#include <stdio.h>

int main(void) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int failures = 0;

    /* ---- a: two CGFloat args (scaleXBy:yBy:), verified through a mapped point.
     * identity scaled by (3,5): point (2,4) -> (6,20). A mis-marshalled CGFloat
     * (read as an 8-byte double from 2 slots) makes m11/m22 garbage/denormal. */
    NSAffineTransform *s = [NSAffineTransform transform];
    [s scaleXBy:3.0 yBy:5.0];
    NSPoint ps = [s transformPoint:NSMakePoint(2.0, 4.0)];
    if ((int)(ps.x + 0.5) == 6)  puts("ok a1"); else { puts("FAIL a1 scale.x"); failures++; }
    if ((int)(ps.y + 0.5) == 20) puts("ok a2"); else { puts("FAIL a2 scale.y"); failures++; }

    /* ---- b: two CGFloat args (translateXBy:yBy:) — tests positional alignment
     * (arg1 must align after the single-slot arg0). identity translated by
     * (7,11): point (0,0) -> (7,11). */
    NSAffineTransform *t = [NSAffineTransform transform];
    [t translateXBy:7.0 yBy:11.0];
    NSPoint pt = [t transformPoint:NSMakePoint(0.0, 0.0)];
    if ((int)(pt.x + 0.5) == 7)  puts("ok b1"); else { puts("FAIL b1 trans.x"); failures++; }
    if ((int)(pt.y + 0.5) == 11) puts("ok b2"); else { puts("FAIL b2 trans.y"); failures++; }

    /* ---- c: single CGFloat arg (rotateByDegrees:) — 90deg maps (1,0)->(0,1). */
    NSAffineTransform *r = [NSAffineTransform transform];
    [r rotateByDegrees:90.0];
    NSPoint pr = [r transformPoint:NSMakePoint(1.0, 0.0)];
    if ((int)(pr.x + 0.5) == 0) puts("ok c1"); else { puts("FAIL c1 rot.x"); failures++; }
    if ((int)(pr.y + 0.5) == 1) puts("ok c2"); else { puts("FAIL c2 rot.y"); failures++; }

    [pool drain];
    exit(failures);
}
