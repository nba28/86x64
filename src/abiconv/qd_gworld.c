/*
 * qd_gworld.c — classic QuickDraw offscreen graphics reimplemented on
 * CoreGraphics. THE 2D substrate under classic QuickTime media rendering,
 * Carbon app drawing, and game offscreen compositing.
 *
 * Color QuickDraw (GWorlds/CGrafPorts, PixMaps, CopyBits, the pen/color/
 * pattern/clip drawing model, GDevices) was REMOVED from modern macOS, so a
 * no-op is not an acceptable end state (the functionality-first rule):
 * translated apps that draw into a GWorld and read the pixels back — or
 * CopyBits them onto another port — need the REAL semantics. classic QuickTime
 * decodes each frame INTO a GWorld and CopyBits it out, so this is the
 * substrate the Movie-Toolbox / codec path (agent a06720eb) renders through.
 *
 * WHAT THIS OWNS (all GONE on modern macOS):
 *   - GWorld / CGrafPort  = a low-4GB port owning a CGBitmapContext over a
 *     low-4GB pixel buffer, PLUS a REAL classic PixMap record (i386 pack(2)
 *     layout) kept in sync, so translated code that dereferences
 *     (**pm).baseAddr/rowBytes/bounds directly reads the truth.
 *   - CopyBits / CopyMask = a CoreGraphics image blit honoring the classic
 *     pixel formats (32-ARGB big-endian / 16-555 / 1-bit / 8-indexed+CLUT),
 *     srcRect->dstRect scaling, and the maskRgn clip. Foreign (app-owned) src
 *     AND dst BitMap/PixMap records are supported by wrapping their bytes.
 *   - the pen / color / pattern / clip state machine driving Erase/Paint/Fill/
 *     Frame/Invert Rect+Rgn, MoveTo/LineTo/Line.
 *   - a REAL main-screen GDevice record (gdRect = CGMainDisplayID bounds).
 *   - CreateCGContextForPort: hands the caller the port's live CGContextRef.
 *
 * REGIONS are NOT reimplemented here: the classic Region engine SURVIVES in
 * modern ApplicationServices (it backs HIShape) and abigen already emits
 * correct i386<->native marshalling for it (wrapping the >4GB RgnHandle into
 * the objc arena on return, unwrapping on the way in). We therefore CONSUME a
 * RgnHandle by unwrapping the arena handle to its native RgnHandle and clipping
 * via HIShapeCreateWithQDRgn. (The surviving-native region ops abigen has not
 * yet emitted — SectRgn/UnionRgn/... — are missing only because they are absent
 * from the legacy consider-set's target-import list; the fix is consider-set
 * coverage, a generic build-config change, not a reimplementation.)
 *
 * COORDINATES: the port's CGBitmapContext carries a FLIPPED base CTM
 * (translate(0,h); scale(1,-1)) so QD top-left y-down coords feed CG paths
 * directly. CGContextDrawImage under a flipped CTM mirrors the image, so image
 * blits locally re-flip around the destination rect.
 *
 * UNIVERSAL: triggers only on the classic QuickDraw entry points (Mach-O
 * symbol surface), never on an app name. Reached from translated i386 code via
 * the ___<Name> MTSHIM trampolines (maptable_tramp.asm; rdi -> &i386 args[0],
 * result in eax); excluded from abigen via custom.syms. ABICONV_QD_TRACE=1
 * logs operations.
 */

#include "carbon_shim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <os/lock.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <CoreFoundation/CoreFoundation.h>

/* objc_shim.c proxy arena: 64-bit pointer <-> 32-bit i386 handle. A genuine
 * low value / NULL passes straight through, so a raw <4GB ref is unharmed. */
extern uint32_t x64_objc_wrap(uint64_t real);
extern uint64_t x64_objc_unwrap(uint32_t h);

/* Resource Manager Handle shim (rm_shim.c): GetResource(ResType, short id) ->
 * a low-4GB Handle holding a COPY of the resource bytes. GetPicture routes here
 * to load 'PICT' resources for real. arg block: a[0]=ResType, a[1]=id. */
extern uint32_t shim_GetResource(uint32_t *a);

static int qd_trace(void)
{
   static int t = -1;
   if (t < 0) t = getenv("ABICONV_QD_TRACE") ? 1 : 0;
   return t;
}
#define QDLOG(...) do { if (qd_trace()) fprintf(stderr, "[qd] " __VA_ARGS__); } while (0)

/* ---- classic records (mac68k / i386 2-byte packing) --------------------- */
#pragma pack(push, 2)
typedef struct { int16_t v, h; } QDPoint;
typedef struct { int16_t top, left, bottom, right; } QDRect;
typedef struct { uint16_t red, green, blue; } QDRGBColor;
typedef struct { uint8_t pat[8]; } QDPattern;

/* classic PixMap — 50 bytes (verified vs 10.6 SDK QD/QuickdrawTypes.h). */
typedef struct {
   uint32_t baseAddr;         /*  0 Ptr (i386) to pixels                     */
   int16_t  rowBytes;         /*  4 offset to next line; high bit => PixMap  */
   QDRect   bounds;           /*  6                                          */
   int16_t  pmVersion;        /* 14 */
   int16_t  packType;         /* 16 */
   int32_t  packSize;         /* 18 */
   int32_t  hRes;             /* 22 Fixed 16.16 */
   int32_t  vRes;             /* 26 */
   int16_t  pixelType;        /* 30 0=indexed, 16=RGBDirect */
   int16_t  pixelSize;        /* 32 bits per pixel */
   int16_t  cmpCount;         /* 34 */
   int16_t  cmpSize;          /* 36 */
   uint32_t pixelFormat;      /* 38 OSType */
   uint32_t pmTable;          /* 42 CTabHandle (i386) */
   uint32_t pmExt;            /* 46 */
} QDPixMap;                   /* = 50 */

typedef struct { int16_t value; QDRGBColor rgb; } QDColorSpec;   /* 8 */
typedef struct {
   int32_t     ctSeed;
   int16_t     ctFlags;
   int16_t     ctSize;        /* #entries - 1 */
   QDColorSpec ctTable[1];
} QDColorTable;

/* classic GDevice — 62 bytes (10.6 SDK, pack(2)). */
typedef struct {
   int16_t  gdRefNum, gdID, gdType;
   uint32_t gdITable;
   int16_t  gdResPref;
   uint32_t gdSearchProc, gdCompProc;
   int16_t  gdFlags;
   uint32_t gdPMap;
   int32_t  gdRefCon;
   uint32_t gdNextGD;
   QDRect   gdRect;
   int32_t  gdMode;
   int16_t  gdCCBytes, gdCCDepth;
   uint32_t gdCCXData, gdCCXMask, gdExt;
} QDGDevice;                  /* = 62 */

typedef struct { QDPoint pnLoc, pnSize; int16_t pnMode; QDPattern pnPat; } QDPenState;
#pragma pack(pop)

enum { qdNoErr = 0, qdParamErr = -50, qdMemFullErr = -108 };
static __thread int16_t g_qd_err;
static void qd_set_err(int16_t e) { g_qd_err = e; }

/* ---- surviving-native lookups (regions/HIShape) via dlsym ---------------- */
#define QD_NATIVE(var, ty, name) \
   static ty var; if (!var) { var = (ty)dlsym(RTLD_DEFAULT, name); }
typedef void   *(*qn_p_v)(void);
typedef void    (*qn_v_p)(void *);
typedef void    (*qn_v_pp)(void *, void *);
typedef void    (*qn_v_pssss)(void *, int16_t, int16_t, int16_t, int16_t);
typedef void   *(*qn_hishape_from_rgn)(void *);
typedef int32_t (*qn_hishape_path)(void *, CGContextRef);
typedef void    (*qn_cfrelease)(void *);

/* Clip `ctx` to a native RgnHandle (already unwrapped). Returns 1 if applied. */
static int clip_ctx_to_native_rgn(CGContextRef ctx, void *natRgn)
{
   if (!ctx || !natRgn) return 0;
   QD_NATIVE(mk, qn_hishape_from_rgn, "HIShapeCreateWithQDRgn");
   QD_NATIVE(rp, qn_hishape_path,     "HIShapeReplacePathInCGContext");
   QD_NATIVE(rel, qn_cfrelease,       "CFRelease");
   if (!mk || !rp) return 0;
   void *shape = mk(natRgn);
   if (!shape) return 0;
   rp(shape, ctx);
   CGContextClip(ctx);
   if (rel) rel(shape);
   return 1;
}

/* ---- the port / GWorld object -------------------------------------------- */
#define QD_PORT_MAGIC 0x51504F52u   /* 'QPOR' */
/* PixMapHandle block: the visible PixMap record FIRST (so PixMapPtr == &pm),
 * then our owner/validity trailer. */
typedef struct { QDPixMap pm; uint16_t pad; uint32_t magic; struct qd_port *owner; } qd_pm_blk;

typedef struct qd_port {
   uint32_t     magic;
   void        *base;
   int          rowBytes, width, height, depth, owns_base;
   CGContextRef ctx;
   uint32_t     pm_handle;        /* PixMapHandle (cm Handle -> qd_pm_blk)   */
   QDRect       bounds;           /* portRect (top-left origin)              */
   int16_t      origin_h, origin_v;
   QDRGBColor   fore, back;
   QDPoint      pen_loc, pen_size;
   int16_t      pen_mode;
   QDPattern    pen_pat, back_pat;
   int          pen_hidden;
   int16_t      txFont, txFace, txSize, txMode;
   void        *clip_nat;         /* native RgnHandle clip (owned), or NULL  */
   void        *win;              /* real 64-bit WindowRef when this port IS
                                   * a window's GrafPort, else NULL — see the
                                   * window-backed-port block below.         */
   struct qd_port *next;
} qd_port;

