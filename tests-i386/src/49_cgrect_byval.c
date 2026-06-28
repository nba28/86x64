/*
 * 49_cgrect_byval — abigen FP-by-value-struct argument marshalling (the CG/NS
 * geometry family).
 *
 * i386 passes CGPoint/CGSize/CGRect BY VALUE with 4-byte `float` CGFloat fields
 * (cdecl, on the stack); x86_64 CoreGraphics expects them with 8-byte `double`
 * fields, classified by SysV: a <=16-byte struct (CGPoint/CGSize) into SSE
 * registers, a >16-byte struct (CGRect, 32 bytes) into MEMORY (on the stack).
 * abigen used to SKIP every such function ("byval struct: non-long field"), so
 * the i386 cdecl call reached the native SysV entry with the wrong registers/
 * stack and faulted (Quinn's -[QuinnImageController updateCGCellImages:] ->
 * CGImageCreateWithImageInRect(CGImageRef, CGRect)).
 *
 * FIX: abigen reads each i386 field as a 4-byte float and widens it to a double
 * (cvtss2sd), placing CGPoint/CGSize in xmm registers and CGRect on the stack.
 *
 * This exercises BOTH register classes against real CoreGraphics functions that
 * abigen now shims:
 *   SSE class   — CGPointEqualToPoint / CGSizeEqualToSize (value-checked: a
 *                 widening bug flips equality, since all values are exactly
 *                 representable in float AND double).
 *   MEMORY class— CGImageCreateWithImageInRect(CGImageRef, CGRect): a NULL image
 *                 returns NULL (smoke: the rect is marshalled, no frame corrupt),
 *                 and a real 10x10 bitmap cropped to {1,1,5,4} yields a 5x4
 *                 sub-image (value: the rect's size fields reach the right stack
 *                 slots with the right widened values).
 *
 * CoreGraphics is not in the i386 sysroot, so these are undefined dynamic_lookup
 * imports (see the Makefile rule); the 86x64 pipeline's static-interpose
 * redirects them to libabiconv's ___CG* shims, exactly as for a real binary's
 * CoreGraphics binds. Validation is by EXIT CODE (float printf varargs are a
 * separate known gap): seven independent checks each contribute a distinct bit;
 * all-correct == 127, and a failing/garbled marshalling clears its bit (or
 * crashes outright).
 */
extern void exit(int status);

typedef float CGFloat;                       /* i386: CGFloat is a 4-byte float */
typedef struct { CGFloat x; CGFloat y; }            CGPoint;
typedef struct { CGFloat width; CGFloat height; }   CGSize;
typedef struct { CGPoint origin; CGSize size; }     CGRect;

typedef void *CGColorSpaceRef;
typedef void *CGContextRef;
typedef void *CGImageRef;

extern _Bool CGPointEqualToPoint(CGPoint a, CGPoint b);
extern _Bool CGSizeEqualToSize(CGSize a, CGSize b);

extern CGColorSpaceRef CGColorSpaceCreateDeviceGray(void);
extern CGContextRef    CGBitmapContextCreate(void *data, unsigned long w,
                                             unsigned long h, unsigned long bpc,
                                             unsigned long bpr, CGColorSpaceRef space,
                                             unsigned int bitmapInfo);
extern CGImageRef      CGBitmapContextCreateImage(CGContextRef c);
extern CGImageRef      CGImageCreateWithImageInRect(CGImageRef img, CGRect rect);
extern unsigned long   CGImageGetWidth(CGImageRef img);
extern unsigned long   CGImageGetHeight(CGImageRef img);

int main(void) {
   /* --- SSE class: CGPoint / CGSize in xmm registers --- */
   CGPoint p   = {1.5f, 2.5f};
   CGPoint p_eq = {1.5f, 2.5f};
   CGPoint p_ne = {1.5f, 9.0f};
   CGSize  s   = {3.5f, 4.5f};
   CGSize  s_eq = {3.5f, 4.5f};
   CGSize  s_ne = {3.5f, 9.5f};

   int t1 = CGPointEqualToPoint(p, p_eq) ? 1 : 0;   /* equal   -> 1 */
   int t2 = CGPointEqualToPoint(p, p_ne) ? 0 : 1;   /* unequal -> check passes */
   int t3 = CGSizeEqualToSize(s, s_eq)   ? 1 : 0;   /* equal   -> 1 */
   int t4 = CGSizeEqualToSize(s, s_ne)   ? 0 : 1;   /* unequal -> check passes */

   /* --- MEMORY class: CGRect on the stack --- */
   CGRect crop = {{1.0f, 1.0f}, {5.0f, 4.0f}};
   int t5 = CGImageCreateWithImageInRect((CGImageRef)0, crop) == (CGImageRef)0 ? 1 : 0;

   CGColorSpaceRef gray = CGColorSpaceCreateDeviceGray();
   CGContextRef ctx = CGBitmapContextCreate((void *)0, 10, 10, 8, 0, gray, 0u);
   CGImageRef img = CGBitmapContextCreateImage(ctx);
   CGImageRef sub = CGImageCreateWithImageInRect(img, crop);
   int t6 = (CGImageGetWidth(sub)  == 5) ? 1 : 0;
   int t7 = (CGImageGetHeight(sub) == 4) ? 1 : 0;

   exit(t1*1 + t2*2 + t3*4 + t4*8 + t5*16 + t6*32 + t7*64);   /* 127 */
   return 0;
}
