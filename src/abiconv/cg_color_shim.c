/*
 * cg_color_shim.c — forward i386-float[] -> native-double[] CGFloat-array
 * marshalling for CGContextSetFillColor / CGContextSetStrokeColor.
 *
 * CGContextSetFillColor(CGContextRef c, const CGFloat components[]) takes a
 * variable-length array of colour components (one per colour-space channel,
 * PLUS one for alpha). On i386 CGFloat is a 32-bit FLOAT, on x86_64 it is a
 * 64-bit DOUBLE. The abigen-generated shim marshals `const CGFloat*` as a plain
 * pointer (no element conversion), so native CoreGraphics reads DOUBLES out of
 * the i386 FLOAT array — 8 bytes per element from a 4-byte-per-element buffer.
 * The first "double" is the two adjacent floats reinterpreted (e.g. a white
 * fill {1.0f,1.0f,1.0f,1.0f} -> components[0] = the bits {1.0f|1.0f} as a
 * double ~= 3e-41 ~ 0), so the fill colour comes out ~black + garbage.
 * Quinn's -[QuinnBoardView drawBackgroundInRect:] fills the WHITE game well
 * this way (CGColorSpaceCreateDeviceRGB + CGContextSetFillColor{1,1,1,a} +
 * CGContextFillRect) -> the well renders BLACK.
 *
 * The array LENGTH is not in the call — it is (number of colour-space
 * components + 1 for alpha). CoreGraphics exposes no "current fill colour
 * space" getter, and the API contract requires the colour space to be set
 * (CGContextSetFillColorSpace) before the colour, so we intercept the
 * ColorSpace setters too and record, per CGContextRef, the component count via
 * CGColorSpaceGetNumberOfComponents(cs)+1. On SetFill/StrokeColor we read that
 * count, convert count i386 floats -> count native doubles on a per-call stack
 * buffer (reentrant / thread-safe; CoreGraphics may draw off the main thread),
 * and call native with the doubles.
 *
 * Wired through maptable_tramp.asm (MTSHIM ___CGContextSet{Fill,Stroke}Color[
 * Space]); rdi -> &i386 args[0]; excluded from abigen via custom.syms. The
 * CGContextRef / CGColorSpaceRef args arrive as low-4GB arena handles (the
 * `^v` opaque-token wrap for graphicsPort etc.), unwrapped here.
 *
 * UNIVERSAL: triggers on the CGContextSet{Fill,Stroke}Color entry points, never
 * on any app. (The scalar forms — CGContextSetRGBFillColor / SetGrayFillColor —
 * are already correctly marshalled by the struct/CGFloat classifier via
 * maptable_tramp.asm GEOSHIM; only the variable-length ARRAY form needed this.)
 */
#include <CoreGraphics/CoreGraphics.h>
#include <stdint.h>
#include <stddef.h>
#include <os/lock.h>

/* low-4GB arena handle -> real 64-bit pointer (objc_shim.c). Passes a genuine
 * low value / NULL straight through, so a raw <4GB CGContextRef is unharmed. */
extern uint64_t x64_objc_unwrap(uint32_t h);

/* ---- CGFloat element conversion primitives -----------------------------
 * Named (not inlined) so the reverse-direction gradient shim (cg_function_shim.c)
 * can later be refactored to share them; today each side keeps its own loop. */
void cgfloat_i386_to_native(double *dst, const float *src, size_t n)
{
   for (size_t i = 0; i < n; i++) { dst[i] = (double)src[i]; }
}
void cgfloat_native_to_i386(float *dst, const double *src, size_t n)
{
   for (size_t i = 0; i < n; i++) { dst[i] = (float)src[i]; }
}

/* ---- per-CGContextRef colour-space component counts --------------------
 * Small open-addressing map keyed by the (real) CGContextRef. fill_n/stroke_n
 * = number of CGFloat components the matching SetColor array carries
 * (colour-space channels + 1 alpha). Default 4 (= RGBA) until the app sets a
 * colour space (which it must, per the API contract, before setting a colour).
 * A stale entry for a freed+reused context self-corrects: SetColorSpace runs
 * before SetColor on every draw. */
#define CGC_MAP_CAP   256u          /* power of two */
#define CGC_DEFAULT_N 4u            /* RGBA fallback */
#define CGC_MAX_N     32u           /* clamp: no CG colour space has >32 chans */

struct cgc_ent { uint64_t ctx; uint32_t fill_n; uint32_t stroke_n; };
static struct cgc_ent  g_cgc[CGC_MAP_CAP];
static os_unfair_lock  g_cgc_lock = OS_UNFAIR_LOCK_INIT;

