/*
 * quicktime_image.c — the QuickTime still-image decode stack reimplemented on
 * modern ImageIO / CoreGraphics:
 *   - the GraphicsImporter component family ('grip') — Get*ForDataRef/ForFile,
 *     SetDataHandle/SetDataReference/SetDataFile, GetNaturalBounds,
 *     GetImageDescription, SetGWorld/SetBoundsRect, Draw, ...
 *   - the QuickDraw offscreen GWorld + PixMap (NewGWorld[FromPtr], DisposeGWorld,
 *     GetGWorldPixMap, GetPixBaseAddr/RowBytes/Bounds, LockPixels, GetGWorld/
 *     SetGWorld).
 *
 * WHY: QuickTime.framework is GONE on modern macOS — these symbols lazy-bind to
 * a @rpath/QuickTime.framework that does not exist (so the call would fault),
 * and the GraphicsImporter Component-Manager machinery is dead. iPhoto opens a
 * 'grip' importer to DECODE every photo/thumbnail in the grid. We register a
 * GraphicsImporter backend with the (reimplemented) Component Manager
 * [carbon_component.c] and route decode to CGImageSource: the importer holds
 * the encoded bytes / a CGImageSource, Draw renders the decoded CGImage into the
 * bound GWorld's low-4GB pixel buffer, and the caller reads ARGB pixels back via
 * GetPixBaseAddr.
 *
 * UNIVERSAL: any i386 app decoding images through the QuickTime GraphicsImporter
 * (the standard pre-ImageIO API — iLife, countless Carbon apps) is served. Set
 * ABICONV_QT_TRACE=1 to log the importer calls (ingestion path, sizes).
 *
 * Reached via the ___GraphicsImport* / ___NewGWorld* / ___GetPix* trampolines in
 * maptable_tramp.asm. The QuickTime originals are unshimmed by abigen (QuickTime
 * isn't in its consider-set); the QuickDraw/Memory ones are excluded via
 * custom.syms so these overrides win.
 */

#include "carbon_shim.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

extern void *malloc(size_t);
extern void *calloc(size_t, size_t);
extern void  free(void *);

static int qt_trace(void)
{
   static int t = -1;
   if (t < 0) t = getenv("ABICONV_QT_TRACE") ? 1 : 0;
   return t;
}
#define QTLOG(...) do { if (qt_trace()) fprintf(stderr, "[qt] " __VA_ARGS__); } while (0)

/* ---- classic types we expose to the i386 caller (byte-exact) ------------ */

/* QuickDraw Rect: four 16-bit signed coords, top/left/bottom/right. */
typedef struct { int16_t top, left, bottom, right; } QDRect;

/* QuickTime ImageDescription (86 bytes, naturally aligned == classic layout).
 * iPhoto reads width/height/depth/cType/hRes/vRes directly by offset. */
typedef struct {
   int32_t  idSize;          /* 0  */
   uint32_t cType;           /* 4  codec FourCC */
   int32_t  resvd1;          /* 8  */
   int16_t  resvd2;          /* 12 */
   int16_t  dataRefIndex;    /* 14 */
   int16_t  version;         /* 16 */
   int16_t  revisionLevel;   /* 18 */
   int32_t  vendor;          /* 20 */
   uint32_t temporalQuality; /* 24 */
   uint32_t spatialQuality;  /* 28 */
   int16_t  width;           /* 32 */
   int16_t  height;          /* 34 */
   int32_t  hRes;            /* 36 Fixed 16.16 */
   int32_t  vRes;            /* 40 Fixed 16.16 */
   int32_t  dataSize;        /* 44 */
   int16_t  frameCount;      /* 48 */
   uint8_t  name[32];        /* 50 Str31 */
   int16_t  depth;           /* 82 */
   int16_t  clutID;          /* 84 */
} ImageDescriptionRec;        /* logical size 86 */
#define IMAGEDESC_SIZE 86

/* ---- GWorld / PixMap ----------------------------------------------------- */

