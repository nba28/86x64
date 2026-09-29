/*
 * agl_drawable_shim.c — ONE job: the AGL drawable binding of a Carbon window.
 *
 * THE PROBLEM (measured, not inferred — src/86x64/probes/):
 *   Every 32-bit-era Carbon+OpenGL app attaches its GL context with
 *       aglSetDrawable(ctx, GetWindowPort(win));
 *   64-bit macOS deleted GetWindowPort, so the drawable was NULL and
 *       aglSetDrawable(ctx, NULL) -> 0,   no GL context, nothing renders,
 *   which drops the app into an error path that 2006 hardware never exercised.
 *   (Halo CE alone has 21 aglSetDrawable call sites and ZERO aglSetWindowRef.)
 *   AGL itself is entirely PRESENT for x86_64 — including the modern
 *       aglSetWindowRef(ctx, win) -> 1,  GL_RENDERER = Apple M4 Pro
 *   which is the replacement path.
 *
 * THE FIX, and why it lives here rather than in the generated bridge:
 *   qd_gworld.c now hands GetWindowPort a REAL port that records its WindowRef.
 *   This file recognises such a port STRUCTURALLY (qd_port_window() != NULL —
 *   never an app name, never a heuristic on the value) and routes the attach to
 *   native aglSetWindowRef.  Anything else keeps today's behaviour exactly:
 *   the raw drawable is forwarded to native aglSetDrawable.
 *
 *   Intercepting aglSetDrawable is REQUIRED FOR SAFETY, not an optimisation.
 *   Native aglSetDrawable still exists, and abigen's legacy bridge does a real
 *   `call _aglSetDrawable` with the unwrapped drawable.  That is harmless only
 *   while the drawable is always NULL; the moment GetWindowPort returns a
 *   qd_port, the generated bridge would hand AGL a pointer to a struct that is
 *   not a GrafPort and AGL would dereference it.
 *
 * WHY aglGetDrawable IS SHIMMED TOO (and nothing else in AGL is):
 *   MEASURED: after a successful aglSetWindowRef, native aglGetDrawable returns
 *   NULL — the classic getter is blind to a WindowRef attach.  Carbon+AGL apps
 *   universally save/restore the drawable around any QuickDraw UI:
 *       saved = aglGetDrawable(ctx);  aglSetDrawable(ctx, NULL);
 *       ...alert / cursor / PICT...   aglSetDrawable(ctx, saved);
 *   (Halo does this at i386 0x33b6, 0x3497, 0x3553, 0x2a899e, 0x2a8d10,
 *   0x2a9b1d.)  With a blind getter the restore would re-attach NULL and GL
 *   would die permanently at the first alert.  So this file owns the
 *   ctx -> drawable association and answers the getter from it.  set/get of the
 *   one piece of state being repaired is ONE job; the rest of AGL (SetFullScreen,
 *   UpdateContext, SwapBuffers, ...) survives natively and is deliberately NOT
 *   touched — there is no evidence any of it is broken.
 *
 * MEASURED, so the save/restore cycle is known to work:
 *   detach aglSetWindowRef(ctx,NULL) -> 1, RE-attach aglSetWindowRef(ctx,win)
 *   -> 1 with a live renderer again.
 *
 * ABI: reached from translated i386 code through the ___agl* MTSHIM
 * trampolines (maptable_tramp.asm; rdi -> &i386 args[0], result in eax).  The
 * MTSHIM entries also remove these two symbols from abigen's legacy pass, so
 * this file is the only definition.  Opaque-pointer args (AGLContext,
 * AGLDrawable = CGrafPtr) arrive as proxy handles or raw low-4GB values;
 * x64_objc_unwrap resolves both.
 *
 * KILL SWITCH: M64_NO_AGL_WINDOWREF=1 disarms the whole substrate — GetWindowPort
 * returns NULL again and every call here forwards raw to native AGL, i.e. the
 * exact pre-fix behaviour.  Used by the A/B guard (tests-i386, agl-drawable).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <os/lock.h>
#include "gap.h"

/* objc_shim.c proxy arena: 64-bit pointer <-> 32-bit i386 handle. A genuine
 * low value / NULL passes straight through. */
