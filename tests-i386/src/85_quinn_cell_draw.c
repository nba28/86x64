/*
 * 85_quinn_cell_draw — offscreen pixel-readback repro of Quinn 3.5.7's
 * _QuinnGeneralFastDrawCells cell-draw primitive sequence, translated i386 ->
 * x86_64 and run under the REAL libabiconv CG bridges. Verifies the ACTUAL
 * PIXELS written, headlessly (in-process bitmap readback — no display needed).
 *
 * WHY: Quinn's game board GRID + settled cells + stats sidebar render blank on
 * the translated build (the falling piece draws). The board draw path is:
 *   -[QuinnLocalBoardView drawBoardInRect:] -> _QuinnGeneralFastDrawCells (i386
 *   0x38308), whose per-OCCUPIED-cell op set (verified by disassembly) is:
 *     CGContextSetAlpha(ctx, a)                    (geo idx 66)
 *     [once]  cs = CGColorSpaceCreateDeviceRGB()   (abigen C-ABI, opaque-ptr ret)
 *             CGContextSetFillColorSpace(ctx, cs)  (cg_color_shim, per-ctx fill_n)
 *             CGColorSpaceRelease(cs)
 *     solid cell:  CGContextSetFillColor(ctx, float[4])  (cg_color_shim)
 *                  CGContextFillRect(ctx, CGRect byval)  (geo idx 50)
 *     sprite cell: CGContextDrawImage(ctx, CGRect byval, CGImageRef) (geo idx 71)
 * The CGContextRef Quinn draws into is an opaque `^v` graphicsPort handle; here
 * we stand in a CGBitmapContext (also delivered as an arena handle by abigen's
 * opaque-ptr return wrap) so we can READ BACK the pixels the bridge produced.
 *
 * The CGImage for a sprite cell is built by -[QuinnImageController
 * updateCGCellImages:] via CGImageCreateWithImageInRect (i386 0x55485) from a
 * master cells-image — reproduced here (crop a master bitmap, round-tripping the
 * CGImageRef through the arena handle) so CGContextDrawImage's image arg exercises
 * the SAME handle-unwrap path (geo_cfptr_arg) Quinn hits.
 *
 * This isolates every un-ruled CG-marshalling candidate for the invisible board:
 *   (a) CGColorSpaceCreateDeviceRGB via the generic (non-geo) bridge -> if the
 *       returned colour-space handle is wrong, SetFillColorSpace records the
 *       wrong fill_n and SetFillColor mis-converts -> wrong fill colour.
 *   (b) per-CGContextRef fill_n staleness in cg_color_shim's map.
 *   (c) CGContextDrawImage CGImageRef arena-handle unwrap.
 *   (d) by-value CGRect float->double geometry widening (FillRect / DrawImage).
 * A wrong colour, wrong rect location/size, or a missing image shows up as a
 * mismatched readback pixel and clears that check's bit.
 *
 * CoreGraphics is not in the i386 sysroot, so every CG entry is an undefined
 * dynamic_lookup import the 86x64 pipeline's static-interpose redirects to
 * libabiconv's ___CG* shims (exactly as for a real binary). Validation is by
 * EXIT CODE: eight independent bits, all-correct == 255. A garbled marshalling
 * clears its bit (or crashes outright).
 */
extern void exit(int status);

typedef float CGFloat;                        /* i386: CGFloat is a 4-byte float */
typedef struct { CGFloat x; CGFloat y; }            CGPoint;
typedef struct { CGFloat width; CGFloat height; }   CGSize;
typedef struct { CGPoint origin; CGSize size; }     CGRect;

typedef void *CGColorSpaceRef;
typedef void *CGContextRef;
typedef void *CGImageRef;

/* bitmapInfo: kCGImageAlphaPremultipliedLast = 1. */
enum { kCGImageAlphaPremultipliedLast = 1 };

extern CGColorSpaceRef CGColorSpaceCreateDeviceRGB(void);
extern void            CGColorSpaceRelease(CGColorSpaceRef cs);

