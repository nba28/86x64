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
#include <CoreGraphics/CoreGraphics.h>   // self-drawn group-box frame
#include <CoreText/CoreText.h>           // group-box title
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
// self-drawn popup: menu build + PopUpMenuSelect
typedef void *MenuRef;
static OSStatus (*n_CreateNewMenu)(uint16_t, uint32_t, MenuRef *);
static OSStatus (*n_AppendMenuItemTextWithCFString)(MenuRef, CFStringRef, uint32_t, uint32_t, uint16_t *);
static void     (*n_SetMenuID)(MenuRef, int16_t);
static void     (*n_InsertMenu)(MenuRef, int16_t);
static void     (*n_DeleteMenu)(int16_t);
static uint32_t (*n_PopUpMenuSelect)(MenuRef, int16_t, int16_t, int16_t);
/* GetControlEventTarget returns an EventTargetRef, which is a 64-bit POINTER,
 * not an OSStatus.  Declaring it as OSStatus (int32_t) made the compiler read
 * only %eax and truncate the real 64-bit target pointer to 32 bits; the garbage
 * low pointer was then handed to InstallEventHandler and HIToolbox's
 * PushEventHandler faulted walking the handler chain at [target+0x68]
 * (EXC_BAD_ACCESS at Halo startup when materializing settings-window popups).
 * Keep the return type a pointer so all 64 bits survive. */
static void *   (*n_GetControlEventTarget2)(ControlRef);
static OSStatus (*n_InstallEventHandler2)(void *, void *, uint32_t, const void *, void *, void *);
static OSStatus (*n_HIViewSetNeedsDisplay)(HIViewRef, uint8_t);
static OSStatus (*n_HIViewGetBounds)(HIViewRef, HIRectD *);
static OSStatus (*n_HIViewConvertPoint)(void *, HIViewRef, HIViewRef);  /* pt,from,to */
static OSStatus (*n_HIViewSetVisible)(HIViewRef, uint8_t);

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
    R(CreateNewMenu); R(AppendMenuItemTextWithCFString); R(SetMenuID);
    R(InsertMenu); R(DeleteMenu); R(PopUpMenuSelect);
    R(HIViewSetNeedsDisplay); R(HIViewGetBounds); R(HIViewConvertPoint); R(HIViewSetVisible);
#undef R
    n_GetControlEventTarget2 = (void *)dlsym(RTLD_DEFAULT, "GetControlEventTarget");
    n_InstallEventHandler2   = (void *)dlsym(RTLD_DEFAULT, "InstallEventHandler");
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

// ---- self-drawn group-box frame ----------------------------------------
// CreateGroupBoxControl and every group-box HIObject class were removed from
// 64-bit HIToolbox (probed: absent), so an IBCarbonGroupBox has no real control
// to draw its titled border. Materialize it as a base com.apple.hiview with a
// kEventControlDraw handler that strokes a rounded-rect frame and draws the
// title — so the settings window's sections are visible instead of a blank gap.
// The title text is copied into the handler's user-data (freed with the view is
// impractical here; the frame lives for the window's life, a small leak that a
// modal settings window incurs once — acceptable and bounded).
struct gbox { char title[128]; };