#define GW_MAGIC 0x47574c44u      /* 'GWLD' */
typedef struct gw_state {
   uint32_t magic;
   void    *base;                 /* low-4GB ARGB pixel buffer */
   int      rowBytes;
   int      width, height;
   int      depth;                /* bits per pixel (always 32 here) */
   int      owns_base;            /* free base on dispose */
   uint32_t pixmap_handle;        /* lazily-created PixMapHandle (cm Handle) */
} gw_state;

/* The block a PixMapHandle points at: just a back-pointer to the GWorld. Only
 * our accessors read it; the i386 caller treats the PixMapHandle opaquely. */
typedef struct { gw_state *gw; } gw_pixmap;

static gw_state *gw_from_i386(uint32_t h)
{
   if (!h) return NULL;
   gw_state *gw = (gw_state *)i386_ptr(h);
   return gw->magic == GW_MAGIC ? gw : NULL;   /* low-4GB; safe to peek */
}

/* Thread-local "current port" for GetGWorld/SetGWorld. */
static __thread uint32_t tl_cur_port;
static __thread uint32_t tl_cur_gd;

/* An opaque, never-dereferenced GDHandle token for GetGWorldDevice. */
#define FAKE_GDEVICE 0xF2000001u

static uint32_t make_pixmap_handle(gw_state *gw)
{
   if (gw->pixmap_handle)
      return gw->pixmap_handle;
   uint32_t h = cm_new_handle(sizeof(gw_pixmap), 0);
   if (!h) return 0;
   gw_pixmap *pm = (gw_pixmap *)cm_handle_block(h);
   pm->gw = gw;
   gw->pixmap_handle = h;
   return h;
}

static gw_state *gw_from_pixmap(uint32_t pmHandle)
{
   void *blk = cm_handle_block(pmHandle);
   if (!blk) return NULL;
   gw_state *gw = ((gw_pixmap *)blk)->gw;
   return (gw && gw->magic == GW_MAGIC) ? gw : NULL;
}

/* ---- GraphicsImporter instance ------------------------------------------ */

typedef struct gi_state {
   CFDataRef         data;        /* encoded bytes (retained), or NULL */
   CGImageSourceRef  src;         /* lazily created from data/url */
   CFURLRef          url;         /* file source (retained), or NULL */
   uint32_t          gw;          /* bound GWorld (i386 handle), or 0 */
   int               have_dest;
   QDRect            destRect;    /* where to draw within the GWorld */
   int32_t           flags;
   int32_t           quality;
} gi_state;

static cm_result gi_open(cm_instance *inst)
{
   gi_state *gi = (gi_state *)calloc(1, sizeof(gi_state));
   if (!gi) return cmMemFullErr;
   cm_inst_set_storage(inst, gi);
   QTLOG("open importer subtype=0x%08x storage=%p\n", cm_inst_subtype(inst), (void*)gi);
   return cmNoErr;
}

static void gi_close(cm_instance *inst)
{
   gi_state *gi = (gi_state *)cm_inst_storage(inst);
   if (!gi) return;
   if (gi->src)  CFRelease(gi->src);
   if (gi->data) CFRelease(gi->data);
   if (gi->url)  CFRelease(gi->url);
   free(gi);
}

/* Resolve the i386 ComponentInstance handle to its gi_state, or NULL. */
static gi_state *gi_from_i386(uint32_t h)
{
   cm_instance *inst = cm_inst_from_i386(h);
   return inst ? (gi_state *)cm_inst_storage(inst) : NULL;
}

/* Build the CGImageSource on demand from whichever data source was set. */
static CGImageSourceRef gi_source(gi_state *gi)
{
   if (gi->src) return gi->src;
   if (gi->data) gi->src = CGImageSourceCreateWithData(gi->data, NULL);
   else if (gi->url) gi->src = CGImageSourceCreateWithURL(gi->url, NULL);
   return gi->src;
}

