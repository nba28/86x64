// carbon_ui_shim.c — graceful shims for dead Carbon HIToolbox UI surface that Civ IV links.
//
// The classic GrafPort-coupled Window Manager / Control Manager / Appearance (Theme) calls
// and the Display Manager (DM*) were removed from 64-bit/modern macOS. Civ IV runs its real
// UI through its own OpenGL renderer; these calls are legacy chrome (window update regions,
// classic scroll/value controls, theme text, multi-display enumeration). Each shim returns
// the value its 10.6-SDK signature defines for a benign "nothing to do / not available"
// outcome, and zero-fills any out-parameter so callers never read stack garbage.
//
// MTSHIM convention: rdi -> &i386 args[0] (4-byte cdecl slots), uint32_t result in eax.

#include <stdint.h>
#include <string.h>

#define PTR(n) ((void *)(uintptr_t)args[(n)])
#define UI_NO_ERR   (0)
#define UI_PARAM_ERR (-50)

typedef struct { int16_t v, h; } QDPoint;

// ---- Window Manager update / port / refcon (no classic window): no-op or noErr ----
void     shim_BeginUpdate(uint32_t *args)       { (void)args; }
void     shim_EndUpdate(uint32_t *args)         { (void)args; }
void     shim_SetWRefCon(uint32_t *args)        { (void)args; }
void     shim_SetPortWindowPort(uint32_t *args) { (void)args; }
uint32_t shim_GetWindowPort(uint32_t *args)     { (void)args; return 0; }   // CGrafPtr NULL
uint32_t shim_GetWindowFromPort(uint32_t *args) { (void)args; return 0; }   // WindowRef NULL
uint32_t shim_InvalWindowRect(uint32_t *args)   { (void)args; return UI_NO_ERR; }
uint32_t shim_ValidWindowRect(uint32_t *args)   { (void)args; return UI_NO_ERR; }
uint32_t shim_SetWindowContentColor(uint32_t *args)        { (void)args; return UI_NO_ERR; }
uint32_t shim_SetWindowProxyCreatorAndType(uint32_t *args) { (void)args; return UI_NO_ERR; }
// GetWindowRegion(window, code, RgnHandle ioWinRgn): leaves the caller's region as-is.
uint32_t shim_GetWindowRegion(uint32_t *args)   { (void)args; return UI_NO_ERR; }

// ---- Control Manager (classic 16/32-bit value controls): no-op / zero ----
void     shim_Draw1Control(uint32_t *args)           { (void)args; }
void     shim_SetControl32BitValue(uint32_t *args)   { (void)args; }
void     shim_SetControl32BitMaximum(uint32_t *args) { (void)args; }
void     shim_SetControlMaximum(uint32_t *args)      { (void)args; }
uint32_t shim_GetControl32BitValue(uint32_t *args)   { (void)args; return 0; }
uint32_t shim_GetControlPopupMenuHandle(uint32_t *a) { (void)a; return 0; }  // MenuRef NULL

// ---- Appearance / Theme text: we don't draw; report success and sane measurements ----
uint32_t shim_DrawThemeTextBox(uint32_t *args) { (void)args; return UI_NO_ERR; }
// GetThemeTextDimensions(str, fontID, state, wrap, Point* ioBounds, SInt16* outBaseline):
// fill a non-degenerate metric so layout math (line height, baseline) stays well-defined.
uint32_t shim_GetThemeTextDimensions(uint32_t *args) {
    QDPoint *io = (QDPoint *)PTR(4);
    int16_t *baseline = (int16_t *)PTR(5);
    if (io) { if (io->h <= 0) io->h = 8; io->v = 16; }
    if (baseline) *baseline = 12;
    return UI_NO_ERR;
}

// ---- Display Manager: removed; report unavailable so callers fall back to CGDirectDisplay
// (Civ IV also links CGGetDisplaysWithPoint / CGDisplay*, which resolve natively). ----
uint32_t shim_DMGetDeskRegion(uint32_t *args) {
    uint32_t *out = (uint32_t *)PTR(0); if (out) *out = 0; return (uint32_t)UI_PARAM_ERR;
}
uint32_t shim_DMGetDisplayIDByGDevice(uint32_t *args) {
    uint32_t *out = (uint32_t *)PTR(1); if (out) *out = 0; return (uint32_t)UI_PARAM_ERR;
}
uint32_t shim_DMGetGDeviceByDisplayID(uint32_t *args) {
    uint32_t *out = (uint32_t *)PTR(1); if (out) *out = 0; return (uint32_t)UI_PARAM_ERR;
}

// ---- FindWindow / MenuSelect: FORWARD to the live native HIToolbox ----
// Both survive in the modern x86_64 HIToolbox (deprecated, header-hidden behind
// !__LP64__ — which is exactly why abigen has no prototype, and why their
// by-value Point arg is inexpressible for it anyway). A by-value Point is a
// 4-byte struct: the i386 caller passes it in one arg slot and x86_64 SysV
// packs the identical byte image into the low 32 bits of the first integer
// register, so the packed uint32 forwards VERBATIM. Resolved via dlsym (the
// modern SDK .tbd may omit 32-bit-only exports the shared-cache binary still
// carries; libabiconv links Carbon, so HIToolbox is in-process) with a
// graceful classic fallback if truly absent.
#include <dlfcn.h>
extern uint32_t x64_objc_wrap(uint64_t real);   // objc_shim.c: low-4GB handle for >4GB ptrs

// WindowPartCode FindWindow(Point thePoint, WindowRef *window)
uint32_t shim_FindWindow(uint32_t *args) {
    typedef int16_t (*fn_t)(uint32_t, void **);
    static fn_t fn; static int looked;
    if (!looked) { fn = (fn_t)dlsym(RTLD_DEFAULT, "FindWindow"); looked = 1; }
    uint32_t *out32 = (uint32_t *)PTR(1);
    if (!fn) {                       // absent: classic "hit nothing" answer
        if (out32) *out32 = 0;       //   window = NULL
        return 0;                    //   partcode = inDesk
    }
    void *win = 0;
    int32_t part = fn(args[0], &win);
    if (out32) {                     // native WindowRef may live above 4GB:
        uint64_t w = (uint64_t)(uintptr_t)win;   // wrap it into a proxy handle
        *out32 = (w >> 32) ? x64_objc_wrap(w) : (uint32_t)w;
    }
    return (uint32_t)part;
}

// SInt32 MenuSelect(Point startPt) — returns packed menuID(hi)/item(lo).
uint32_t shim_MenuSelect(uint32_t *args) {
    typedef int32_t (*fn_t)(uint32_t);
    static fn_t fn; static int looked;
    if (!looked) { fn = (fn_t)dlsym(RTLD_DEFAULT, "MenuSelect"); looked = 1; }
    if (!fn) return 0;               // absent: "no selection made"
    return (uint32_t)fn(args[0]);
}