static qd_port      *g_ports;
static os_unfair_lock g_ports_lk = OS_UNFAIR_LOCK_INIT;
static __thread uint32_t tl_cur_port;
static __thread uint32_t tl_cur_gd;

static qd_port *port_validate(qd_port *p)
{
   if (!p) return NULL;
   os_unfair_lock_lock(&g_ports_lk);
   for (qd_port *q = g_ports; q; q = q->next)
      if (q == p && q->magic == QD_PORT_MAGIC) { os_unfair_lock_unlock(&g_ports_lk); return p; }
   os_unfair_lock_unlock(&g_ports_lk);
   return NULL;
}
static qd_port *port_from_i386(uint32_t h)
{ return h ? port_validate((qd_port *)i386_ptr(h)) : NULL; }
static qd_port *cur_port(void) { return port_from_i386(tl_cur_port); }

/* Find the port whose visible PixMap/BitMap record is at i386 address recPtr. */
static qd_port *port_from_record(uint32_t recPtr)
{
   if (!recPtr) return NULL;
   qd_port *p = NULL;
   os_unfair_lock_lock(&g_ports_lk);
   for (qd_port *q = g_ports; q; q = q->next) {
      if (q->magic != QD_PORT_MAGIC || !q->pm_handle) continue;
      qd_pm_blk *blk = (qd_pm_blk *)cm_handle_block(q->pm_handle);
      if (blk && to_i386(&blk->pm) == recPtr) { p = q; break; }
   }
   os_unfair_lock_unlock(&g_ports_lk);
   return p;
}

static void pm_sync(qd_port *p)
{
   qd_pm_blk *blk = (qd_pm_blk *)cm_handle_block(p->pm_handle);
   if (!blk) return;
   QDPixMap *pm = &blk->pm;
   pm->baseAddr    = to_i386(p->base);
   pm->rowBytes    = (int16_t)(p->rowBytes | 0x8000);      /* PixMap flag     */
   pm->bounds      = p->bounds;
   pm->pmVersion   = 0; pm->packType = 0; pm->packSize = 0;
   pm->hRes = 72 << 16; pm->vRes = 72 << 16;
   pm->pixelType   = (p->depth >= 16) ? 16 : 0;
   pm->pixelSize   = (int16_t)p->depth;
   pm->cmpCount    = (p->depth >= 16) ? 3 : 1;
   pm->cmpSize     = (p->depth == 32) ? 8 : (p->depth == 16 ? 5 : (int16_t)p->depth);
   pm->pixelFormat = (uint32_t)p->depth;
   pm->pmExt       = 0;
   blk->magic = QD_PORT_MAGIC; blk->owner = p;
}

static CGContextRef make_ctx(void *base, int w, int h, int rowBytes, int depth)
{
   CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
   CGContextRef ctx = NULL;
   if (depth == 32)
      ctx = CGBitmapContextCreate(base, w, h, 8, rowBytes, cs,
               kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Big);
   else if (depth == 16)
      ctx = CGBitmapContextCreate(base, w, h, 5, rowBytes, cs,
               kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder16Big);
   CGColorSpaceRelease(cs);
   if (ctx) {
      CGContextTranslateCTM(ctx, 0, h);
      CGContextScaleCTM(ctx, 1, -1);
      CGContextSetShouldAntialias(ctx, false);
   }
   return ctx;
}

static void port_default_state(qd_port *p)
{
   p->fore.red = p->fore.green = p->fore.blue = 0;
   p->back.red = p->back.green = p->back.blue = 0xFFFF;
   p->pen_size.v = p->pen_size.h = 1;
   p->pen_mode = 8;                        /* patCopy */
   memset(p->pen_pat.pat, 0xFF, 8);        /* black */
   memset(p->back_pat.pat, 0x00, 8);       /* white */
   p->txSize = 12; p->txMode = 1;
}

static uint32_t port_create(uint32_t outPtr, const QDRect *bounds, int depth,
                            void *ext_base, int ext_rowBytes)
{
   int w = bounds ? bounds->right - bounds->left : 0;
   int h = bounds ? bounds->bottom - bounds->top : 0;
   if (w <= 0 || h <= 0 || w > 32767 || h > 32767) {
      put_u32(outPtr, 0); qd_set_err(qdParamErr); return (uint32_t)qdParamErr;
   }
   if (depth != 32 && depth != 16 && depth != 8 && depth != 1) depth = 32;
   int rowBytes = ext_rowBytes;
   if (rowBytes <= 0) rowBytes = ((w * depth + 7) / 8 + 15) & ~15;

   qd_port *p = (qd_port *)calloc(1, sizeof(qd_port));
   if (!p) { put_u32(outPtr, 0); qd_set_err(qdMemFullErr); return (uint32_t)qdMemFullErr; }
   p->magic = QD_PORT_MAGIC;
   p->width = w; p->height = h; p->depth = depth; p->rowBytes = rowBytes;
   p->bounds = *bounds; p->origin_h = bounds->left; p->origin_v = bounds->top;
   if (ext_base) { p->base = ext_base; p->owns_base = 0; }
   else          { p->base = calloc(1, (size_t)rowBytes * h); p->owns_base = 1; }
   if (!p->base) { free(p); put_u32(outPtr, 0); qd_set_err(qdMemFullErr); return (uint32_t)qdMemFullErr; }
   p->ctx = make_ctx(p->base, w, h, rowBytes, depth);
   p->pm_handle = cm_new_handle(sizeof(qd_pm_blk), 1);
   port_default_state(p);
   pm_sync(p);

   os_unfair_lock_lock(&g_ports_lk);
   p->next = g_ports; g_ports = p;
   os_unfair_lock_unlock(&g_ports_lk);

   put_u32(outPtr, to_i386(p));
   QDLOG("port_create %dx%d d=%d rb=%d base=%p ctx=%p -> %08x\n",
         w, h, depth, rowBytes, p->base, (void *)p->ctx, to_i386(p));
   qd_set_err(qdNoErr);
   return 0;
}

static void port_destroy(qd_port *p)
{
   os_unfair_lock_lock(&g_ports_lk);
   for (qd_port **q = &g_ports; *q; q = &(*q)->next)
      if (*q == p) { *q = p->next; break; }
   os_unfair_lock_unlock(&g_ports_lk);
   if (p->ctx) CGContextRelease(p->ctx);
   if (p->clip_nat) { QD_NATIVE(dr, qn_v_p, "DisposeRgn"); if (dr) dr(p->clip_nat); }
   if (p->pm_handle) cm_dispose_handle(p->pm_handle);
   if (p->owns_base && p->base) free(p->base);
   p->magic = 0;
   free(p);
}

/* ---- shared accessors for sibling shims (quicktime_image.c importer) ----- */
/* Resolve a GWorld/port handle (0 => current port) to its pixel geometry. */
int qd_port_pixels(uint32_t port_h, void **base, int *rowBytes, int *w, int *h)
{
   qd_port *p = port_from_i386(port_h ? port_h : tl_cur_port);
   if (!p) return 0;
   if (base) *base = p->base;
   if (rowBytes) *rowBytes = p->rowBytes;
   if (w) *w = p->width;
   if (h) *h = p->height;
   return 1;
}
/* Borrow the port's live CGContextRef (not retained), or NULL. */
CGContextRef qd_port_context(uint32_t port_h)
{
   qd_port *p = port_from_i386(port_h ? port_h : tl_cur_port);
   return p ? p->ctx : NULL;
}

/* ---- window-backed ports: THE Carbon + AGL drawable substrate -------------
 *
 * 64-bit macOS deleted GetWindowPort / GetWindowFromPort outright, so every
 * 32-bit-era Carbon app that asks a window for its GrafPort got NULL. That is
 * benign for drawing (this file owns that anyway) but FATAL for OpenGL: the
 * universal Carbon+AGL idiom is
 *      aglSetDrawable(ctx, GetWindowPort(win));
 * and with a NULL port there is no drawable at all — measured, on a real
 * compositing window: aglSetDrawable(ctx, NULL) -> 0 and no GL context, while
 * aglSetWindowRef(ctx, win) -> 1 with a live accelerated renderer. So the
 * window's port has to become a REAL object that still knows which window it
 * came from, and the AGL entry points have to recognise it (agl_drawable_shim.c).
 *
 * A window port is an ORDINARY qd_port from this same registry with the real
 * 64-bit WindowRef recorded in ->win.  Consequences that matter:
 *   - every existing GetPort / SetPort / PixMap / CopyBits / CreateCGContext-
 *     ForPort path accepts it with no special case, and none of them can be
 *     handed a half-formed port (it has real bounds, a real PixMap record and a
 *     real backing CGBitmapContext, exactly like a GWorld);
 *   - the REGISTRY IS THE CACHE — one port per WindowRef, found by walking
 *     g_ports — so port_destroy (DisposePort / DisposeGWorld / UpdateGWorld)
 *     un-caches it for free and no second table can dangle.
 *
 * Universal: keyed on "this port carries a WindowRef", never on an app name.
 * M64_NO_AGL_WINDOWREF=1 disarms the whole substrate (GetWindowPort returns to
 * NULL and the AGL shim forwards raw), which is the A/B guard's kill switch.
 */
typedef int32_t (*qn_getwindowbounds)(void *, uint16_t, QDRect *);

int qd_agl_windowref_enabled(void)
{
   static int e = -1;
   if (e < 0) e = getenv("M64_NO_AGL_WINDOWREF") ? 0 : 1;
   return e;
}

/* Serializes find-or-create so two threads cannot mint two ports for one
 * window. Distinct from g_ports_lk, which port_create takes for itself. */
