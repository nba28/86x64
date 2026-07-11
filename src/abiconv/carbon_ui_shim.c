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
#include <dlfcn.h>

#define PTR(n) ((void *)(uintptr_t)args[(n)])
#define UI_NO_ERR   (0)
#define UI_PARAM_ERR (-50)

typedef struct { int16_t v, h; } QDPoint;

// arena bridge + self-drawn Control Manager registry (carbon_nib_shim.c).  A
// translated ControlRef arrives as a low-4GB arena handle; unwrap to the real
// 64-bit HIView pointer before matching it against the self-drawn registry or
// forwarding to native HIToolbox.
extern uint64_t x64_objc_unwrap(uint32_t h);
extern uint32_t x64_objc_wrap(uint64_t real);
extern int sd_ctrl_set_value(void *ctrl, int32_t value);
extern int sd_ctrl_get_value(void *ctrl, int32_t *out);
#define UICTRL(n) ((void *)(uintptr_t)x64_objc_unwrap(args[(n)]))
#define UIDL(fn, ret, a) static ret (*fn) a; if (!fn) fn = (ret (*) a)dlsym(RTLD_DEFAULT, #fn)

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

// ---- Control Manager (classic 16/32-bit value controls) ----
void     shim_Draw1Control(uint32_t *args)           { (void)args; }
void     shim_SetControl32BitMaximum(uint32_t *args) { (void)args; }
void     shim_SetControlMaximum(uint32_t *args)      { (void)args; }
uint32_t shim_GetControlPopupMenuHandle(uint32_t *a) { (void)a; return 0; }  // MenuRef NULL

// SetControl32BitValue(ControlRef, SInt32 value): for a self-drawn popup this
// selects the pushed menu item; for a real (checkbox/radio) control forward to
// native so its state and appearance update.  (The old no-op silently dropped the
// value Halo pushes at window-load — every popup showed item 0.)
void shim_SetControl32BitValue(uint32_t *args) {
    void *c = UICTRL(0);
    int32_t v = (int32_t)args[1];
    if (sd_ctrl_set_value(c, v)) return;
    UIDL(SetControl32BitValue, void, (void *, int32_t));
    if (SetControl32BitValue && c) SetControl32BitValue(c, v);
}

// GetControl32BitValue(ControlRef): current value (popup selection / checkbox state).
uint32_t shim_GetControl32BitValue(uint32_t *args) {
    void *c = UICTRL(0);
    int32_t out = 0;
    if (sd_ctrl_get_value(c, &out)) return (uint32_t)out;
    UIDL(GetControl32BitValue, int32_t, (void *));
    if (GetControl32BitValue && c) return (uint32_t)GetControl32BitValue(c);
    return 0;
}

// ---- Appearance / Theme text: REAL CoreText implementations moved to
// carbon_themetext_shim.c (DrawThemeTextBox / DrawThemeText /
// GetThemeTextDimensions) — the old no-ops here left text-shaped holes. ----

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