/* caller holds g_cgc_lock. Never returns NULL (evicts the home slot if full). */
static struct cgc_ent *cgc_slot(uint64_t ctx)
{
   uint32_t home = (uint32_t)((ctx >> 4) ^ (ctx >> 21)) & (CGC_MAP_CAP - 1u);
   for (uint32_t i = 0; i < CGC_MAP_CAP; i++) {
      uint32_t idx = (home + i) & (CGC_MAP_CAP - 1u);
      if (g_cgc[idx].ctx == ctx) { return &g_cgc[idx]; }
      if (g_cgc[idx].ctx == 0) {
         g_cgc[idx].ctx = ctx;
         g_cgc[idx].fill_n = CGC_DEFAULT_N;
         g_cgc[idx].stroke_n = CGC_DEFAULT_N;
         return &g_cgc[idx];
      }
   }
   g_cgc[home].ctx = ctx;                 /* table full: reuse the home slot */
   g_cgc[home].fill_n = CGC_DEFAULT_N;
   g_cgc[home].stroke_n = CGC_DEFAULT_N;
   return &g_cgc[home];
}

static uint32_t cgc_components(CGColorSpaceRef cs)
{
   if (!cs) { return CGC_DEFAULT_N; }
   size_t n = CGColorSpaceGetNumberOfComponents(cs) + 1u;   /* + alpha */
   if (n == 0 || n > CGC_MAX_N) { n = CGC_DEFAULT_N; }
   return (uint32_t)n;
}

/* ---- shims. i386-cdecl entry: a = &args[0], each a 4-byte i386 slot. ---- */

/* CGContextSetFillColorSpace(CGContextRef c, CGColorSpaceRef space) */
uint32_t shim_CGContextSetFillColorSpace(uint32_t *a)
{
   CGContextRef    ctx = (CGContextRef)(uintptr_t)x64_objc_unwrap(a[0]);
   CGColorSpaceRef cs  = (CGColorSpaceRef)(uintptr_t)x64_objc_unwrap(a[1]);
   if (!ctx) { return 0; }
   uint32_t n = cgc_components(cs);
   os_unfair_lock_lock(&g_cgc_lock);
   cgc_slot((uint64_t)(uintptr_t)ctx)->fill_n = n;
   os_unfair_lock_unlock(&g_cgc_lock);
   CGContextSetFillColorSpace(ctx, cs);
   return 0;
}

/* CGContextSetStrokeColorSpace(CGContextRef c, CGColorSpaceRef space) */
uint32_t shim_CGContextSetStrokeColorSpace(uint32_t *a)
{
   CGContextRef    ctx = (CGContextRef)(uintptr_t)x64_objc_unwrap(a[0]);
   CGColorSpaceRef cs  = (CGColorSpaceRef)(uintptr_t)x64_objc_unwrap(a[1]);
   if (!ctx) { return 0; }
   uint32_t n = cgc_components(cs);
   os_unfair_lock_lock(&g_cgc_lock);
   cgc_slot((uint64_t)(uintptr_t)ctx)->stroke_n = n;
   os_unfair_lock_unlock(&g_cgc_lock);
   CGContextSetStrokeColorSpace(ctx, cs);
   return 0;
}

/* CGContextSetFillColor(CGContextRef c, const CGFloat components[]) */
uint32_t shim_CGContextSetFillColor(uint32_t *a)
{
   CGContextRef ctx    = (CGContextRef)(uintptr_t)x64_objc_unwrap(a[0]);
   const float *comp_i = (const float *)(uintptr_t)a[1];   /* i386 float[], <4GB */
   if (!ctx || !comp_i) { return 0; }
   uint32_t n;
   os_unfair_lock_lock(&g_cgc_lock);
   n = cgc_slot((uint64_t)(uintptr_t)ctx)->fill_n;
   os_unfair_lock_unlock(&g_cgc_lock);
   if (n > CGC_MAX_N) { n = CGC_MAX_N; }
   double comp_n[CGC_MAX_N];
   cgfloat_i386_to_native(comp_n, comp_i, n);
   CGContextSetFillColor(ctx, comp_n);
   return 0;
}

/* CGContextSetStrokeColor(CGContextRef c, const CGFloat components[]) */
uint32_t shim_CGContextSetStrokeColor(uint32_t *a)
{
   CGContextRef ctx    = (CGContextRef)(uintptr_t)x64_objc_unwrap(a[0]);
   const float *comp_i = (const float *)(uintptr_t)a[1];
   if (!ctx || !comp_i) { return 0; }
   uint32_t n;
   os_unfair_lock_lock(&g_cgc_lock);
   n = cgc_slot((uint64_t)(uintptr_t)ctx)->stroke_n;
   os_unfair_lock_unlock(&g_cgc_lock);
   if (n > CGC_MAX_N) { n = CGC_MAX_N; }
   double comp_n[CGC_MAX_N];
   cgfloat_i386_to_native(comp_n, comp_i, n);
   CGContextSetStrokeColor(ctx, comp_n);
   return 0;
}
