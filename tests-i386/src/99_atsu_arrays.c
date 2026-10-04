/*
 * 99_atsu_arrays — libabiconv's own ATSUI subset on CoreText
 * (src/abiconv/atsu_shim.c). Native ATSUI is blocked on macOS 14+ (the first
 * real call traps and the OS shows an "Update Required" dialog), so Portal 2's
 * vgui fonts (COSXFont::Create / GetCharRGBA) need this exact sequence served
 * without it; unbridged, ATSUSetAttributes bound raw and returned through a
 * fused 8-byte slot (rip 0x8080998_80e3a2677).
 *
 *  ATSUSetAttributes   ByteCount[] and value-pointer[] are 4 bytes per entry
 *  ATSUCreateTextLayoutWithTextPtr   UniCharCount run lengths, style handles
 *  ATSUDirectGet...    ATSLayoutRecord is 14 bytes here (18 native)
 *  ATSUGlyphGetIdealMetrics   glyph IDs read with stride 14
 *
 *  ATSUDrawText       draws white glyphs into a CGBitmapContext (pixels set)
 *
 * Exit 42 = all right. 10 + step = the step that failed.
 */
#include <stdint.h>
#include <stdlib.h>

typedef int32_t OSStatus;
typedef void *ATSUStyle, *ATSUTextLayout;
#pragma pack(push, 2)
typedef struct { uint16_t glyph; uint32_t flags; uint32_t offset; int32_t realPos; } Rec;
#pragma pack(pop)
typedef struct { float advx, advy, sbx, sby, osbx, osby; } Ideal;

extern OSStatus ATSUCreateStyle(ATSUStyle *);
extern OSStatus ATSUSetAttributes(ATSUStyle, uint32_t, const uint32_t *, const uint32_t *, void *const *);
extern OSStatus ATSUCreateTextLayoutWithTextPtr(const uint16_t *, uint32_t, uint32_t, uint32_t, uint32_t,
                                                const uint32_t *, ATSUStyle *, ATSUTextLayout *);
extern OSStatus ATSUDirectGetLayoutDataArrayPtrFromTextLayout(ATSUTextLayout, uint32_t, uint32_t,
                                                              void **, uint32_t *);
extern OSStatus ATSUDirectReleaseLayoutDataArrayPtr(void *, uint32_t, void **);
extern OSStatus ATSUGlyphGetIdealMetrics(ATSUStyle, uint32_t, uint16_t *, uint32_t, Ideal *);
extern OSStatus ATSUDisposeTextLayout(ATSUTextLayout);
extern OSStatus ATSUSetLayoutControls(ATSUTextLayout, uint32_t, const uint32_t *, const uint32_t *, void *const *);
extern OSStatus ATSUDrawText(ATSUTextLayout, uint32_t, uint32_t, int32_t, int32_t);
extern void *CGColorSpaceCreateDeviceRGB(void);
extern void *CGBitmapContextCreate(void *, uint32_t, uint32_t, uint32_t, uint32_t, void *, uint32_t);

int main(void) {
   ATSUStyle st = 0;
   if (ATSUCreateStyle(&st) != 0 || !st) exit(10);

   int32_t size = 24 << 16;                 /* kATSUSizeTag: Fixed */
   uint8_t bold = 1;                        /* kATSUQDBoldfaceTag: Boolean */
   float white[4] = {1, 1, 1, 1};           /* kATSURGBAlphaColorTag */
   uint32_t tags[3] = {262, 256, 288}, sizes[3] = {4, 1, 16};
   void *vals[3] = {&size, &bold, white};
   if (ATSUSetAttributes(st, 3, tags, sizes, vals) != 0) exit(11);

   uint16_t text[3] = {'A', 'B', 'C'};
   uint32_t run = 3;
   ATSUTextLayout lay = 0;
   if (ATSUCreateTextLayoutWithTextPtr(text, 0, 3, 3, 1, &run, &st, &lay) != 0 || !lay) exit(12);

   ATSUTextLayout lay2 = 0;                 /* kATSUFromTextBeginning / kATSUToTextEnd */
   uint32_t to_end = 0xffffffffu;
   if (ATSUCreateTextLayoutWithTextPtr(text, 0xffffffffu, 0xffffffffu, 3, 1, &to_end, &st, &lay2) != 0 || !lay2) exit(23);
   ATSUDisposeTextLayout(lay2);

   Rec *recs = 0; uint32_t n = 0;
   if (ATSUDirectGetLayoutDataArrayPtrFromTextLayout(lay, 0, 100, (void **)&recs, &n) != 0 || !recs) exit(13);
   if (n < 4) exit(14);                    /* 3 glyphs + the terminator */
   for (uint32_t i = 0; i < 4; i++)
      if (recs[i].offset != 2 * i) exit(15);   /* the 14-byte i386 layout */

   Ideal m[3];
   if (ATSUGlyphGetIdealMetrics(st, 3, &recs[0].glyph, sizeof(Rec), m) != 0) exit(16);
   for (int i = 0; i < 3; i++)
      if (!(m[i].advx > 1.0f)) exit(17);

   if (ATSUDirectReleaseLayoutDataArrayPtr(0, 100, (void **)&recs) != 0 || recs) exit(18);

   static uint8_t px[32 * 64 * 4];
   void *ctx = CGBitmapContextCreate(px, 64, 32, 8, 64 * 4, CGColorSpaceCreateDeviceRGB(), 1);
   if (!ctx) exit(19);
   uint32_t ctag = 32767, csize = 4;
   void *cval = &ctx;
   if (ATSUSetLayoutControls(lay, 1, &ctag, &csize, &cval) != 0) exit(20);
   if (ATSUDrawText(lay, 0, 3, 2 << 16, 8 << 16) != 0) exit(21);
   int lit = 0;
   for (unsigned i = 3; i < sizeof px; i += 4) lit += px[i] != 0;
   if (lit < 20) exit(22);                 /* alpha set where the glyphs went */
   ATSUDisposeTextLayout(lay);
   exit(42);
}
