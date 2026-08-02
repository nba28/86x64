/*
 * cgl_macro_shim.c — ONE job: the CGL/AGL CONTEXT OBJECT as translated i386 code
 * actually uses it, i.e. as a DEREFERENCEABLE struct with an embedded GL dispatch
 * table, not as an opaque handle.
 *
 * (Distinct from agl_drawable_shim.c, which owns the drawable BINDING, and from
 * agl_renderer_shim.c, which owns renderer-info ENUMERATION. Different API
 * surface, different failure, its own kill switch — the one-shim-one-job rule.)
 *
 * ── THE PROBLEM ────────────────────────────────────────────────────────────────
 * Apple shipped <OpenGL/CGLMacro.h> specifically so that performance-critical
 * 32-bit apps — games — could skip the GL entry points entirely. Including it
 * #defines every GL call to go straight through the context object:
 *
 *     #define glGetString(name)  (*(cgl_ctx)->disp.get_string)((cgl_ctx)->rend, name)
 *
 * and <OpenGL/CGLContext.h> declares that object as
 *
 *     struct _CGLContextObject {
 *         GLIContext          rend;      // offset 0
 *         GLIFunctionDispatch disp;      // BY VALUE: 760 function pointers
 *         CGLPrivateObj       priv;
 *         void               *stak;
 *     };
 *
 * So such an app NEVER CALLS A GL SYMBOL. It loads a function pointer out of the
 * context and jumps through it, passing the context's renderer as an extra
 * leading argument. `AGLContext` is the same object (AGL is a thin layer over
 * CGL), so this applies to aglCreateContext's result too.
 *
 * Handing that app a proxy ARENA HANDLE for the context — which is what every
 * >4GB pointer returned to i386 code normally gets — is therefore fatal, and
 * fatal in the worst possible way. objc_shim.c mints a handle as
 *
 *     uint32_t handle = (uint32_t)(uintptr_t)&g_arena[slot];
 *
 * i.e. THE ADDRESS OF THE 8-BYTE SLOT holding the native pointer. Read as a
 * _CGLContextObject that means:
 *
 *     ctx->rend              = *(u32*)handle          = the LOW HALF of the
 *                                                       native context pointer
 *     ctx->disp.get_string   = *(u32*)(handle+0x1D8)  = unrelated arena bytes = 0
 *
 * and the app jumps through 0. Measured verbatim in a live Halo fault dump:
 * `rip=0`, and the arg slots the call had already pushed were
 * `[rsp+4]=0x3302a400` (exactly the low half of the live native context
 * `0x7fd23302a400`) and `[rsp+8]=0x1F03` = GL_EXTENSIONS.
 *
 * ★Pointing the handle at the REAL context does not help either: the native
 * struct has 8-byte fields, so the i386 offset 0x1D8 (field 117) lands on native
 * field ~58. **The 32-bit and 64-bit layouts of _CGLContextObject are
 * fundamentally incompatible**, and a purpose-built i386-layout SHADOW is the
 * only construction that can work.
 *
 * ── THE FIX ────────────────────────────────────────────────────────────────────
 * Whenever a bridged API hands an AGLContext/CGLContextObj to translated code,
 * return a low-4GB shadow in the i386 layout:
 *
 *     shadow->rend    = a token (the shadow's own address), never a truncated
 *                       pointer — nothing may be dereferenced out of it
 *     shadow->disp[i] = a static i386-callable thunk (gli_tramp.asm) that DROPS
 *                       the leading `rend` argument and tail-jumps to the
 *                       abigen-generated bridge ___gl<Name>
 *
 * Dropping one leading slot is signature-independent: in i386 cdecl every
 * argument, floats included, is a 4-byte stack slot. That is what makes a
 * 760-entry table possible without knowing 760 prototypes.
 *
 * ★The slot -> GL function mapping is GENERATED (gen_gli_dispatch.py ->
 * gli_dispatch_names.h) from the ERA-CORRECT 10.6 header, because the field ORDER
 * IS THE ABI and it is era-specific: the 10.6 and current gliDispatch.h agree only
 * for indices 0..440 and diverge after — and real callers reach past that (Halo
 * uses slots up to 630). Copying the LIVE native dispatch table slot-for-slot is
 * therefore wrong by construction and is deliberately not done. Every generated
 * name is verified against libabiconv's actual export list; unverifiable slots are
 * NULL and get a loud diagnostic stub, never a null pointer (Rule A — a null here
 * is exactly the `jmp *0` being fixed).
 *
 * Universal: it triggers on "a native CGL/AGL context is crossing to translated
 * code", never on an app name. CGLMacro.h is the idiom Apple published FOR games,
 * so every 32-bit Carbon/AGL title with a real renderer needs this.
 *
 * KILL SWITCH  M64_NO_CGL_MACRO=1 — hand back plain arena handles exactly as
 *   before, i.e. reproduce the pre-fix `jmp *0`. Used by the tests-i386 A/B guard,
 *   which fails if the two arms do not differ.
 * TRACE        ABICONV_AGL_TRACE=1 (shared with the other AGL shims).
 *
 * ABI: MTSHIM convention — rdi -> &i386 args[0] (4-byte cdecl slots), result in
 * eax.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <os/lock.h>

#include "gli_dispatch_names.h"

/* objc_shim.c proxy arena: 64-bit pointer <-> 32-bit i386 handle. */
extern uint32_t x64_objc_wrap(uint64_t real);
extern uint64_t x64_objc_unwrap(uint32_t h);

