/*
 * 99_cg_rect_arrays — offscreen pixel-readback repro for the CGFloat-GEOMETRY-
 * ARRAY argument family: CG C functions taking a POINTER TO AN ARRAY of
 * CGRect/CGPoint (plus a count), translated i386 -> x86_64 and run under the
 * REAL libabiconv bridges.
 *
 * WHY: abigen's generated shims for these entries mis-marshal the array TWO
 * ways (verified in the generated abiconv.asm for ___CGContextClipToRects):
 *   1. elements are copied with raw 8-byte movsd loads — the i386 array holds
 *      4-byte FLOATS (i386 CGFloat), so each "double" read is two adjacent
 *      floats reinterpreted (garbage magnitudes);
 *   2. only ONE element is converted into a stack temp while the original
 *      count is passed through — native CG then reads count 32-byte rects
 *      from a single-rect buffer.
 * Quinn's -[LCDCell drawDigitElements:opacity:context:] partial-redraw path
 * runs CGContextSaveGState + CGContextClipToRects(dirtyCellRects) +
 * CGContextClipToMask + CGContextFillRect: a garbage clip either erases the
 * digit fills (empty clip) or fails to confine them. The same abigen shape
 * covers CGContextAddRects and CGContextStrokeLineSegments (CGPoint pairs).
 *
 * Like 49/79/85, CoreGraphics isn't in the i386 sysroot, so every CG entry is
 * an undefined dynamic_lookup import the pipeline's static-interpose redirects
 * to libabiconv's ___CG* shims. Validation by EXIT CODE: eight independent
 * bits, all-correct == 255.
 */
extern void exit(int status);

typedef float CGFloat;                        /* i386: CGFloat is a 4-byte float */
typedef struct { CGFloat x; CGFloat y; }            CGPoint;
typedef struct { CGFloat width; CGFloat height; }   CGSize;
typedef struct { CGPoint origin; CGSize size; }     CGRect;

typedef void *CGColorSpaceRef;
typedef void *CGContextRef;

enum { kCGImageAlphaPremultipliedLast = 1 };

extern CGColorSpaceRef CGColorSpaceCreateDeviceRGB(void);
extern void            CGColorSpaceRelease(CGColorSpaceRef cs);
extern CGContextRef    CGBitmapContextCreate(void *data, unsigned long w,
                                             unsigned long h, unsigned long bpc,
                                             unsigned long bpr, CGColorSpaceRef space,
                                             unsigned int bitmapInfo);
extern void            CGContextRelease(CGContextRef c);

extern void CGContextSetFillColorSpace(CGContextRef c, CGColorSpaceRef s);
extern void CGContextSetFillColor(CGContextRef c, const CGFloat *comp);
extern void CGContextSetStrokeColorSpace(CGContextRef c, CGColorSpaceRef s);
extern void CGContextSetStrokeColor(CGContextRef c, const CGFloat *comp);
extern void CGContextSetLineWidth(CGContextRef c, CGFloat w);
extern void CGContextFillRect(CGContextRef c, CGRect r);
extern void CGContextSaveGState(CGContextRef c);
extern void CGContextRestoreGState(CGContextRef c);

/* the geometry-ARRAY family under test */
extern void CGContextClipToRects(CGContextRef c, const CGRect *rects, unsigned long n);
extern void CGContextAddRects(CGContextRef c, const CGRect *rects, unsigned long n);
extern void CGContextFillPath(CGContextRef c);
extern void CGContextStrokeLineSegments(CGContextRef c, const CGPoint *pts,
                                        unsigned long n);

/* one RGBA8888 pixel (premultiplied-last) at buffer (x, row) */
static const unsigned char *px(const unsigned char *buf, int x, int row) {
   return buf + (unsigned long)row * (16 * 4) + (unsigned long)x * 4u;
}
static int is_white(const unsigned char *p) {
   return p[0] > 200 && p[1] > 200 && p[2] > 200;
}
static int is_black(const unsigned char *p) {
   return p[0] < 40 && p[1] < 40 && p[2] < 40;
}