extern CGContextRef    CGBitmapContextCreate(void *data, unsigned long w,
                                             unsigned long h, unsigned long bpc,
                                             unsigned long bpr, CGColorSpaceRef space,
                                             unsigned int bitmapInfo);
extern void           *CGBitmapContextGetData(CGContextRef c);
extern void            CGContextRelease(CGContextRef c);

extern void CGContextSetAlpha(CGContextRef c, CGFloat a);
extern void CGContextSetFillColorSpace(CGContextRef c, CGColorSpaceRef s);
extern void CGContextSetFillColor(CGContextRef c, const CGFloat *components);
extern void CGContextFillRect(CGContextRef c, CGRect r);
extern void CGContextDrawImage(CGContextRef c, CGRect r, CGImageRef img);

extern CGImageRef CGBitmapContextCreateImage(CGContextRef c);
extern CGImageRef CGImageCreateWithImageInRect(CGImageRef img, CGRect r);
extern void       CGImageRelease(CGImageRef img);

/* One RGBA8888 pixel from a premultiplied-last bitmap at (x,y). */
static void px(const unsigned char *buf, unsigned long bpr, int x, int y,
               unsigned char out[4]) {
   const unsigned char *p = buf + (unsigned long)y * bpr + (unsigned long)x * 4u;
   out[0] = p[0]; out[1] = p[1]; out[2] = p[2]; out[3] = p[3];
}
static int nearby(unsigned char a, unsigned char b) {         /* +-2 tolerance */
   int d = (int)a - (int)b; return d < 0 ? -d <= 2 : d <= 2;
}

