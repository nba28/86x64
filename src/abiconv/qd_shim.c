// qd_shim.c — graceful shims for the dead classic QuickDraw 2D API.
//
// Color QuickDraw (the GrafPort/CGrafPtr drawing model: RGBBackColor, FillRect, CopyBits,
// the QDGlobals patterns, GDevice enumeration, named-pixmap cursors, QuickDraw Text) was
// removed wholesale from 64-bit/modern macOS — the symbols are gone from ApplicationServices
// and their declarations were stripped from <QD/Quickdraw.h>. Civ IV (an Aspyr Carbon/C++
// port) still references ~60 of them, so its translated `__jt_ptrs` slots fail to bind at
// dyld LOAD ("Symbol not found: _RGBBackColor, Expected in ApplicationServices").
//
// Civ IV's real rendering is OpenGL/AGL; the QuickDraw surface is legacy UI/offscreen/cursor
// glue that has no meaning without the (dead) GrafPort model. So:
//   * void state-setters that mutate a GrafPort (color/pen/text/clip/origin) -> no-op
//     (there is no port; the OpenGL path is unaffected).
//   * fresh-handle creators (CreateNewPort, GetMainDevice, OpenCPicture, ...) -> NULL
//     (the canonical "could not create" value; callers null-check).
//   * out-param getters (GetForeColor, GetQDGlobalsWhite, GetPortBounds, ...) -> fill the
//     caller's buffer with a benign-but-valid value (NOT left uninitialized: that is the
//     silent-corruption trap), then return it where the real API returns the pointer.
// Each return value is derived from the function's 10.6-SDK signature, never a blind 0.
//
// Reached from translated i386-cdecl code via the MTSHIM trampoline (maptable_tramp.asm):
// rdi -> &i386 args[0] (4-byte cdecl slots), uint32_t result in eax. i386 pointers are
// 32-bit low-4GB addresses, used directly after zero-extension.

#include <stdint.h>
#include <string.h>

#define PTR(n) ((void *)(uintptr_t)args[(n)])

typedef struct { int16_t top, left, bottom, right; } QDRect;
typedef struct { uint16_t red, green, blue; } QDRGBColor;
typedef struct { uint8_t pat[8]; } QDPattern;
typedef struct { int16_t ascent, descent, widMax, leading; } QDFontInfo;
// PenState {Point pnLoc; Point pnSize; short pnMode; Pattern pnPat;} = 4+4+2+8 = 18 bytes
typedef struct { int16_t pnLocV, pnLocH, pnSizeV, pnSizeH, pnMode; uint8_t pnPat[8]; } QDPenState;

// ---- Color: setting the fore/back color of a (nonexistent) port is a no-op ----
void shim_RGBForeColor(uint32_t *args)         { (void)args; }
void shim_RGBBackColor(uint32_t *args)         { (void)args; }
void shim_ForeColor(uint32_t *args)            { (void)args; }
void shim_BackColor(uint32_t *args)            { (void)args; }
void shim_SetQDGlobalsRandomSeed(uint32_t *a)  { (void)a; }

// Color getters: report opaque black foreground / white background so callers that read
// the current color get a sane, fully-initialized value rather than stack garbage.
static void fill_black(QDRGBColor *c) { if (c) { c->red = c->green = c->blue = 0x0000; } }
static void fill_white(QDRGBColor *c) { if (c) { c->red = c->green = c->blue = 0xFFFF; } }
void shim_GetForeColor(uint32_t *args)     { fill_black((QDRGBColor *)PTR(0)); }
void shim_GetBackColor(uint32_t *args)     { fill_white((QDRGBColor *)PTR(0)); }
uint32_t shim_GetPortForeColor(uint32_t *args) { fill_black((QDRGBColor *)PTR(1)); return args[1]; }
uint32_t shim_GetPortBackColor(uint32_t *args) { fill_white((QDRGBColor *)PTR(1)); return args[1]; }

// ---- Pen / text drawing state: no-op (no port) ----
void shim_PenSize(uint32_t *args)   { (void)args; }
void shim_PenNormal(uint32_t *args) { (void)args; }
void shim_MoveTo(uint32_t *args)    { (void)args; }
void shim_LineTo(uint32_t *args)    { (void)args; }
void shim_TextFont(uint32_t *args)  { (void)args; }
void shim_TextSize(uint32_t *args)  { (void)args; }
void shim_TextFace(uint32_t *args)  { (void)args; }
void shim_DrawText(uint32_t *args)  { (void)args; }
void shim_SetOrigin(uint32_t *args) { (void)args; }

void shim_GetPenState(uint32_t *args) {
    QDPenState *ps = (QDPenState *)PTR(0);
    if (ps) { memset(ps, 0, sizeof(*ps)); ps->pnSizeV = ps->pnSizeH = 1; memset(ps->pnPat, 0xFF, 8); }
}
void shim_SetPenState(uint32_t *args) { (void)args; }

// FontInfo getter: fill a small sane metric block so text-measuring callers don't divide by
// zero. TextWidth -> 0 (zero-width: we don't draw, but the measurement is well-defined).
void shim_GetFontInfo(uint32_t *args) {
    QDFontInfo *fi = (QDFontInfo *)PTR(0);
    if (fi) { fi->ascent = 12; fi->descent = 3; fi->widMax = 8; fi->leading = 1; }
}
uint32_t shim_TextWidth(uint32_t *args)     { (void)args; return 0; }
uint32_t shim_GetPortTextFont(uint32_t *a)  { (void)a; return 0; }
uint32_t shim_GetPortTextFace(uint32_t *a)  { (void)a; return 0; }

