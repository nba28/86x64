/*
 * cg_geoarray_shim.c — forward i386-float[] -> native-double[] marshalling for
 * C drawing functions that take an ARRAY OF CGFloat GEOMETRY STRUCTS
 * (CGRect[] / CGPoint[]) plus a count:
 *
 *   CGContextClipToRects(ctx, const CGRect rects[], size_t count)
 *   CGContextAddRects(ctx, const CGRect rects[], size_t count)
 *   CGContextStrokeLineSegments(ctx, const CGPoint points[], size_t count)
 *   NSRectFillList(const NSRect rects[], NSInteger count)
 *   NSRectFillListUsingOperation(const NSRect rects[], NSInteger count, op)
 *   NSRectFillListWithGrays(const NSRect rects[], const CGFloat grays[], count)
 *
 * On i386 CGFloat is a 4-byte FLOAT (CGRect = 16 bytes), on x86_64 an 8-byte
 * DOUBLE (CGRect = 32 bytes). The abigen-generated shims mis-marshalled this
 * shape two ways (verified in the generated ___CGContextClipToRects):
 *   1. elements copied with raw 8-byte moves — each "double" read is two
 *      adjacent i386 floats reinterpreted (garbage magnitudes);
 *   2. only ONE element converted into a stack temp while the original count
 *      is passed through — native CG reads `count` 32-byte elements from a
 *      single-element buffer.
 * Observable damage: a garbage/empty clip that erases every subsequent fill
 * (Quinn -[LCDCell drawDigitElements:opacity:context:] partial-redraw path:
 * SaveGState + ClipToRects(dirtyCellRects) + ClipToMask + FillRect), garbage
 * path rects, garbage stroke segments — anywhere a legacy app hands CG/AppKit
 * a rect/point LIST. Repro + guard: tests-i386/src/99_cg_rect_arrays.c.
 *
 * Fix: convert the FULL count element-wise (float -> double) into a scratch
 * buffer and call native. Small lists convert on the stack (reentrant,
 * drawing may run off the main thread); larger ones malloc.
 *
 * Wired through maptable_tramp.asm (MTSHIM ___<sym>); rdi -> &i386 args[0];
 * the MTSHIM instantiation auto-excludes these from abigen. The CGContextRef
 * arrives as a low-4GB arena handle (the `^v` opaque-token wrap), unwrapped
 * here; the arrays are raw i386 (low-4GB) pointers readable directly.
 *
 * UNIVERSAL: triggers on the entry points' structural shape (CGFloat-struct
 * array + count), never on any app. The NSRectFillListWithColors* variants
 * additionally carry an NSColor* object array (per-element handle unwrap) and
 * stay on the todo list — no current target imports them.
 */
#include <CoreGraphics/CoreGraphics.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

/* low-4GB arena handle -> real 64-bit pointer (objc_shim.c). Passes a genuine
 * low value / NULL straight through. */
extern uint64_t x64_objc_unwrap(uint32_t h);

/* AppKit's C fill-list entries (libabiconv links AppKit; NSRect == CGRect and
 * NSInteger == long on x86_64). Declared here so this stays a C file. */
extern void NSRectFillList(const CGRect *rects, long count);
extern void NSRectFillListUsingOperation(const CGRect *rects, long count,
                                         unsigned long op);
extern void NSRectFillListWithGrays(const CGRect *rects, const CGFloat *grays,
                                    long count);

/* i386-layout elements */
struct rect_i386  { float x, y, w, h; };
struct point_i386 { float x, y; };

#define GEOARR_STACK_N 64u          /* rects converted on the stack per call */
#define GEOARR_MAX_N   (1u << 20)   /* sanity clamp on the element count */

/* widen n i386 rects; returns the native array (stackbuf or malloc), NULL on
 * overflow/OOM. *heap gets the malloc'd pointer to free (or NULL). */
static CGRect *widen_rects(const struct rect_i386 *src, uint32_t n,
                           CGRect *stackbuf, CGRect **heap) {
   *heap = NULL;
   if (n > GEOARR_MAX_N) { return NULL; }
   CGRect *dst = stackbuf;
   if (n > GEOARR_STACK_N) {
      dst = (CGRect *)malloc((size_t)n * sizeof(CGRect));
      if (!dst) { return NULL; }
      *heap = dst;
   }
   for (uint32_t i = 0; i < n; i++) {
      dst[i].origin.x    = (double)src[i].x;
      dst[i].origin.y    = (double)src[i].y;
      dst[i].size.width  = (double)src[i].w;
      dst[i].size.height = (double)src[i].h;
   }
   return dst;
}

/* ---- shims. i386-cdecl entry: a = &args[0], each a 4-byte i386 slot. ---- */

