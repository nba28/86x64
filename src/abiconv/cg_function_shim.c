/*
 * cg_function_shim.c — CGFunctionCreate gradient/shading evaluate-callback bridge.
 *
 * iPhoto's -[?] drawGradient:startColor:endColor: builds a CGShading via
 * CGFunctionCreate, passing an i386 `evaluate` callback. Native CoreGraphics
 * (CGContextDrawShading) then SAMPLES that callback many times with the SysV
 * contract:
 *
 *     void evaluate(void *info, const CGFloat *in, CGFloat *out);
 *
 * where `in`/`out` are NATIVE buffers: 64-bit pointers (usually >4 GB) holding
 * `CGFloat` = 64-bit DOUBLE arrays. The abigen-generated ___CGFunctionCreate
 * wrapped the i386 evaluate through the GENERIC callback bridge, which passed
 * the native args to the i386 callback as raw 4-byte words. That truncated the
 * 64-bit `out` pointer into the i386 4-byte slot -> the i386 callback's
 * `movss %xmm0,(out)` stored to a low garbage address -> SIGBUS/SIGSEGV
 * (deterministic site iPhoto.dylib +0x3328e, run-dependent fault address =
 * low-32 of the native buffer). Even past the crash the colours would be
 * garbage: i386 CGFloat = 32-bit FLOAT, so the callback reads `in` / writes
 * `out` as floats while native wrote/reads doubles.
 *
 * This hand-shim replaces the generic wrap. On CGFunctionCreate we capture the
 * i386 evaluate/releaseInfo/info + domain/range dimensions in a context, and
 * install NATIVE evaluate/release trampolines. On each native evaluate call we
 * marshal correctly: native double `in[domainDim]` -> LOW-4GB i386 float `in`,
 * call the i386 evaluate on a fresh low-4GB stack, then i386 float `out` ->
 * native double `out[rangeDim]`. Per-call scratch is malloc'd (libabiconv
 * malloc is low-4GB; see malloc_shim.c) so the callback is reentrant / thread-
 * safe (CGContextDrawShading may sample it off the main thread). The domain/
 * range arrays are ALSO i386 float -> native double converted for the create
 * call. Universal: any i386 app using CGFunction / CGShading / CGGradient-with-
 * function; triggers on CGFunctionCreate, not on any app.
 *
 * Wired through maptable_tramp.asm (MTSHIM ___CGFunctionCreate): rdi -> &i386
 * args[0], CFTypeRef return wrapped to a low-4GB handle in eax.
 */
#include <CoreGraphics/CoreGraphics.h>
#include <stdint.h>
#include <stdlib.h>

/* call an i386 fnptr with `nwords` 4-byte args on a low-4GB stack (cb_bridge.c) */
extern uint32_t _86x64_call_i386(uint64_t fn, uint64_t nwords,
                                 const uint32_t *words, uint64_t lowstack_top);
/* native 64-bit pointer -> 32-bit arena handle the i386 code can store */
extern uint32_t x64_objc_wrap(uint64_t real);

#define CGFN_LOWSTACK_SZ (256u * 1024u)   /* matches CB_LOWSTACK_SZ */

struct cgfn_ctx {
   uint32_t i386_evaluate;   /* i386 CGFunctionEvaluateCallback fnptr */
   uint32_t i386_release;    /* i386 CGFunctionReleaseInfoCallback fnptr (0 = none) */
   uint32_t i386_info;       /* the info handle the i386 callback expects (unchanged) */
   uint32_t domain_dim;      /* # inputs  -> in[domain_dim] */
   uint32_t range_dim;       /* # outputs -> out[range_dim] */
};

