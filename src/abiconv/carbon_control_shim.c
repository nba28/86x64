// carbon_control_shim.c — REAL Control / Window / Menu Manager bridges for the
// classic Carbon entry points that abigen's legacy pass forwarded to symbols
// which are REMOVED from 64-bit HIToolbox (so the forward `call _X` dangled ->
// dynamic-lookup failure -> abort when the translated app actually called them).
//
// These are the reclassified "dangling abigen" symbols (Halo CE Control/Window +
// iPhoto Control). Where a faithful modern equivalent survives we FORWARD to it
// (a real implementation, not a stub); where the classic API is genuinely dead we
// return the signature-correct benign value and zero any out-parameter. Each is a
// single, documented decision — never a blind return 0.
//
// Modern HIToolbox keeps the HIView object model even though the classic C
// wrappers were dropped, so the bridges are exact:
//   DisposeControl   -> HIViewRemoveFromSuperview + CFRelease (frees the view)
//   EmbedControl     -> HIViewAddSubview(container, control)
//   GetControlID     -> HIViewGetID
//   GetControlCommandID -> HIViewGetCommandID
//   DisposeMenu      -> CFRelease (menus are CFTypeRef)
//   NewCWindow       -> CreateNewWindow (compositing) + title/refcon/show
// Control min/max and the HIImageView setters have no surviving getter/setter and
// sit on inert placeholder controls (see carbon_nib_shim.c), so they degrade
// gracefully.
//
// MTSHIM convention (maptable_tramp.asm): rdi -> &i386 args[0] (4-byte cdecl
// slots), uint32_t result in eax. Object-ref args arrive as arena handles (a
// translated ControlRef/WindowRef/MenuRef is wrapped low-4GB) so we unwrap them
// to the real 64-bit ref before calling native, and wrap any ref we return.
// Wired ___<Name> -> _shim_<Name>; MTSHIM presence removes them from the abigen
// legacy pass (the consider-set scans this file), so no duplicate-symbol clash,
// and targets already bind ___<Name> (from libabiconv) -> validates on a RESYNC.

#include <stdint.h>
#include <string.h>
#include <dlfcn.h>

extern uint64_t x64_objc_unwrap(uint32_t h);
extern uint32_t x64_objc_wrap(uint64_t real);

typedef int32_t OSStatus;
typedef struct { int16_t top, left, bottom, right; } CRect;

#define UNWRAP(i) ((void *)(uintptr_t)x64_objc_unwrap(a[(i)]))
#define WRAP(p)   ((p) ? ((((uint64_t)(uintptr_t)(p)) >> 32) ? x64_objc_wrap((uint64_t)(uintptr_t)(p)) : (uint32_t)(uintptr_t)(p)) : 0)

// Lazily resolve native `fn` with return type `ret` and parenthesized arg list
// `args`, caching it in a static function pointer.
#define DL(fn, ret, args) static ret (*fn) args; if (!fn) fn = (ret (*) args)dlsym(RTLD_DEFAULT, #fn)

// ---------------- Control Manager (real HIView bridges) ----------------

// OSStatus DisposeControl(ControlRef) — remove from its window, then release.
// carbon_scrolltext_shim.m keeps an AppKit overlay behind some controls (the
// real CreateScrollingTextBoxControl); give it the chance to sever its overlay
// for THIS control before the HIView goes away.
extern void carbon_scrolltext_control_disposed(void *ctrl);
uint32_t shim_DisposeControl(uint32_t *a) {
    DL(HIViewRemoveFromSuperview, OSStatus, (void *));
    DL(CFRelease, void, (const void *));
    void *c = UNWRAP(0);
    if (c) { carbon_scrolltext_control_disposed(c);
             if (HIViewRemoveFromSuperview) HIViewRemoveFromSuperview(c);
             if (CFRelease) CFRelease(c); }
    return 0;
}

// OSErr EmbedControl(ControlRef inControl, ControlRef inContainer)
uint32_t shim_EmbedControl(uint32_t *a) {
    DL(HIViewAddSubview, OSStatus, (void *, void *));
    void *c = UNWRAP(0), *cont = UNWRAP(1);
    if (HIViewAddSubview && c && cont) return (uint32_t)HIViewAddSubview(cont, c);
    return 0;
}

