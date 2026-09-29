// qd_shim.c — the small, port-independent slice of the classic QuickDraw API:
// QDGlobals constant patterns, named-pixmap cursors, text measurement, coord
// conversion, and pure Point/Rect arithmetic.
//
// The STATEFUL drawing model (GWorlds/CGrafPorts, PixMaps, CopyBits, the pen/
// color/pattern/clip state machine, GDevices) is REALLY reimplemented on
// CoreGraphics in qd_gworld.c (the functionality-first rule — a no-op that
// disables real drawing is not an acceptable end state). This file keeps only
// the pieces that (a) are pure math (Point/Rect), (b) return a fixed classic
// constant (the QDGlobals bit patterns), or (c) are genuinely cosmetic/dead on
// an offscreen substrate (named-pixmap hardware cursors, text-metric stubs used
// only for layout when no real font stack is wired).
//
// Reached from translated i386-cdecl code via the MTSHIM trampoline
// (maptable_tramp.asm): rdi -> &i386 args[0] (4-byte cdecl slots), uint32_t
// result in eax. i386 pointers are 32-bit low-4GB addresses, used directly
// after zero-extension.

#include <stdint.h>
#include <string.h>
#include "gap.h"

#define PTR(n) ((void *)(uintptr_t)args[(n)])

typedef struct { int16_t top, left, bottom, right; } QDRect;
typedef struct { uint8_t pat[8]; } QDPattern;
typedef struct { int16_t ascent, descent, widMax, leading; } QDFontInfo;

// ---- Coordinate conversion ----
// ★These were identity no-ops, correct ONLY while the port origin is (0,0) —
// the offscreen GWorld case they were written for. For a real on-screen window
// (inset by its title bar and by wherever the user put it) the identity is
// wrong, and an app that converts a global mouse point to local coordinates to
// hit-test its own UI gets a screen point back and finds the pointer inside
// nothing. Now origin-aware via the shared substrate; when no window can be
// measured ci_content_origin reports failure and these degrade to the previous
// identity behaviour, which is right for an unset port origin.
// See classic_input_coords.c.
typedef struct { int16_t v, h; } QDPointCC;
extern int ci_content_origin(int16_t *ox, int16_t *oy);   // classic_input_coords.c

void shim_GlobalToLocal(uint32_t *args) {
    QDPointCC *pt = (QDPointCC *)PTR(0);
    if (!pt) return;
    int16_t ox = 0, oy = 0;
    if (!ci_content_origin(&ox, &oy)) return;
    pt->h = (int16_t)(pt->h - ox);
    pt->v = (int16_t)(pt->v - oy);
}

void shim_LocalToGlobal(uint32_t *args) {
    QDPointCC *pt = (QDPointCC *)PTR(0);
    if (!pt) return;
    int16_t ox = 0, oy = 0;
    if (!ci_content_origin(&ox, &oy)) return;
    pt->h = (int16_t)(pt->h + ox);
    pt->v = (int16_t)(pt->v + oy);
}

// ---- Random-seed setter: no live QDGlobals to mutate.
void shim_SetQDGlobalsRandomSeed(uint32_t *a) { GAP_STUB(a); }

// ---- Text: without a wired classic font stack we do not raster QD text (apps
// draw real UI text through ATSU/CoreText). Provide well-defined metrics so
// measuring callers don't divide by zero; DrawText is a no-op raster.
void shim_DrawText(uint32_t *args) { GAP_STUB(args); }
void shim_GetFontInfo(uint32_t *args) {
    QDFontInfo *fi = (QDFontInfo *)PTR(0);
    if (fi) { fi->ascent = 12; fi->descent = 3; fi->widMax = 8; fi->leading = 1; }
}
uint32_t shim_TextWidth(uint32_t *args) { GAP_STUB(args); return 0; }

// ---- Pictures (PICT record/replay): recording a PICT is not supported; report
// failure / no-op disposal. (PICT *decode* — DrawPicture — is bridged to ImageIO
// where a real substrate is available; not here.)
uint32_t shim_OpenCPicture(uint32_t *args) { GAP_STUB(args); return 0; }
void     shim_ClosePicture(uint32_t *args) { GAP_STUB(args); }
void     shim_KillPicture(uint32_t *args)  { (void)args; }
void     shim_DisposeCTable(uint32_t *args) { (void)args; }