extern uint32_t x64_objc_wrap(uint64_t real);
extern uint64_t x64_objc_unwrap(uint32_t h);

/* cgl_macro_shim.c owns the CGL/AGL CONTEXT OBJECT: because <OpenGL/CGLMacro.h>
 * makes 32-bit apps DEREFERENCE their context, it hands the i386 side an
 * i386-layout SHADOW rather than a proxy arena handle. So a context arriving
 * here is a shadow, not something x64_objc_unwrap knows about — resolve it
 * through its owner first, and keep the arena path for anything that reached
 * i386 by another route. */
extern uint64_t cgl_macro_ctx_native(uint32_t h);
static void *agl_ctx_in(uint32_t h)
{
   uint64_t n = cgl_macro_ctx_native(h);
   if (!n) n = x64_objc_unwrap(h);
   return (void *)(uintptr_t)n;
}

/* qd_gworld.c window-backed port substrate. */
extern void *qd_port_window(uint32_t port_h);
extern int   qd_is_port(uint32_t port_h);
extern int   qd_agl_windowref_enabled(void);

typedef unsigned char GLboolean;

/* cgl_fullscreen_shim.m / cg_display_fullscreen_shim.c: while an app's virtual
 * fullscreen mode is up, its context renders on the fullscreen surface, and a
 * native aglSetWindowRef would pull it back into the (hidden) Carbon window. */
extern int      cglfs_present(void *ctx, uint32_t dpy);
extern int      cglfs_presenting(const void *ctx);
extern uint32_t cgdisp_virtual_display(void);

/* ---- make libabiconv's AGL bridges self-sufficient ------------------------
 * libabiconv exports ~52 ___agl* ABI bridges whose bodies do a flat-namespace
 * `call _aglXxx`, and this file dlsym's aglSetWindowRef.  Both need AGL to be
 * LOADED in the process.  libabiconv cannot link it: AGL.framework has had no
 * SDK stub for years (`ld -framework AGL` fails) even though the code is very
 * much alive in the dyld shared cache.  An app that carries its own AGL load
 * command is fine, but one whose AGL dependency did not survive translation
 * gets an unbound lazy stub and jumps to address 0 on its first AGL call.
 * dlopen by shared-cache path fixes that for every such bridge at once.  This
 * is strictly cheaper than the ~25 frameworks libabiconv already links
 * unconditionally (OpenGL among them, which AGL is a thin layer over), and
 * merely loading it opens no window-server connection. */
__attribute__((constructor))
static void agl_load_framework(void)
{
   (void)dlopen("/System/Library/Frameworks/AGL.framework/AGL", RTLD_LAZY | RTLD_GLOBAL);
}

#define AGL_NATIVE(var, ty, name) \
   static ty var; \
   if (!var && !(var = (ty)dlsym(RTLD_DEFAULT, name))) GAP_ONCE("dlsym", name, __func__, 0);
typedef GLboolean (*agl_set_win)(void *, void *);
typedef GLboolean (*agl_set_draw)(void *, void *);
typedef void     *(*agl_get_draw)(void *);

/* ---- ctx -> drawable association ----------------------------------------
 * Small open table: a process has a handful of GL contexts (Halo: one, plus a
 * transient during a fullscreen/windowed switch). Entries are only ever added
 * or cleared, so a linear scan under one lock is both simplest and correct. */
#define AGL_MAX_CTX 32
static struct { void *ctx; uint32_t drawable; } g_bind[AGL_MAX_CTX];
static os_unfair_lock g_bind_lk = OS_UNFAIR_LOCK_INIT;

static void bind_set(void *ctx, uint32_t drawable)
{
   os_unfair_lock_lock(&g_bind_lk);
   int free_slot = -1;
   for (int i = 0; i < AGL_MAX_CTX; i++) {
      if (g_bind[i].ctx == ctx) { g_bind[i].drawable = drawable; goto done; }
      if (!g_bind[i].ctx && free_slot < 0) free_slot = i;
   }
   if (drawable && free_slot >= 0) {
      g_bind[free_slot].ctx = ctx;
      g_bind[free_slot].drawable = drawable;
   }
done:
   os_unfair_lock_unlock(&g_bind_lk);
}

