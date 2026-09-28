/* 99_objc_cgfloat_generated.m — a CGFloat arg of a system method that only
 * the GENERATED cgfloat table knows (gen_cgfloat_sels.py) must arrive intact.
 *
 * -[NSAffineTransform rotateByRadians:] takes a CGFloat: a 4-byte float pushed
 * by the i386 caller, an 8-byte double natively. Without a mask bit the forward
 * bridge reads 8 bytes (the float fused with the next stack slot) and rotates
 * by garbage. rotateByRadians: was never on the old hand-curated list.
 * Exit 42 = the transform is a 90-degree rotation. */
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(void) {
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   NSAffineTransform *t = [NSAffineTransform transform];
   [t rotateByRadians:(CGFloat)1.5707963267948966];
   NSPoint p = [t transformPoint:NSMakePoint(1.0, 0.0)];
   int ok = fabs(p.x) < 1e-4 && fabs(p.y - 1.0) < 1e-4;
   printf("rotateByRadians: %s\n", ok ? "ok" : "WRONG");
   if (!ok) { printf("  (1,0) -> (%g,%g)\n", (double)p.x, (double)p.y); }
   [pool release];
   exit(ok ? 42 : 1);
}
