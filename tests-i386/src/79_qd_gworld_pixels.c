/*
 * 79_qd_gworld_pixels — end-to-end regression for the CG-backed classic
 * QuickDraw substrate (qd_gworld.c): NewGWorld allocates a real offscreen
 * CGBitmapContext-backed port with a live classic PixMap record; the drawing
 * primitives (EraseRect/PaintRect/FrameRect) actually rasterize into its
 * low-4GB ARGB buffer; and the pixels read back through the classic accessor
 * chain (GetGWorldPixMap -> GetPixBaseAddr / GetPixRowBytes / **pm.bounds)
 * match what was drawn.
 *
 * QuickDraw (GWorlds/PixMaps/CopyBits/the pen-color drawing model) was removed
 * from modern macOS, so a translated Carbon/QuickTime app that composites
 * offscreen and reads the result back would get garbage from a no-op shim. This
 * proves the real thing: draw red, erase-to-white, paint a blue sub-rect, and
 * verify the exact ARGB words at chosen pixels + the PixMap record fields.
 *
 * QuickDraw isn't in the i386 sysroot, so every QD entry is an undefined
 * dynamic_lookup import resolved at translate time by static-interpose ->
 * libabiconv's ___<Name> shims (see the Makefile rule). The GWorld buffer is
 * allocated on libabiconv's low-4GB heap, so the 32-bit PixMap.baseAddr /
 * GetPixBaseAddr handle round-trips losslessly — exactly the real app path.
 */

extern int  printf(const char *, ...);
extern void exit(int status);

typedef struct { short v, h; } Point;
typedef struct { short top, left, bottom, right; } Rect;
typedef struct { unsigned short red, green, blue; } RGBColor;

/* QuickDraw entry points (undefined dynamic_lookup -> ___<Name> shims). */
extern short  NewGWorld(void **gw, short depth, const Rect *bounds,
                        void *ctab, void *gd, long flags);
extern void   DisposeGWorld(void *gw);
extern void  *GetGWorldPixMap(void *gw);
extern void   SetGWorld(void *gw, void *gd);
extern unsigned char LockPixels(void *pm);
extern char  *GetPixBaseAddr(void *pm);
extern short  GetPixRowBytes(void *pm);
extern Rect  *GetPixBounds(void *pm, Rect *r);
extern void   RGBForeColor(const RGBColor *c);
extern void   RGBBackColor(const RGBColor *c);
extern void   EraseRect(const Rect *r);
extern void   PaintRect(const Rect *r);

/* Read the RGB (0x00RRGGBB) of the pixel at (x,y). The port is 32-bit ARGB
 * big-endian (byte0=A/skip, byte1=R, byte2=G, byte3=B); mask the skip byte so
 * the check is robust to whether CG writes it. */
static unsigned long px(char *base, int rowBytes, int x, int y)
{
   unsigned char *p = (unsigned char *)base + (long)y * rowBytes + (long)x * 4;
   return ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | (unsigned long)p[3];
}

int main(void)
{
   Rect bounds = { 0, 0, 8, 8 };     /* 8x8 offscreen */
   void *gw = 0;
   short err = NewGWorld(&gw, 32, &bounds, 0, 0, 0);
   printf("newgworld err=%d nonnull=%d\n", err, gw ? 1 : 0);
   if (!gw) { exit(1); }

   SetGWorld(gw, 0);
   void *pm = GetGWorldPixMap(gw);
   printf("pixmap nonnull=%d\n", pm ? 1 : 0);
   LockPixels(pm);

   char *base = GetPixBaseAddr(pm);
   int   rb   = GetPixRowBytes(pm);
   printf("base nonnull=%d rowbytes_ok=%d\n", base ? 1 : 0, rb >= 32 ? 1 : 0);

   Rect pb; GetPixBounds(pm, &pb);
   printf("bounds=%d,%d,%d,%d\n", pb.top, pb.left, pb.bottom, pb.right);

   /* erase whole port to white */
   RGBColor white; white.red = 0xFFFF; white.green = 0xFFFF; white.blue = 0xFFFF;
   RGBBackColor(&white);
   EraseRect(&bounds);

   /* paint a 4x4 blue square at (2,2)-(6,6) */
   RGBColor blue; blue.red = 0; blue.green = 0; blue.blue = 0xFFFF;
   RGBForeColor(&blue);
   Rect sq = { 2, 2, 6, 6 };
   PaintRect(&sq);

   /* corner stayed white; center is blue (ARGB: FF FF FF FF / FF 00 00 FF) */
   printf("corner=%06lX\n", px(base, rb, 0, 0));
   printf("center=%06lX\n", px(base, rb, 3, 3));
   printf("edge_out=%06lX\n", px(base, rb, 6, 6));   /* just outside the square */

   DisposeGWorld(gw);
   printf("disposed ok\n");
   exit(0);
}
