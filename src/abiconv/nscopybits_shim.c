/*
 * nscopybits_shim.c — restore the legacy -[NSView gState] + NSCopyBits view
 * pixel-blit that modern macOS broke.
 *
 * The pre-10.10 offscreen view-capture idiom copies a source view's pixels with
 *     NSCopyBits([sourceView gState], srcRect, destPoint)
 * into a lockFocus'd destination. Quinn's -[NSView(QuinnExtensions) imageFromRect:]
 * uses it to build the board REFLECTION's source image (the mirrored settled
 * pieces on the reflective floor). On modern macOS -[NSView gState] returns 0
 * (the window-server graphics-state IDs are gone), so NSCopyBits(0, ...) copies
 * NOTHING and the reflected pieces vanish (the static gradient reflects via a
 * different, direct draw path, which is why only the pieces are missing).
 *
 * objc_shim.c restores the contract in two halves: (1) a swizzle of
 * -[NSView gState] hands a LEGACY-originated caller a non-zero TOKEN
 * (GSTATE_TOKEN_BASE + slot) that maps back to the source view; (2) THIS shim,
 * replacing the abigen ___NSCopyBits, recognises that token, resolves the source
 * view (_86x64_gstate_token_view) and RENDERS its srcRect into the CURRENT focus
 * context at destPoint via cacheDisplayInRect: + [rep drawInRect:] — the modern
 * equivalent of the old window-server blit. A genuine gState (or any non-token
 * value) falls through to native NSCopyBits unchanged.
 *
 * UNIVERSAL: triggers on the token structural property, never on any app. Wired
 * through maptable_tramp.asm (MTSHIM ___NSCopyBits) which excludes it from
 * abigen; rdi -> &i386 args[0]. i386 arg layout (all 4-byte slots):
 *   a[0]      = srcGState (NSInteger, i386 4-byte)
 *   a[1..4]   = srcRect   (NSRect: 4 x i386 float)
 *   a[5..6]   = destPoint (NSPoint: 2 x i386 float)
 */
#include <stdint.h>
#include <string.h>

typedef struct objc_object *id;
typedef struct objc_selector *SEL;
extern id   objc_getClass(const char *);
extern SEL  sel_registerName(const char *);
extern id   objc_msgSend(id, SEL, ...);

/* CGFloat/CGRect/CGPoint at NATIVE (x86_64) width for the AppKit re-sends. */
typedef struct { double x, y; }          NSPoint64;
typedef struct { double w, h; }          NSSize64;
typedef struct { NSPoint64 o; NSSize64 s; } NSRect64;

/* token -> source view (objc_shim.c); NULL for a non-token gState. */
extern id   _86x64_gstate_token_view(long tok);
/* native NSCopyBits (real AppKit symbol; we only reach it for non-token gStates). */
extern void NSCopyBits(long srcGState, NSRect64 srcRect, NSPoint64 destPoint);

/* matches objc_shim.c GSTATE_TOKEN_BASE / _CAP. */
#define NSCB_TOKEN_BASE 0x67530000L
#define NSCB_TOKEN_CAP  256

static SEL S(const char *n) { return sel_registerName(n); }

/* i386-cdecl entry: a = &args[0], each a 4-byte i386 slot. */
uint32_t shim_NSCopyBits(uint32_t *a)
{
   long   gs = (long)(int32_t)a[0];
   float  sx, sy, sw, sh, dx, dy;
   memcpy(&sx, &a[1], 4); memcpy(&sy, &a[2], 4);
   memcpy(&sw, &a[3], 4); memcpy(&sh, &a[4], 4);
   memcpy(&dx, &a[5], 4); memcpy(&dy, &a[6], 4);

   NSRect64  src  = { { (double)sx, (double)sy }, { (double)sw, (double)sh } };
   NSPoint64 dest = { (double)dx, (double)dy };

   /* non-token gState: genuine native path (widen already done). */
   if (gs < NSCB_TOKEN_BASE || gs >= NSCB_TOKEN_BASE + NSCB_TOKEN_CAP) {
      NSCopyBits(gs, src, dest);
      return 0;
   }

   /* token: resolve the source view and render its srcRect into the current
    * focus context at destPoint (the 10.6 window-server blit, restored). */
   id view = _86x64_gstate_token_view(gs);
   if (!view) { return 0; }

   /* cacheDisplayInRect: re-runs the source view's drawRect: into a rep that
    * captures its CURRENT pixels (the settled pieces the reflection mirrors). */
   id rep = ((id(*)(id, SEL, NSRect64))objc_msgSend)(
      view, S("bitmapImageRepForCachingDisplayInRect:"), src);
   if (!rep) { return 0; }
   ((void(*)(id, SEL, NSRect64, id))objc_msgSend)(
      view, S("cacheDisplayInRect:toBitmapImageRep:"), src, rep);

   /* draw the captured rep into the current context at destPoint, srcRect-sized
    * (points). The caller has already lockFocus'd the destination. */
   NSRect64 dr = { { dest.x, dest.y }, { src.s.w, src.s.h } };
   ((void(*)(id, SEL, NSRect64))objc_msgSend)(rep, S("drawInRect:"), dr);
   return 0;
}