// ---- Rect / region drawing: no-op ----
void shim_FillRect(uint32_t *args)  { (void)args; }
void shim_FrameRect(uint32_t *args) { (void)args; }
void shim_PaintRect(uint32_t *args) { (void)args; }
void shim_ClipRect(uint32_t *args)  { (void)args; }
void shim_SetClip(uint32_t *args)   { (void)args; }
void shim_CopyBits(uint32_t *args)  { (void)args; }
// GetClip copies the current clip into the caller's (already-allocated) region handle; with
// no port there is nothing to copy, so leave the handle as-is (a no-op, not a corruption).
void shim_GetClip(uint32_t *args)   { (void)args; }

// Point coordinate conversion: with no port, local == global, so identity (leave pt). This
// is the correct degenerate behavior, not a guess.
void shim_GlobalToLocal(uint32_t *args) { (void)args; }
void shim_LocalToGlobal(uint32_t *args) { (void)args; }

// ---- Ports: cannot create a GrafPort; report failure (NULL) ----
uint32_t shim_CreateNewPort(uint32_t *args)              { (void)args; return 0; }
uint32_t shim_CreateNewPortForCGDisplayID(uint32_t *a)   { (void)a; return 0; }
void     shim_DisposePort(uint32_t *args)                { (void)args; }
void     shim_SetPort(uint32_t *args)                    { (void)args; }
void     shim_SetPortBounds(uint32_t *args)              { (void)args; }
void     shim_GetPort(uint32_t *args) { uint32_t *out = (uint32_t *)PTR(0); if (out) *out = 0; }
uint32_t shim_GetPortBitMapForCopyBits(uint32_t *a)      { (void)a; return 0; }
// GetPortBounds(port, Rect*) fills + returns the rect; default to a 1024x768 origin rect so
// layout callers get non-degenerate dimensions.
uint32_t shim_GetPortBounds(uint32_t *args) {
    QDRect *r = (QDRect *)PTR(1);
    if (r) { r->top = 0; r->left = 0; r->bottom = 768; r->right = 1024; }
    return args[1];
}
// Region getters return the passed-in region handle unchanged.
uint32_t shim_GetPortClipRegion(uint32_t *args)    { return args[1]; }
uint32_t shim_GetPortVisibleRegion(uint32_t *args) { return args[1]; }

// ---- CGContext bridge: no CGrafPtr to wrap; report failure ----
#define QD_PARAM_ERR (-50)   // paramErr
uint32_t shim_CreateCGContextForPort(uint32_t *args) {
    uint32_t *out = (uint32_t *)PTR(1); if (out) *out = 0; return (uint32_t)QD_PARAM_ERR;
}

// ---- Pictures: cannot record a PICT; report failure / no-op disposal ----
uint32_t shim_OpenCPicture(uint32_t *args) { (void)args; return 0; }
void     shim_ClosePicture(uint32_t *args) { (void)args; }
void     shim_KillPicture(uint32_t *args)  { (void)args; }
void     shim_DisposeCTable(uint32_t *args) { (void)args; }

// ---- QDGlobals constant patterns: fill the caller's Pattern with the real bit patterns ----
static void fill_pat(uint32_t *args, const uint8_t p[8]) {
    QDPattern *out = (QDPattern *)PTR(0); if (out) memcpy(out->pat, p, 8);
}
uint32_t shim_GetQDGlobalsWhite(uint32_t *args)     { static const uint8_t p[8]={0,0,0,0,0,0,0,0};                         fill_pat(args,p); return args[0]; }
uint32_t shim_GetQDGlobalsBlack(uint32_t *args)     { static const uint8_t p[8]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; fill_pat(args,p); return args[0]; }
uint32_t shim_GetQDGlobalsGray(uint32_t *args)      { static const uint8_t p[8]={0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55}; fill_pat(args,p); return args[0]; }
uint32_t shim_GetQDGlobalsLightGray(uint32_t *args) { static const uint8_t p[8]={0x88,0x22,0x88,0x22,0x88,0x22,0x88,0x22}; fill_pat(args,p); return args[0]; }
uint32_t shim_GetQDGlobalsDarkGray(uint32_t *args)  { static const uint8_t p[8]={0x77,0xDD,0x77,0xDD,0x77,0xDD,0x77,0xDD}; fill_pat(args,p); return args[0]; }

// ---- GDevice enumeration: no classic GDevices; report none ----
uint32_t shim_GetMainDevice(uint32_t *args)       { (void)args; return 0; }
uint32_t shim_GetDeviceList(uint32_t *args)       { (void)args; return 0; }
uint32_t shim_GetNextDevice(uint32_t *args)       { (void)args; return 0; }
void     shim_SetGDevice(uint32_t *args)          { (void)args; }
uint32_t shim_TestDeviceAttribute(uint32_t *a)    { (void)a; return 0; }   // Boolean false

// ---- Named-pixmap cursors (custom hardware cursors): cosmetic; report success/no-op ----
uint32_t shim_QDRegisterNamedPixMapCursor(uint32_t *a)   { (void)a; return 0; }  // noErr
uint32_t shim_QDSetNamedPixMapCursor(uint32_t *a)        { (void)a; return 0; }
uint32_t shim_QDUnregisterNamedPixMapCursur(uint32_t *a) { (void)a; return 0; }  // (sic) Apple typo
uint32_t shim_QDIsNamedPixMapCursorRegistered(uint32_t *a) { (void)a; return 0; } // Boolean false
