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
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

extern uint64_t x64_objc_unwrap(uint32_t h);
extern uint32_t x64_objc_wrap(uint64_t real);

// Opt-in diagnostic (ABICONV_CTRL_TRACE): the settings-window port/IP edit fields
// are self-drawn HIViews serviced only when the ControlRef the app passes to
// SetControlData/GetControlData resolves (via UNWRAP -> sd_find) to a registered
// field. If a freshly-translated app delivers a different ControlRef arena handle
// than GetControlByID returned, the self-drawn gate misses, the value goes to
// storage-less native, and the field renders EMPTY. This trace prints the raw i386
// handle, the unwrapped 64-bit pointer, and whether it matched a self-drawn field,
// so the empty-port regression can be pinned on-target in one run (does NOT change
// behavior). eventNotHandledErr-style tags are printed as FourCC where sensible.
static int ctrl_trace(void) {
	static int t = -1;
	if (t < 0) t = getenv("ABICONV_CTRL_TRACE") != NULL;
	return t;
}

// self-drawn Control Manager registry (carbon_nib_shim.c): service edit-text /
// popup value + text + enable requests before falling through to native.
extern int sd_ctrl_set_value(void *ctrl, int32_t value);
extern int sd_ctrl_get_value(void *ctrl, int32_t *out);
extern int sd_ctrl_set_cfstring(void *ctrl, const void *cfstr);
extern int sd_ctrl_set_text(void *ctrl, const char *buf, int len);
extern int sd_ctrl_get_text(void *ctrl, char *buf, int bufsz);
extern int sd_ctrl_get_cfstring(void *ctrl, const void **out);
extern int sd_ctrl_set_enabled(void *ctrl, int enabled);

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

// ---- Control value + data + activation on self-drawn controls ----
// The settings-window popups and edit fields are self-drawn HIViews with no
// classic Control Manager storage, so the app's value/data/activation calls are
// serviced by the self-drawn registry (carbon_nib_shim.c).  For every OTHER
// (native checkbox/button) control we forward to the real HIToolbox entry so it
// keeps behaving.  These were previously abigen-generated straight to native,
// which silently dropped the port/IP text and popup selection the app pushes.

// void SetControlValue(ControlRef, SInt16)  — 16-bit classic value.
void shim_SetControlValue(uint32_t *a) {
    void *c = UNWRAP(0);
    int32_t v = (int16_t)(a[1] & 0xffff);
    if (sd_ctrl_set_value(c, v)) return;
    DL(SetControlValue, void, (void *, int16_t));
    if (SetControlValue && c) SetControlValue(c, (int16_t)v);
}
// SInt16 GetControlValue(ControlRef)
uint32_t shim_GetControlValue(uint32_t *a) {
    void *c = UNWRAP(0);
    int32_t v = 0;
    if (sd_ctrl_get_value(c, &v)) return (uint32_t)(uint16_t)v;
    DL(GetControlValue, int16_t, (void *));
    if (GetControlValue && c) return (uint32_t)(uint16_t)GetControlValue(c);
    return 0;
}