/* Natural pixel dimensions WITHOUT a full decode (fast for grid thumbnails). */
static int gi_dimensions(gi_state *gi, int *w, int *h)
{
   CGImageSourceRef s = gi_source(gi);
   if (!s) return 0;
   CFDictionaryRef props = CGImageSourceCopyPropertiesAtIndex(s, 0, NULL);
   if (!props) return 0;
   int ok = 0;
   CFNumberRef nw = CFDictionaryGetValue(props, kCGImagePropertyPixelWidth);
   CFNumberRef nh = CFDictionaryGetValue(props, kCGImagePropertyPixelHeight);
   if (nw && nh && CFNumberGetValue(nw, kCFNumberIntType, w) &&
       CFNumberGetValue(nh, kCFNumberIntType, h))
      ok = 1;
   CFRelease(props);
   return ok;
}

/* ---- data ingestion ----------------------------------------------------- */

static void gi_reset_source(gi_state *gi)
{
   if (gi->src)  { CFRelease(gi->src);  gi->src  = NULL; }
   if (gi->data) { CFRelease(gi->data); gi->data = NULL; }
   if (gi->url)  { CFRelease(gi->url);  gi->url  = NULL; }
}

static void gi_set_bytes(gi_state *gi, const void *bytes, uint32_t len)
{
   gi_reset_source(gi);
   if (bytes && len)
      gi->data = CFDataCreate(NULL, (const UInt8 *)bytes, len);
   QTLOG("set data bytes=%p len=%u\n", bytes, len);
}

/* classic data-reference subtypes */
#define kHandleDataRef kFourCC('h','n','d','l')
#define kURLDataRef    kFourCC('u','r','l',' ')

static void gi_set_dataref(gi_state *gi, uint32_t dataRef, uint32_t dataRefType)
{
   if (dataRefType == kHandleDataRef) {
      /* dataRef is a Handle whose block holds the data Handle (4 bytes). */
      void *refblk = cm_handle_block(dataRef);
      uint32_t dataHandle = refblk ? *(uint32_t *)refblk : 0;
      gi_set_bytes(gi, cm_handle_block(dataHandle), cm_handle_size(dataHandle));
   } else if (dataRefType == kURLDataRef) {
      /* dataRef is a Handle holding a C-string URL. */
      const char *u = (const char *)cm_handle_block(dataRef);
      if (u) {
         CFStringRef s = CFStringCreateWithCString(NULL, u, kCFStringEncodingUTF8);
         if (s) {
            gi_reset_source(gi);
            gi->url = CFURLCreateWithString(NULL, s, NULL);
            CFRelease(s);
         }
      }
      QTLOG("set dataref url='%s'\n", u ? u : "(null)");
   } else {
      QTLOG("set dataref UNHANDLED type=0x%08x\n", dataRefType);
   }
}

/* ---- the DRAW: decode into the bound GWorld ----------------------------- */

static cm_result gi_draw(gi_state *gi)
{
   gw_state *gw = gw_from_i386(gi->gw ? gi->gw : tl_cur_port);
   if (!gw || !gw->base) { QTLOG("draw: no target GWorld\n"); return cmParamErr; }

   CGImageSourceRef s = gi_source(gi);
   if (!s) { QTLOG("draw: no image source\n"); return cmParamErr; }
   CGImageRef img = CGImageSourceCreateImageAtIndex(s, 0, NULL);
   if (!img) { QTLOG("draw: decode failed\n"); return cmCantOpenErr; }

   CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
   /* ARGB, 8bpc, big-endian 32-bit word == classic k32ARGBPixelFormat byte
    * order (A,R,G,B) that a 32-bit QuickDraw GWorld holds. */
   CGContextRef ctx = CGBitmapContextCreate(
      gw->base, gw->width, gw->height, 8, gw->rowBytes, cs,
      kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Big);
   CGColorSpaceRelease(cs);
   if (!ctx) { CGImageRelease(img); QTLOG("draw: no bitmap ctx\n"); return cmMemFullErr; }

   /* Destination rect (default: whole GWorld). QuickDraw is top-left origin,
    * CoreGraphics bottom-left -> flip vertically. */
   double dx = 0, dy = 0, dw = gw->width, dh = gw->height;
   if (gi->have_dest) {
      dx = gi->destRect.left;
      dy = gi->destRect.top;
      dw = gi->destRect.right  - gi->destRect.left;
      dh = gi->destRect.bottom - gi->destRect.top;
   }
   CGContextTranslateCTM(ctx, 0, gw->height);
   CGContextScaleCTM(ctx, 1, -1);
   CGContextDrawImage(ctx, CGRectMake(dx, dy, dw, dh), img);
   CGContextFlush(ctx);

   QTLOG("draw: %ldx%ld -> gworld %dx%d rb=%d at (%g,%g %gx%g)\n",
         CGImageGetWidth(img), CGImageGetHeight(img),
         gw->width, gw->height, gw->rowBytes, dx, dy, dw, dh);

   CGContextRelease(ctx);
   CGImageRelease(img);
   return cmNoErr;
}

