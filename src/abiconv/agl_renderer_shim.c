/*
 * agl_renderer_shim.c — ONE job: the renderer-info ENUMERATION family (AGL, and
 * CGLDescribeRenderer's classic answers).
 *
 * (Distinct from agl_drawable_shim.c, which owns the drawable BINDING of a
 * Carbon window. Different API family, different failure, different kill
 * switch — see the one-shim-one-job rule.)
 *
 * THE PROBLEM (measured on this machine, src/86x64/probes/aglrenderer.c):
 *   Every 32-bit-era Mac 3D layer asks "what GPUs are there, and are any of
 *   them accelerated?" with the QuickDraw-typed
 *       AGLRendererInfo aglQueryRendererInfo(const AGLDevice *gdevs, GLint ndev)
 *   where AGLDevice IS GDHandle (AGL/agl.h:39, deprecated 10.5). GDHandle does
 *   not exist on 64-bit macOS, and the surviving 64-bit aglQueryRendererInfo
 *   is a dead entry point:
 *       aglQueryRendererInfo(&anyGDHandle, 1) -> NULL,   aglGetError() == 0
 *       aglQueryRendererInfo(&NULL, 1)        -> NULL,   aglGetError() == 0
 *   It cannot succeed, for any input, ever. An app therefore counts ZERO
 *   renderers and takes an error path 2006 hardware never exercised.
 *
 *   Its live replacement works completely:
 *       aglQueryRendererInfoForCGDirectDisplayIDs(&CGMainDisplayID(), 1)
 *         -> 2 renderers; renderer 0 ACCELERATED=1, RENDERER_ID=0x1027f00,
 *            VIDEO_MEMORY=INT_MAX, TEXTURE_MEMORY=INT_MAX  (Apple M4 Pro)
 *   i.e. exactly the four properties classic code describes, all answering.
 *
 *   This is the same shape as GetWindowPort -> aglSetWindowRef (commit
 *   6e9d284): a dead QuickDraw-typed API whose CGDirectDisplayID-typed
 *   successor is alive. The bridge is the GDevice registry in qd_gworld.c,
 *   which minted the GDevice FROM a CGDirectDisplayID in the first place, so
 *   the mapping is recorded fact and not a guess.
 *
 * WHY THE WHOLE FAMILY IS OWNED HERE, not just the query:
 *   A real AGLRendererInfo is a native heap pointer (>4GB, measured
 *   0x6000025a1770). abigen's generated legacy bridge would truncate it into
 *   the i386 caller's 4-byte eax and the next call would dereference rubbish.
 *   So the query WRAPS its result into a low-4GB proxy handle, and
 *   next/describe/destroy UNWRAP it. Token ownership must be end to end or it
 *   is not ownership. These MTSHIM entries also remove the five symbols from
 *   abigen's legacy pass, so this file is their only definition.
 *
 * RULE A (a shim that fails still leaves every out-param DEFINED):
 *   aglDescribeRenderer writes *value before it can return false. Classic
 *   callers routinely store the value and check the GLboolean later, or not at
 *   all.
 *
 * KILL SWITCH  M64_NO_AGL_RENDERERINFO=1 — forward everything raw to native
 *   AGL, i.e. the exact pre-fix behaviour (query returns NULL). Used by the
 *   tests-i386 A/B guard, which fails if the two arms do not differ.
 *
 * ABI: MTSHIM convention — rdi -> &i386 args[0] (4-byte cdecl slots), result
 * in eax. Pointer args are i386 addresses in the shared low 4GB.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <CoreGraphics/CoreGraphics.h>
#include "gap.h"

/* objc_shim.c proxy arena: 64-bit pointer <-> 32-bit i386 handle. */
extern uint32_t x64_objc_wrap(uint64_t real);
extern uint64_t x64_objc_unwrap(uint32_t h);

/* qd_gworld.c GDevice registry — the one authority on GDHandle <-> display. */
extern uint32_t qd_gdevice_display_id(uint32_t gdh);

typedef unsigned char GLboolean;
typedef int           GLint;
typedef void         *AGLRendererInfo;

#define AGL_MAX_DEVICES 32