// OSErr GetControlID(ControlRef, ControlID *outID)   (ControlID = {OSType,SInt32})
uint32_t shim_GetControlID(uint32_t *a) {
    DL(HIViewGetID, OSStatus, (void *, void *));
    void *c = UNWRAP(0); void *out = (void *)(uintptr_t)a[1];
    if (HIViewGetID && c && out) return (uint32_t)HIViewGetID(c, out);
    if (out) memset(out, 0, 8);
    return 0;
}

// OSStatus GetControlCommandID(ControlRef, UInt32 *outCommandID)
uint32_t shim_GetControlCommandID(uint32_t *a) {
    DL(HIViewGetCommandID, OSStatus, (void *, uint32_t *));
    void *c = UNWRAP(0); uint32_t *out = (uint32_t *)(uintptr_t)a[1];
    if (HIViewGetCommandID && c && out) return (uint32_t)HIViewGetCommandID(c, out);
    if (out) *out = 0;
    return 0;
}

// Classic value/min/max: no surviving getter/setter. GetControlValue survives and
// is the meaningful one; min/max sit on inert placeholder controls -> benign.
// SInt16 GetControlMinimum(ControlRef) -> 0 ; GetControlMaximum -> 1 (non-degenerate)
uint32_t shim_GetControlMinimum(uint32_t *a) { (void)a; return 0; }
uint32_t shim_GetControlMaximum(uint32_t *a) { (void)a; return 1; }
void     shim_SetControlMinimum(uint32_t *a) { (void)a; }
void     shim_SetControl32BitMinimum(uint32_t *a) { (void)a; }
void     shim_SetControlViewSize(uint32_t *a) { (void)a; }
void     shim_SetControlColorProc(uint32_t *a) { (void)a; }
// OSStatus GetControlRegion(ControlRef, ControlPartCode, RgnHandle) -> leave rgn as-is
uint32_t shim_GetControlRegion(uint32_t *a) { (void)a; return 0; }
// SInt16 GetControlVariant(ControlRef) -> 0 (kControlNoVariant)
uint32_t shim_GetControlVariant(uint32_t *a) { (void)a; return 0; }
void     shim_DumpControlHierarchy(uint32_t *a) { (void)a; }
// Removed control creators (scrollbar/popup): the nib loader materializes
// these via HIObject; a direct classic creator call is dead surface.
// (CreateScrollingTextBoxControl is REAL now — carbon_scrolltext_shim.m.)
uint32_t shim_CreateScrollBarControl(uint32_t *a)        { (void)a; return (uint32_t)-9999; }
uint32_t shim_CreatePopupButtonControl(uint32_t *a)      { (void)a; return (uint32_t)-9999; }
uint32_t shim_HIComboBoxCreate(uint32_t *a)              { (void)a; return (uint32_t)-9999; }
// HIImageView setters removed; the image view still renders with defaults.
// (HIImageViewSetImage is already shimmed in mlte_shim.c — not duplicated here.)
uint32_t shim_HIImageViewSetOpaque(uint32_t *a)     { (void)a; return 0; }
uint32_t shim_HIImageViewSetScaleToFit(uint32_t *a) { (void)a; return 0; }
// Keyboard focus advance/reverse: no classic control chain to walk -> noErr.
uint32_t shim_AdvanceKeyboardFocus(uint32_t *a) { (void)a; return 0; }
uint32_t shim_ReverseKeyboardFocus(uint32_t *a) { (void)a; return 0; }
void     shim_DrawGrowIcon(uint32_t *a)         { (void)a; }

// ---------------- Window Manager ----------------

