// carbon_nib_shim.c — REAL IBCarbon NIB window/control materialization.
//
// PROBLEM (Halo CE / Civ IV / iPhoto, live gap): a translated i386 app loads its
// Carbon interface from an IBCarbon .nib via CreateNibReference + CreateWindowFromNib.
// Modern macOS still ships HIToolbox's IBCarbonRuntime, and the abigen legacy pass
// forwards these calls to it — BUT that native runtime has been gutted for 64-bit:
//
//   1. Non-compositing window records fail: CreateNewWindow(kDocumentWindowClass,
//      <no kWindowCompositingAttribute>, ...) returns -5601
//      (errUnsupportedWindowAttributesForClass). Halo's Graphics/EULA/Calibration
//      windows carry no compositing attribute -> CreateWindowFromNib returns NULL,
//      so those windows never appear (lost functionality).
//   2. Dropped control classes: IBCarbonRuntime no longer maps IBCarbonEditText (and
//      others) to a real control, logging "Using a UserPane for unregistered class:
//      IBCarbonEditText" and substituting an inert UserPane. The C creators for edit
//      text / user pane / group box / popup / separator were removed from HIToolbox
//      (CreateEditUnicodeTextControl, CreateUserPaneControl, ... all ABSENT); only
//      RootControl/StaticText/PushButton/CheckBox/Icon creators survive, plus a few
//      HIObject classes (HIPopupButton, HISlider, base hiview) via HIObjectCreate.
//
// FIX (real functionality, universal): intercept CreateNibReference[WithCFBundle] to
// remember each nib's on-disk objects.xib path, and intercept CreateWindowFromNib to
//   (a) try native IBCarbonRuntime first — for compositing windows it works, so we
//       return the genuine native window+controls with ZERO behavior change; else
//   (b) fall back to our OWN nib reader: parse objects.xib, CreateNewWindow with
//       kWindowCompositingAttribute forced on (fixes -5601), and build every control
//       with the best surviving modern API (native creators where present, HIObject
//       for popup/slider, a base hiview for the removed edit-text/user-pane/group-box
//       — a graceful, visible placeholder), wiring each control's ControlID and
//       HICommand from the nib so the app's Carbon event handlers still find them.
//
// This trades the classic gutted path for real modern controls and, crucially, makes
// the previously-NULL settings windows materialize. It is UNIVERSAL: any translated
// i386 app that loads a Carbon nib benefits (triggers on the CreateWindowFromNib
// structure, never on an app name). Removed edit-text is genuinely dead surface on
// modern macOS (Cocoa-only), so a base-view placeholder is the faithful best effort;
// everything else regains real functionality.
//
// MTSHIM convention (maptable_tramp.asm): rdi -> &i386 args[0] (4-byte cdecl slots),
// uint32_t (OSStatus) result in eax. Wired ___<Name> -> _shim_<Name>; these five nib
// symbols are removed from the abigen legacy pass by their MTSHIM presence (the
// legacy consider-set scans maptable_tramp.asm), so there is no duplicate-symbol
// collision. Since translated targets already bind ___CreateWindowFromNib
// (from libabiconv), swapping the implementation validates on a RESYNC, no retranslate.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <CoreFoundation/CoreFoundation.h>
#include "carbon_nib_parse.h"   // pure, headless-testable nib XML reader

// arena bridge (objc_shim.c): i386 handle / i386 CFSTR constant <-> real 64-bit ptr.
extern uint64_t x64_objc_unwrap(uint32_t h);
extern uint32_t x64_objc_wrap(uint64_t real);

// AppKit window-host bootstrap (carbon_appkit_host.c): without it a pure-Carbon
// translated app has no WindowManagement delegate — windows half-bridge, then
// AppKit panics in NSCGSWindow _createContext inside the app's own event loop.
void carbon_ensure_window_host(void);

typedef void *WindowRef, *ControlRef, *HIViewRef, *HIObjectRef, *IBNibRef;
typedef int32_t OSStatus;
typedef struct { int16_t top, left, bottom, right; } CRect;
typedef struct { uint32_t signature; int32_t id; } CtrlID;
typedef struct { uint32_t attributes; uint32_t commandID; } HICommandLite;
typedef struct { double x, y, w, h; } HIRectD;

// ---- window attribute bits (MacWindows.h) ----
#define kWinCompositing   (1u << 19)
#define kWinStdHandler    (1u << 25)
#define kWinCloseBox      (1u << 0)
#define kWinHZoom         (1u << 1)
#define kWinVZoom         (1u << 2)
#define kWinCollapseBox   (1u << 3)
#define kWinResizable     (1u << 4)