/* NATIVE evaluate trampoline CoreGraphics calls while sampling the shading. */
static void cgfn_evaluate(void *info, const CGFloat *in, CGFloat *out)
{
   struct cgfn_ctx *c = (struct cgfn_ctx *)info;
   uint32_t din = c->domain_dim, dout = c->range_dim;

   void *stk     = malloc(CGFN_LOWSTACK_SZ);              /* low-4GB call stack */
   float *in_low  = din  ? (float *)malloc(din  * sizeof(float)) : (float *)0;
   float *out_low = dout ? (float *)malloc(dout * sizeof(float)) : (float *)0;
   if (!stk || (din && !in_low) || (dout && !out_low)) {
      /* degrade, don't crash: give CG a defined (0) colour rather than fault */
      for (uint32_t i = 0; i < dout; i++) { out[i] = 0.0; }
      free(stk); free(in_low); free(out_low);
      return;
   }

   for (uint32_t i = 0; i < din;  i++) { in_low[i]  = (float)in[i]; }  /* double -> float */
   for (uint32_t i = 0; i < dout; i++) { out_low[i] = 0.0f; }

   uint64_t top = ((uint64_t)(uintptr_t)stk + CGFN_LOWSTACK_SZ) & ~0xfULL;
   uint32_t words[3] = { c->i386_info,
                         (uint32_t)(uintptr_t)in_low,
                         (uint32_t)(uintptr_t)out_low };
   _86x64_call_i386((uint64_t)c->i386_evaluate, 3, words, top);

   for (uint32_t i = 0; i < dout; i++) { out[i] = (double)out_low[i]; } /* float -> double */

   free(stk); free(in_low); free(out_low);
}

/* NATIVE release trampoline; also frees the context. Always installed (so the
 * context is freed) even when the i386 supplied no releaseInfo. */
static void cgfn_release(void *info)
{
   struct cgfn_ctx *c = (struct cgfn_ctx *)info;
   if (!c) { return; }
   if (c->i386_release) {
      void *stk = malloc(CGFN_LOWSTACK_SZ);
      if (stk) {
         uint64_t top = ((uint64_t)(uintptr_t)stk + CGFN_LOWSTACK_SZ) & ~0xfULL;
         uint32_t words[1] = { c->i386_info };
         _86x64_call_i386((uint64_t)c->i386_release, 1, words, top);
         free(stk);
      }
   }
   free(c);
}

/* i386-cdecl entry (maptable_tramp.asm MTSHIM ___CGFunctionCreate). a = &args[0]:
 *   a[0] info, a[1] domainDim, a[2] domain(float*), a[3] rangeDim,
 *   a[4] range(float*), a[5] callbacks{version,evaluate,releaseInfo}. */
uint32_t shim_CGFunctionCreate(uint32_t *a)
{
   uint32_t info32     = a[0];
   uint32_t domain_dim = a[1];
   uint32_t domain32   = a[2];
   uint32_t range_dim  = a[3];
   uint32_t range32    = a[4];
   const uint32_t *cbs = (const uint32_t *)(uintptr_t)a[5];   /* i386 CGFunctionCallbacks */
   if (!cbs) { return 0; }

   struct cgfn_ctx *c = (struct cgfn_ctx *)malloc(sizeof *c);
   if (!c) { return 0; }
   c->i386_evaluate = cbs[1];
   c->i386_release  = cbs[2];
   c->i386_info     = info32;
   c->domain_dim    = domain_dim;
   c->range_dim     = range_dim;

   /* i386 CGFloat = 32-bit float; native = 64-bit double. Convert the domain/
    * range min/max pairs (2 per dimension). CGFunctionCreate copies them, so
    * stack buffers suffice. */
   const float *dom_i = domain32 ? (const float *)(uintptr_t)domain32 : (const float *)0;
   const float *rng_i = range32  ? (const float *)(uintptr_t)range32  : (const float *)0;
   double dom_n[128], rng_n[128];
   double *dom_p = (double *)0, *rng_p = (double *)0;
   if (dom_i && domain_dim) {
      uint32_t n = 2u * domain_dim; if (n > 128) { n = 128; }
      for (uint32_t i = 0; i < n; i++) { dom_n[i] = (double)dom_i[i]; }
      dom_p = dom_n;
   }
   if (rng_i && range_dim) {
      uint32_t n = 2u * range_dim; if (n > 128) { n = 128; }
      for (uint32_t i = 0; i < n; i++) { rng_n[i] = (double)rng_i[i]; }
      rng_p = rng_n;
   }

   CGFunctionCallbacks ncb = { 0, cgfn_evaluate, cgfn_release };
   CGFunctionRef fn = CGFunctionCreate(c, domain_dim, dom_p, range_dim, rng_p, &ncb);
   if (!fn) { free(c); return 0; }
   return x64_objc_wrap((uint64_t)(uintptr_t)fn);
}