static os_unfair_lock g_winport_lk = OS_UNFAIR_LOCK_INIT;

static qd_port *port_from_window(void *win)
{
   qd_port *p = NULL;
   os_unfair_lock_lock(&g_ports_lk);
   for (qd_port *q = g_ports; q; q = q->next)
      if (q->magic == QD_PORT_MAGIC && q->win == win) { p = q; break; }
   os_unfair_lock_unlock(&g_ports_lk);
   return p;
}

/* GetWindowPort(win): the window's CGrafPtr — created on demand, then stable
 * for the life of the window (repeated calls return the same i386 handle). */
uint32_t qd_port_for_window(void *win)
{
   if (!win || !qd_agl_windowref_enabled()) return 0;

   os_unfair_lock_lock(&g_winport_lk);
   qd_port *p = port_from_window(win);
   if (p) {
      uint32_t h = to_i386(p);
      os_unfair_lock_unlock(&g_winport_lk);
      return h;
   }

   /* Size the port to the window's CONTENT region expressed in the LOCAL
    * (0-origin) coordinates GetWindowPortBounds reports.  A window with no
    * usable content rect yet falls back to the main display so the port is
    * always well-formed — port_create rejects a degenerate rect, and returning
    * NULL is exactly the breakage being fixed. */
   QDRect r = { 0, 0, 0, 0 };
   QD_NATIVE(gwb, qn_getwindowbounds, "GetWindowBounds");
   if (gwb) {
      QDRect cr = { 0, 0, 0, 0 };
      if (gwb(win, 33 /*kWindowContentRgn*/, &cr) == 0) {
         r.bottom = (int16_t)(cr.bottom - cr.top);
         r.right  = (int16_t)(cr.right  - cr.left);
      }
   }
   if (r.bottom <= 0 || r.right <= 0) {
      CGDirectDisplayID d = CGMainDisplayID();
      r.bottom = (int16_t)CGDisplayPixelsHigh(d);
      r.right  = (int16_t)CGDisplayPixelsWide(d);
   }

   uint32_t h = 0;
   if (port_create(to_i386(&h), &r, 32, NULL, 0) != 0) h = 0;
   qd_port *np = port_from_i386(h);
   if (np) np->win = win; else h = 0;
   os_unfair_lock_unlock(&g_winport_lk);
   QDLOG("port_for_window win=%p -> %08x (%dx%d)\n", win, h, r.right, r.bottom);
   return h;
}

/* GetWindowFromPort(port), and the STRUCTURAL test agl_drawable_shim.c uses
 * ("is this drawable really a window?"): the WindowRef this port stands for,
 * or NULL for an ordinary offscreen GWorld / a value that is not a port. */
void *qd_port_window(uint32_t port_h)
{
   qd_port *p = port_from_i386(port_h);
   return p ? p->win : NULL;
}

/* Is this i386 value one of OUR ports (window-backed or an offscreen GWorld)?
 * Used by agl_drawable_shim.c to refuse to hand a qd_port that is NOT a window
 * to native AGL: our struct is not a GrafPort, so AGL would dereference a wild
 * pointer.  Failing the call is the only safe answer. */
int qd_is_port(uint32_t port_h)
{
   return port_from_i386(port_h) != NULL;
}

/* ---- graphics-state helpers ---------------------------------------------- */
static void set_cg_fill_qd(CGContextRef ctx, const QDRGBColor *c)
{ CGContextSetRGBFillColor(ctx, c->red/65535.0, c->green/65535.0, c->blue/65535.0, 1.0); }
static void set_cg_stroke_qd(CGContextRef ctx, const QDRGBColor *c)
{ CGContextSetRGBStrokeColor(ctx, c->red/65535.0, c->green/65535.0, c->blue/65535.0, 1.0); }
static int pat_is_black(const QDPattern *p){ for (int i=0;i<8;i++) if (p->pat[i]!=0xFF) return 0; return 1; }
static int pat_is_white(const QDPattern *p){ for (int i=0;i<8;i++) if (p->pat[i]!=0x00) return 0; return 1; }

static void port_begin(qd_port *p)
{
   CGContextSaveGState(p->ctx);
   if (p->clip_nat) clip_ctx_to_native_rgn(p->ctx, p->clip_nat);
}
static void port_end(qd_port *p) { CGContextRestoreGState(p->ctx); }
static CGRect qd_rect_to_cg(const QDRect *r)
{ return CGRectMake(r->left, r->top, r->right - r->left, r->bottom - r->top); }

/* ======================================================================== */
/* GWorld / port lifecycle                                                  */
/* ======================================================================== */

/* QDErr NewGWorld(GWorldPtr*, short depth, const Rect*, CTab, GD, flags) */
uint32_t shim_NewGWorld(uint32_t *a)
{
   int d = (int16_t)a[1]; if (d == 0) d = 32;
   return port_create(a[0], (const QDRect *)i386_ptr(a[2]), d, NULL, 0);
}
/* QDErr NewGWorldFromPtr(GWorldPtr*, depth, const Rect*, CTab, GD, flags, Ptr base, long rowBytes) */
uint32_t shim_NewGWorldFromPtr(uint32_t *a)
{
   int d = (a[1] <= 64) ? (int)a[1] : 32;
   return port_create(a[0], (const QDRect *)i386_ptr(a[2]), d, i386_ptr(a[6]), (int32_t)a[7]);
}
/* OSErr QTNewGWorldFromPtr(GWorldPtr*, OSType pixFmt, const Rect*, CTab, GD, flags, void*base, long rowBytes) */
uint32_t shim_QTNewGWorldFromPtr(uint32_t *a)
{
   int d = (a[1] <= 64) ? (int)a[1] : 32;
   return port_create(a[0], (const QDRect *)i386_ptr(a[2]), d, i386_ptr(a[6]), (int32_t)a[7]);
}
/* CGrafPtr CreateNewPort(void) — offscreen port sized to the main screen. */
uint32_t shim_CreateNewPort(uint32_t *a)
{
   (void)a;
   CGDirectDisplayID d = CGMainDisplayID();
   QDRect r = { 0, 0, (int16_t)CGDisplayPixelsHigh(d), (int16_t)CGDisplayPixelsWide(d) };
   uint32_t out = 0;
   port_create(to_i386(&out), &r, 32, NULL, 0);
   return out;
}
uint32_t shim_CreateNewPortForCGDisplayID(uint32_t *a)
{ (void)a; uint32_t z[1] = {0}; return shim_CreateNewPort(z); }

uint32_t shim_DisposeGWorld(uint32_t *a)
{
   qd_port *p = port_from_i386(a[0]);
   if (!p) return 0;
   if (tl_cur_port == a[0]) tl_cur_port = 0;
   port_destroy(p);
   return 0;
}
void shim_DisposePort(uint32_t *a) { (void)shim_DisposeGWorld(a); }

/* GWorldFlags UpdateGWorld(GWorldPtr*, short depth, const Rect*, CTab, GD, flags) */
uint32_t shim_UpdateGWorld(uint32_t *a)
{
   uint32_t gw = get_u32(a[0]);
   qd_port *p = port_from_i386(gw);
   const QDRect *nb = (const QDRect *)i386_ptr(a[2]);
   if (!p || !nb) { qd_set_err(qdParamErr); return (uint32_t)qdParamErr; }
   int nw = nb->right - nb->left, nh = nb->bottom - nb->top;
   int nd = (int16_t)a[1] ? (int16_t)a[1] : p->depth;
   if (nw == p->width && nh == p->height && nd == p->depth) {
      p->bounds = *nb; pm_sync(p); return 0;
   }
   uint32_t np_h = 0;
   if (port_create(to_i386(&np_h), nb, nd, NULL, 0)) return (uint32_t)qdMemFullErr;
   qd_port *np = port_from_i386(np_h);
   if (np && np->depth == p->depth) {
      int ch = nh < p->height ? nh : p->height;
      int cb = (nw < p->width ? nw : p->width) * p->depth / 8;
      for (int y = 0; y < ch; y++)
         memcpy((char*)np->base + (size_t)y*np->rowBytes,
                (char*)p->base  + (size_t)y*p->rowBytes, (size_t)cb);
      np->fore=p->fore; np->back=p->back; np->pen_pat=p->pen_pat; np->back_pat=p->back_pat;
   }
   if (tl_cur_port == gw) tl_cur_port = np_h;
   port_destroy(p);
   put_u32(a[0], np_h);
   return 1 << 14;
}

/* PixMapHandle GetGWorldPixMap / GetPortPixMap */
uint32_t shim_GetGWorldPixMap(uint32_t *a)
{ qd_port *p = port_from_i386(a[0]); return p ? p->pm_handle : 0; }
uint32_t shim_GetPortPixMap(uint32_t *a) { return shim_GetGWorldPixMap(a); }

/* const BitMap *GetPortBitMapForCopyBits(CGrafPtr) — &pm. */
uint32_t shim_GetPortBitMapForCopyBits(uint32_t *a)
{
   qd_port *p = port_from_i386(a[0]);
   if (!p) return 0;
   qd_pm_blk *blk = (qd_pm_blk *)cm_handle_block(p->pm_handle);
   return blk ? to_i386(&blk->pm) : 0;
}