/* ======================================================================== */
/* i386-cdecl entry points                                                  */
/* ======================================================================== */

/* ---- GraphicsImporter --------------------------------------------------- */

/* ComponentResult GraphicsImportSetDataHandle(ci, Handle dataHandle); */
uint32_t shim_GraphicsImportSetDataHandle(uint32_t *a)
{
   gi_state *gi = gi_from_i386(a[0]);
   if (!gi) return cmBadComponentType;
   gi_set_bytes(gi, cm_handle_block(a[1]), cm_handle_size(a[1]));
   return cmNoErr;
}

/* ComponentResult GraphicsImportSetDataReference(ci, Handle dataRef, OSType type); */
uint32_t shim_GraphicsImportSetDataReference(uint32_t *a)
{
   gi_state *gi = gi_from_i386(a[0]);
   if (!gi) return cmBadComponentType;
   gi_set_dataref(gi, a[1], a[2]);
   return cmNoErr;
}

/* ComponentResult GraphicsImportSetDataFile(ci, const FSSpec *spec); */
uint32_t shim_GraphicsImportSetDataFile(uint32_t *a)
{
   gi_state *gi = gi_from_i386(a[0]);
   if (!gi) return cmBadComponentType;
   /* FSSpec -> path is handled by the file opener; here we record nothing
    * useful without a live File Manager. Logged so we can see if it is hit. */
   QTLOG("SetDataFile(spec=%08x) — FSSpec path unsupported; prefer dataRef\n", a[1]);
   return cmParamErr;
}

/* ComponentResult GraphicsImportGetNaturalBounds(ci, Rect *naturalBounds); */
uint32_t shim_GraphicsImportGetNaturalBounds(uint32_t *a)
{
   gi_state *gi = gi_from_i386(a[0]);
   if (!gi) return cmBadComponentType;
   int w = 0, h = 0;
   if (!gi_dimensions(gi, &w, &h)) return cmCantOpenErr;
   QDRect *r = (QDRect *)i386_ptr(a[1]);
   if (r) { r->top = 0; r->left = 0; r->bottom = (int16_t)h; r->right = (int16_t)w; }
   QTLOG("GetNaturalBounds -> %dx%d\n", w, h);
   return cmNoErr;
}

/* ComponentResult GraphicsImportGetImageDescription(ci, ImageDescriptionHandle *desc); */
uint32_t shim_GraphicsImportGetImageDescription(uint32_t *a)
{
   gi_state *gi = gi_from_i386(a[0]);
   if (!gi) return cmBadComponentType;
   int w = 0, h = 0;
   if (!gi_dimensions(gi, &w, &h)) return cmCantOpenErr;

   uint32_t handle = cm_new_handle(IMAGEDESC_SIZE, 1);
   if (!handle) return cmMemFullErr;
   ImageDescriptionRec *d = (ImageDescriptionRec *)cm_handle_block(handle);
   d->idSize  = IMAGEDESC_SIZE;
   d->cType   = cm_inst_subtype(cm_inst_from_i386(a[0]));
   d->version = 1;
   d->vendor  = kFourCC('a','p','p','l');
   d->temporalQuality = 0;
   d->spatialQuality  = 0x000003FF;   /* codecLosslessQuality-ish */
   d->width   = (int16_t)w;
   d->height  = (int16_t)h;
   d->hRes    = 72 << 16;
   d->vRes    = 72 << 16;
   d->dataSize = 0;
   d->frameCount = 1;
   d->depth   = 32;
   d->clutID  = -1;
   put_u32(a[1], handle);
   QTLOG("GetImageDescription -> %dx%d depth32 handle=%08x\n", w, h, handle);
   return cmNoErr;
}