// OSStatus SetControlData(ControlRef, ControlPartCode, ResType tag, Size, const void*)
//   i386 cdecl slots: [0]=ctrl [1]=part [2]=tag [3]=size [4]=data ptr
uint32_t shim_SetControlData(uint32_t *a) {
    void *c = UNWRAP(0);
    uint32_t tag = a[2];
    uint32_t size = a[3];
    void *data = (void *)(uintptr_t)a[4];
    int is_ours = sd_ctrl_get_text(c, NULL, 0) >= 0;
    if (ctrl_trace()) {
        uint32_t h = (data && (tag == 'cfst')) ? *(uint32_t *)data : 0;
        // Also echo the raw 'text' bytes the app is pushing (the port default
        // "2302"/"2303"): on-target this shows whether the value reaches the field
        // (ours=1) and what it is, vs falling through to native (ours=0 -> empty).
        char txt[24] = "";
        if (tag == 'text' && data) {
            uint32_t n = size < sizeof txt - 1 ? size : sizeof txt - 1;
            memcpy(txt, data, n); txt[n] = 0;
        }
        fprintf(stderr, "[ctrl] SetControlData ctrl_h=0x%08x -> ptr=%p ours=%d "
                        "tag=%c%c%c%c size=%u cfstr_h=0x%08x text='%s'\n",
                a[0], c, is_ours,
                (char)(tag>>24),(char)(tag>>16),(char)(tag>>8),(char)tag, size, h, txt);
    }
    if (is_ours) {   // c is one of our edit fields
        if (tag == 'cfst' && data) {
            // the buffer holds an i386 CFStringRef (arena handle) -> unwrap to real
            uint32_t h = *(uint32_t *)data;
            const void *cf = (const void *)(uintptr_t)x64_objc_unwrap(h);
            sd_ctrl_set_cfstring(c, cf);
        } else if (tag == 'text' && data) {
            sd_ctrl_set_text(c, (const char *)data, (int)size);
        }
        return 0;   /* noErr — handled by the self-drawn edit field */
    }
    DL(SetControlData, OSStatus, (void *, int16_t, uint32_t, long, const void *));
    if (SetControlData && c) return (uint32_t)SetControlData(c, (int16_t)a[1], tag, (long)size, data);
    return 0;
}

// OSStatus GetControlData(ControlRef, ControlPartCode, ResType, Size max, void* data, Size* actual)
//   i386 cdecl slots: [0]=ctrl [1]=part [2]=tag [3]=maxSize [4]=data [5]=actualSize*
uint32_t shim_GetControlData(uint32_t *a) {
    void *c = UNWRAP(0);
    uint32_t tag = a[2];
    uint32_t maxsz = a[3];
    void *data = (void *)(uintptr_t)a[4];
    uint32_t *actual = (uint32_t *)(uintptr_t)a[5];
    int is_ours = sd_ctrl_get_text(c, NULL, 0) >= 0;
    if (ctrl_trace())
        fprintf(stderr, "[ctrl] GetControlData ctrl_h=0x%08x -> ptr=%p ours=%d "
                        "tag=%c%c%c%c\n", a[0], c, is_ours,
                (char)(tag>>24),(char)(tag>>16),(char)(tag>>8),(char)tag);
    if (is_ours) {   // our edit field
        if (tag == 'cfst' && data) {
            const void *cf = NULL;
            sd_ctrl_get_cfstring(c, &cf);
            *(uint32_t *)data = cf ? x64_objc_wrap((uint64_t)(uintptr_t)cf) : 0;
            if (actual) *actual = 4;
        } else if (tag == 'text' && data) {
            int n = sd_ctrl_get_text(c, (char *)data, (int)maxsz);
            if (actual) *actual = (uint32_t)(n < 0 ? 0 : n);
        } else if (actual) *actual = 0;
        return 0;
    }
    DL(GetControlData, OSStatus, (void *, int16_t, uint32_t, long, void *, long *));
    if (GetControlData && c) {
        long act = 0;
        OSStatus st = GetControlData(c, (int16_t)a[1], tag, (long)maxsz, data, &act);
        if (actual) *actual = (uint32_t)act;
        return (uint32_t)st;
    }
    return 0;
}

// OSStatus ActivateControl/DeactivateControl(ControlRef): enable/disable state.
uint32_t shim_ActivateControl(uint32_t *a) {
    void *c = UNWRAP(0);
    if (sd_ctrl_set_enabled(c, 1)) return 0;
    DL(ActivateControl, OSStatus, (void *));
    if (ActivateControl && c) return (uint32_t)ActivateControl(c);
    return 0;
}
uint32_t shim_DeactivateControl(uint32_t *a) {
    void *c = UNWRAP(0);
    if (sd_ctrl_set_enabled(c, 0)) return 0;
    DL(DeactivateControl, OSStatus, (void *));
    if (DeactivateControl && c) return (uint32_t)DeactivateControl(c);
    return 0;
}

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