/* Ptr GetPixBaseAddr(PixMapHandle) */
uint32_t shim_GetPixBaseAddr(uint32_t *a)
{
   qd_pm_blk *blk = (qd_pm_blk *)cm_handle_block(a[0]);
   return (blk && blk->magic == QD_PORT_MAGIC) ? blk->pm.baseAddr : 0;
}
uint32_t shim_GetPixRowBytes(uint32_t *a)
{
   qd_pm_blk *blk = (qd_pm_blk *)cm_handle_block(a[0]);
   return (blk && blk->magic == QD_PORT_MAGIC) ? (uint32_t)(blk->pm.rowBytes & 0x3FFF) : 0;
}
uint32_t shim_GetPixBounds(uint32_t *a)
{
   qd_pm_blk *blk = (qd_pm_blk *)cm_handle_block(a[0]);
   QDRect *r = (QDRect *)i386_ptr(a[1]);
   if (blk && r && blk->magic == QD_PORT_MAGIC) *r = blk->pm.bounds;
   return a[1];
}
uint32_t shim_LockPixels(uint32_t *a)   { (void)a; return 1; }
uint32_t shim_UnlockPixels(uint32_t *a) { (void)a; return 0; }

/* ---- current-port / device plumbing -------------------------------------- */
uint32_t shim_GetMainDevice(uint32_t *a);   /* fwd */
uint32_t shim_GetGWorld(uint32_t *a)
{
   put_u32(a[0], tl_cur_port);
   uint32_t z[1] = {0};
   put_u32(a[1], tl_cur_gd ? tl_cur_gd : shim_GetMainDevice(z));
   return 0;
}
uint32_t shim_SetGWorld(uint32_t *a) { tl_cur_port = a[0]; tl_cur_gd = a[1]; return 0; }
void shim_GetPort(uint32_t *a) { put_u32(a[0], tl_cur_port); }
void shim_SetPort(uint32_t *a) { tl_cur_port = a[0]; }
void shim_SetOrigin(uint32_t *a)
{ qd_port *p = cur_port(); if (p) { p->origin_h = (int16_t)a[0]; p->origin_v = (int16_t)a[1]; } }
uint32_t shim_GetPortBounds(uint32_t *a)
{
   qd_port *p = port_from_i386(a[0]);
   QDRect *r = (QDRect *)i386_ptr(a[1]);
   if (r) { if (p) *r = p->bounds; else { r->top=0;r->left=0;r->bottom=768;r->right=1024; } }
   return a[1];
}
void shim_SetPortBounds(uint32_t *a)
{
   qd_port *p = port_from_i386(a[0]);
   QDRect *r = (QDRect *)i386_ptr(a[1]);
   if (p && r) { p->bounds = *r; pm_sync(p); }
}

/* CGContextRef bridge: hand back the port's live context (REAL). */
uint32_t shim_CreateCGContextForPort(uint32_t *a)
{
   qd_port *p = port_from_i386(a[0]);
   uint32_t *out = (uint32_t *)i386_ptr(a[1]);
   if (!p || !p->ctx) { if (out) *out = 0; return (uint32_t)qdParamErr; }
   CGContextRetain(p->ctx);
   if (out) *out = x64_objc_wrap((uint64_t)(uintptr_t)p->ctx);
   return 0;
}
uint32_t shim_QDBeginCGContext(uint32_t *a) { return shim_CreateCGContextForPort(a); }
uint32_t shim_QDEndCGContext(uint32_t *a)
{
   CGContextRef ctx = (CGContextRef)(uintptr_t)x64_objc_unwrap(get_u32(a[1]));
   if (ctx) { CGContextFlush(ctx); CGContextRelease(ctx); }
   put_u32(a[1], 0);
   return 0;
}
uint32_t shim_QDError(uint32_t *a) { (void)a; return (uint32_t)(int32_t)g_qd_err; }
uint32_t shim_QDFlushPortBuffer(uint32_t *a)
{
   qd_port *p = port_from_i386(a[0]);
   if (p && p->ctx) CGContextFlush(p->ctx);
   return 0;
}
uint32_t shim_QDIsPortBuffered(uint32_t *a)   { (void)a; return 0; }
uint32_t shim_QDIsPortBufferDirty(uint32_t *a){ (void)a; return 0; }

/* ---- color / pen / text state -------------------------------------------- */
void shim_RGBForeColor(uint32_t *a)
{ qd_port *p = cur_port(); QDRGBColor *c=(QDRGBColor*)i386_ptr(a[0]); if (p&&c) p->fore=*c; }
void shim_RGBBackColor(uint32_t *a)
{ qd_port *p = cur_port(); QDRGBColor *c=(QDRGBColor*)i386_ptr(a[0]); if (p&&c) p->back=*c; }
static void classic_color(QDRGBColor *o, uint32_t code)
{
   switch (code) {
      case 30:  o->red=o->green=o->blue=0xFFFF; break;         /* whiteColor */
      case 33:  o->red=o->green=o->blue=0x0000; break;         /* blackColor */
      case 205: o->red=0xFFFF;o->green=0;o->blue=0; break;     /* redColor */
      case 341: o->red=0;o->green=0xFFFF;o->blue=0; break;     /* greenColor */
      case 409: o->red=0;o->green=0;o->blue=0xFFFF; break;     /* blueColor */
      default:  o->red=o->green=o->blue=0x0000; break;
   }
}
void shim_ForeColor(uint32_t *a){ qd_port *p=cur_port(); if (p) classic_color(&p->fore, a[0]); }
void shim_BackColor(uint32_t *a){ qd_port *p=cur_port(); if (p) classic_color(&p->back, a[0]); }
void shim_GetForeColor(uint32_t *a)
{
   qd_port *p=cur_port(); QDRGBColor *c=(QDRGBColor*)i386_ptr(a[0]);
   if (c) { if (p) *c=p->fore; else { c->red=c->green=c->blue=0; } }
}
void shim_GetBackColor(uint32_t *a)
{
   qd_port *p=cur_port(); QDRGBColor *c=(QDRGBColor*)i386_ptr(a[0]);
   if (c) { if (p) *c=p->back; else { c->red=c->green=c->blue=0xFFFF; } }
}
uint32_t shim_GetPortForeColor(uint32_t *a)
{
   qd_port *p=port_from_i386(a[0]); QDRGBColor *c=(QDRGBColor*)i386_ptr(a[1]);
   if (c) { if (p) *c=p->fore; else { c->red=c->green=c->blue=0; } }
   return a[1];
}
uint32_t shim_GetPortBackColor(uint32_t *a)
{
   qd_port *p=port_from_i386(a[0]); QDRGBColor *c=(QDRGBColor*)i386_ptr(a[1]);
   if (c) { if (p) *c=p->back; else { c->red=c->green=c->blue=0xFFFF; } }
   return a[1];
}

void shim_PenSize(uint32_t *a)
{ qd_port *p=cur_port(); if (p){ p->pen_size.h=(int16_t)a[0]; p->pen_size.v=(int16_t)a[1]; } }
void shim_PenNormal(uint32_t *a)
{ (void)a; qd_port *p=cur_port(); if (p){ p->pen_size.v=p->pen_size.h=1; p->pen_mode=8; memset(p->pen_pat.pat,0xFF,8);} }
void shim_PenMode(uint32_t *a){ qd_port *p=cur_port(); if (p) p->pen_mode=(int16_t)a[0]; }
void shim_PenPat(uint32_t *a){ qd_port *p=cur_port(); QDPattern *pat=(QDPattern*)i386_ptr(a[0]); if (p&&pat) p->pen_pat=*pat; }
void shim_BackPat(uint32_t *a){ qd_port *p=cur_port(); QDPattern *pat=(QDPattern*)i386_ptr(a[0]); if (p&&pat) p->back_pat=*pat; }
void shim_HidePen(uint32_t *a){ (void)a; qd_port *p=cur_port(); if (p) p->pen_hidden++; }
void shim_ShowPen(uint32_t *a){ (void)a; qd_port *p=cur_port(); if (p&&p->pen_hidden) p->pen_hidden--; }
void shim_MoveTo(uint32_t *a){ qd_port *p=cur_port(); if (p){ p->pen_loc.h=(int16_t)a[0]; p->pen_loc.v=(int16_t)a[1]; } }
void shim_Move(uint32_t *a){ qd_port *p=cur_port(); if (p){ p->pen_loc.h+=(int16_t)a[0]; p->pen_loc.v+=(int16_t)a[1]; } }

static void stroke_line(qd_port *p, int x0,int y0,int x1,int y1)
{
   if (!p->ctx || p->pen_hidden) return;
   port_begin(p);
   set_cg_stroke_qd(p->ctx, &p->fore);
   CGContextSetLineWidth(p->ctx, p->pen_size.h ? p->pen_size.h : 1);
   CGContextBeginPath(p->ctx);
   CGContextMoveToPoint(p->ctx, x0 + 0.5, y0 + 0.5);
   CGContextAddLineToPoint(p->ctx, x1 + 0.5, y1 + 0.5);
   CGContextStrokePath(p->ctx);
   port_end(p);
}
void shim_LineTo(uint32_t *a)
{
   qd_port *p=cur_port(); if (!p) return;
   int x1=(int16_t)a[0], y1=(int16_t)a[1];
   stroke_line(p, p->pen_loc.h, p->pen_loc.v, x1, y1);
   p->pen_loc.h=(int16_t)x1; p->pen_loc.v=(int16_t)y1;
}
void shim_Line(uint32_t *a)
{
   qd_port *p=cur_port(); if (!p) return;
   int x1=p->pen_loc.h+(int16_t)a[0], y1=p->pen_loc.v+(int16_t)a[1];
   stroke_line(p, p->pen_loc.h, p->pen_loc.v, x1, y1);
   p->pen_loc.h=(int16_t)x1; p->pen_loc.v=(int16_t)y1;
}
void shim_TextFont(uint32_t *a){ qd_port *p=cur_port(); if (p) p->txFont=(int16_t)a[0]; }
void shim_TextSize(uint32_t *a){ qd_port *p=cur_port(); if (p) p->txSize=(int16_t)a[0]; }
void shim_TextFace(uint32_t *a){ qd_port *p=cur_port(); if (p) p->txFace=(int16_t)a[0]; }
void shim_TextMode(uint32_t *a){ qd_port *p=cur_port(); if (p) p->txMode=(int16_t)a[0]; }
uint32_t shim_GetPortTextFont(uint32_t *a){ qd_port *p=port_from_i386(a[0]); return p?(uint16_t)p->txFont:0; }
uint32_t shim_GetPortTextFace(uint32_t *a){ qd_port *p=port_from_i386(a[0]); return p?(uint16_t)p->txFace:0; }
uint32_t shim_GetPortTextSize(uint32_t *a){ qd_port *p=port_from_i386(a[0]); return p?(uint16_t)p->txSize:0; }
uint32_t shim_GetPortTextMode(uint32_t *a){ qd_port *p=port_from_i386(a[0]); return p?(uint16_t)p->txMode:0; }