/* ComponentResult GraphicsImportSetGWorld(ci, CGrafPtr port, GDHandle gd); */
uint32_t shim_GraphicsImportSetGWorld(uint32_t *a)
{
   gi_state *gi = gi_from_i386(a[0]);
   if (!gi) return cmBadComponentType;
   gi->gw = a[1];
   QTLOG("SetGWorld(port=%08x)\n", a[1]);
   return cmNoErr;
}

/* ComponentResult GraphicsImportSetBoundsRect(ci, const Rect *bounds); */
uint32_t shim_GraphicsImportSetBoundsRect(uint32_t *a)
{
   gi_state *gi = gi_from_i386(a[0]);
   if (!gi) return cmBadComponentType;
   QDRect *r = (QDRect *)i386_ptr(a[1]);
   if (r) { gi->destRect = *r; gi->have_dest = 1; }
   return cmNoErr;
}

/* ComponentResult GraphicsImportGetBoundsRect(ci, Rect *bounds); */
uint32_t shim_GraphicsImportGetBoundsRect(uint32_t *a)
{
   gi_state *gi = gi_from_i386(a[0]);
   if (!gi) return cmBadComponentType;
   QDRect *r = (QDRect *)i386_ptr(a[1]);
   if (!r) return cmParamErr;
   if (gi->have_dest) {
      *r = gi->destRect;
   } else {
      int w = 0, h = 0;
      gi_dimensions(gi, &w, &h);
      r->top = 0; r->left = 0; r->bottom = (int16_t)h; r->right = (int16_t)w;
   }
   return cmNoErr;
}

/* ComponentResult GraphicsImportDraw(ci); */
uint32_t shim_GraphicsImportDraw(uint32_t *a)
{
   gi_state *gi = gi_from_i386(a[0]);
   if (!gi) return cmBadComponentType;
   return gi_draw(gi);
}

/* ComponentResult GraphicsImportSetFlags(ci, long flags); */
uint32_t shim_GraphicsImportSetFlags(uint32_t *a)
{
   gi_state *gi = gi_from_i386(a[0]);
   if (gi) gi->flags = (int32_t)a[1];
   return cmNoErr;
}
/* ComponentResult GraphicsImportSetQuality(ci, CodecQ quality); */
uint32_t shim_GraphicsImportSetQuality(uint32_t *a)
{
   gi_state *gi = gi_from_i386(a[0]);
   if (gi) gi->quality = (int32_t)a[1];
   return cmNoErr;
}
/* ComponentResult GraphicsImportSetMatrix(ci, const MatrixRecord *m); — ignored */
uint32_t shim_GraphicsImportSetMatrix(uint32_t *a) { (void)a; return cmNoErr; }
/* ComponentResult GraphicsImportSetGraphicsMode / SetFlags variants — n/a */

/* ComponentResult GraphicsImportGetColorSyncProfile(ci, Handle *profile); */
uint32_t shim_GraphicsImportGetColorSyncProfile(uint32_t *a)
{
   put_u32(a[1], 0);                 /* no embedded profile -> NULL, noErr */
   return cmNoErr;
}

/* ComponentResult GraphicsImportGetDataOffsetAndSize(ci, ulong *off, ulong *size); */
uint32_t shim_GraphicsImportGetDataOffsetAndSize(uint32_t *a)
{
   gi_state *gi = gi_from_i386(a[0]);
   if (!gi) return cmBadComponentType;
   put_u32(a[1], 0);
   put_u32(a[2], gi->data ? (uint32_t)CFDataGetLength(gi->data) : 0);
   return cmNoErr;
}