int main(void) {
   const unsigned long W = 16, H = 16, BPR = 16 * 4;
   int bits = 0;
   CGFloat white[4] = {1.0f, 1.0f, 1.0f, 1.0f};

   /* ---- A: CGContextClipToRects(two rects) + whole-surface fill.
    * Legacy contract: the fill lands ONLY inside the two clip rects. ---- */
   {
      unsigned char buf[16 * 16 * 4];
      unsigned i;
      for (i = 0; i < sizeof buf; i++) { buf[i] = 0; }             /* black */
      CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
      CGContextRef ctx = CGBitmapContextCreate(buf, W, H, 8, BPR, cs,
                                               kCGImageAlphaPremultipliedLast);
      CGRect clips[2] = {{{2.0f, 2.0f}, {4.0f, 4.0f}},
                         {{10.0f, 10.0f}, {4.0f, 4.0f}}};
      CGContextSaveGState(ctx);
      CGContextClipToRects(ctx, clips, 2);
      CGContextSetFillColorSpace(ctx, cs);
      CGContextSetFillColor(ctx, white);
      CGRect whole = {{0.0f, 0.0f}, {16.0f, 16.0f}};
      CGContextFillRect(ctx, whole);
      CGContextRestoreGState(ctx);

      /* CG y-up: point (x, y) -> buffer row (15 - y) */
      bits |= is_white(px(buf, 3, 15 - 3))   ? 1 : 0;   /* bit0 inside rect A  */
      bits |= is_white(px(buf, 11, 15 - 11)) ? 2 : 0;   /* bit1 inside rect B  */
      bits |= is_black(px(buf, 8, 15 - 8))   ? 4 : 0;   /* bit2 between: clipped */

      CGContextRelease(ctx);
      CGColorSpaceRelease(cs);
   }

   /* ---- B: CGContextAddRects(two rects) + FillPath.
    * Legacy contract: both rects fill, nothing else. ---- */
   {
      unsigned char buf[16 * 16 * 4];
      unsigned i;
      for (i = 0; i < sizeof buf; i++) { buf[i] = 0; }
      CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
      CGContextRef ctx = CGBitmapContextCreate(buf, W, H, 8, BPR, cs,
                                               kCGImageAlphaPremultipliedLast);
      CGRect rs[2] = {{{1.0f, 1.0f}, {5.0f, 5.0f}},
                      {{9.0f, 9.0f}, {5.0f, 5.0f}}};
      CGContextSetFillColorSpace(ctx, cs);
      CGContextSetFillColor(ctx, white);
      CGContextAddRects(ctx, rs, 2);
      CGContextFillPath(ctx);

      bits |= is_white(px(buf, 3, 15 - 3))   ? 8 : 0;   /* bit3 inside rect A  */
      bits |= is_white(px(buf, 11, 15 - 11)) ? 16 : 0;  /* bit4 inside rect B  */
      bits |= is_black(px(buf, 7, 15 - 7))   ? 32 : 0;  /* bit5 between: empty */

      CGContextRelease(ctx);
      CGColorSpaceRelease(cs);
   }

   /* ---- C: CGContextStrokeLineSegments(one horizontal segment).
    * Legacy contract: the stroke lands on the segment, not elsewhere. ---- */
   {
      unsigned char buf[16 * 16 * 4];
      unsigned i;
      for (i = 0; i < sizeof buf; i++) { buf[i] = 0; }
      CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
      CGContextRef ctx = CGBitmapContextCreate(buf, W, H, 8, BPR, cs,
                                               kCGImageAlphaPremultipliedLast);
      CGPoint seg[2] = {{2.0f, 12.0f}, {14.0f, 12.0f}};
      CGContextSetStrokeColorSpace(ctx, cs);
      CGContextSetStrokeColor(ctx, white);
      CGContextSetLineWidth(ctx, 2.0f);
      CGContextStrokeLineSegments(ctx, seg, 2);

      bits |= is_white(px(buf, 8, 15 - 12)) ? 64 : 0;   /* bit6 on the segment */
      bits |= is_black(px(buf, 8, 15 - 5))  ? 128 : 0;  /* bit7 off it: empty  */

      CGContextRelease(ctx);
      CGColorSpaceRelease(cs);
   }

   exit(bits);                    /* 255 == every check passed */
   return 0;
}