/* gli_tramp.asm */
extern uint64_t x64_gli_thunk_table[];   /* stub addresses, i386-callable      */
extern uint64_t x64_gli_nslots;
extern uint64_t g_gli_target[];          /* resolved ___glXxx bridge per slot  */

typedef unsigned char GLboolean;
typedef int           GLint;

static int agl_trace(void)
{
   static int t = -1;
   if (t < 0) t = getenv("ABICONV_AGL_TRACE") ? 1 : 0;
   return t;
}
#define CGLLOG(...) do { if (agl_trace()) { \
      fprintf(stderr, "[cglmacro] " __VA_ARGS__); fflush(stderr); } } while (0)

static int disarmed(void)
{
   static int d = -1;
   if (d < 0) d = getenv("M64_NO_CGL_MACRO") ? 1 : 0;
   return d;
}

/* ---- the i386-layout shadow --------------------------------------------- */

/* Exactly struct _CGLContextObject in the 32-bit layout: a 4-byte GLIContext
 * followed by the dispatch table by value. `priv`/`stak` follow; some callers
 * poke at them, so they exist and are zero rather than off the end of the
 * allocation. */
typedef struct {
   uint32_t rend;
   uint32_t disp[GLI_DISPATCH_SLOTS];
   uint32_t priv;
   uint32_t stak;
} cgl_shadow_i386;

typedef struct cgl_shadow {
   cgl_shadow_i386   *obj;      /* low-4GB, what the i386 side sees           */
   uint64_t           native;   /* the real CGLContextObj/AGLContext          */
   struct cgl_shadow *next;
} cgl_shadow;

static cgl_shadow    *g_shadows;
static os_unfair_lock g_lk = OS_UNFAIR_LOCK_INIT;

/* Resolve every dispatch slot ONCE: gli_slot_name[i] -> the abigen bridge
 * ___gl<Name>. dlsym() prepends one '_', and the bridge's nlist name is
 * "___gl<Name>", so the key is "__gl<Name>". Prefer this image's own copy (a
 * bundle carries one libabiconv per directory), falling back to the global
 * search — the GL bridges are stateless forwarders, so any copy is equivalent. */
static void gli_targets_init(void)
{
   static int done = 0;
   if (done) return;
   done = 1;

   void *self = NULL;
   Dl_info di;
   if (dladdr((void *)&gli_targets_init, &di) && di.dli_fname)
      self = dlopen(di.dli_fname, RTLD_NOLOAD | RTLD_LAZY);

   char key[128];
   int mapped = 0, missing = 0;
   for (uint32_t i = 0; i < GLI_DISPATCH_SLOTS; i++) {
      const char *nm = gli_slot_name[i];
      if (!nm) { missing++; continue; }
      snprintf(key, sizeof(key), "__%s", nm);
      void *fn = self ? dlsym(self, key) : NULL;
      if (!fn) fn = dlsym(RTLD_DEFAULT, key);
      if (fn) { g_gli_target[i] = (uint64_t)(uintptr_t)fn; mapped++; }
      else    { missing++; }
   }
   CGLLOG("dispatch table: %d/%d slots bound, %d unmapped (loud stub)\n",
          mapped, GLI_DISPATCH_SLOTS, missing);
}

/* Called from gli_tramp.asm for a slot with no verified bridge. Loud ONCE per
 * slot, then silent — a per-frame GL call must not turn the log into the
 * bottleneck. Returns nothing; the thunk returns a defined 0. */
void x64_gli_unmapped(uint32_t slot);
void x64_gli_unmapped(uint32_t slot)
{
   static uint8_t reported[GLI_DISPATCH_SLOTS];
   if (slot < GLI_DISPATCH_SLOTS && reported[slot]) return;
   if (slot < GLI_DISPATCH_SLOTS) reported[slot] = 1;
   fprintf(stderr, "[cglmacro] UNMAPPED GL dispatch slot %u (i386 offset 0x%x) "
                   "called and ignored — no verified ___gl* bridge\n",
           slot, 4u + 4u * slot);
   fflush(stderr);
}