// ---- lazily-resolved native entry points (all via dlsym: header-gated / removed
//      C wrappers are still live symbols in the shared HIToolbox) ----
static OSStatus (*n_CreateNibReference)(CFStringRef, IBNibRef *);
static OSStatus (*n_CreateNibReferenceWithCFBundle)(CFBundleRef, CFStringRef, IBNibRef *);
static OSStatus (*n_CreateWindowFromNib)(IBNibRef, CFStringRef, WindowRef *);
static OSStatus (*n_DisposeNibReference)(IBNibRef);
static OSStatus (*n_SetMenuBarFromNib)(IBNibRef, CFStringRef);
static OSStatus (*n_CreateNewWindow)(uint32_t, uint32_t, const CRect *, WindowRef *);
static OSStatus (*n_SetWindowTitleWithCFString)(WindowRef, CFStringRef);
static OSStatus (*n_CreateRootControl)(WindowRef, ControlRef *);
static ControlRef (*n_HIViewGetRoot)(WindowRef);
static OSStatus (*n_CreateStaticTextControl)(WindowRef, const CRect *, CFStringRef, const void *, ControlRef *);
static OSStatus (*n_CreatePushButtonControl)(WindowRef, const CRect *, CFStringRef, ControlRef *);
static OSStatus (*n_CreateCheckBoxControl)(WindowRef, const CRect *, CFStringRef, int32_t, uint8_t, ControlRef *);
static OSStatus (*n_CreateIconControl)(WindowRef, const CRect *, const void *, uint8_t, ControlRef *);
static OSStatus (*n_HIObjectCreate)(CFStringRef, void *, HIObjectRef *);
static OSStatus (*n_HIViewAddSubview)(HIViewRef, HIViewRef);
static OSStatus (*n_HIViewSetFrame)(HIViewRef, const HIRectD *);
static OSStatus (*n_SetControlID)(ControlRef, const CtrlID *);
static OSStatus (*n_SetControlCommandID)(ControlRef, uint32_t);
static CFBundleRef (*n_CFBundleGetMainBundle)(void);
static uint32_t (*n_GetAvailableWindowAttributes)(uint32_t);

static int g_resolved;
static void resolve_once(void) {
    if (g_resolved) return;
    g_resolved = 1;
#define R(f) n_##f = (void *)dlsym(RTLD_DEFAULT, #f)
    R(CreateNibReference); R(CreateNibReferenceWithCFBundle); R(CreateWindowFromNib);
    R(DisposeNibReference); R(SetMenuBarFromNib); R(CreateNewWindow);
    R(SetWindowTitleWithCFString); R(CreateRootControl); R(HIViewGetRoot);
    R(CreateStaticTextControl); R(CreatePushButtonControl); R(CreateCheckBoxControl);
    R(CreateIconControl); R(HIObjectCreate); R(HIViewAddSubview); R(HIViewSetFrame);
    R(SetControlID); R(SetControlCommandID); R(CFBundleGetMainBundle);
    R(GetAvailableWindowAttributes);
#undef R
}

// ---- nibref -> objects.xib absolute path table (small, single-threaded UI) ----
#define NIB_MAX 32
static struct { IBNibRef ref; char xib[1024]; } g_nibs[NIB_MAX];
static void nib_record(IBNibRef ref, const char *xib) {
    for (int i = 0; i < NIB_MAX; i++)
        if (g_nibs[i].ref == NULL || g_nibs[i].ref == ref) {
            g_nibs[i].ref = ref;
            snprintf(g_nibs[i].xib, sizeof g_nibs[i].xib, "%s", xib);
            return;
        }
}
static const char *nib_lookup(IBNibRef ref) {
    for (int i = 0; i < NIB_MAX; i++)
        if (g_nibs[i].ref == ref) return g_nibs[i].xib;
    return NULL;
}

// Resolve <nibName>.nib/objects.xib inside a CFBundle -> absolute path.
static int resolve_xib(CFBundleRef bundle, CFStringRef nibName, char *out, int outsz) {
    if (!bundle || !nibName) return 0;
    CFURLRef u = CFBundleCopyResourceURL(bundle, nibName, CFSTR("nib"), NULL);
    if (!u) return 0;
    char dir[1024];
    Boolean ok = CFURLGetFileSystemRepresentation(u, true, (UInt8 *)dir, sizeof dir);
    CFRelease(u);
    if (!ok) return 0;
    snprintf(out, outsz, "%s/objects.xib", dir);
    return 1;
}