static int renderer_info_enabled(void)
{
   static int e = -1;
   if (e < 0) e = getenv("M64_NO_AGL_RENDERERINFO") ? 0 : 1;
   return e;
}

/* AGL.framework has no SDK stub; agl_drawable_shim.c's constructor dlopens it
 * by shared-cache path for the whole library, so RTLD_DEFAULT finds it here. */
#define AGL_NATIVE(var, ty, name) \
   static ty var; static int var##_done; \
   if (!var##_done) { var = (ty)dlsym(RTLD_DEFAULT, name); var##_done = 1; \
      if (!var) GAP_ONCE("dlsym", name, __func__, 0); }

typedef AGLRendererInfo (*agl_q_gd)(const void *, GLint);
typedef AGLRendererInfo (*agl_q_cg)(const CGDirectDisplayID *, GLint);
typedef GLboolean       (*agl_desc)(AGLRendererInfo, GLint, GLint *);
typedef AGLRendererInfo (*agl_next)(AGLRendererInfo);
typedef void            (*agl_destroy)(AGLRendererInfo);

/* A >4GB native ref must be wrapped; a low value passes through unchanged.
 * Mirrors abigen's opaque-pointer return rule (and agl_drawable_shim.c). */
static uint32_t ri_wrap(AGLRendererInfo r)
{
   uint64_t v = (uint64_t)(uintptr_t)r;
   return (v >> 32) ? x64_objc_wrap(v) : (uint32_t)v;
}
static AGLRendererInfo ri_unwrap(uint32_t h)
{
   return (AGLRendererInfo)(uintptr_t)x64_objc_unwrap(h);
}

/* ---- AGLRendererInfo aglQueryRendererInfo(const AGLDevice *gdevs, GLint ndev)
 *
 * Classic semantics: gdevs is an array of `ndev` GDHandles; a NULL array means
 * "every device". Translate each GDHandle through the registry that produced
 * it, then ask the surviving CGDirectDisplayID-typed entry point. A device we
 * did not mint is not silently replaced by the main display — it is dropped,
 * and an empty result is reported as the classic NULL, because inventing a
 * display the caller did not ask about would be a lie about the hardware. */
uint32_t shim_aglQueryRendererInfo(uint32_t *a)
{
   uint32_t gdevs_p = a[0];
   int32_t  ndev    = (int32_t)a[1];

   if (!renderer_info_enabled()) {
      AGL_NATIVE(qgd, agl_q_gd, "aglQueryRendererInfo");
      AGLRendererInfo r = qgd ? qgd((const void *)(uintptr_t)gdevs_p, ndev) : NULL;
      return ri_wrap(r);
   }

   CGDirectDisplayID ids[AGL_MAX_DEVICES];
   uint32_t n = 0;

   if (gdevs_p && ndev > 0) {
      const uint32_t *gd = (const uint32_t *)(uintptr_t)gdevs_p;
      for (int32_t i = 0; i < ndev && n < AGL_MAX_DEVICES; i++) {
         uint32_t id = qd_gdevice_display_id(gd[i]);
         if (id) ids[n++] = (CGDirectDisplayID)id;   /* a GDHandle not ours is dropped */
      }
   } else {
      /* NULL device list == "all devices" in the classic API. */
      if (CGGetActiveDisplayList(AGL_MAX_DEVICES, ids, &n) != kCGErrorSuccess) n = 0;
   }

   if (!n) {
      return 0;
   }

   AGL_NATIVE(qcg, agl_q_cg, "aglQueryRendererInfoForCGDirectDisplayIDs");
   if (!qcg) {
      return 0;
   }
   AGLRendererInfo r = qcg(ids, (GLint)n);
   uint32_t h = ri_wrap(r);
   return h;
}

/* ---- AGLRendererInfo aglQueryRendererInfoForCGDirectDisplayIDs(
 *                          const CGDirectDisplayID *dspIDs, GLint ndev)
 * Owned here purely so the returned token stays inside this file's arena:
 * a translated app that calls the modern entry point directly must get a
 * handle its aglNextRendererInfo/aglDescribeRenderer can still resolve. */