// ---- QDGlobals constant patterns: fill the caller's Pattern with the real bit
// patterns (exact classic values, not a guess).
static void fill_pat(uint32_t *args, const uint8_t p[8]) {
    QDPattern *out = (QDPattern *)PTR(0); if (out) memcpy(out->pat, p, 8);
}
uint32_t shim_GetQDGlobalsWhite(uint32_t *args)     { static const uint8_t p[8]={0,0,0,0,0,0,0,0};                         fill_pat(args,p); return args[0]; }
uint32_t shim_GetQDGlobalsBlack(uint32_t *args)     { static const uint8_t p[8]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; fill_pat(args,p); return args[0]; }
uint32_t shim_GetQDGlobalsGray(uint32_t *args)      { static const uint8_t p[8]={0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55}; fill_pat(args,p); return args[0]; }
uint32_t shim_GetQDGlobalsLightGray(uint32_t *args) { static const uint8_t p[8]={0x88,0x22,0x88,0x22,0x88,0x22,0x88,0x22}; fill_pat(args,p); return args[0]; }
uint32_t shim_GetQDGlobalsDarkGray(uint32_t *args)  { static const uint8_t p[8]={0x77,0xDD,0x77,0xDD,0x77,0xDD,0x77,0xDD}; fill_pat(args,p); return args[0]; }

// ---- Named-pixmap cursors (custom hardware cursors): cosmetic; report success/no-op.
uint32_t shim_QDRegisterNamedPixMapCursor(uint32_t *a)   { GAP_STUB(a); return 0; }  // noErr
uint32_t shim_QDSetNamedPixMapCursor(uint32_t *a)        { GAP_STUB(a); return 0; }
uint32_t shim_QDUnregisterNamedPixMapCursur(uint32_t *a) { GAP_STUB(a); return 0; }  // (sic) Apple typo
uint32_t shim_QDIsNamedPixMapCursorRegistered(uint32_t *a) { GAP_STUB(a); return 0; } // Boolean false

// ---- Point geometry (REAL implementations — exact classic math, not stubs) ----
// These are pure arithmetic on Point/Rect; abigen cannot emit them because a
// by-value Point ({SInt16 v,h} = ONE 4-byte i386 arg slot) is neither of its
// supported by-value struct shapes (integer-long / homogeneous-FP). The packed
// slot's memory image is {v @+0, h @+2}, so as a little-endian uint32:
// low 16 bits = v, high 16 bits = h — identical to the x86_64 SysV packing.
typedef struct { int16_t v, h; } QDPointG;
#define PT_V(a) ((int16_t)((a) & 0xffffu))
#define PT_H(a) ((int16_t)((a) >> 16))

// AddPt/SubPt(Point src, Point *dst)
void shim_AddPt(uint32_t *args) {
    QDPointG *dst = (QDPointG *)PTR(1); if (!dst) return;
    dst->v = (int16_t)(dst->v + PT_V(args[0]));
    dst->h = (int16_t)(dst->h + PT_H(args[0]));
}
void shim_SubPt(uint32_t *args) {
    QDPointG *dst = (QDPointG *)PTR(1); if (!dst) return;
    dst->v = (int16_t)(dst->v - PT_V(args[0]));
    dst->h = (int16_t)(dst->h - PT_H(args[0]));
}
// Boolean EqualPt(Point pt1, Point pt2): packed 4-byte images compare exactly.
uint32_t shim_EqualPt(uint32_t *args) { return args[0] == args[1]; }

// Boolean PtInRect(Point pt, const Rect *r): in iff top<=v<bottom, left<=h<right.
uint32_t shim_PtInRect(uint32_t *args) {
    const QDRect *r = (const QDRect *)PTR(1); if (!r) return 0;
    int16_t v = PT_V(args[0]), h = PT_H(args[0]);
    return (v >= r->top && v < r->bottom && h >= r->left && h < r->right) ? 1 : 0;
}

// long PinRect(const Rect *theRect, Point thePt) — pins the point inside the
// rect (right/bottom pin at edge-1). The RESULT packing is the classic register
// convention and is the OPPOSITE of the arg-slot image: HIGH word = v, LOW word
// = h (Inside Macintosh; callers unpack with HiWord/LoWord).
uint32_t shim_PinRect(uint32_t *args) {
    const QDRect *r = (const QDRect *)PTR(0);
    int16_t v = PT_V(args[1]), h = PT_H(args[1]);
    if (r) {
        if (v < r->top)         v = r->top;
        if (v >= r->bottom)     v = (int16_t)(r->bottom - 1);
        if (h < r->left)        h = r->left;
        if (h >= r->right)      h = (int16_t)(r->right - 1);
    }
    return ((uint32_t)(uint16_t)v << 16) | (uint16_t)h;
}