void shim_GetPenState(uint32_t *a)
{
   QDPenState *ps=(QDPenState*)i386_ptr(a[0]); qd_port *p=cur_port();
   if (!ps) return;
   memset(ps,0,sizeof(*ps));
   if (p){ ps->pnLoc=p->pen_loc; ps->pnSize=p->pen_size; ps->pnMode=p->pen_mode; ps->pnPat=p->pen_pat; }
   else { ps->pnSize.v=ps->pnSize.h=1; memset(ps->pnPat.pat,0xFF,8); }
}
void shim_SetPenState(uint32_t *a)
{
   QDPenState *ps=(QDPenState*)i386_ptr(a[0]); qd_port *p=cur_port();
   if (p&&ps){ p->pen_loc=ps->pnLoc; p->pen_size=ps->pnSize; p->pen_mode=ps->pnMode; p->pen_pat=ps->pnPat; }
}

/* ---- clip ---------------------------------------------------------------- */
uint32_t shim_SetClip(uint32_t *a)
{
   qd_port *p=cur_port(); if (!p) return 0;
   void *nat = (void *)(uintptr_t)x64_objc_unwrap(a[0]);
   QD_NATIVE(newr, qn_p_v, "NewRgn");
   QD_NATIVE(copy, qn_v_pp, "CopyRgn");
   if (!p->clip_nat && newr) p->clip_nat = newr();
   if (p->clip_nat && nat && copy) copy(nat, p->clip_nat);
   return 0;
}
uint32_t shim_GetClip(uint32_t *a)
{
   qd_port *p=cur_port();
   void *nat = (void *)(uintptr_t)x64_objc_unwrap(a[0]);
   QD_NATIVE(copy, qn_v_pp, "CopyRgn");
   QD_NATIVE(setr, qn_v_pssss, "SetRectRgn");
   if (!nat) return 0;
   if (p && p->clip_nat && copy) copy(p->clip_nat, nat);
   else if (setr) setr(nat, -32767, -32767, 32767, 32767);
   return 0;
}
uint32_t shim_ClipRect(uint32_t *a)
{
   qd_port *p=cur_port(); const QDRect *r=(const QDRect*)i386_ptr(a[0]);
   if (!p||!r) return 0;
   QD_NATIVE(newr, qn_p_v, "NewRgn");
   QD_NATIVE(setr, qn_v_pssss, "SetRectRgn");
   if (!p->clip_nat && newr) p->clip_nat = newr();
   if (p->clip_nat && setr) setr(p->clip_nat, r->left, r->top, r->right, r->bottom);
   return 0;
}
uint32_t shim_GetPortClipRegion(uint32_t *a)
{
   qd_port *p=port_from_i386(a[0]);
   void *nat = (void *)(uintptr_t)x64_objc_unwrap(a[1]);
   QD_NATIVE(copy, qn_v_pp, "CopyRgn");
   if (p && p->clip_nat && nat && copy) copy(p->clip_nat, nat);
   return a[1];
}
uint32_t shim_GetPortVisibleRegion(uint32_t *a)
{
   qd_port *p=port_from_i386(a[0]);
   void *nat = (void *)(uintptr_t)x64_objc_unwrap(a[1]);
   QD_NATIVE(setr, qn_v_pssss, "SetRectRgn");
   if (p && nat && setr) setr(nat, p->bounds.left, p->bounds.top, p->bounds.right, p->bounds.bottom);
   return a[1];
}

/* ---- rect fills ---------------------------------------------------------- */
static void fill_rect_color(qd_port *p, const QDRect *r, const QDRGBColor *col, const QDPattern *pat)
{
   if (!p->ctx) return;
   port_begin(p);
   if (pat && !pat_is_black(pat) && !pat_is_white(pat)) {
      CGContextClipToRect(p->ctx, qd_rect_to_cg(r));
      for (int y=r->top; y<r->bottom; y++) {
         uint8_t row = pat->pat[(y & 7)];
         for (int x=r->left; x<r->right; x++) {
            int on = (row >> (7 - (x & 7))) & 1;
            set_cg_fill_qd(p->ctx, on ? &p->fore : &p->back);
            CGContextFillRect(p->ctx, CGRectMake(x, y, 1, 1));
         }
      }
   } else {
      set_cg_fill_qd(p->ctx, (pat && pat_is_white(pat)) ? &p->back : col);
      CGContextFillRect(p->ctx, qd_rect_to_cg(r));
   }
   port_end(p);
}
void shim_PaintRect(uint32_t *a)
{ qd_port *p=cur_port(); const QDRect *r=(const QDRect*)i386_ptr(a[0]); if (p&&r) fill_rect_color(p,r,&p->fore,&p->pen_pat); }
void shim_EraseRect(uint32_t *a)
{ qd_port *p=cur_port(); const QDRect *r=(const QDRect*)i386_ptr(a[0]); if (p&&r) fill_rect_color(p,r,&p->back,NULL); }
void shim_FillRect(uint32_t *a)
{
   qd_port *p=cur_port(); const QDRect *r=(const QDRect*)i386_ptr(a[0]);
   const QDPattern *pat=(const QDPattern*)i386_ptr(a[1]);
   if (p&&r) fill_rect_color(p,r,&p->fore,pat);
}
void shim_FrameRect(uint32_t *a)
{
   qd_port *p=cur_port(); const QDRect *r=(const QDRect*)i386_ptr(a[0]);
   if (!p||!r||!p->ctx||p->pen_hidden) return;
   port_begin(p);
   set_cg_stroke_qd(p->ctx, &p->fore);
   int pw = p->pen_size.h ? p->pen_size.h : 1;
   CGContextSetLineWidth(p->ctx, pw);
   CGContextStrokeRect(p->ctx, CGRectMake(r->left+pw/2.0, r->top+pw/2.0,
                                          (r->right-r->left)-pw, (r->bottom-r->top)-pw));
   port_end(p);
}
/* void InvertRect(const Rect*) — exact per-pixel complement (32-bit ARGB). */
void shim_InvertRect(uint32_t *a)
{
   qd_port *p=cur_port(); const QDRect *r=(const QDRect*)i386_ptr(a[0]);
   if (!p||!r||p->depth!=32||!p->base) return;
   int t = r->top<0?0:r->top, l = r->left<0?0:r->left;
   int b = r->bottom>p->height?p->height:r->bottom, rr = r->right>p->width?p->width:r->right;
   for (int y=t; y<b; y++) {
      uint32_t *row = (uint32_t*)((char*)p->base + (size_t)y*p->rowBytes);
      for (int x=l; x<rr; x++) row[x] = ~row[x] | 0xFF000000u;
   }
}

/* ---- region fills (consume the surviving-native RgnHandle) ---------------- */
static int native_rgn_bbox(void *natRgn, QDRect *out)
{
   QD_NATIVE(gb, qn_v_pp, "GetRegionBounds");
   if (!gb || !natRgn) return 0;
   gb(natRgn, out);
   return 1;
}
static void fill_rgn_color(qd_port *p, void *natRgn, const QDRGBColor *col)
{
   if (!p->ctx || !natRgn) return;
   QDRect bb;
   if (!native_rgn_bbox(natRgn, &bb)) return;
   port_begin(p);
   clip_ctx_to_native_rgn(p->ctx, natRgn);
   set_cg_fill_qd(p->ctx, col);
   CGContextFillRect(p->ctx, qd_rect_to_cg(&bb));
   port_end(p);
}
void shim_PaintRgn(uint32_t *a){ qd_port *p=cur_port(); void *n=(void*)(uintptr_t)x64_objc_unwrap(a[0]); if (p) fill_rgn_color(p,n,&p->fore); }
void shim_EraseRgn(uint32_t *a){ qd_port *p=cur_port(); void *n=(void*)(uintptr_t)x64_objc_unwrap(a[0]); if (p) fill_rgn_color(p,n,&p->back); }
void shim_FillRgn(uint32_t *a){ qd_port *p=cur_port(); void *n=(void*)(uintptr_t)x64_objc_unwrap(a[0]); if (p) fill_rgn_color(p,n,&p->fore); }
void shim_InvertRgn(uint32_t *a)
{
   qd_port *p=cur_port(); void *n=(void*)(uintptr_t)x64_objc_unwrap(a[0]);
   if (!p||!n) return;
   QDRect bb; if (!native_rgn_bbox(n,&bb)) return;
   QDRect *heap = (QDRect*)malloc(sizeof(QDRect)); if (!heap) return;
   *heap = bb; uint32_t ia[1] = { to_i386(heap) };
   shim_InvertRect(ia);   /* bbox approximation for invert-region */
   free(heap);
}
void shim_FrameRgn(uint32_t *a)
{
   qd_port *p=cur_port(); void *n=(void*)(uintptr_t)x64_objc_unwrap(a[0]);
   if (!p||!n||!p->ctx) return;
   port_begin(p);
   clip_ctx_to_native_rgn(p->ctx, n);
   set_cg_stroke_qd(p->ctx, &p->fore);
   CGContextSetLineWidth(p->ctx, p->pen_size.h?p->pen_size.h:1);
   QDRect bb; if (native_rgn_bbox(n,&bb)) CGContextStrokeRect(p->ctx, qd_rect_to_cg(&bb));
   port_end(p);
}