/* native context -> i386 shadow, created once per context. */
static uint32_t shadow_for(uint64_t native)
{
   if (!native) return 0;
   os_unfair_lock_lock(&g_lk);
   for (cgl_shadow *s = g_shadows; s; s = s->next)
      if (s->native == native) {
         uint32_t h = (uint32_t)(uintptr_t)s->obj;
         os_unfair_lock_unlock(&g_lk);
         return h;
      }
   gli_targets_init();

   cgl_shadow *s = (cgl_shadow *)calloc(1, sizeof(*s));
   cgl_shadow_i386 *o = (cgl_shadow_i386 *)calloc(1, sizeof(*o));
   if (!s || !o) { free(s); free(o); os_unfair_lock_unlock(&g_lk); return 0; }
   /* Everything the i386 side can see must live in the low 4GB. libabiconv's
    * allocator is the translated heap, so this holds; refuse rather than hand
    * back a truncated pointer if it ever does not. */
   if ((uintptr_t)o >= 0x100000000ULL) {
      fprintf(stderr, "[cglmacro] shadow allocated above 4GB (%p) — refusing\n", (void *)o);
      free(s); free(o); os_unfair_lock_unlock(&g_lk); return 0;
   }
   for (uint32_t i = 0; i < GLI_DISPATCH_SLOTS && i < x64_gli_nslots; i++)
      o->disp[i] = (uint32_t)x64_gli_thunk_table[i];
   /* rend is a TOKEN, not a pointer to anything: the thunks drop it. Using the
    * shadow's own address keeps it unique, non-NULL and low-4GB, so a caller
    * that merely stores or compares it behaves sanely. */
   o->rend  = (uint32_t)(uintptr_t)o;
   s->obj    = o;
   s->native = native;
   s->next   = g_shadows;
   g_shadows = s;
   os_unfair_lock_unlock(&g_lk);
   CGLLOG("shadow %08x <- native context %p (disp[117]=%08x)\n",
          (uint32_t)(uintptr_t)o, (void *)(uintptr_t)native, o->disp[117]);
   return (uint32_t)(uintptr_t)o;
}

/* i386 value -> native context, if it is one of our shadows. Exported so the
 * sibling AGL shims (agl_drawable_shim.c) resolve a shadow the same way. */
uint64_t cgl_macro_ctx_native(uint32_t h);
uint64_t cgl_macro_ctx_native(uint32_t h)
{
   if (!h) return 0;
   uint64_t n = 0;
   os_unfair_lock_lock(&g_lk);
   for (cgl_shadow *s = g_shadows; s; s = s->next)
      if ((uint32_t)(uintptr_t)s->obj == h) { n = s->native; break; }
   os_unfair_lock_unlock(&g_lk);
   return n;
}

static void shadow_drop(uint32_t h)
{
   os_unfair_lock_lock(&g_lk);
   for (cgl_shadow **p = &g_shadows; *p; p = &(*p)->next)
      if ((uint32_t)(uintptr_t)(*p)->obj == h) {
         cgl_shadow *s = *p; *p = s->next;
         free(s->obj); free(s);
         break;
      }
   os_unfair_lock_unlock(&g_lk);
}

/* Accept EITHER a shadow (ours) or a plain arena handle (anything that reached
 * the i386 side before this shim existed, or through a path we do not own). */
static void *ctx_in(uint32_t h)
{
   uint64_t n = cgl_macro_ctx_native(h);
   if (!n) n = x64_objc_unwrap(h);
   return (void *)(uintptr_t)n;
}

/* Native context -> what the i386 side should hold. */
static uint32_t ctx_out(void *native)
{
   if (!native) return 0;
   if (disarmed()) return x64_objc_wrap((uint64_t)(uintptr_t)native);
   return shadow_for((uint64_t)(uintptr_t)native);
}

/* ---- the AGL context family --------------------------------------------- */

extern void *aglCreateContext(void *pix, void *share);
extern GLboolean aglDestroyContext(void *ctx);
extern void *aglGetCurrentContext(void);
extern GLboolean aglSetCurrentContext(void *ctx);
extern GLboolean aglUpdateContext(void *ctx);
extern void aglSwapBuffers(void *ctx);
extern GLboolean aglSetInteger(void *ctx, unsigned pname, const GLint *params);
extern GLboolean aglGetInteger(void *ctx, unsigned pname, GLint *params);
extern GLboolean aglSetFullScreen(void *ctx, int w, int h, int freq, int device);
extern GLint aglGetVirtualScreen(void *ctx);
extern GLboolean aglSetVirtualScreen(void *ctx, GLint screen);
extern GLboolean aglTexImagePBuffer(void *ctx, void *pbuf, GLint source);
extern GLboolean aglSetPBuffer(void *ctx, void *pbuf, GLint face, GLint level, GLint screen);