// ============================ nib XML mini-parser ============================
// The pure, framework-free nib reader lives in carbon_nib_parse.h (headless-unit-
// tested). CRect is byte-identical to nibx_rect, so we parse straight into it.
static CFStringRef cfs(const char *s) {
    return CFStringCreateWithCString(NULL, s ? s : "", kCFStringEncodingUTF8);
}

// Set ControlID + HICommand on a freshly-built control from its nib metadata so the
// app's Carbon event handlers (GetControlByID / command dispatch) still resolve it.
static void wire_ids(const char *x, long lo, long hi, ControlRef c) {
    if (!c) return;
    uint32_t sig = 0, cmd = 0; int cid = 0;
    int have_sig = nibx_ostype(x, lo, hi, "controlSignature", &sig);
    int have_id  = nibx_int(x, lo, hi, "controlID", &cid);
    int have_cmd = nibx_ostype(x, lo, hi, "command", &cmd);
    if ((have_sig || have_id) && n_SetControlID) {
        CtrlID id = { have_sig ? sig : cmd, cid };
        n_SetControlID(c, &id);
    }
    if (have_cmd && n_SetControlCommandID) n_SetControlCommandID(c, cmd);
}

static void set_frame(ControlRef c, const CRect *r) {
    if (c && n_HIViewSetFrame) {
        HIRectD fr = { r->left, r->top, r->right - r->left, r->bottom - r->top };
        n_HIViewSetFrame(c, &fr);
    }
}

// Build every control directly under [lo,hi) and embed into `parent`.
static void build_children(const char *x, long lo, long hi, ControlRef parent, WindowRef win) {
    long p = lo;
    while (p < hi) {
        const char *o = strstr(x + p, "<object "); if (!o || o - x >= hi) break;
        long os = o - x;
        const char *cp = strstr(o, "class=\""); if (!cp) break; cp += 7;
        const char *ce = strchr(cp, '"'); if (!ce) break;
        char cls[64]; int cn = (int)(ce - cp); if (cn > 63) cn = 63; memcpy(cls, cp, cn); cls[cn] = 0;
        long oe = nibx_match_end(x, os); if (oe < 0 || oe > hi) oe = hi;
        long il = ce - x, ih = oe;

        char bs[64] = ""; CRect r = { 0, 0, 0, 0 }; if (nibx_str(x, il, ih, "bounds", bs, sizeof bs)) nibx_rect_parse(bs, (nibx_rect *)&r);
        char title[512] = ""; if (nibx_str(x, il, ih, "title", title, sizeof title)) nibx_unescape(title);
        ControlRef c = NULL;

        if (!strcmp(cls, "IBCarbonRootControl")) {           // recurse into root's kids
            build_children(x, il, ih, parent, win); p = oe; continue;
        } else if (!strcmp(cls, "IBCarbonMenu") || !strcmp(cls, "IBCarbonMenuItem") ||
                   !strcmp(cls, "IBCarbonHILayoutInfo")) {   // not view controls
            p = oe; continue;
        } else if (!strcmp(cls, "IBCarbonStaticText")) {
            n_CreateStaticTextControl(win, &r, cfs(title), NULL, &c);
        } else if (!strcmp(cls, "IBCarbonButton")) {
            n_CreatePushButtonControl(win, &r, cfs(title), &c);
        } else if (!strcmp(cls, "IBCarbonCheckBox")) {
            n_CreateCheckBoxControl(win, &r, cfs(title), 0, 1, &c);
        } else if (!strcmp(cls, "IBCarbonIcon")) {
            n_CreateIconControl(win, &r, NULL, 0, &c);
        } else if (!strcmp(cls, "IBCarbonPopupButton")) {
            if (n_HIObjectCreate) n_HIObjectCreate(CFSTR("com.apple.HIPopupButton"), NULL, (HIObjectRef *)&c);
            set_frame(c, &r);
        } else if (!strcmp(cls, "IBCarbonGroupBox") || !strcmp(cls, "IBCarbonUserPane")) {
            // removed control class -> base hiview container (holds children, draws nothing)
            if (n_HIObjectCreate) n_HIObjectCreate(CFSTR("com.apple.hiview"), NULL, (HIObjectRef *)&c);
            set_frame(c, &r);
            wire_ids(x, il, ih, c);
            if (c && parent) n_HIViewAddSubview(parent, c);
            build_children(x, il, ih, c ? c : parent, win);   // its kids embed in it
            p = oe; continue;
        } else if (!strcmp(cls, "IBCarbonEditText") || !strcmp(cls, "IBCarbonSeparator") ||
                   !strcmp(cls, "IBCarbonRelevanceBar") || !strcmp(cls, "IBCarbonLittleArrows")) {
            // genuinely-removed surface -> base hiview placeholder (visible, inert)
            if (n_HIObjectCreate) n_HIObjectCreate(CFSTR("com.apple.hiview"), NULL, (HIObjectRef *)&c);
            set_frame(c, &r);
        } else {
            p = oe; continue;                                 // unknown: skip subtree
        }

        if (c) { wire_ids(x, il, ih, c); if (parent) n_HIViewAddSubview(parent, c); }
        p = oe;
    }
}