/* THE GL RENDER TARGET, for anyone who has to reason about the window an app is
 * actually drawing into.  This file already owns the ctx -> drawable
 * association, so answering "which WindowRef currently has a GL drawable
 * attached, and through which context" is a read of state it owns rather than a
 * second job.  cg_display_fullscreen_shim.c uses it to identify the window the
 * classic `CGCaptureAllDisplays + CGDisplaySwitchToMode` fullscreen idiom means
 * to present, structurally — never by window title, class or app name.
 * Returns 1 and fills the out-params when a window-backed drawable is attached. */
int agl_gl_render_target(void **win_out, void **ctx_out)
{
   int found = 0;
   os_unfair_lock_lock(&g_bind_lk);
   for (int i = 0; i < AGL_MAX_CTX; i++) {
      if (!g_bind[i].ctx || !g_bind[i].drawable) continue;
      void *w = qd_port_window(g_bind[i].drawable);
      if (!w) continue;
      if (win_out) *win_out = w;
      if (ctx_out) *ctx_out = g_bind[i].ctx;
      found = 1;                        /* last attach wins */
   }
   os_unfair_lock_unlock(&g_bind_lk);
   return found;
}

/* A window whose SIZE changed out from under a live GL context leaves that
 * context's drawable geometry stale: the backing store keeps the old
 * dimensions while the window has new ones, and the app draws into a mismatched
 * buffer -- torn output and uninitialised grey where the new area is.
 *
 * Carbon apps are expected to call aglUpdateContext themselves when they handle
 * their own resize, and some (Halo) do not. Whoever changes a window's bounds
 * must therefore make the attached contexts re-read their geometry; this is
 * that call, keyed on the WINDOW so the caller needs to know nothing about
 * contexts. Universal: it triggers on "this window has GL attached", never on
 * an app.
 *
 * Returns the number of contexts updated. */
int agl_update_contexts_for_window(void *win)
{
   void *ctxs[AGL_MAX_CTX];
   int n = 0;
   os_unfair_lock_lock(&g_bind_lk);
   for (int i = 0; i < AGL_MAX_CTX && n < AGL_MAX_CTX; i++) {
      if (!g_bind[i].ctx || !g_bind[i].drawable) continue;
      if (qd_port_window(g_bind[i].drawable) == win) ctxs[n++] = g_bind[i].ctx;
   }
   os_unfair_lock_unlock(&g_bind_lk);   /* never call out under the lock */

   static GLboolean (*upd)(void *);
   if (!upd) upd = (GLboolean (*)(void *))dlsym(RTLD_DEFAULT, "aglUpdateContext");
   if (!upd) return 0;
   for (int i = 0; i < n; i++) upd(ctxs[i]);
   return n;
}

static uint32_t bind_get(void *ctx);

/* A GL window whose content is larger than the screen's usable area goes on the
 * surface as a scaled ordinary window (cgl_fullscreen_shim.m). Called wherever a
 * GL window can reach its final size: attach, show, SetWindowBounds. */
extern int cglfs_present_window(void *ctx, int w, int h);
void agl_window_fit(void *win)
{
   if (!win || cgdisp_virtual_display()) return;   /* fullscreen owns it then */
   void *ctx = NULL;
   os_unfair_lock_lock(&g_bind_lk);
   for (int i = 0; i < AGL_MAX_CTX; i++)
      if (g_bind[i].ctx && g_bind[i].drawable && qd_port_window(g_bind[i].drawable) == win) {
         ctx = g_bind[i].ctx; break;
      }
   os_unfair_lock_unlock(&g_bind_lk);
   if (!ctx) return;
   static int32_t (*getb)(void *, uint16_t, int16_t *);
   if (!getb) getb = (int32_t (*)(void *, uint16_t, int16_t *))dlsym(RTLD_DEFAULT, "GetWindowBounds");
   int16_t r[4] = { 0, 0, 0, 0 };                    /* top, left, bottom, right */
   if (!getb || getb(win, 33 /* kWindowContentRgn */, r) != 0) return;
   (void)cglfs_present_window(ctx, r[3] - r[1], r[2] - r[0]);
}

/* The fullscreen surface was taken down: give ctx back to the window it is
 * bound to (the surface never changed the binding, only the native target). */
