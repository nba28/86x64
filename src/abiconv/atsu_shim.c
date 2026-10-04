/*
 * atsu_shim.c — the ATSUI / ATS font subset an i386 app draws text with,
 * implemented on CoreText. ONE job: be ATSUI for translated callers.
 *
 * Native ATSUI still EXPORTS every ATSU* symbol, but macOS 14+ blocks it at
 * run time: the first real call traps inside ATS and the OS shows an "Update
 * Required ... not compatible with macOS 14" dialog. So nothing here may call
 * native ATSU, and every ATSU/ATS symbol a target uses is listed in
 * custom.syms (abigen's generated bridges forward to the dead native).
 *
 * Scope = what Portal 2's vguimatsurface COSXFont reaches (font lookup and
 * metrics, style attributes, one-style layouts, DirectAccess layout records,
 * glyph metrics, drawing into a CGBitmapContext). Objects are low-4GB structs
 * handed to the caller as-is; fonts are small integer IDs (ATSFontRef ==
 * ATSUFontID here). Unknown attribute tags are LOUD (GAP) and ignored.
 *
 * Guard: tests-i386 atsu-arrays.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>
#include "gap.h"

extern uint64_t _86x64_unwrap_obj_arg(uint32_t handle);
extern uint32_t x64_objc_wrap(uint64_t real);

#define P(v) ((void *)(uintptr_t)(uint32_t)(v))
#define paramErr (-50)
#define kATSUInvalidFontErr (-8796)
#define FIX(x) ((int32_t)lround((x) * 65536.0))

/* ---- fonts: ATSFontRef = index + 1 ----------------------------------------- */
static pthread_mutex_t g_lk = PTHREAD_MUTEX_INITIALIZER;
static CTFontRef g_fonts[1024];            /* each at size 1 */
static uint32_t g_nfonts;

static uint32_t font_id(CTFontRef f) {     /* takes ownership */
   CFStringRef ps = CTFontCopyPostScriptName(f);
   pthread_mutex_lock(&g_lk);
   uint32_t id = 0;
   for (uint32_t i = 0; i < g_nfonts && !id; i++) {
      CFStringRef q = CTFontCopyPostScriptName(g_fonts[i]);
      if (CFEqual(ps, q)) id = i + 1;
      CFRelease(q);
   }
   if (!id && g_nfonts < 1024) { g_fonts[g_nfonts++] = f; id = g_nfonts; f = NULL; }
   pthread_mutex_unlock(&g_lk);
   CFRelease(ps);
   if (f) CFRelease(f);
   return id;
}
static CTFontRef font_get(uint32_t id) {
   pthread_mutex_lock(&g_lk);
   CTFontRef f = (id && id <= g_nfonts) ? g_fonts[id - 1] : NULL;
   pthread_mutex_unlock(&g_lk);
   return f;
}

static int name_matches(CTFontRef f, CFStringRef want, int ps_only) {
   CFStringRef names[3] = { CTFontCopyPostScriptName(f),
                            ps_only ? NULL : CTFontCopyFullName(f),
                            ps_only ? NULL : CTFontCopyFamilyName(f) };
   int hit = 0;
   for (int i = 0; i < 3; i++) {
      if (!names[i]) continue;
      if (CFStringCompare(names[i], want, kCFCompareCaseInsensitive) == kCFCompareEqualTo) hit = 1;
      CFRelease(names[i]);
   }
   return hit;
}

/* CoreText falls back to a default font for an unknown name; ATS says "none". */
static uint32_t find_font(uint32_t name_h, int ps_only) {
   CFStringRef name = (CFStringRef)(uintptr_t)_86x64_unwrap_obj_arg(name_h);
   if (!name) return 0;
   CTFontRef f = CTFontCreateWithName(name, 1.0, NULL);
   if (!f) return 0;
   if (!name_matches(f, name, ps_only)) { CFRelease(f); return 0; }
   return font_id(f);
}

/* ATSFontRef ATSFontFindFromName(CFStringRef, ATSOptionFlags) */
uint32_t shim_ATSFontFindFromName(uint32_t *a) { return find_font(a[0], 0); }
/* ATSFontRef ATSFontFindFromPostScriptName(CFStringRef, ATSOptionFlags) */
uint32_t shim_ATSFontFindFromPostScriptName(uint32_t *a) { return find_font(a[0], 1); }