// Custom-build window `wname` from objects.xib. Returns a WindowRef or NULL.
static WindowRef build_window(const char *xibpath, CFStringRef wname) {
    char wc[256]; if (!CFStringGetCString(wname, wc, sizeof wc, kCFStringEncodingUTF8)) return NULL;
    long len; char *x = nibx_slurp(xibpath, &len); if (!x) return NULL;
    WindowRef win = NULL;

    int id = nibx_nametable_id(x, wc);
    long ws = id >= 0 ? nibx_window_offset(x, id) : -1;
    if (ws < 0) { free(x); return NULL; }
    long we = nibx_match_end(x, ws); if (we < 0) we = len;

    char wr[64] = ""; CRect R = { 0, 0, 0, 0 };
    nibx_str(x, ws, we, "windowRect", wr, sizeof wr); nibx_rect_parse(wr, (nibx_rect *)&R);
    char title[256] = ""; if (nibx_str(x, ws, we, "title", title, sizeof title)) nibx_unescape(title);
    int wclass = 6 /*kDocumentWindowClass*/; nibx_int(x, ws, we, "carbonWindowClass", &wclass);

    // Decoration attributes come from the nib's window booleans (IB writes the
    // non-default FALSE values out explicitly; absent = classic default ON for
    // close/collapse). CRUCIALLY, they must then be masked to what the window
    // CLASS supports: kMovableModalWindowClass(4) — Halo's EULA / Graphics /
    // Calibration windows — rejects every decoration bit, and CreateNewWindow
    // fails the whole window with -5601 errUnsupportedWindowAttributesForClass
    // (the old unconditional closeBox|collapseBox meant those windows NEVER
    // materialized). GetAvailableWindowAttributes(class) is the exact native
    // mask; the bare retry below covers its absence.
    uint32_t attrs = kWinCompositing | kWinStdHandler;
    int b;
    if (!nibx_bool(x, ws, we, "hasCloseBox", &b) || b)        attrs |= kWinCloseBox;
    if (!nibx_bool(x, ws, we, "hasCollapseBox", &b) || b)     attrs |= kWinCollapseBox;
    if (nibx_bool(x, ws, we, "isResizable", &b) && b)         attrs |= kWinResizable;
    if (nibx_bool(x, ws, we, "hasHorizontalZoom", &b) && b)   attrs |= kWinHZoom;
    if (nibx_bool(x, ws, we, "hasVerticalZoom", &b) && b)     attrs |= kWinVZoom;
    if (n_GetAvailableWindowAttributes)
        attrs &= n_GetAvailableWindowAttributes((uint32_t)wclass)
                 | kWinCompositing | kWinStdHandler;
    if (!n_CreateNewWindow) { free(x); return NULL; }
    if (n_CreateNewWindow((uint32_t)wclass, attrs, &R, &win) != 0 || !win) {
        // last-resort: a bare compositing window of that class (still -5601?
        // then the class itself is gone; give up)
        win = NULL;
        if (n_CreateNewWindow((uint32_t)wclass, kWinCompositing | kWinStdHandler,
                              &R, &win) != 0 || !win) {
            free(x); return NULL;
        }
    }
    if (n_SetWindowTitleWithCFString) n_SetWindowTitleWithCFString(win, cfs(title));

    ControlRef root = NULL;
    if (!n_CreateRootControl || n_CreateRootControl(win, &root) != 0 || !root)
        root = n_HIViewGetRoot ? n_HIViewGetRoot(win) : NULL;

    const char *rc = strstr(x + ws, "class=\"IBCarbonRootControl\"");
    if (rc && rc - x < we) {
        long rs = rc - x; while (rs > ws && strncmp(x + rs, "<object ", 8)) rs--;
        const char *gt = strchr(x + rs, '>'); long rlo = gt ? gt - x + 1 : rs;
        long rend = nibx_match_end(x, rs); if (rend < 0 || rend > we) rend = we;
        build_children(x, rlo, rend, root, win);
    }
    free(x);
    return win;
}