void agl_reattach_window(void *ctx)
{
   void *win = qd_port_window(bind_get(ctx));
   AGL_NATIVE(setwin, agl_set_win, "aglSetWindowRef");
   if (win && setwin) (void)setwin(ctx, win);
}

static uint32_t bind_get(void *ctx)
{
   uint32_t d = 0;
   os_unfair_lock_lock(&g_bind_lk);
   for (int i = 0; i < AGL_MAX_CTX; i++)
      if (g_bind[i].ctx == ctx) { d = g_bind[i].drawable; break; }
   os_unfair_lock_unlock(&g_bind_lk);
   return d;
}

/* ---- GLboolean aglSetDrawable(AGLContext ctx, AGLDrawable draw) ---------- */
uint32_t shim_aglSetDrawable(uint32_t *a)
{
   void *ctx = agl_ctx_in(a[0]);
   uint32_t draw_h = a[1];
   if (!ctx) return 0;

   /* Was this context attached through the WindowRef path? Then the detach and
    * the re-attach must both go through aglSetWindowRef — native
    * aglSetDrawable cannot see, or undo, a WindowRef attachment. */
   const int was_ours = bind_get(ctx) != 0;
   void *win = qd_agl_windowref_enabled() ? qd_port_window(draw_h) : NULL;

   if (win) {
      if (cglfs_presenting(ctx)) { bind_set(ctx, draw_h); return 1; }
      AGL_NATIVE(setwin, agl_set_win, "aglSetWindowRef");
      if (setwin) {
         GLboolean ok = setwin(ctx, win);
         if (ok) {
            bind_set(ctx, draw_h);
            const uint32_t dpy = cgdisp_virtual_display();
            if (dpy) cglfs_present(ctx, dpy);   /* attached after the switch */
            else agl_window_fit(win);
            return 1;
         }
         bind_set(ctx, 0);
         return 0;
      }
   }

   if (!draw_h && was_ours && cglfs_presenting(ctx)) {
      /* Save/restore around QuickDraw UI while on the surface: nothing to
       * detach natively, and the restore re-binds through the branch above. */
      bind_set(ctx, 0);
      return 1;
   }
   if (!draw_h && was_ours) {
      /* The classic detach on a WindowRef-attached context. */
      AGL_NATIVE(setwin, agl_set_win, "aglSetWindowRef");
      if (setwin) {
         GLboolean ok = setwin(ctx, NULL);
         bind_set(ctx, 0);
         return ok ? 1 : 0;
      }
   }

   /* A qd_port that is NOT window-backed (an offscreen GWorld the app asked to
    * render into). Our port struct is not a GrafPort, so forwarding it would
    * have native AGL dereference a wild pointer — fail the call instead. Same
    * rule as the PAGEZERO C-string gate: never hand a framework a pointer that
    * is structurally incapable of being what it expects. */
   if (qd_is_port(draw_h)) {
      bind_set(ctx, 0);
      return 0;
   }

   /* Not one of ours: preserve today's behaviour exactly — forward the raw
    * (unwrapped) drawable to native aglSetDrawable. */
   bind_set(ctx, 0);
   AGL_NATIVE(setdraw, agl_set_draw, "aglSetDrawable");
   if (!setdraw) return 0;
   void *raw = (void *)(uintptr_t)x64_objc_unwrap(draw_h);
   GLboolean ok = setdraw(ctx, raw);
   return ok ? 1 : 0;
}

/* ---- AGLDrawable aglGetDrawable(AGLContext ctx) -------------------------- */
uint32_t shim_aglGetDrawable(uint32_t *a)
{
   void *ctx = agl_ctx_in(a[0]);
   if (!ctx) return 0;

   uint32_t ours = bind_get(ctx);
   if (ours) {
      return ours;                       /* already an i386 port pointer */
   }

   AGL_NATIVE(getdraw, agl_get_draw, "aglGetDrawable");
   if (!getdraw) return 0;
   void *d = getdraw(ctx);
   /* Mirror abigen's opaque-pointer return rule: a >4GB ref would truncate in
    * the i386 caller's eax, so wrap it; a low value passes through. */
   uint64_t v = (uint64_t)(uintptr_t)d;
   return (v >> 32) ? x64_objc_wrap(v) : (uint32_t)v;
}