/* ComponentResult GraphicsImportGetMetaData(ci, void *outUserData); — none */
uint32_t shim_GraphicsImportGetMetaData(uint32_t *a) { (void)a; return cmCantOpenErr; }

/* ComponentResult GraphicsImportReadData(ci, void *buf, ulong off, ulong len); */
uint32_t shim_GraphicsImportReadData(uint32_t *a)
{
   gi_state *gi = gi_from_i386(a[0]);
   if (!gi || !gi->data) return cmParamErr;
   void *buf = i386_ptr(a[1]);
   uint32_t off = a[2], len = a[3];
   uint32_t total = (uint32_t)CFDataGetLength(gi->data);
   if (off > total) off = total;
   if (off + len > total) len = total - off;
   if (buf && len) memcpy(buf, CFDataGetBytePtr(gi->data) + off, len);
   return cmNoErr;
}

/* ComponentResult GetGraphicsImporterForDataRef(Handle dataRef, OSType type,
 *                                               ComponentInstance *gi); */
uint32_t shim_GetGraphicsImporterForDataRef(uint32_t *a)
{
   uint32_t inst = cm_open_default(kGraphicsImporterType, 0);
   gi_state *gi = gi_from_i386(inst);
   if (!gi) { put_u32(a[2], 0); return cmCantOpenErr; }
   gi_set_dataref(gi, a[0], a[1]);
   put_u32(a[2], inst);
   return cmNoErr;
}

/* ComponentResult GetGraphicsImporterForFile(const FSSpec *spec,
 *                                            ComponentInstance *gi); */
uint32_t shim_GetGraphicsImporterForFile(uint32_t *a)
{
   QTLOG("GetGraphicsImporterForFile(spec=%08x) — FSSpec unsupported\n", a[0]);
   put_u32(a[1], 0);
   return cmParamErr;
}

/* ---- QuickDraw GWorld / PixMap ------------------------------------------ */

static uint32_t new_gworld(uint32_t outPtr, int w, int h, int rowBytes,
                           void *base, int owns)
{
   if (w <= 0 || h <= 0) { put_u32(outPtr, 0); return cmParamErr; }
   if (rowBytes <= 0) rowBytes = (w * 4 + 15) & ~15;
   gw_state *gw = (gw_state *)calloc(1, sizeof(gw_state));
   if (!gw) { put_u32(outPtr, 0); return cmMemFullErr; }
   gw->magic = GW_MAGIC;
   gw->width = w; gw->height = h; gw->depth = 32; gw->rowBytes = rowBytes;
   if (base) { gw->base = base; gw->owns_base = 0; }
   else      { gw->base = calloc(1, (size_t)rowBytes * h); gw->owns_base = 1; }
   if (!gw->base) { free(gw); put_u32(outPtr, 0); return cmMemFullErr; }
   (void)owns;
   put_u32(outPtr, to_i386(gw));
   QTLOG("NewGWorld %dx%d rb=%d base=%p owns=%d\n", w, h, rowBytes, gw->base, gw->owns_base);
   return cmNoErr;
}

/* QDErr NewGWorld(GWorldPtr*, short depth, const Rect*, CTabHandle, GDHandle, GWorldFlags); */
uint32_t shim_NewGWorld(uint32_t *a)
{
   QDRect *b = (QDRect *)i386_ptr(a[2]);
   int w = b ? b->right - b->left : 0;
   int h = b ? b->bottom - b->top : 0;
   return new_gworld(a[0], w, h, 0, NULL, 1);
}

/* QDErr NewGWorldFromPtr(GWorldPtr*, depth, const Rect*, CTab, GD, flags,
 *                        Ptr base, long rowBytes); */
uint32_t shim_NewGWorldFromPtr(uint32_t *a)
{
   QDRect *b = (QDRect *)i386_ptr(a[2]);
   int w = b ? b->right - b->left : 0;
   int h = b ? b->bottom - b->top : 0;
   return new_gworld(a[0], w, h, (int)a[7], i386_ptr(a[6]), 0);
}