// ============================ MTSHIM entry points ============================

// OSStatus CreateNibReference(CFStringRef inNibName, IBNibRef *outNibRef)
uint32_t shim_CreateNibReference(uint32_t *a) {
    resolve_once();
    carbon_ensure_window_host();
    if (!n_CreateNibReference) return (uint32_t)-108;
    CFStringRef name = (CFStringRef)(uintptr_t)x64_objc_unwrap(a[0]);
    IBNibRef ref = NULL;
    OSStatus st = n_CreateNibReference(name, &ref);
    if (st == 0 && ref) {
        char xib[1024];
        CFBundleRef mb = n_CFBundleGetMainBundle ? n_CFBundleGetMainBundle() : NULL;
        if (resolve_xib(mb, name, xib, sizeof xib)) nib_record(ref, xib);
    }
    uint32_t *out = (uint32_t *)(uintptr_t)a[1];
    if (out) *out = ref ? x64_objc_wrap((uint64_t)(uintptr_t)ref) : 0;
    return (uint32_t)st;
}

// OSStatus CreateNibReferenceWithCFBundle(CFBundleRef, CFStringRef, IBNibRef*)
uint32_t shim_CreateNibReferenceWithCFBundle(uint32_t *a) {
    resolve_once();
    carbon_ensure_window_host();
    if (!n_CreateNibReferenceWithCFBundle) return (uint32_t)-108;
    CFBundleRef bundle = (CFBundleRef)(uintptr_t)x64_objc_unwrap(a[0]);
    CFStringRef name   = (CFStringRef)(uintptr_t)x64_objc_unwrap(a[1]);
    IBNibRef ref = NULL;
    OSStatus st = n_CreateNibReferenceWithCFBundle(bundle, name, &ref);
    if (st == 0 && ref) {
        char xib[1024];
        if (resolve_xib(bundle, name, xib, sizeof xib)) nib_record(ref, xib);
    }
    uint32_t *out = (uint32_t *)(uintptr_t)a[2];
    if (out) *out = ref ? x64_objc_wrap((uint64_t)(uintptr_t)ref) : 0;
    return (uint32_t)st;
}

// OSStatus CreateWindowFromNib(IBNibRef, CFStringRef inName, WindowRef *outWindow)
uint32_t shim_CreateWindowFromNib(uint32_t *a) {
    resolve_once();
    carbon_ensure_window_host();
    IBNibRef ref  = (IBNibRef)(uintptr_t)x64_objc_unwrap(a[0]);
    CFStringRef wn = (CFStringRef)(uintptr_t)x64_objc_unwrap(a[1]);
    uint32_t *out = (uint32_t *)(uintptr_t)a[2];

    WindowRef w = NULL;
    OSStatus st = n_CreateWindowFromNib ? n_CreateWindowFromNib(ref, wn, &w) : (OSStatus)-108;

    if ((st != 0 || !w)) {                       // native gutted path failed (e.g. -5601)
        const char *xib = nib_lookup(ref);
        if (xib && wn) {
            WindowRef bw = build_window(xib, wn);
            if (bw) { w = bw; st = 0; }
        }
    }
    if (out) *out = w ? x64_objc_wrap((uint64_t)(uintptr_t)w) : 0;
    return (uint32_t)st;
}

// OSStatus DisposeNibReference(IBNibRef) — forward; drop our path record.
uint32_t shim_DisposeNibReference(uint32_t *a) {
    resolve_once();
    IBNibRef ref = (IBNibRef)(uintptr_t)x64_objc_unwrap(a[0]);
    for (int i = 0; i < NIB_MAX; i++) if (g_nibs[i].ref == ref) g_nibs[i].ref = NULL;
    return n_DisposeNibReference ? (uint32_t)n_DisposeNibReference(ref) : 0;
}

// OSStatus SetMenuBarFromNib(IBNibRef, CFStringRef) — forward to native (menu bar
// classes survive; if it ever fails we still return native's status).
uint32_t shim_SetMenuBarFromNib(uint32_t *a) {
    resolve_once();
    IBNibRef ref = (IBNibRef)(uintptr_t)x64_objc_unwrap(a[0]);
    CFStringRef mn = (CFStringRef)(uintptr_t)x64_objc_unwrap(a[1]);
    return n_SetMenuBarFromNib ? (uint32_t)n_SetMenuBarFromNib(ref, mn) : (uint32_t)-108;
}