/* ======================================================================== */
/* CopyBits / CopyMask                                                      */
/* ======================================================================== */

/* Build a CGImage from a classic BitMap/PixMap record (i386 addr) over sub-rect
 * `sr`. Supports 32-ARGB-BE, 16-555-BE, 1-bit, 8-bit indexed (via pmTable). */
static CGImageRef image_from_record(uint32_t recPtr, const QDRect *sr)
{
   if (!recPtr) return NULL;
   const QDPixMap *pm = (const QDPixMap *)i386_ptr(recPtr);
   int isPix = (pm->rowBytes & 0x8000) != 0;
   int rowBytes = pm->rowBytes & 0x3FFF;
   int depth = isPix ? pm->pixelSize : 1;
   void *base = i386_ptr(pm->baseAddr);
   if (!base || rowBytes <= 0) return NULL;

   int bx = pm->bounds.left, by = pm->bounds.top;
   int sx = sr->left - bx, sy = sr->top - by;
   int sw = sr->right - sr->left, sh = sr->bottom - sr->top;
   if (sw <= 0 || sh <= 0 || sx < 0 || sy < 0) return NULL;

   CGColorSpaceRef cs = NULL;
   CGBitmapInfo info = 0;
   int bpc = 8, bpp = 32;
   const void *sub = (const char *)base + (size_t)sy * rowBytes + (size_t)sx * depth / 8;

   if (depth == 32) {
      cs = CGColorSpaceCreateDeviceRGB();
      info = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Big; bpc = 8; bpp = 32;
   } else if (depth == 16) {
      cs = CGColorSpaceCreateDeviceRGB();
      info = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder16Big; bpc = 5; bpp = 16;
   } else if (depth == 8) {
      const QDColorTable *ct = pm->pmTable ? (const QDColorTable *)cm_handle_block(pm->pmTable) : NULL;
      if (!ct) return NULL;
      int n = ct->ctSize + 1;
      if (n < 1 || n > 256) return NULL;
      unsigned char *tbl = (unsigned char *)malloc((size_t)n * 3);
      if (!tbl) return NULL;
      for (int i = 0; i < n; i++) {
         const QDColorSpec *e = &ct->ctTable[i];
         int idx = (e->value >= 0 && e->value < n) ? e->value : i;
         tbl[idx*3+0] = e->rgb.red   >> 8;
         tbl[idx*3+1] = e->rgb.green >> 8;
         tbl[idx*3+2] = e->rgb.blue  >> 8;
      }
      CGColorSpaceRef base_cs = CGColorSpaceCreateDeviceRGB();
      cs = CGColorSpaceCreateIndexed(base_cs, n - 1, tbl);
      CGColorSpaceRelease(base_cs);
      free(tbl);
      info = 0; bpc = 8; bpp = 8;
   } else if (depth == 1) {
      cs = CGColorSpaceCreateDeviceGray();
      info = 0; bpc = 1; bpp = 1;
   } else {
      return NULL;
   }
   if (!cs) return NULL;

   CGDataProviderRef prov = CGDataProviderCreateWithData(NULL, sub, (size_t)rowBytes * sh, NULL);
   CGImageRef img = NULL;
   if (prov) {
      img = CGImageCreate(sw, sh, bpc, bpp, rowBytes, cs, info, prov, NULL, false, kCGRenderingIntentDefault);
      CGDataProviderRelease(prov);
   }
   CGColorSpaceRelease(cs);
   return img;
}

static void copybits_core(uint32_t srcRec, uint32_t dstRec, const QDRect *srcR,
                          const QDRect *dstR, int16_t mode, void *maskNat)
{
   if (!srcR || !dstR) return;
   qd_port *dp = port_from_record(dstRec);
   CGContextRef ctx = NULL;
   int made_ctx = 0, dbx = 0, dby = 0;
   if (dp) { ctx = dp->ctx; dbx = dp->bounds.left; dby = dp->bounds.top; }
   else {
      const QDPixMap *pm = (const QDPixMap *)i386_ptr(dstRec);
      if (!pm) return;
      int isPix = (pm->rowBytes & 0x8000) != 0;
      int drb = pm->rowBytes & 0x3FFF;
      int dd = isPix ? pm->pixelSize : 1;
      int w = pm->bounds.right - pm->bounds.left, h = pm->bounds.bottom - pm->bounds.top;
      void *b = i386_ptr(pm->baseAddr);
      if (!b || (dd != 32 && dd != 16)) { QDLOG("CopyBits: foreign dst depth %d unsupported\n", dd); return; }
      ctx = make_ctx(b, w, h, drb, dd);
      made_ctx = 1; dbx = pm->bounds.left; dby = pm->bounds.top;
   }
   if (!ctx) return;

   CGImageRef img = image_from_record(srcRec, srcR);
   if (!img) { if (made_ctx) CGContextRelease(ctx); QDLOG("CopyBits: src decode failed\n"); return; }

   double dx = dstR->left - dbx, dy = dstR->top - dby;
   double dw = dstR->right - dstR->left, dhh = dstR->bottom - dstR->top;

   CGContextSaveGState(ctx);
   if (maskNat) clip_ctx_to_native_rgn(ctx, maskNat);
   /* CGContextDrawImage under the flipped base CTM mirrors vertically; undo
    * around the destination rect so the image lands upright. */
   CGContextTranslateCTM(ctx, 0, dy + dhh);
   CGContextScaleCTM(ctx, 1, -1);
   CGContextDrawImage(ctx, CGRectMake(dx, 0, dw, dhh), img);
   CGContextRestoreGState(ctx);
   CGContextFlush(ctx);
   CGImageRelease(img);
   if (made_ctx) CGContextRelease(ctx);
   (void)mode;
   QDLOG("CopyBits src=%08x dst=%08x -> (%g,%g %gx%g)\n", srcRec, dstRec, dx, dy, dw, dhh);
}
/* void CopyBits(const BitMap*src, const BitMap*dst, const Rect*srcR,
 *               const Rect*dstR, short mode, RgnHandle maskRgn) */
uint32_t shim_CopyBits(uint32_t *a)
{
   void *mask = a[5] ? (void*)(uintptr_t)x64_objc_unwrap(a[5]) : NULL;
   copybits_core(a[0], a[1], (const QDRect*)i386_ptr(a[2]),
                 (const QDRect*)i386_ptr(a[3]), (int16_t)a[4], mask);
   return 0;
}
/* void CopyMask(src, mask, dst, srcR, maskR, dstR) — approximate as an
 * unmasked copy (32-bit src carries its own alpha). */
uint32_t shim_CopyMask(uint32_t *a)
{
   copybits_core(a[0], a[2], (const QDRect*)i386_ptr(a[3]),
                 (const QDRect*)i386_ptr(a[5]), 0, NULL);
   return 0;
}
uint32_t shim_CopyDeepMask(uint32_t *a)
{
   copybits_core(a[0], a[3], (const QDRect*)i386_ptr(a[4]),
                 (const QDRect*)i386_ptr(a[6]), (int16_t)a[7], NULL);
   return 0;
}

/* ======================================================================== */
/* GDevice — a real main-screen device record                              */
/* ======================================================================== */
static uint32_t g_main_gdevice;
static CGDirectDisplayID g_main_gdevice_id;   /* the display it was minted from */
static os_unfair_lock g_gd_lk = OS_UNFAIR_LOCK_INIT;

static uint32_t make_main_gdevice(void)
{
   os_unfair_lock_lock(&g_gd_lk);
   if (g_main_gdevice) { os_unfair_lock_unlock(&g_gd_lk); return g_main_gdevice; }
   CGDirectDisplayID d = CGMainDisplayID();
   g_main_gdevice_id = d;
   int w = (int)CGDisplayPixelsWide(d), h = (int)CGDisplayPixelsHigh(d);
   if (w <= 0) w = 1024; if (h <= 0) h = 768;
   uint32_t pmh = cm_new_handle(sizeof(qd_pm_blk), 1);
   qd_pm_blk *pmb = (qd_pm_blk *)cm_handle_block(pmh);
   pmb->pm.rowBytes = (int16_t)((w*4) | 0x8000);
   pmb->pm.bounds = (QDRect){0,0,(int16_t)h,(int16_t)w};
   pmb->pm.pixelSize = 32; pmb->pm.pixelType = 16; pmb->pm.cmpCount = 3; pmb->pm.cmpSize = 8;
   pmb->pm.hRes = 72<<16; pmb->pm.vRes = 72<<16;
   pmb->magic = QD_PORT_MAGIC;
   uint32_t gdh = cm_new_handle(sizeof(QDGDevice), 1);
   QDGDevice *gd = (QDGDevice *)cm_handle_block(gdh);
   gd->gdType = 2;
   gd->gdFlags = (int16_t)((1 << 15) | (1 << 13));   /* mainScreen | screenActive */
   gd->gdPMap = pmh;
   gd->gdRect = (QDRect){0,0,(int16_t)h,(int16_t)w};
   gd->gdCCDepth = 32;
   g_main_gdevice = gdh;
   os_unfair_lock_unlock(&g_gd_lk);
   QDLOG("main GDevice %dx%d -> %08x\n", w, h, gdh);
   return gdh;
}
/* ---- GDevice <-> CGDirectDisplayID ---------------------------------------
 * Classic code identifies a screen by GDHandle; every modern replacement API
 * (CGDisplay*, aglQueryRendererInfoForCGDirectDisplayIDs, CGDisplayIDToOpenGL-
 * DisplayMask) identifies it by CGDirectDisplayID.  We MINTED our GDevice from
 * a CGDirectDisplayID, so the mapping is a fact we already hold rather than
 * something to guess.  Exported so the Display Manager entry points
 * (carbon_ui_shim.c) and the AGL renderer-info family (agl_renderer_shim.c)
 * share one authority instead of each inventing its own.
 *
 * Returns 0 for a handle we never minted — callers decide what that means;
 * they must not silently substitute the main display for an unknown device.
 * The registry currently holds exactly one entry because DMGetNextScreenDevice/
 * GetNextDevice present a single screen; keep both halves in step if that
 * changes. */