/* OSErr QTNewGWorldFromPtr(GWorldPtr*, OSType pixFmt, const Rect*, CTab, GD,
 *                          flags, void *base, long rowBytes); */
uint32_t shim_QTNewGWorldFromPtr(uint32_t *a)
{
   QDRect *b = (QDRect *)i386_ptr(a[2]);
   int w = b ? b->right - b->left : 0;
   int h = b ? b->bottom - b->top : 0;
   return new_gworld(a[0], w, h, (int)a[7], i386_ptr(a[6]), 0);
}

/* void DisposeGWorld(GWorldPtr gw); */
uint32_t shim_DisposeGWorld(uint32_t *a)
{
   gw_state *gw = gw_from_i386(a[0]);
   if (!gw) return 0;
   if (gw->pixmap_handle) cm_dispose_handle(gw->pixmap_handle);
   if (gw->owns_base && gw->base) free(gw->base);
   gw->magic = 0;
   free(gw);
   return 0;
}

/* PixMapHandle GetGWorldPixMap(GWorldPtr gw); */
uint32_t shim_GetGWorldPixMap(uint32_t *a)
{
   gw_state *gw = gw_from_i386(a[0]);
   return gw ? make_pixmap_handle(gw) : 0;
}

/* PixMapHandle GetPortPixMap(CGrafPtr port); — a CGrafPtr is a GWorldPtr here */
uint32_t shim_GetPortPixMap(uint32_t *a)
{
   gw_state *gw = gw_from_i386(a[0]);
   return gw ? make_pixmap_handle(gw) : 0;
}

/* GDHandle GetGWorldDevice(GWorldPtr gw); */
uint32_t shim_GetGWorldDevice(uint32_t *a) { (void)a; return FAKE_GDEVICE; }

/* void GetGWorld(CGrafPtr *port, GDHandle *gd); */
uint32_t shim_GetGWorld(uint32_t *a)
{
   put_u32(a[0], tl_cur_port);
   put_u32(a[1], tl_cur_gd ? tl_cur_gd : FAKE_GDEVICE);
   return 0;
}
/* void SetGWorld(CGrafPtr port, GDHandle gd); */
uint32_t shim_SetGWorld(uint32_t *a)
{
   tl_cur_port = a[0];
   tl_cur_gd   = a[1];
   return 0;
}

/* Ptr GetPixBaseAddr(PixMapHandle pm); */
uint32_t shim_GetPixBaseAddr(uint32_t *a)
{
   gw_state *gw = gw_from_pixmap(a[0]);
   return gw ? to_i386(gw->base) : 0;
}
/* SInt16 GetPixRowBytes(PixMapHandle pm); (the "long" form returns rowBytes) */
uint32_t shim_GetPixRowBytes(uint32_t *a)
{
   gw_state *gw = gw_from_pixmap(a[0]);
   return gw ? (uint32_t)gw->rowBytes : 0;
}
/* Rect *GetPixBounds(PixMapHandle pm, Rect *bounds); */
uint32_t shim_GetPixBounds(uint32_t *a)
{
   gw_state *gw = gw_from_pixmap(a[0]);
   QDRect *r = (QDRect *)i386_ptr(a[1]);
   if (gw && r) { r->top = 0; r->left = 0; r->bottom = gw->height; r->right = gw->width; }
   return a[1];
}
/* Boolean LockPixels(PixMapHandle pm); — our pixels never move */
uint32_t shim_LockPixels(uint32_t *a) { (void)a; return 1; }
/* void UnlockPixels(PixMapHandle pm); */
uint32_t shim_UnlockPixels(uint32_t *a) { (void)a; return 0; }

/* ---- register the GraphicsImporter backend at load ---------------------- */

__attribute__((constructor))
static void quicktime_image_register(void)
{
   /* subtype 0 = serve ANY image type; ImageIO detects the real format. */
   cm_register_backend(kGraphicsImporterType, 0, 0,
                       gi_open, gi_close, "ImageIO GraphicsImporter");
}