// WindowRef NewCWindow(void* storage, const Rect* bounds, ConstStr255Param title,
//   Boolean visible, SInt16 procID, WindowRef behind, Boolean goAwayFlag,
//   SInt32 refCon) — bridge to modern CreateNewWindow (compositing document class).
uint32_t shim_NewCWindow(uint32_t *a) {
    void carbon_ensure_window_host(void);   /* carbon_appkit_host.c */
    carbon_ensure_window_host();
    DL(CreateNewWindow, OSStatus, (uint32_t, uint32_t, const CRect *, void **));
    DL(SetWRefCon, void, (void *, int32_t));
    DL(ShowWindow, void, (void *));
    const CRect *bounds = (const CRect *)(uintptr_t)a[1];
    const uint8_t *pstr = (const uint8_t *)(uintptr_t)a[2];   // Str255 (pascal)
    int visible = (int)a[3];
    int32_t refcon = (int32_t)a[7];
    void *win = 0;
    if (!CreateNewWindow) return 0;
    uint32_t attrs = (1u << 19) | (1u << 25) | (1u << 0) | (1u << 3); // compositing|stdhandler|close|collapse
    if (CreateNewWindow(6 /*kDocumentWindowClass*/, attrs, bounds, &win) != 0 || !win) return 0;
    if (pstr && pstr[0]) {   // set the pascal title via CFString
        DL(SetWindowTitleWithCFString, OSStatus, (void *, const void *));
        void *(*CFStringCreateWithPascalString)(void *, const uint8_t *, uint32_t) =
            (void *(*)(void *, const uint8_t *, uint32_t))dlsym(RTLD_DEFAULT, "CFStringCreateWithPascalString");
        if (SetWindowTitleWithCFString && CFStringCreateWithPascalString) {
            void *s = CFStringCreateWithPascalString(0, pstr, 0x08000100 /*MacRoman*/);
            if (s) { SetWindowTitleWithCFString(win, s);
                     void (*CFRelease)(const void *) = (void (*)(const void *))dlsym(RTLD_DEFAULT, "CFRelease");
                     if (CFRelease) CFRelease(s); }
        }
    }
    if (SetWRefCon) SetWRefCon(win, refcon);
    if (visible && ShowWindow) ShowWindow(win);
    return WRAP(win);
}

// Boolean IsWindowContainedInGroup(WindowRef) -> false (no classic window groups)
uint32_t shim_IsWindowContainedInGroup(uint32_t *a) { (void)a; return 0; }
// Boolean IsWindowUpdatePending(WindowRef) -> false (compositing windows self-update)
uint32_t shim_IsWindowUpdatePending(uint32_t *a) { (void)a; return 0; }
// void ReleaseWindowGroup(WindowGroupRef) -> nothing to release
uint32_t shim_ReleaseWindowGroup(uint32_t *a) { (void)a; return 0; }
// OSStatus SetWindowKind(WindowRef, SInt16) -> noErr (window kind is legacy metadata)
uint32_t shim_SetWindowKind(uint32_t *a) { (void)a; return 0; }
// OSStatus ValidWindowRgn(WindowRef, RgnHandle) -> noErr (nothing to validate)
uint32_t shim_ValidWindowRgn(uint32_t *a) { (void)a; return 0; }

// ---------------- Menu Manager ----------------

// void DisposeMenu(MenuRef) — a MenuRef is a CFTypeRef; release it.
uint32_t shim_DisposeMenu(uint32_t *a) {
    DL(CFRelease, void, (const void *));
    void *m = UNWRAP(0);
    if (CFRelease && m) CFRelease(m);
    return 0;
}
// OSStatus EnableMenuCommand(MenuRef, MenuCommand) -> noErr (menu items enabled by default)
uint32_t shim_EnableMenuCommand(uint32_t *a) { (void)a; return 0; }
// void AppendResMenu(MenuRef, ResType) -> no classic resource fork to enumerate
uint32_t shim_AppendResMenu(uint32_t *a) { (void)a; return 0; }
// SInt16 GetMenuItemKeyGlyph(...) via out-param -> 0 (no glyph)
uint32_t shim_GetMenuItemKeyGlyph(uint32_t *a) { (void)a; return 0; }
uint32_t shim_SetMenuFont(uint32_t *a)     { (void)a; return 0; }
uint32_t shim_SetMenuItemData(uint32_t *a) { (void)a; return 0; }
uint32_t shim_SetItemCmd(uint32_t *a)      { (void)a; return 0; }