uint32_t qd_gdevice_display_id(uint32_t gdh)
{
   if (!gdh) return 0;
   make_main_gdevice();                       /* idempotent; ensures the map exists */
   os_unfair_lock_lock(&g_gd_lk);
   uint32_t id = (gdh == g_main_gdevice) ? (uint32_t)g_main_gdevice_id : 0;
   os_unfair_lock_unlock(&g_gd_lk);
   return id;
}

/* The inverse. 0 if this display is not the one we present. */
uint32_t qd_gdevice_for_display_id(uint32_t display_id)
{
   uint32_t gdh = make_main_gdevice();
   os_unfair_lock_lock(&g_gd_lk);
   uint32_t r = (display_id == (uint32_t)g_main_gdevice_id) ? gdh : 0;
   os_unfair_lock_unlock(&g_gd_lk);
   return r;
}

uint32_t shim_GetMainDevice(uint32_t *a)  { (void)a; return make_main_gdevice(); }
uint32_t shim_GetDeviceList(uint32_t *a)  { (void)a; return make_main_gdevice(); }
uint32_t shim_GetGDevice(uint32_t *a)     { (void)a; return tl_cur_gd ? tl_cur_gd : make_main_gdevice(); }
uint32_t shim_GetNextDevice(uint32_t *a)  { (void)a; return 0; }
void     shim_SetGDevice(uint32_t *a)     { tl_cur_gd = a[0]; }
uint32_t shim_GetGWorldDevice(uint32_t *a){ (void)a; return make_main_gdevice(); }
uint32_t shim_TestDeviceAttribute(uint32_t *a)
{
   uint32_t attr = a[1];
   return (attr == 0 || attr == 2 || attr == 5) ? 1 : 0;   /* screen/active */
}

/* ======================================================================== */
/* PICT — classic Picture playback bridged to ImageIO                       */
/* ======================================================================== */
/* Modern macOS removed QuickDraw's PICT interpreter, BUT ImageIO still ships a
 * read-only com.apple.pict decoder. An in-memory PicHandle holds the picture
 * body ([SInt16 picSize][Rect picFrame][opcodes...]) WITHOUT the 512-byte PICT
 * file header; prepend a zero header so ImageIO accepts it, then decode. */
static CGImageRef pict_decode(const void *body, size_t len)
{
   if (!body || !len) return NULL;
   /* "wrapped-image PicHandle" (quicktime_movie_bridge.m GetMoviePict, and any
    * shim that snapshots a frame): a classic 10-byte header ([picSize][picFrame])
    * then the tag 'MVPX' and a self-describing image (PNG/etc.). Decode the image
    * directly — ImageIO can't ENCODE PICT, so this is how a live frame becomes a
    * drawable PicHandle. Detected by the tag at offset 10; falls through to the
    * real com.apple.pict path for genuine pictures. */
   if (len > 14) {
      const uint8_t *b = (const uint8_t *)body;
      if (b[10]=='M' && b[11]=='V' && b[12]=='P' && b[13]=='X') {
         CFDataRef id_ = CFDataCreate(NULL, b + 14, (CFIndex)(len - 14));
         CGImageSourceRef is = id_ ? CGImageSourceCreateWithData(id_, NULL) : NULL;
         CGImageRef im = is ? CGImageSourceCreateImageAtIndex(is, 0, NULL) : NULL;
         if (is) CFRelease(is);
         if (id_) CFRelease(id_);
         return im;
      }
   }
   CFMutableDataRef d = CFDataCreateMutable(NULL, 0);
   if (!d) return NULL;
   static const UInt8 hdr512[512] = { 0 };
   CFDataAppendBytes(d, hdr512, 512);
   CFDataAppendBytes(d, (const UInt8 *)body, (CFIndex)len);

   CFStringRef k = CFSTR("kCGImageSourceTypeIdentifierHint");
   CFStringRef v = CFSTR("com.apple.pict");
   CFDictionaryRef opt = CFDictionaryCreate(NULL, (const void **)&k, (const void **)&v, 1,
                                            &kCFTypeDictionaryKeyCallBacks,
                                            &kCFTypeDictionaryValueCallBacks);
   CGImageSourceRef src = CGImageSourceCreateWithData(d, opt);
   CGImageRef img = src ? CGImageSourceCreateImageAtIndex(src, 0, NULL) : NULL;
   if (src) CFRelease(src);
   if (opt) CFRelease(opt);
   CFRelease(d);
   return img;
}

/* Draw a decoded CGImage into port `p` at QD-space rect (dst), flip-corrected. */
static void draw_image_into_port(qd_port *p, CGImageRef img, const QDRect *dst)
{
   if (!p->ctx || !img) return;
   QDRect dr = dst ? *dst : p->bounds;
   double dx = dr.left, dw = dr.right - dr.left, dh = dr.bottom - dr.top, dy = dr.top;
   CGContextSaveGState(p->ctx);
   if (p->clip_nat) clip_ctx_to_native_rgn(p->ctx, p->clip_nat);
   CGContextTranslateCTM(p->ctx, 0, dy + dh);
   CGContextScaleCTM(p->ctx, 1, -1);
   CGContextDrawImage(p->ctx, CGRectMake(dx, 0, dw, dh), img);
   CGContextRestoreGState(p->ctx);
   CGContextFlush(p->ctx);
}

/* void DrawPicture(PicHandle myPicture, const Rect *dstRect) */
uint32_t shim_DrawPicture(uint32_t *a)
{
   qd_port *p = cur_port();
   if (!p || !p->ctx || !a[0]) return 0;
   void    *body = cm_handle_block(a[0]);
   uint32_t len  = cm_handle_size(a[0]);
   CGImageRef img = pict_decode(body, len);
   if (!img) { QDLOG("DrawPicture: decode failed (len=%u)\n", len); return 0; }
   draw_image_into_port(p, img, (const QDRect *)i386_ptr(a[1]));
   QDLOG("DrawPicture %ldx%ld\n", CGImageGetWidth(img), CGImageGetHeight(img));
   CGImageRelease(img);
   return 0;
}

/* PicHandle GetPicture(short picID) — load the 'PICT' resource for real via the
 * Resource Manager (rm_shim.c), which hands back a low-4GB Handle holding a copy
 * of the picture bytes that DrawPicture can then decode. */
uint32_t shim_GetPicture(uint32_t *a)
{
   uint32_t ra[2] = { kFourCC('P','I','C','T'), a[0] };
   return shim_GetResource(ra);
}

/* short GetPictInfo / QDGetPictureBounds etc. are rarely used; a caller that
 * needs the picture frame reads (**PicHandle).picFrame directly (offset 2). */

/* ---- QDPictToCGContext: the modern PICT-provider -> CGContext API --------
 * (QDPictCreateWithProvider/WithURL, QDPictDrawToCGContext, QDPictGetBounds,
 * QDPictRelease) was itself removed from 64-bit macOS. Reimplement on ImageIO:
 * a QDPictRef is a low-4GB struct holding the decoded CGImage + its bounds. */
#define QDPICT_MAGIC 0x51504354u   /* 'QPCT' */
typedef struct { uint32_t magic; CGImageRef img; QDRect frame; } qd_pict;

static qd_pict *qdpict_from_i386(uint32_t h)
{
   if (!h) return NULL;
   qd_pict *q = (qd_pict *)i386_ptr(h);
   return q->magic == QDPICT_MAGIC ? q : NULL;
}
static uint32_t qdpict_wrap_data(CFDataRef data)
{
   if (!data) return 0;
   CGImageRef img = pict_decode(CFDataGetBytePtr(data), (size_t)CFDataGetLength(data));
   if (!img) return 0;
   qd_pict *q = (qd_pict *)calloc(1, sizeof(qd_pict));
   if (!q) { CGImageRelease(img); return 0; }
   q->magic = QDPICT_MAGIC;
   q->img = img;
   q->frame.top = 0; q->frame.left = 0;
   q->frame.bottom = (int16_t)CGImageGetHeight(img);
   q->frame.right  = (int16_t)CGImageGetWidth(img);
   return to_i386(q);
}

/* QDPictRef QDPictCreateWithProvider(CGDataProviderRef provider) */
uint32_t shim_QDPictCreateWithProvider(uint32_t *a)
{
   CGDataProviderRef prov = (CGDataProviderRef)(uintptr_t)x64_objc_unwrap(a[0]);
   if (!prov) return 0;
   CFDataRef data = CGDataProviderCopyData(prov);
   uint32_t r = qdpict_wrap_data(data);
   if (data) CFRelease(data);
   return r;
}
/* QDPictRef QDPictCreateWithURL(CFURLRef url) */
uint32_t shim_QDPictCreateWithURL(uint32_t *a)
{
   CFURLRef url = (CFURLRef)(uintptr_t)x64_objc_unwrap(a[0]);
   if (!url) return 0;
   CGDataProviderRef prov = CGDataProviderCreateWithURL(url);
   if (!prov) return 0;
   CFDataRef data = CGDataProviderCopyData(prov);
   uint32_t r = qdpict_wrap_data(data);
   if (data) CFRelease(data);
   CGDataProviderRelease(prov);
   return r;
}
/* CGRect QDPictGetBounds(QDPictRef) — returns a CGRect (SRET on i386: a[0] is
 * the hidden return-struct pointer, a[1] is the QDPictRef). Bounds are integer
 * QD coords widened to CGFloat(float on i386). */