/* OSStatus ATSFontGetName(ATSFontRef, ATSOptionFlags, CFStringRef *oName) */
uint32_t shim_ATSFontGetName(uint32_t *a) {
   uint32_t *out = P(a[2]);
   if (out) *out = 0;
   CTFontRef f = font_get(a[0]);
   if (!f || !out) return (uint32_t)paramErr;
   CFStringRef n = CTFontCopyFullName(f);
   *out = n ? x64_objc_wrap((uint64_t)(uintptr_t)n) : 0;
   return 0;
}

/* OSStatus ATSFontGetHorizontalMetrics(ATSFontRef, ATSOptionFlags, ATSFontMetrics *)
 * i386 ATSFontMetrics: UInt32 version + 14 Float32 (CGFloat), for a 1-point font. */
uint32_t shim_ATSFontGetHorizontalMetrics(uint32_t *a) {
   float *m = P(a[2]);
   CTFontRef f = font_get(a[0]);
   if (!m) return (uint32_t)paramErr;
   memset(m, 0, 60);
   if (!f) return (uint32_t)kATSUInvalidFontErr;
   CGRect bb = CTFontGetBoundingBox(f);
   UniChar x = 'x'; CGGlyph gx = 0; CGSize adv = {0, 0};
   if (CTFontGetGlyphsForCharacters(f, &x, &gx, 1)) CTFontGetAdvancesForGlyphs(f, kCTFontOrientationHorizontal, &gx, &adv, 1);
   ((uint32_t *)m)[0] = 0;                                   /* version */
   m[1] = (float)CTFontGetAscent(f);
   m[2] = (float)-CTFontGetDescent(f);                       /* ATS: negative */
   m[3] = (float)CTFontGetLeading(f);
   m[4] = (float)adv.width;                                  /* avgAdvanceWidth */
   m[5] = (float)bb.size.width;                              /* maxAdvanceWidth */
   m[6] = (float)bb.origin.x;                                /* minLeftSideBearing */
   m[7] = 0;                                                 /* minRightSideBearing */
   m[10] = (float)CTFontGetCapHeight(f);
   m[11] = (float)CTFontGetXHeight(f);
   m[12] = (float)CTFontGetSlantAngle(f);
   m[13] = (float)CTFontGetUnderlinePosition(f);
   m[14] = (float)CTFontGetUnderlineThickness(f);
   return 0;
}

/* OSStatus ATSFontActivateFromMemory(LogicalAddress, ByteCount, ATSFontContext,
 *   ATSFontFormat, void *, ATSOptionFlags, ATSFontContainerRef *oContainer) */
uint32_t shim_ATSFontActivateFromMemory(uint32_t *a) {
   static uint32_t next_container = 1;
   uint32_t *out = P(a[6]);
   if (out) *out = 0;
   CFDataRef d = CFDataCreate(NULL, P(a[0]), (CFIndex)a[1]);
   CGDataProviderRef dp = d ? CGDataProviderCreateWithCFData(d) : NULL;
   CGFontRef cg = dp ? CGFontCreateWithDataProvider(dp) : NULL;
   int ok = cg && CTFontManagerRegisterGraphicsFont(cg, NULL);
   if (cg) CFRelease(cg);
   if (dp) CFRelease(dp);
   if (d) CFRelease(d);
   if (!ok) return (uint32_t)paramErr;
   if (out) *out = __atomic_fetch_add(&next_container, 1, __ATOMIC_RELAXED);
   return 0;
}

/* ---- styles ------------------------------------------------------------------ */
#define STYLE_MAGIC 0x41545355u   /* 'ATSU' */
#define LAYOUT_MAGIC 0x4154534cu  /* 'ATSL' */
struct style {
   uint32_t magic, font; int32_t size;      /* Fixed points */
   uint8_t bold, italic, underline;
   float rgba[4];
   CTFontRef ct;                            /* built lazily, dropped on change */
};
struct layout {
   uint32_t magic, len;
   UniChar *text;
   struct style *style;
   CGContextRef ctx;
};

static struct style *style_of(uint32_t h) {
   struct style *s = P(h);
   return (s && s->magic == STYLE_MAGIC) ? s : NULL;
}
static struct layout *layout_of(uint32_t h) {
   struct layout *l = P(h);
   return (l && l->magic == LAYOUT_MAGIC) ? l : NULL;
}