#define I386PTR(v) ((void *)(uintptr_t)(uint32_t)(v))

uint32_t shim_aglCreateContext(uint32_t *a)
{
   void *pix   = (void *)(uintptr_t)x64_objc_unwrap(a[0]);
   void *share = ctx_in(a[1]);
   void *ctx   = aglCreateContext(pix, share);
   CGLLOG("aglCreateContext(pix=%p, share=%p) = %p\n", pix, share, ctx);
   return ctx_out(ctx);
}

uint32_t shim_aglDestroyContext(uint32_t *a)
{
   void *ctx = ctx_in(a[0]);
   GLboolean ok = ctx ? aglDestroyContext(ctx) : 0;
   shadow_drop(a[0]);
   return ok;
}

uint32_t shim_aglGetCurrentContext(uint32_t *a)
{
   (void)a;
   /* Only ever report a context we already shadow: minting a shadow here for a
    * context the app never received would hand it a table for a renderer it did
    * not ask for. ctx_out reuses the existing shadow when there is one. */
   return ctx_out(aglGetCurrentContext());
}

uint32_t shim_aglSetCurrentContext(uint32_t *a)
{
   void *ctx = ctx_in(a[0]);
   GLboolean ok = aglSetCurrentContext(ctx);
   CGLLOG("aglSetCurrentContext(%08x -> %p) = %d\n", a[0], ctx, (int)ok);
   return ok;
}

uint32_t shim_aglUpdateContext(uint32_t *a) { return aglUpdateContext(ctx_in(a[0])); }
void     shim_aglSwapBuffers(uint32_t *a)   { aglSwapBuffers(ctx_in(a[0])); }

uint32_t shim_aglSetInteger(uint32_t *a)
{
   return aglSetInteger(ctx_in(a[0]), a[1], (const GLint *)I386PTR(a[2]));
}
uint32_t shim_aglGetInteger(uint32_t *a)
{
   /* Rule A: define the out-param before any path that can fail. */
   GLint *out = (GLint *)I386PTR(a[2]);
   if (out) *out = 0;
   return aglGetInteger(ctx_in(a[0]), a[1], out);
}
uint32_t shim_aglSetFullScreen(uint32_t *a)
{
   return aglSetFullScreen(ctx_in(a[0]), (int)a[1], (int)a[2], (int)a[3], (int)a[4]);
}
uint32_t shim_aglGetVirtualScreen(uint32_t *a)
{
   return (uint32_t)aglGetVirtualScreen(ctx_in(a[0]));
}
uint32_t shim_aglSetVirtualScreen(uint32_t *a)
{
   return aglSetVirtualScreen(ctx_in(a[0]), (GLint)a[1]);
}
uint32_t shim_aglTexImagePBuffer(uint32_t *a)
{
   return aglTexImagePBuffer(ctx_in(a[0]),
                             (void *)(uintptr_t)x64_objc_unwrap(a[1]), (GLint)a[2]);
}
uint32_t shim_aglSetPBuffer(uint32_t *a)
{
   return aglSetPBuffer(ctx_in(a[0]), (void *)(uintptr_t)x64_objc_unwrap(a[1]),
                        (GLint)a[2], (GLint)a[3], (GLint)a[4]);
}

/* ---- the CGL context family (the same object, reached the other way) ------ */

extern int   CGLCreateContext(void *pix, void *share, void **ctx);
extern int   CGLDestroyContext(void *ctx);
extern void *CGLGetCurrentContext(void);
extern int   CGLSetCurrentContext(void *ctx);

uint32_t shim_CGLCreateContext(uint32_t *a)
{
   uint32_t *out = (uint32_t *)I386PTR(a[2]);
   if (out) *out = 0;                                  /* Rule A */
   void *ctx = NULL;
   int err = CGLCreateContext((void *)(uintptr_t)x64_objc_unwrap(a[0]),
                              ctx_in(a[1]), &ctx);
   if (!err && out) *out = ctx_out(ctx);
   CGLLOG("CGLCreateContext -> err=%d ctx=%p\n", err, ctx);
   return (uint32_t)err;
}

uint32_t shim_CGLDestroyContext(uint32_t *a)
{
   void *ctx = ctx_in(a[0]);
   int err = ctx ? CGLDestroyContext(ctx) : 0;
   shadow_drop(a[0]);
   return (uint32_t)err;
}

uint32_t shim_CGLGetCurrentContext(uint32_t *a)
{
   (void)a;
   return ctx_out(CGLGetCurrentContext());
}

uint32_t shim_CGLSetCurrentContext(uint32_t *a)
{
   return (uint32_t)CGLSetCurrentContext(ctx_in(a[0]));
}