/* CGContextClipToRects(CGContextRef c, const CGRect rects[], size_t count) */
uint32_t shim_CGContextClipToRects(uint32_t *a)
{
   CGContextRef ctx = (CGContextRef)(uintptr_t)x64_objc_unwrap(a[0]);
   const struct rect_i386 *src = (const struct rect_i386 *)(uintptr_t)a[1];
   uint32_t n = a[2];
   if (!ctx || (n && !src)) { return 0; }
   CGRect stackbuf[GEOARR_STACK_N], *heap, *rects;
   rects = widen_rects(src, n, stackbuf, &heap);
   if (n && !rects) { return 0; }
   CGContextClipToRects(ctx, rects, n);
   free(heap);
   return 0;
}

/* CGContextAddRects(CGContextRef c, const CGRect rects[], size_t count) */
uint32_t shim_CGContextAddRects(uint32_t *a)
{
   CGContextRef ctx = (CGContextRef)(uintptr_t)x64_objc_unwrap(a[0]);
   const struct rect_i386 *src = (const struct rect_i386 *)(uintptr_t)a[1];
   uint32_t n = a[2];
   if (!ctx || (n && !src)) { return 0; }
   CGRect stackbuf[GEOARR_STACK_N], *heap, *rects;
   rects = widen_rects(src, n, stackbuf, &heap);
   if (n && !rects) { return 0; }
   CGContextAddRects(ctx, rects, n);
   free(heap);
   return 0;
}

/* CGContextStrokeLineSegments(CGContextRef c, const CGPoint points[], size_t count) */
uint32_t shim_CGContextStrokeLineSegments(uint32_t *a)
{
   CGContextRef ctx = (CGContextRef)(uintptr_t)x64_objc_unwrap(a[0]);
   const struct point_i386 *src = (const struct point_i386 *)(uintptr_t)a[1];
   uint32_t n = a[2];
   if (!ctx || (n && !src)) { return 0; }
   if (n > GEOARR_MAX_N) { return 0; }
   CGPoint stackbuf[2 * GEOARR_STACK_N], *heap = NULL, *pts = stackbuf;
   if (n > 2 * GEOARR_STACK_N) {
      pts = (CGPoint *)malloc((size_t)n * sizeof(CGPoint));
      if (!pts) { return 0; }
      heap = pts;
   }
   for (uint32_t i = 0; i < n; i++) {
      pts[i].x = (double)src[i].x;
      pts[i].y = (double)src[i].y;
   }
   CGContextStrokeLineSegments(ctx, pts, n);
   free(heap);
   return 0;
}

/* NSRectFillList(const NSRect rects[], NSInteger count) */
uint32_t shim_NSRectFillList(uint32_t *a)
{
   const struct rect_i386 *src = (const struct rect_i386 *)(uintptr_t)a[0];
   int32_t sn = (int32_t)a[1];                 /* i386 NSInteger is signed 32 */
   if (sn <= 0 || !src) { return 0; }
   uint32_t n = (uint32_t)sn;
   CGRect stackbuf[GEOARR_STACK_N], *heap, *rects;
   rects = widen_rects(src, n, stackbuf, &heap);
   if (!rects) { return 0; }
   NSRectFillList(rects, (long)n);
   free(heap);
   return 0;
}

/* NSRectFillListUsingOperation(const NSRect rects[], NSInteger count, NSCompositingOperation op) */
uint32_t shim_NSRectFillListUsingOperation(uint32_t *a)
{
   const struct rect_i386 *src = (const struct rect_i386 *)(uintptr_t)a[0];
   int32_t sn = (int32_t)a[1];
   uint32_t op = a[2];
   if (sn <= 0 || !src) { return 0; }
   uint32_t n = (uint32_t)sn;
   CGRect stackbuf[GEOARR_STACK_N], *heap, *rects;
   rects = widen_rects(src, n, stackbuf, &heap);
   if (!rects) { return 0; }
   NSRectFillListUsingOperation(rects, (long)n, (unsigned long)op);
   free(heap);
   return 0;
}

/* NSRectFillListWithGrays(const NSRect rects[], const CGFloat grays[], NSInteger count) */
uint32_t shim_NSRectFillListWithGrays(uint32_t *a)
{
   const struct rect_i386 *src = (const struct rect_i386 *)(uintptr_t)a[0];
   const float *grays_i = (const float *)(uintptr_t)a[1];
   int32_t sn = (int32_t)a[2];
   if (sn <= 0 || !src || !grays_i) { return 0; }
   uint32_t n = (uint32_t)sn;
   if (n > GEOARR_MAX_N) { return 0; }
   CGRect stackbuf[GEOARR_STACK_N], *heap, *rects;
   rects = widen_rects(src, n, stackbuf, &heap);
   if (!rects) { return 0; }
   double gstack[GEOARR_STACK_N], *gheap = NULL, *grays = gstack;
   if (n > GEOARR_STACK_N) {
      grays = (double *)malloc((size_t)n * sizeof(double));
      if (!grays) { free(heap); return 0; }
      gheap = grays;
   }
   for (uint32_t i = 0; i < n; i++) { grays[i] = (double)grays_i[i]; }
   NSRectFillListWithGrays(rects, grays, (long)n);
   free(gheap);
   free(heap);
   return 0;
}