static CTFontRef style_font(struct style *s) {
   if (s->ct) return s->ct;
   CGFloat pt = s->size ? s->size / 65536.0 : 12.0;
   CTFontRef base = font_get(s->font);
   CTFontRef f = base ? CTFontCreateCopyWithAttributes(base, pt, NULL, NULL)
                      : CTFontCreateWithName(CFSTR("Helvetica"), pt, NULL);
   CTFontSymbolicTraits want = (s->bold ? kCTFontTraitBold : 0) | (s->italic ? kCTFontTraitItalic : 0);
   if (f && want) {
      CTFontRef t = CTFontCreateCopyWithSymbolicTraits(f, pt, NULL, want, want);
      if (t) { CFRelease(f); f = t; }
   }
   s->ct = f;
   return f;
}

/* OSStatus ATSUCreateStyle(ATSUStyle *oStyle) */
uint32_t shim_ATSUCreateStyle(uint32_t *a) {
   uint32_t *out = P(a[0]);
   if (!out) return (uint32_t)paramErr;
   struct style *s = calloc(1, sizeof *s);                 /* libabiconv calloc: low heap */
   s->magic = STYLE_MAGIC; s->size = 12 << 16;
   s->rgba[3] = 1.0f;                                     /* opaque black, ATSU's default */
   *out = (uint32_t)(uintptr_t)s;
   return 0;
}

/* OSStatus ATSUSetAttributes(ATSUStyle, ItemCount, const ATSUAttributeTag[],
 *   const ByteCount[], const ATSUAttributeValuePtr[])  — all 4-byte i386 arrays */
uint32_t shim_ATSUSetAttributes(uint32_t *a) {
   struct style *s = style_of(a[0]);
   if (!s) return (uint32_t)paramErr;
   const uint32_t *tags = P(a[2]), *sizes = P(a[3]), *vals = P(a[4]);
   for (uint32_t i = 0; i < a[1]; i++) {
      const uint8_t *v = P(vals[i]);
      if (!v) continue;
      switch (tags[i]) {
      case 261: memcpy(&s->font, v, 4); break;                      /* kATSUFontTag */
      case 262: memcpy(&s->size, v, 4); break;                      /* kATSUSizeTag */
      case 256: s->bold = v[0]; break;                              /* kATSUQDBoldfaceTag */
      case 257: s->italic = v[0]; break;                            /* kATSUQDItalicTag */
      case 258: s->underline = v[0]; break;                         /* kATSUQDUnderlineTag */
      case 288: if (sizes[i] >= 16) memcpy(s->rgba, v, 16); break;  /* kATSURGBAlphaColorTag */
      case 283: break;   /* kATSUStyleRenderingOptionsTag: CoreText antialiases by the context's settings */
      default: x64_gap_hit("ATSU", "style attribute tag", 0, tags[i]); break;
      }
   }
   if (s->ct) { CFRelease(s->ct); s->ct = NULL; }
   return 0;
}

/* ---- layouts ----------------------------------------------------------------- */
/* OSStatus ATSUCreateTextLayoutWithTextPtr(ConstUniCharArrayPtr, UniCharArrayOffset,
 *   UniCharCount, UniCharCount total, ItemCount nRuns, const UniCharCount runLengths[],
 *   ATSUStyle styles[], ATSUTextLayout *oLayout) */
uint32_t shim_ATSUCreateTextLayoutWithTextPtr(uint32_t *a) {
   uint32_t *out = P(a[7]);
   if (out) *out = 0;
   const uint32_t *styles = P(a[6]);
   if (!out) return (uint32_t)paramErr;
   if (a[4] > 1) x64_gap_hit("ATSU", "multi-style text layout (first style used)", 0, a[4]);
   struct layout *l = calloc(1, sizeof *l);
   l->magic = LAYOUT_MAGIC;
   /* kATSUFromTextBeginning / kATSUToTextEnd (0xFFFFFFFF) are relative to the
    * total length (Portal 2 GetKernedCharWidth passes both) */
   uint32_t off = a[1] == 0xffffffffu ? 0 : a[1];
   uint32_t len = a[2] == 0xffffffffu ? a[3] - off : a[2];
   if (off > a[3] || len > a[3] - off) { free(l); return (uint32_t)paramErr; }
   l->len = len;
   l->text = malloc((l->len ? l->len : 1) * sizeof(UniChar));
   memcpy(l->text, (const UniChar *)P(a[0]) + off, l->len * sizeof(UniChar));
   l->style = (styles && a[4]) ? style_of(styles[0]) : NULL;
   *out = (uint32_t)(uintptr_t)l;
   return 0;
}