int main(void) {
   const unsigned long W = 16, H = 16, BPR = 16 * 4;
   int bits = 0;

   /* ----- FILL path: SetFillColorSpace(deviceRGB) + SetFillColor + FillRect
    * fill a WHITE 4x4 cell at board coords (5,6); read back the pixels. ----- */
   {
      unsigned char buf[16 * 16 * 4];
      for (unsigned i = 0; i < sizeof buf; i++) buf[i] = 0;      /* start black */
      CGColorSpaceRef csctx = CGColorSpaceCreateDeviceRGB();
      CGContextRef ctx = CGBitmapContextCreate(buf, W, H, 8, BPR, csctx,
                                               kCGImageAlphaPremultipliedLast);

      CGContextSetAlpha(ctx, 1.0f);                             /* geo 66 */
      CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();       /* generic ret (a) */
      CGContextSetFillColorSpace(ctx, cs);                      /* cg_color_shim */
      CGColorSpaceRelease(cs);
      CGFloat white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
      CGContextSetFillColor(ctx, white);                        /* cg_color_shim */
      CGRect cell = {{5.0f, 6.0f}, {4.0f, 4.0f}};               /* byval CGRect (d) */
      CGContextFillRect(ctx, cell);                             /* geo 50 */

      unsigned char *out = (unsigned char *)CGBitmapContextGetData(ctx);
      if (!out) out = buf;                 /* GetData handle-unwrap; fall back */
      unsigned char c[4], e[4];
      /* CG y-axis is bottom-up: rect origin.y=6 -> buffer rows [H-6-4 .. H-6). */
      /* center of the filled cell: board (7, 8) -> px (7, H-1-8)=(7,7). */
      px(out, BPR, 7, 7, c);
      bits |= (c[0] == 255 && c[1] == 255 && c[2] == 255) ? 1 : 0;   /* bit0: colour */
      /* a pixel OUTSIDE the cell (board (1,1) -> px (1, 14)) stays black. */
      px(out, BPR, 1, 14, e);
      bits |= (e[0] == 0 && e[1] == 0 && e[2] == 0) ? 2 : 0;         /* bit1: geometry */
      /* the fill did NOT bleed one cell to the left (board (3,8) -> px(3,7)). */
      px(out, BPR, 3, 7, e);
      bits |= (e[0] == 0 && e[1] == 0 && e[2] == 0) ? 4 : 0;         /* bit2: left edge */
      /* and reached the right edge but not past it (board (8,8) filled, (9,8) not) */
      { unsigned char in8[4], out9[4];
        px(out, BPR, 8, 7, in8); px(out, BPR, 9, 7, out9);
        bits |= (in8[0] == 255 && out9[0] == 0) ? 8 : 0; }           /* bit3: right edge */

      CGContextRelease(ctx);
      CGColorSpaceRelease(csctx);
   }

   /* ----- DRAWIMAGE path: build a solid-red master image, crop it via
    * CGImageCreateWithImageInRect (arena round-trip), draw it into the ctx at a
    * byval rect; read back the drawn pixels. ----- */
   {
      unsigned char dst[16 * 16 * 4];
      for (unsigned i = 0; i < sizeof dst; i++) dst[i] = 0;      /* start black */
      CGColorSpaceRef csctx = CGColorSpaceCreateDeviceRGB();
      CGContextRef ctx = CGBitmapContextCreate(dst, W, H, 8, BPR, csctx,
                                               kCGImageAlphaPremultipliedLast);

      /* master 8x8 solid-RED image via its own bitmap context */
      unsigned char src[8 * 8 * 4];
      for (int i = 0; i < 8 * 8; i++) {
         src[i*4+0] = 255; src[i*4+1] = 0; src[i*4+2] = 0; src[i*4+3] = 255;
      }
      CGColorSpaceRef cssrc = CGColorSpaceCreateDeviceRGB();
      CGContextRef mctx = CGBitmapContextCreate(src, 8, 8, 8, 8 * 4, cssrc,
                                                kCGImageAlphaPremultipliedLast);
      CGImageRef master = CGBitmapContextCreateImage(mctx);     /* opaque ret -> handle */
      /* crop the whole 8x8 (round-trips the CGImageRef handle) (c) */
      CGRect whole = {{0.0f, 0.0f}, {8.0f, 8.0f}};
      CGImageRef cell = CGImageCreateWithImageInRect(master, whole);
      CGImageRef img = cell ? cell : master;

      CGContextSetAlpha(ctx, 1.0f);
      CGRect at = {{4.0f, 4.0f}, {8.0f, 8.0f}};                 /* byval rect (d) */
      CGContextDrawImage(ctx, at, img);                         /* geo 71 */

      unsigned char *out = (unsigned char *)CGBitmapContextGetData(ctx);
      if (!out) out = dst;
      unsigned char c[4], e[4];
      /* image center: board (8, 8) -> px (8, H-1-8)=(8,7): should be RED */
      px(out, BPR, 8, 7, c);
      bits |= (nearby(c[0], 255) && nearby(c[1], 0) && nearby(c[2], 0)) ? 16 : 0;  /* bit4: image present */
      /* outside the drawn 8x8 (board (1,1) -> px(1,14)) stays black */
      px(out, BPR, 1, 14, e);
      bits |= (e[0] == 0 && e[1] == 0 && e[2] == 0) ? 32 : 0;               /* bit5: image geometry */
      /* the image bit is not shifted: board (4,4)..(11,11) covered, (12,12) not.
       * board (11,11)->px(11,4) red; board(12,12)->px(12,3) black */
      { unsigned char inr[4], outr[4];
        px(out, BPR, 11, 4, inr); px(out, BPR, 12, 3, outr);
        bits |= (nearby(inr[0], 255) && outr[0] == 0) ? 64 : 0; }          /* bit6: image extent */
      /* crop round-trip produced a usable image (non-NULL) */
      bits |= (cell != (CGImageRef)0) ? 128 : 0;                           /* bit7: crop handle */

      if (cell) CGImageRelease(cell);
      CGImageRelease(master);
      CGContextRelease(mctx);
      CGColorSpaceRelease(cssrc);
      CGContextRelease(ctx);
      CGColorSpaceRelease(csctx);
   }

   exit(bits);                    /* 255 == every check passed */
   return 0;
}
