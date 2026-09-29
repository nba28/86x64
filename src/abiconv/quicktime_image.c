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
 * (the standard pre-ImageIO API — iLife, countless Carbon apps) is served.
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
#include "gap.h"

extern void *malloc(size_t);
extern void *calloc(size_t, size_t);
extern void  free(void *);

/* The GWorld/PixMap offscreen substrate lives in qd_gworld.c (CG-backed). The
 * GraphicsImporter renders into a bound GWorld/port via this shared accessor
 * (port handle 0 => the current port). */
extern int qd_port_pixels(uint32_t port_h, void **base, int *rowBytes, int *w, int *h);

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
   }
}

/* ---- the DRAW: decode into the bound GWorld ----------------------------- */

static cm_result gi_draw(gi_state *gi)
{
   void *base = NULL; int rowBytes = 0, gw_w = 0, gw_h = 0;
   if (!qd_port_pixels(gi->gw, &base, &rowBytes, &gw_w, &gw_h) || !base) {
      return cmParamErr;
   }

   CGImageSourceRef s = gi_source(gi);
   if (!s) { return cmParamErr; }
   CGImageRef img = CGImageSourceCreateImageAtIndex(s, 0, NULL);
   if (!img) { return cmCantOpenErr; }

   CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
   /* ARGB, 8bpc, big-endian 32-bit word == classic k32ARGBPixelFormat byte
    * order (A,R,G,B) that a 32-bit QuickDraw GWorld holds. The port's own
    * CGContext uses AlphaNoneSkipFirst; here we build a decode context over the
    * SAME low-4GB buffer with premultiplied alpha so PNG/etc. composite. */
   CGContextRef ctx = CGBitmapContextCreate(
      base, gw_w, gw_h, 8, rowBytes, cs,
      kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Big);
   CGColorSpaceRelease(cs);
   if (!ctx) { CGImageRelease(img); return cmMemFullErr; }

   /* Destination rect (default: whole GWorld). QuickDraw is top-left origin,
    * CoreGraphics bottom-left -> flip vertically. */
   double dx = 0, dy = 0, dw = gw_w, dh = gw_h;
   if (gi->have_dest) {
      dx = gi->destRect.left;
      dy = gi->destRect.top;
      dw = gi->destRect.right  - gi->destRect.left;
      dh = gi->destRect.bottom - gi->destRect.top;
   }
   CGContextTranslateCTM(ctx, 0, gw_h);
   CGContextScaleCTM(ctx, 1, -1);
   CGContextDrawImage(ctx, CGRectMake(dx, dy, dw, dh), img);
   CGContextFlush(ctx);

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
   return cmNoErr;
}

/* ComponentResult GraphicsImportSetGWorld(ci, CGrafPtr port, GDHandle gd); */
uint32_t shim_GraphicsImportSetGWorld(uint32_t *a)
{
   gi_state *gi = gi_from_i386(a[0]);
   if (!gi) return cmBadComponentType;
   gi->gw = a[1];
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
uint32_t shim_GraphicsImportSetMatrix(uint32_t *a) { GAP_STUB(a); return cmNoErr; }
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
uint32_t shim_GraphicsImportGetMetaData(uint32_t *a) { GAP_STUB(a); return cmCantOpenErr; }

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
   put_u32(a[1], 0);
   return cmParamErr;
}

/* The QuickDraw GWorld / PixMap entry points (NewGWorld, GetGWorldPixMap,
 * GetPixBaseAddr, LockPixels, SetGWorld, ...) live in qd_gworld.c, which
 * implements them as CG-backed offscreen ports. The GraphicsImporter above
 * renders into those ports via qd_port_pixels(). */

/* ---- register the GraphicsImporter backend at load ---------------------- */

__attribute__((constructor))
static void quicktime_image_register(void)
{
   /* subtype 0 = serve ANY image type; ImageIO detects the real format. */
   cm_register_backend(kGraphicsImporterType, 0, 0,
                       gi_open, gi_close, "ImageIO GraphicsImporter");
}