/* OSStatus ATSUSetLayoutControls(ATSUTextLayout, ItemCount, tags[], sizes[], values[]) */
uint32_t shim_ATSUSetLayoutControls(uint32_t *a) {
   struct layout *l = layout_of(a[0]);
   if (!l) return (uint32_t)paramErr;
   const uint32_t *tags = P(a[2]), *vals = P(a[4]);
   for (uint32_t i = 0; i < a[1]; i++) {
      const uint32_t *v = P(vals[i]);
      if (!v) continue;
      switch (tags[i]) {
      case 32767: l->ctx = (CGContextRef)(uintptr_t)(*v ? _86x64_unwrap_obj_arg(*v) : 0); break;
      case 1: case 5: break;   /* line width / flush factor: single-line glyph runs, no justification */
      default: x64_gap_hit("ATSU", "layout control tag", 0, tags[i]); break;
      }
   }
   return 0;
}

/* OSStatus ATSUSetTransientFontMatching(ATSUTextLayout, Boolean) */
uint32_t shim_ATSUSetTransientFontMatching(uint32_t *a) {
   return layout_of(a[0]) ? 0 : (uint32_t)paramErr;
}

/* OSStatus ATSUDisposeTextLayout(ATSUTextLayout) */
uint32_t shim_ATSUDisposeTextLayout(uint32_t *a) {
   struct layout *l = layout_of(a[0]);
   if (!l) return (uint32_t)paramErr;
   l->magic = 0;
   free(l->text);
   free(l);
   return 0;
}

/* glyphs + advances of a layout's text in its style's font */
static uint32_t shape(struct layout *l, CGGlyph *g, CGSize *adv) {
   CTFontRef f = l->style ? style_font(l->style) : NULL;
   if (!f) return 0;
   CTFontGetGlyphsForCharacters(f, l->text, g, l->len);
   CTFontGetAdvancesForGlyphs(f, kCTFontOrientationHorizontal, g, adv, l->len);
   return 1;
}

/* OSStatus ATSUDrawText(ATSUTextLayout, UniCharArrayOffset, UniCharCount, Fixed x, Fixed y) */
uint32_t shim_ATSUDrawText(uint32_t *a) {
   struct layout *l = layout_of(a[0]);
   if (!l || !l->ctx || !l->style || l->len > 4096) return (uint32_t)paramErr;
   uint32_t off = a[1] == 0xffffffffu ? 0 : a[1];
   uint32_t n = a[2] == 0xffffffffu ? l->len - off : a[2];
   if (off > l->len || n > l->len - off) return (uint32_t)paramErr;
   CGGlyph g[4096]; CGSize adv[4096]; CGPoint pos[4096];
   if (!shape(l, g, adv)) return (uint32_t)kATSUInvalidFontErr;
   CGFloat x = (int32_t)a[3] / 65536.0, y = (int32_t)a[4] / 65536.0;
   for (uint32_t i = 0; i < off; i++) x += adv[i].width;
   for (uint32_t i = 0; i < n; i++) { pos[i] = CGPointMake(x, y); x += adv[off + i].width; }
   const float *c = l->style->rgba;
   CGContextSetRGBFillColor(l->ctx, c[0], c[1], c[2], c[3]);
   CTFontDrawGlyphs(style_font(l->style), g + off, pos, n, l->ctx);
   /* ponytail: kATSUQDUnderlineTag is recorded but not drawn; draw a rule here if a target shows one */
   return 0;
}

/* ---- DirectAccess ------------------------------------------------------------ */
#pragma pack(push, 2)
struct rec32 { uint16_t glyph; uint32_t flags; uint32_t offset; int32_t realPos; };  /* i386 ATSLayoutRecord */
#pragma pack(pop)
_Static_assert(sizeof(struct rec32) == 14, "i386 ATSLayoutRecord");

/* OSStatus ATSUDirectGetLayoutDataArrayPtrFromTextLayout(ATSUTextLayout,
 *   UniCharArrayOffset line, ATSUDirectDataSelector, void *oArray[], ItemCount *oCount) */