// (call, ev) are EventHandlerCallRef / EventRef — opaque here to avoid pulling in
// Carbon.h (this TU only has CoreFoundation). eventNotHandledErr == -9874.
static OSStatus gbox_draw(void *call, void *ev, void *ud) {
    (void)call;
    struct gbox *g = (struct gbox *)ud;
    static OSStatus (*gep)(void *, uint32_t, uint32_t, uint32_t *, unsigned long, unsigned long *, void *);
    static void *(*hvgb)(void *, void *);   /* HIViewGetBounds */
    static void *(*gcev)(void *);            /* GetControlEventTarget (unused here) */
    if (!gep) gep = (void *)dlsym(RTLD_DEFAULT, "GetEventParameter");
    if (!hvgb) hvgb = (void *)dlsym(RTLD_DEFAULT, "HIViewGetBounds");
    (void)gcev;
    CGContextRef cg = NULL;
    if (gep) gep(ev, 'cntx' /*kEventParamCGContextRef*/, 'cntx', NULL, sizeof cg, NULL, &cg);
    // the control ref (view) to query its own bounds
    void *view = NULL;
    if (gep) gep(ev, 'ctrl' /*kEventParamDirectObject*/, 'ctrl', NULL, sizeof view, NULL, &view);
    if (!cg || !view || !hvgb) return (OSStatus)-9874;  /* eventNotHandledErr */
    typedef struct { double x, y, w, h; } HR;
    HR b; ((OSStatus(*)(void *, HR *))hvgb)(view, &b);
    // frame inset a little; leave room at top for the title
    CGFloat top = 8;
    CGRect fr = CGRectMake(1, top, b.w - 2, b.h - top - 1);
    CGContextSaveGState(cg);
    CGContextSetRGBStrokeColor(cg, 0.55, 0.55, 0.58, 1.0);
    CGContextSetLineWidth(cg, 1.0);
    CGPathRef p = CGPathCreateWithRoundedRect(fr, 5, 5, NULL);
    CGContextAddPath(cg, p); CGContextStrokePath(cg); CGPathRelease(p);
    // title: punch a gap + draw label at top-left
    if (g && g->title[0]) {
        CFStringRef s = CFStringCreateWithCString(NULL, g->title, kCFStringEncodingUTF8);
        CFStringRef keys[] = { kCTFontAttributeName, kCTForegroundColorAttributeName };
        CTFontRef font = CTFontCreateWithName(CFSTR("Helvetica"), 11, NULL);
        CGColorRef col = CGColorCreateGenericRGB(0.15, 0.15, 0.18, 1);
        CFTypeRef vals[] = { font, col };
        CFDictionaryRef attr = CFDictionaryCreate(NULL, (const void **)keys, (const void **)vals, 2,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFAttributedStringRef as = CFAttributedStringCreate(NULL, s, attr);
        CTLineRef line = CTLineCreateWithAttributedString(as);
        double tw = CTLineGetTypographicBounds(line, NULL, NULL, NULL);
        // clear a gap in the frame stroke behind the title
        CGContextSetRGBFillColor(cg, 0.93, 0.93, 0.93, 1.0);
        CGContextFillRect(cg, CGRectMake(10, top - 6, tw + 8, 13));
        CGContextSetTextPosition(cg, 14, top - 4);
        CTLineDraw(line, cg);
        CFRelease(line); CFRelease(as); CFRelease(attr); CFRelease(col);
        CFRelease(font); CFRelease(s);
    }
    CGContextRestoreGState(cg);
    return noErr;
}

// Create a self-drawing titled group-box frame hiview at r, embedded in parent.
static ControlRef make_group_frame(WindowRef win, const CRect *r, const char *title,
                                   ControlRef parent) {
    (void)win;
    static OSStatus (*inst)(void *, void *, unsigned long, const void *, void *, void *);
    static void *(*getT)(void *);   /* GetControlEventTarget */
    if (!inst) inst = (void *)dlsym(RTLD_DEFAULT, "InstallEventHandler");
    if (!getT) getT = (void *)dlsym(RTLD_DEFAULT, "GetControlEventTarget");
    ControlRef c = NULL;
    if (n_HIObjectCreate) n_HIObjectCreate(CFSTR("com.apple.hiview"), NULL, (HIObjectRef *)&c);
    if (!c) return NULL;
    set_frame(c, r);
    if (inst && getT) {
        struct gbox *g = (struct gbox *)calloc(1, sizeof *g);
        if (g && title) { strncpy(g->title, title, sizeof g->title - 1); }
        struct { uint32_t cls, kind; } dr = { 'cntl', 4 /*kEventControlDraw*/ };
        inst(getT(c), (void *)gbox_draw, 1, &dr, g, NULL);
    }
    if (parent) n_HIViewAddSubview(parent, c);
    return c;
}

// ---- self-drawn popup button -------------------------------------------
// The nib's IBCarbonPopupButton is a com.apple.HIPopupButton HIObject, but that
// bare object rejects a menu (SetControlData -30581, init-event -50) and
// CreatePopupButtonControl is gone. So materialize the popup as a self-drawn
// com.apple.hiview: kEventControlDraw paints the current selection + a disclosure
// triangle + bezel; a mouse click (kEventControlHit/Track) opens the menu via
// PopUpMenuSelect (surviving) and updates the selection. The menu is built from
// the nib's nested IBCarbonMenuItem titles. This is universal (any IBCarbon popup)
// and shares the self-draw pattern with the group frame.
#define POPUP_MAX_ITEMS 32
struct popup {
    MenuRef menu;
    int16_t menuID;
    int     nitems;
    int     selected;              /* 1-based; 0 = none */
    char    items[POPUP_MAX_ITEMS][64];
    void   *view;                  /* the hiview */
};
static int16_t g_popup_menu_id = 5000;

static OSStatus popup_draw(void *call, void *ev, void *ud) {
    (void)call;
    struct popup *pu = (struct popup *)ud;
    CGContextRef cg = NULL;
    if (n_HIViewGetBounds == NULL) return (OSStatus)-9874;
    static OSStatus (*gep)(void *, uint32_t, uint32_t, uint32_t *, unsigned long, unsigned long *, void *);
    if (!gep) gep = (void *)dlsym(RTLD_DEFAULT, "GetEventParameter");
    if (gep) gep(ev, 'cntx', 'cntx', NULL, sizeof cg, NULL, &cg);
    void *view = NULL;
    if (gep) gep(ev, 'ctrl', 'ctrl', NULL, sizeof view, NULL, &view);
    if (!cg || !view) return (OSStatus)-9874;
    HIRectD b; n_HIViewGetBounds(view, &b);
    // bezel: rounded light-gray box
    CGContextSaveGState(cg);
    CGRect box = CGRectMake(0.5, 0.5, b.w - 1, b.h - 1);
    CGPathRef path = CGPathCreateWithRoundedRect(box, 4, 4, NULL);
    CGContextSetRGBFillColor(cg, 0.96, 0.96, 0.97, 1.0);
    CGContextAddPath(cg, path); CGContextFillPath(cg);
    CGContextSetRGBStrokeColor(cg, 0.5, 0.5, 0.53, 1.0); CGContextSetLineWidth(cg, 1);
    CGContextAddPath(cg, path); CGContextStrokePath(cg); CGPathRelease(path);
    // current selection text
    const char *sel = (pu->selected >= 1 && pu->selected <= pu->nitems)
                      ? pu->items[pu->selected - 1] : (pu->nitems ? pu->items[0] : "");
    CFStringRef s = CFStringCreateWithCString(NULL, sel, kCFStringEncodingUTF8);
    if (s) {
        CTFontRef font = CTFontCreateWithName(CFSTR("Helvetica"), 11, NULL);
        CGColorRef col = CGColorCreateGenericRGB(0.1, 0.1, 0.12, 1);
        CFStringRef k[] = { kCTFontAttributeName, kCTForegroundColorAttributeName };
        CFTypeRef v[] = { font, col };
        CFDictionaryRef attr = CFDictionaryCreate(NULL, (const void **)k, (const void **)v, 2,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFAttributedStringRef as = CFAttributedStringCreate(NULL, s, attr);
        CTLineRef line = CTLineCreateWithAttributedString(as);
        CGContextSetTextPosition(cg, 8, (b.h - 11) / 2 + 1);
        CTLineDraw(line, cg);
        CFRelease(line); CFRelease(as); CFRelease(attr); CFRelease(col); CFRelease(font); CFRelease(s);
    }
    // disclosure triangle at the right
    CGFloat tx = b.w - 16, ty = b.h / 2;
    CGContextSetRGBFillColor(cg, 0.25, 0.25, 0.3, 1);
    CGContextMoveToPoint(cg, tx, ty + 3);
    CGContextAddLineToPoint(cg, tx + 8, ty + 3);
    CGContextAddLineToPoint(cg, tx + 4, ty - 3);
    CGContextClosePath(cg); CGContextFillPath(cg);
    CGContextRestoreGState(cg);
    return noErr;
}

// Mouse click on the popup -> show the menu via PopUpMenuSelect, update selection.
static OSStatus popup_track(void *call, void *ev, void *ud) {
    (void)call; (void)ev;
    struct popup *pu = (struct popup *)ud;
    if (!pu || !pu->menu || !n_PopUpMenuSelect) return (OSStatus)-9874;
    // global position of the popup: use the current mouse location from the event.
    struct { double x, y; } gpt = { 0, 0 };
    static OSStatus (*gep)(void *, uint32_t, uint32_t, uint32_t *, unsigned long, unsigned long *, void *);
    if (!gep) gep = (void *)dlsym(RTLD_DEFAULT, "GetEventParameter");
    // kEventParamMouseLocation 'mloc' typeHIPoint 'hipt' (global)
    int have = 0;
    if (gep && gep(ev, 'mloc', 'hipt', NULL, sizeof gpt, NULL, &gpt) == 0) have = 1;
    if (n_InsertMenu) n_InsertMenu(pu->menu, -1 /*hierarchical (not in the bar)*/);
    int16_t top = have ? (int16_t)gpt.y : 200;
    int16_t left = have ? (int16_t)gpt.x : 200;
    uint32_t res = n_PopUpMenuSelect(pu->menu, top, left,
                                     (int16_t)(pu->selected > 0 ? pu->selected : 1));
    if (n_DeleteMenu) n_DeleteMenu(pu->menuID);
    int16_t item = (int16_t)(res & 0xffff);
    if (item >= 1 && item <= pu->nitems) {
        pu->selected = item;
        if (n_HIViewSetNeedsDisplay && pu->view) n_HIViewSetNeedsDisplay(pu->view, 1);
    }
    return noErr;
}

// Build a self-drawn popup at r with the menu items nested in [lo,hi) of the nib.
static ControlRef make_popup(const char *x, long lo, long hi, const CRect *r,
                             ControlRef parent) {
    ControlRef c = NULL;
    if (!n_HIObjectCreate) return NULL;
    n_HIObjectCreate(CFSTR("com.apple.hiview"), NULL, (HIObjectRef *)&c);
    if (!c) return NULL;
    set_frame(c, r);
    struct popup *pu = (struct popup *)calloc(1, sizeof *pu);
    if (!pu) { if (parent) n_HIViewAddSubview(parent, c); return c; }
    pu->view = c;
    pu->menuID = g_popup_menu_id++;
    if (n_CreateNewMenu) n_CreateNewMenu(pu->menuID, 0, &pu->menu);
    if (pu->menu && n_SetMenuID) n_SetMenuID(pu->menu, pu->menuID);
    // parse nested IBCarbonMenuItem titles in order
    long p = lo;
    while (p < hi && pu->nitems < POPUP_MAX_ITEMS) {
        const char *o = strstr(x + p, "class=\"IBCarbonMenuItem\"");
        if (!o || (o - x) >= hi) break;
        long os = o - x, objs = os;
        while (objs > lo && strncmp(x + objs, "<object ", 8)) objs--;
        long obje = nibx_match_end(x, objs); if (obje < 0 || obje > hi) obje = hi;
        char t[64] = "";
        if (nibx_str(x, os, obje, "title", t, sizeof t)) {
            nibx_unescape(t);
            if (strcmp(t, "-") != 0) {                  /* skip separators */
                strncpy(pu->items[pu->nitems], t, sizeof pu->items[0] - 1);
                if (pu->menu && n_AppendMenuItemTextWithCFString) {
                    uint16_t idx = 0;
                    CFStringRef cs = CFStringCreateWithCString(NULL, t, kCFStringEncodingUTF8);
                    n_AppendMenuItemTextWithCFString(pu->menu, cs, 0, 0, &idx);
                    if (cs) CFRelease(cs);
                }
                pu->nitems++;
            }
        }
        p = obje;
    }
    pu->selected = pu->nitems ? 1 : 0;                   /* default to first */
    if (n_InstallEventHandler2 && n_GetControlEventTarget2) {
        void *tgt = (void *)n_GetControlEventTarget2(c);
        struct { uint32_t cls, kind; } dr = { 'cntl', 4  /*kEventControlDraw*/ };
        struct { uint32_t cls, kind; } hk = { 'cntl', 1  /*kEventControlHit*/  };
        struct { uint32_t cls, kind; } tk = { 'cntl', 51 /*kEventControlTrack*/};
        n_InstallEventHandler2(tgt, (void *)popup_draw, 1, &dr, pu, NULL);
        n_InstallEventHandler2(tgt, (void *)popup_track, 1, &hk, pu, NULL);
        n_InstallEventHandler2(tgt, (void *)popup_track, 1, &tk, pu, NULL);
    }
    if (parent) n_HIViewAddSubview(parent, c);
    return c;
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
            // self-drawn popup (bare HIPopupButton HIObject can't take a menu on
            // modern macOS) with the dropdown built from the nib's menu items;
            // clicking opens it via PopUpMenuSelect. make_popup embeds it itself.
            make_popup(x, il, ih, &r, parent);
            p = oe; continue;
        } else if (!strcmp(cls, "IBCarbonGroupBox")) {
            // No CreateGroupBoxControl on modern macOS -> self-drawn titled frame.
            // CRUCIAL: the nib stores EVERY control's bounds WINDOW-RELATIVE, even
            // for a group's children. If we re-parented the children INTO the
            // group hiview, HIView would re-interpret their window-relative frame
            // as group-relative and shift them by the group's origin (the "double
            // offset" that scattered Halo's settings controls and pushed the
            // Shader/FSAA/Port controls off-view). So the group is a VISUAL frame
            // only; its children stay siblings on `parent` (root) with their
            // window-relative coords. Draw the frame FIRST so children sit on top.
            make_group_frame(win, &r, title, parent);
            build_children(x, il, ih, parent, win);   // children flat on root
            p = oe; continue;
        } else if (!strcmp(cls, "IBCarbonUserPane")) {
            // user pane: transparent container. Same flat-coord rule as groups —
            // a base hiview here would double-offset its kids; keep them on root.
            build_children(x, il, ih, parent, win);
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
