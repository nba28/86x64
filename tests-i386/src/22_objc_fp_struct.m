/* 22_objc_fp_struct.m — forward and reverse struct-by-value / XMM arg bridging
 * (28th blocker family: NSUnionRect rax=0, struct-by-value args/returns, float
 * args in XMM regs). Tests the ObjC bridge's FP+struct paths.
 *
 * Validates via puts("ok N") / puts("FAIL N ...") + exit(failures).
 * IMPORTANT: no printf("%f") — fp varargs in printf C shims are still broken.
 * All float comparisons done as (int)(x) after scaling.
 */
#include <Foundation/Foundation.h>
#include <stdio.h>

/* Legacy class with a struct-by-value instance method taking a float arg. */
@interface FPTest : NSObject
- (NSRect)pad:(NSRect)r by:(float)f;
@end

@implementation FPTest
- (NSRect)pad:(NSRect)r by:(float)f {
    return NSMakeRect(r.origin.x - f, r.origin.y - f,
                      r.size.width + 2*f, r.size.height + 2*f);
}
@end

int main(void) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int failures = 0;

    /* ---- a: NSUnionRect ---- */
    NSRect u = NSUnionRect(NSMakeRect(0,0,10,10), NSMakeRect(20,5,10,10));
    if ((int)u.origin.x == 0)   puts("ok a1"); else { puts("FAIL a1 origin.x"); failures++; }
    if ((int)u.origin.y == 0)   puts("ok a2"); else { puts("FAIL a2 origin.y"); failures++; }
    if ((int)u.size.width == 30) puts("ok a3"); else { puts("FAIL a3 width"); failures++; }
    if ((int)u.size.height == 15) puts("ok a4"); else { puts("FAIL a4 height"); failures++; }

    /* ---- b: NSStringFromRect / NSRectFromString round-trip ---- */
    NSString *s = NSStringFromRect(u);
    NSRect rt = NSRectFromString(s);
    if ((int)rt.origin.x == 0)    puts("ok b1"); else { puts("FAIL b1 rt.origin.x"); failures++; }
    if ((int)rt.origin.y == 0)    puts("ok b2"); else { puts("FAIL b2 rt.origin.y"); failures++; }
    if ((int)rt.size.width == 30)  puts("ok b3"); else { puts("FAIL b3 rt.width"); failures++; }
    if ((int)rt.size.height == 15) puts("ok b4"); else { puts("FAIL b4 rt.height"); failures++; }

    /* ---- c: NSValue valueWithRect / rectValue ---- */
    NSValue *v = [NSValue valueWithRect:NSMakeRect(1,2,3,4)];
    NSRect r2 = [v rectValue];
    if ((int)r2.origin.x == 1)    puts("ok c1"); else { puts("FAIL c1 r2.x"); failures++; }
    if ((int)r2.origin.y == 2)    puts("ok c2"); else { puts("FAIL c2 r2.y"); failures++; }
    if ((int)r2.size.width == 3)  puts("ok c3"); else { puts("FAIL c3 r2.w"); failures++; }
    if ((int)r2.size.height == 4) puts("ok c4"); else { puts("FAIL c4 r2.h"); failures++; }

    /* ---- d: NSValue valueWithPoint / pointValue ---- */
    NSValue *pv = [NSValue valueWithPoint:NSMakePoint(7,9)];
    NSPoint p = [pv pointValue];
    if ((int)p.x == 7) puts("ok d1"); else { puts("FAIL d1 p.x"); failures++; }
    if ((int)p.y == 9) puts("ok d2"); else { puts("FAIL d2 p.y"); failures++; }

    /* ---- e: rangeOfString: ---- */
    NSString *hay = [NSString stringWithUTF8String:"hello world"];
    NSString *needle = [NSString stringWithUTF8String:"world"];
    NSRange rg = [hay rangeOfString:needle];
    if (rg.location == 6) puts("ok e1"); else { puts("FAIL e1 location"); failures++; }
    if (rg.length == 5)   puts("ok e2"); else { puts("FAIL e2 length"); failures++; }

    /* ---- f: NSNumber doubleValue (double -> int via *2) ---- */
    NSNumber *nd = [NSNumber numberWithDouble:3.5];
    if ((int)([nd doubleValue] * 2.0) == 7) puts("ok f1"); else { puts("FAIL f1 double"); failures++; }

    /* ---- g: NSNumber floatValue ---- */
    NSNumber *nf = [NSNumber numberWithFloat:2.5f];
    if ((int)([nf floatValue] * 2.0f) == 5) puts("ok g1"); else { puts("FAIL g1 float"); failures++; }

    /* ---- h: legacy instance method with struct-by-value + float arg ---- */
    FPTest *ft = [[FPTest alloc] init];
    NSRect pr = [ft pad:NSMakeRect(10,10,10,10) by:2.0f];
    [ft release];
    if ((int)pr.origin.x == 8)    puts("ok h1"); else { puts("FAIL h1 pad.x"); failures++; }
    if ((int)pr.origin.y == 8)    puts("ok h2"); else { puts("FAIL h2 pad.y"); failures++; }
    if ((int)pr.size.width == 14) puts("ok h3"); else { puts("FAIL h3 pad.w"); failures++; }
    if ((int)pr.size.height == 14) puts("ok h4"); else { puts("FAIL h4 pad.h"); failures++; }

    [pool drain];
    exit(failures);
}