uint32_t shim_QDPictGetBounds(uint32_t *a)
{
   float *out = (float *)i386_ptr(a[0]);       /* CGRect {x,y,w,h} as 4 floats */
   qd_pict *q = qdpict_from_i386(a[1]);
   if (out) {
      out[0] = 0.0f; out[1] = 0.0f;
      out[2] = q ? (float)(q->frame.right)  : 0.0f;
      out[3] = q ? (float)(q->frame.bottom) : 0.0f;
   }
   return a[0];
}
/* OSStatus QDPictDrawToCGContext(CGContextRef ctx, CGRect rect, QDPictRef pict)
 * i386 frame: a[0]=ctx, a[1..4]=rect (4 floats), a[5]=pict. */
uint32_t shim_QDPictDrawToCGContext(uint32_t *a)
{
   CGContextRef ctx = (CGContextRef)(uintptr_t)x64_objc_unwrap(a[0]);
   qd_pict *q = qdpict_from_i386(a[5]);
   if (!ctx || !q || !q->img) return (uint32_t)qdParamErr;
   union { uint32_t u; float f; } x = { a[1] }, y = { a[2] }, w = { a[3] }, h = { a[4] };
   CGContextSaveGState(ctx);
   CGContextDrawImage(ctx, CGRectMake(x.f, y.f, w.f, h.f), q->img);
   CGContextRestoreGState(ctx);
   return 0;
}
/* void QDPictRelease(QDPictRef) */
uint32_t shim_QDPictRelease(uint32_t *a)
{
   qd_pict *q = qdpict_from_i386(a[0]);
   if (!q) return 0;
   if (q->img) CGImageRelease(q->img);
   q->magic = 0;
   free(q);
   return 0;
}

/* ======================================================================== */
/* Cursor — GetCursor/SetCursor (the removed classic 16x16 b/w cursor API)  */
/* ======================================================================== */
/* These are ABIGEN-faulting (their generated shim calls a REMOVED native and
 * would crash). Cursor shape is cosmetic on our offscreen substrate: hand back
 * a VALID (never-NULL) CursHandle so callers that do SetCursor(*GetCursor(id))
 * don't deref garbage, and make SetCursor a safe no-op (the live system cursor
 * is unchanged). A full NSCursor bridge is possible later but AppKit-heavy and
 * purely cosmetic. InitCursor/HideCursor/ShowCursor survive natively (abigen). */
/* CursHandle GetCursor(short cursorID) — Cursor = {Bits16 data; Bits16 mask;
 * Point hotSpot} = 16+16+4 = 68 bytes. */
uint32_t shim_GetCursor(uint32_t *a) { (void)a; return cm_new_handle(68, 1); }
/* void SetCursor(const Cursor *crsr) */
uint32_t shim_SetCursor(uint32_t *a) { (void)a; return 0; }
/* void SetCCursor(CCrsrHandle) / void SetCursorComponent — cosmetic no-ops. */
uint32_t shim_SetCCursor(uint32_t *a) { (void)a; return 0; }

/* ======================================================================== */
/* Display Manager (DM*) — screen-device + display-mode enumeration         */
/* ======================================================================== */
/* The classic Display Manager was removed from 64-bit macOS; the abigen shims
 * for these forward to a REMOVED native and would fault. Screen-device
 * enumeration is REAL (our main-screen GDevice); display-MODE enumeration is a
 * callback+nested-VD-struct subsystem (DMGetIndexedDisplayModeFromList invokes
 * a DMDisplayModeListIteratorUPP with a DMDisplayModeListEntryRec of
 * VDResolutionInfo/VDTimingInfo/... records), so for now we return an EMPTY
 * mode list (count=0) — the safe, non-faulting answer a well-behaved caller
 * handles by using the current mode; full per-mode enumeration is tracked as
 * follow-up. DMGetDisplayIDByGDevice/DMGetGDeviceByDisplayID/DMGetDeskRegion
 * live in carbon_ui_shim.c. */

/* GDHandle DMGetFirstScreenDevice(Boolean activeOnly) — the main screen. */
uint32_t shim_DMGetFirstScreenDevice(uint32_t *a) { (void)a; return make_main_gdevice(); }
/* GDHandle DMGetNextScreenDevice(GDHandle theDevice, Boolean activeOnly) — one device. */
uint32_t shim_DMGetNextScreenDevice(uint32_t *a) { (void)a; return 0; }

/* DMDisplayModeListIteratorUPP: a UPP is the proc pointer itself on Carbon-X. */
uint32_t shim_NewDMDisplayModeListIteratorUPP(uint32_t *a) { return a[0]; }
uint32_t shim_DisposeDMDisplayModeListIteratorUPP(uint32_t *a) { (void)a; return 0; }

/* OSErr DMNewDisplayModeList(DisplayIDType, UInt32 flags, UInt32 reserved,
 *                            DMListIndexType *count, DMListType *list) */
#define DM_LIST_MAGIC 0x444d4c53u   /* 'DMLS' */
uint32_t shim_DMNewDisplayModeList(uint32_t *a)
{
   put_u32(a[3], 0);                                   /* count = 0 (empty) */
   uint32_t h = cm_new_handle(4, 1);
   if (h) { uint32_t *b = (uint32_t *)cm_handle_block(h); if (b) *b = DM_LIST_MAGIC; }
   put_u32(a[4], h);
   return 0;                                            /* noErr */
}
/* OSErr DMGetIndexedDisplayModeFromList(DMListType, DMListIndexType index,
 *   UInt32 reserved, DMDisplayModeListIteratorUPP, void *userData) — empty
 * list => not reached in normal use; return paramErr WITHOUT calling back. */
uint32_t shim_DMGetIndexedDisplayModeFromList(uint32_t *a) { (void)a; return (uint32_t)qdParamErr; }
/* OSErr DMDisposeList(DMListType list) */
uint32_t shim_DMDisposeList(uint32_t *a) { if (a[0]) cm_dispose_handle(a[0]); return 0; }

/* ---- GetCTable / DisposeCTable — classic QuickDraw color-table loader -------
 *
 * `CTabHandle GetCTable(short ctID)` loaded a 'clut' resource (the standard
 * system color table for a pixel depth). Native QuickDraw is gone on modern
 * macOS, so the abigen `___GetCTable` forwarded to the dead native impl and
 * returned NULL — Civ IV's HBITMAP_Mac paletted-bitmap path (RTTI 11HBITMAP_Mac,
 * the depth-8/4 arms after the biBitCount jump table) then dereferenced the NULL
 * handle (`movl (%CTab),%reg`; NULL-deref at 0x0) building an indexed bitmap's
 * color table. Callers OVERWRITE the returned table's entries with their own
 * palette (Civ's loop writes ctTable[i].rgb for every index), so the CONTENTS
 * only need to be a valid default; what matters is a real, correctly-SIZED
 * ColorTable handle.
 *
 * ctID convention (classic Inside Macintosh): the standard tables are
 * `32 + depth` (grayscale) and `64 + depth` (color); ctID's low bits carry the
 * pixel depth (1/2/4/8). We size the table to 2^depth entries (capped 256),
 * seed a default ramp, and return a low-4GB Handle (cm_new_handle) whose block
 * layout is the classic ColorTable (QDColorTable here) the indexed-GWorld path
 * (line ~803) already consumes — so the produced table round-trips through our
 * own PixMap indexing too. Generic: any classic caller of GetCTable is served.
 */
uint32_t shim_GetCTable(uint32_t *a)
{
   int16_t ctID = (int16_t)(uint16_t)a[0];
   int depth = ctID & 0x7f;                 /* 32+d / 64+d both carry d in low 7 */
   if (depth <= 0 || depth > 8) { depth = 8; }
   int n = 1 << depth;                       /* 2,4,16,256 entries */
   if (n < 1)   { n = 1; }
   if (n > 256) { n = 256; }

   /* ColorTable header (8B) + n ColorSpec (8B each). */
   uint32_t sz = (uint32_t)(sizeof(QDColorTable) - sizeof(QDColorSpec)
                            + (size_t)n * sizeof(QDColorSpec));
   uint32_t h = cm_new_handle(sz, 1);
   if (!h) { return 0; }
   QDColorTable *ct = (QDColorTable *)cm_handle_block(h);
   if (!ct) { return 0; }

   ct->ctSeed  = 0;
   ct->ctFlags = 0;                          /* pixmap (device) table */
   ct->ctSize  = (int16_t)(n - 1);           /* classic: #entries - 1 */
   /* Seed a default gray ramp so a caller that does NOT overwrite still gets a
    * usable table (indexed 0..n-1); i has value==i, rgb = i scaled to 16-bit. */
   for (int i = 0; i < n; i++) {
      uint16_t g = (uint16_t)((n > 1) ? (i * 0xffff) / (n - 1) : 0);
      ct->ctTable[i].value    = (int16_t)i;
      ct->ctTable[i].rgb.red   = g;
      ct->ctTable[i].rgb.green = g;
      ct->ctTable[i].rgb.blue  = g;
   }
   return h;
}
/* DisposeCTable is hand-shimmed in qd_shim.c (frees the cm handle). */