uint32_t shim_aglQueryRendererInfoForCGDirectDisplayIDs(uint32_t *a)
{
   const CGDirectDisplayID *ids = (const CGDirectDisplayID *)(uintptr_t)a[0];
   GLint ndev = (GLint)a[1];
   AGL_NATIVE(qcg, agl_q_cg, "aglQueryRendererInfoForCGDirectDisplayIDs");
   if (!qcg) return 0;
   AGLRendererInfo r = qcg(ids, ndev);
   return ri_wrap(r);
}

/* ---- AGLRendererInfo aglNextRendererInfo(AGLRendererInfo rend) ---------- */
uint32_t shim_aglNextRendererInfo(uint32_t *a)
{
   AGLRendererInfo r = ri_unwrap(a[0]);
   if (!r) return 0;
   AGL_NATIVE(nxt, agl_next, "aglNextRendererInfo");
   if (!nxt) return 0;
   return ri_wrap(nxt(r));
}

/* ---- GLboolean aglDescribeRenderer(AGLRendererInfo rend, GLint prop,
 *                                    GLint *value)
 * Rule A: *value is defined before any path that can return false. */
/* Depth modes in the classic vocabulary. Every 32-bit-era Mac renderer listed
 * kCGL24Bit/AGL_24_BIT_BIT (0x800); Apple Silicon's GL lists only 0 and 32-bit
 * (0x1001, measured), and Call of Duty 4 rejects a renderer without 24-bit depth
 * -> "Display: no valid displays" -> quits silently. A 24-bit depth request IS
 * honoured there (it gets 32), so the bit states something true. Kill
 * M64_NO_DEPTH24_MODE; guard depth24-mode. */
static GLint classic_depth_modes(GLint prop, GLint v)
{
   static int off = -1;
   if (off < 0) off = getenv("M64_NO_DEPTH24_MODE") != NULL;
   if (!off && prop == 105 /*kCGLRPDepthModes = AGL_DEPTH_MODES*/ && (v & 0x1000 /*32-bit*/))
      v |= 0x800;                                                   /* 24-bit */
   return v;
}

uint32_t shim_aglDescribeRenderer(uint32_t *a)
{
   GLint *value = (GLint *)(uintptr_t)a[2];
   if (value) *value = 0;

   AGLRendererInfo r = ri_unwrap(a[0]);
   GLint prop = (GLint)a[1];
   if (!r || !value) return 0;

   AGL_NATIVE(desc, agl_desc, "aglDescribeRenderer");
   if (!desc) return 0;
   GLboolean ok = desc(r, prop, value);
   if (!ok) *value = 0;                 /* native leaves it untouched on failure */
   else *value = classic_depth_modes(prop, *value);
   return ok ? 1 : 0;
}

/* ---- CGLError CGLDescribeRenderer(CGLRendererInfoObj, GLint, CGLRendererProperty, GLint *)
 * The CGL twin of the same enumeration contract (the query/destroy pair stays on
 * abigen's opaque-handle bridge; the handle is decoded the way it encodes it). */
extern uint64_t _86x64_unwrap_obj_arg(uint32_t h);
uint32_t shim_CGLDescribeRenderer(uint32_t *a)
{
   GLint *value = (GLint *)(uintptr_t)a[3];
   if (value) *value = 0;
   static int (*desc)(void *, GLint, int, GLint *);
   if (!desc) desc = (int (*)(void *, GLint, int, GLint *))dlsym(RTLD_DEFAULT, "CGLDescribeRenderer");
   void *r = (void *)(uintptr_t)_86x64_unwrap_obj_arg(a[0]);
   if (!desc || !r || !value) return 10000;   /* kCGLBadAttribute-range: never "ok" */
   GLint v = 0;
   int err = desc(r, (GLint)a[1], (int)a[2], &v);
   *value = err ? 0 : classic_depth_modes((GLint)a[2], v);
   return (uint32_t)err;
}

/* ---- void aglDestroyRendererInfo(AGLRendererInfo rend) ------------------ */
uint32_t shim_aglDestroyRendererInfo(uint32_t *a)
{
   AGLRendererInfo r = ri_unwrap(a[0]);
   if (!r) return 0;
   AGL_NATIVE(des, agl_destroy, "aglDestroyRendererInfo");
   if (des) des(r);
   return 0;
}