uint32_t shim_ATSUDirectGetLayoutDataArrayPtrFromTextLayout(uint32_t *a) {
   uint32_t *optr = P(a[3]), *ocount = P(a[4]);
   if (optr) *optr = 0;
   if (ocount) *ocount = 0;
   struct layout *l = layout_of(a[0]);
   if (!l || !optr || l->len > 4096) return (uint32_t)paramErr;
   if (a[2] != 100) {           /* kATSUDirectDataLayoutRecordATSLayoutRecordCurrent */
      x64_gap_hit("ATSU", "DirectAccess selector", 0, a[2]);
      return (uint32_t)paramErr;
   }
   CGGlyph g[4096]; CGSize adv[4096];
   if (!shape(l, g, adv)) return (uint32_t)kATSUInvalidFontErr;
   struct rec32 *r = malloc((l->len + 1) * sizeof *r);   /* low heap */
   CGFloat x = 0;
   for (uint32_t i = 0; i < l->len; i++) {
      r[i] = (struct rec32){ g[i], 0, 2 * i, FIX(x) };
      x += adv[i].width;
   }
   r[l->len] = (struct rec32){ 0xffff, 0x00080000 /* kATSGlyphInfoTerminatorGlyph */, 2 * l->len, FIX(x) };
   *optr = (uint32_t)(uintptr_t)r;
   if (ocount) *ocount = l->len + 1;
   return 0;
}

/* OSStatus ATSUDirectReleaseLayoutDataArrayPtr(ATSULineRef, ATSUDirectDataSelector, void *iArray[]) */
uint32_t shim_ATSUDirectReleaseLayoutDataArrayPtr(uint32_t *a) {
   uint32_t *slot = P(a[2]);
   if (!slot) return (uint32_t)paramErr;
   free(P(*slot));
   *slot = 0;
   return 0;
}

/* ---- glyph metrics: IDs read with the caller's byte stride ---------------------- */
static int glyph_boxes(uint32_t *a, CTFontRef *f, CGGlyph *g, CGSize *adv, CGRect *bb) {
   struct style *s = style_of(a[0]);
   uint32_t n = a[1], stride = a[3] ? a[3] : 2;
   if (!s || n > 4096 || !(*f = style_font(s))) return 0;
   const uint8_t *p = P(a[2]);
   for (uint32_t i = 0; i < n; i++) memcpy(&g[i], p + (size_t)i * stride, 2);
   CTFontGetAdvancesForGlyphs(*f, kCTFontOrientationHorizontal, g, adv, n);
   CTFontGetBoundingRectsForGlyphs(*f, kCTFontOrientationHorizontal, g, bb, n);
   return 1;
}

/* OSStatus ATSUGlyphGetIdealMetrics(ATSUStyle, ItemCount, GlyphID[], ByteCount stride,
 *   ATSGlyphIdealMetrics[])  — {advance, sideBearing, otherSideBearing} Float32Points */
uint32_t shim_ATSUGlyphGetIdealMetrics(uint32_t *a) {
   CTFontRef f; CGGlyph g[4096]; CGSize adv[4096]; CGRect bb[4096];
   if (!glyph_boxes(a, &f, g, adv, bb)) return (uint32_t)paramErr;
   float *m = P(a[4]);
   for (uint32_t i = 0; i < a[1]; i++, m += 6) {
      m[0] = (float)adv[i].width; m[1] = 0;
      m[2] = (float)bb[i].origin.x; m[3] = 0;
      m[4] = (float)(adv[i].width - CGRectGetMaxX(bb[i])); m[5] = 0;
   }
   return 0;
}

/* OSStatus ATSUGlyphGetScreenMetrics(ATSUStyle, ItemCount, GlyphID[], ByteCount stride,
 *   Boolean forceAA, Boolean aaSwitch, ATSGlyphScreenMetrics[])
 *   — {deviceAdvance, topLeft} Float32Points, UInt32 height, width,
 *     {sideBearing, otherSideBearing} Float32Points: 40 bytes */
uint32_t shim_ATSUGlyphGetScreenMetrics(uint32_t *a) {
   CTFontRef f; CGGlyph g[4096]; CGSize adv[4096]; CGRect bb[4096];
   if (!glyph_boxes(a, &f, g, adv, bb)) return (uint32_t)paramErr;
   uint8_t *m = P(a[6]);
   for (uint32_t i = 0; i < a[1]; i++, m += 40) {
      CGFloat l = floor(CGRectGetMinX(bb[i])), r = ceil(CGRectGetMaxX(bb[i]));
      CGFloat b = floor(CGRectGetMinY(bb[i])), t = ceil(CGRectGetMaxY(bb[i]));
      if (CGRectIsEmpty(bb[i])) l = r = b = t = 0;
      float adv_x = (float)round(adv[i].width);
      float v[4] = { adv_x, 0, (float)l, (float)t };
      uint32_t hw[2] = { (uint32_t)(t - b), (uint32_t)(r - l) };
      float sb[4] = { (float)l, 0, (float)(adv_x - r), 0 };
      memcpy(m, v, 16); memcpy(m + 16, hw, 8); memcpy(m + 24, sb, 16);
   }
   return 0;
}
