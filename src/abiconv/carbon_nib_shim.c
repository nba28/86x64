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
#include <objc/message.h>                // set the backing NSWindow's title
#include <objc/runtime.h>
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

// ---- self-drawn control registry -----------------------------------------
// The settings-window popups and edit fields are self-drawn com.apple.hiviews
// (the classic popup/edit-text controls were removed from 64-bit HIToolbox).  The
// app drives them with classic Control Manager calls — SetControl32BitValue /
// GetControl32BitValue (popup selection), SetControlData / GetControlData
// (kControlEditText*Tag for the port/IP text), (De)ActivateControl (enable state).
// Those calls arrive in the Control-Manager MTSHIM shims (carbon_ui_shim.c,
// carbon_control_shim.c), which forward to this registry: if the ControlRef is one
// of ours we service it (store the value, redraw, and remember it for read-back);
// otherwise the shim forwards to native HIToolbox for the real (checkbox/button)
// controls.  Registered by the make_* builders; the records live for the window's
// life (a bounded one-time settings-window leak, as elsewhere in this file).
enum { SD_POPUP = 1, SD_EDIT = 2 };
struct sd_entry { void *ctrl; int kind; void *rec; };
#define SD_MAX 128
static struct sd_entry g_sd[SD_MAX];
static int g_sd_n;
static void sd_register(void *ctrl, int kind, void *rec) {
    if (!ctrl || g_sd_n >= SD_MAX) return;
    g_sd[g_sd_n].ctrl = ctrl; g_sd[g_sd_n].kind = kind; g_sd[g_sd_n].rec = rec; g_sd_n++;
}
static struct sd_entry *sd_find(void *ctrl) {
    for (int i = 0; i < g_sd_n; i++) if (g_sd[i].ctrl == ctrl) return &g_sd[i];
    return NULL;
}

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
struct gbox { char title[128]; void *view; CRect r; };

// Pull the CGContext + drawing bounds out of a kEventControlDraw event.  On a base
// com.apple.hiview the draw event carries kEventParamCGContextRef ('cntx') but does
// NOT populate kEventParamDirectObject ('ctrl'), so we can't get the view from the
// event — the caller passes the view it stored at creation.  Bounds come from
// HIViewGetBounds(view); if that yields an empty rect we fall back to the nib frame
// size the caller also stored.  Returns 1 on success (cg + w/h valid).
static int draw_ctx(void *ev, void *view, const CRect *nibr,
                    CGContextRef *out_cg, double *out_w, double *out_h) {
    static OSStatus (*gep)(void *, uint32_t, uint32_t, uint32_t *, unsigned long, unsigned long *, void *);
    if (!gep) gep = (void *)dlsym(RTLD_DEFAULT, "GetEventParameter");
    CGContextRef cg = NULL;
    if (gep) gep(ev, 'cntx' /*kEventParamCGContextRef*/, 'cntx', NULL, sizeof(CGContextRef), NULL, &cg);
    if (!cg) return 0;
    double w = 0, h = 0;
    HIRectD b = { 0, 0, 0, 0 };
    if (view && n_HIViewGetBounds && n_HIViewGetBounds(view, &b) == 0) { w = b.w; h = b.h; }
    if ((w <= 0 || h <= 0) && nibr) { w = nibr->right - nibr->left; h = nibr->bottom - nibr->top; }
    if (w <= 0 || h <= 0) return 0;
    *out_cg = cg; *out_w = w; *out_h = h; return 1;
}

// Typographic width of a line at a given font size (no drawing).
static double measure_text(const char *utf8, double fontsz) {
    if (!utf8 || !utf8[0]) return 0;
    CFStringRef s = CFStringCreateWithCString(NULL, utf8, kCFStringEncodingUTF8);
    if (!s) return 0;
    CTFontRef font = CTFontCreateWithName(CFSTR("Helvetica"), fontsz, NULL);
    CFStringRef k[] = { kCTFontAttributeName };
    CFTypeRef v[] = { font };
    CFDictionaryRef attr = CFDictionaryCreate(NULL, (const void **)k, (const void **)v, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef as = CFAttributedStringCreate(NULL, s, attr);
    CTLineRef line = CTLineCreateWithAttributedString(as);
    double tw = CTLineGetTypographicBounds(line, NULL, NULL, NULL);
    CFRelease(line); CFRelease(as); CFRelease(attr); CFRelease(font); CFRelease(s);
    return tw;
}

// Draw one line of upright text into a HIView draw context.  The HIToolbox draw
// context is FLIPPED (top-left origin, y increasing downward — the same convention
// as the control frame we stroke), but CoreText renders glyphs y-up, so a bare
// CTLineDraw comes out upside-down.  Flip a saved copy of the CTM vertically around
// the view height H, and place the baseline at (H - baseline_from_top) so the text
// reads upright at the requested top-relative position.  `align`: 0 = x is the left
// edge; 1 = x is the RIGHT edge (right-align the text ending at x); returns the
// text width.  Color is RGB in 0..1.
static double draw_text(CGContextRef cg, const char *utf8, double x,
                        double baseline_from_top, double H, double fontsz,
                        double rr, double gg, double bb, int align) {
    if (!utf8 || !utf8[0]) return 0;
    CFStringRef s = CFStringCreateWithCString(NULL, utf8, kCFStringEncodingUTF8);
    if (!s) return 0;
    CTFontRef font = CTFontCreateWithName(CFSTR("Helvetica"), fontsz, NULL);
    CGColorRef col = CGColorCreateGenericRGB(rr, gg, bb, 1);
    CFStringRef k[] = { kCTFontAttributeName, kCTForegroundColorAttributeName };
    CFTypeRef v[] = { font, col };
    CFDictionaryRef attr = CFDictionaryCreate(NULL, (const void **)k, (const void **)v, 2,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef as = CFAttributedStringCreate(NULL, s, attr);
    CTLineRef line = CTLineCreateWithAttributedString(as);
    double tw = CTLineGetTypographicBounds(line, NULL, NULL, NULL);
    double tx = align == 1 ? x - tw : x;
    CGContextSaveGState(cg);
    CGContextTranslateCTM(cg, 0, H);
    CGContextScaleCTM(cg, 1, -1);
    CGContextSetTextMatrix(cg, CGAffineTransformIdentity);
    CGContextSetTextPosition(cg, tx, H - baseline_from_top);
    CTLineDraw(line, cg);
    CGContextRestoreGState(cg);
    CFRelease(line); CFRelease(as); CFRelease(attr); CFRelease(col); CFRelease(font); CFRelease(s);
    return tw;
}

// (call, ev) are EventHandlerCallRef / EventRef — opaque here to avoid pulling in
// Carbon.h (this TU only has CoreFoundation). eventNotHandledErr == -9874.
static OSStatus gbox_draw(void *call, void *ev, void *ud) {
    (void)call;
    struct gbox *g = (struct gbox *)ud;
    CGContextRef cg = NULL; double W = 0, H = 0;
    if (!g || !draw_ctx(ev, g->view, &g->r, &cg, &W, &H)) return (OSStatus)-9874;
    typedef struct { double x, y, w, h; } HR;
    HR b = { 0, 0, W, H };
    // frame inset a little; leave room at top for the title
    CGFloat top = 8;
    CGRect fr = CGRectMake(1, top, b.w - 2, b.h - top - 1);
    CGContextSaveGState(cg);
    CGContextSetRGBStrokeColor(cg, 0.55, 0.55, 0.58, 1.0);
    CGContextSetLineWidth(cg, 1.0);
    CGPathRef p = CGPathCreateWithRoundedRect(fr, 5, 5, NULL);
    CGContextAddPath(cg, p); CGContextStrokePath(cg); CGPathRelease(p);
    // title: punch a gap in the top border, then draw the label upright over it.
    if (g && g->title[0]) {
        double tw = measure_text(g->title, 11);
        // clear a gap in the frame stroke behind the title (cover ~[top-1, top+1])
        CGContextSetRGBFillColor(cg, 0.93, 0.93, 0.93, 1.0);
        CGContextFillRect(cg, CGRectMake(10, top - 6, tw + 8, 13));
        draw_text(cg, g->title, 14, top + 3, H, 11, 0.15, 0.15, 0.18, 0);
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
        if (g) { g->view = c; if (r) g->r = *r; if (title) strncpy(g->title, title, sizeof g->title - 1); }
        struct { uint32_t cls, kind; } dr = { 'cntl', 4 /*kEventControlDraw*/ };
        inst(getT(c), (void *)gbox_draw, 1, &dr, g, NULL);
    }
    // a base com.apple.hiview is created HIDDEN; make it visible so its
    // kEventControlDraw handler is actually invoked by the compositing hierarchy.
    if (n_HIViewSetVisible) n_HIViewSetVisible(c, 1);
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
    char    label[64];             /* the popup's own title, drawn to the left */
    void   *view;                  /* the hiview */
    CRect   r;                     /* nib frame (bounds fallback) */
    int     enabled;               /* 1 = interactive (default) */
    uint32_t command;              /* nib HICommand to fire on change (0 = none) */
    void   *win;                   /* owning window (for ProcessHICommand) */
};
static int16_t g_popup_menu_id = 5000;

static OSStatus popup_draw(void *call, void *ev, void *ud) {
    (void)call;
    struct popup *pu = (struct popup *)ud;
    CGContextRef cg = NULL; double W = 0, H = 0;
    if (!pu || !draw_ctx(ev, pu->view, &pu->r, &cg, &W, &H)) return (OSStatus)-9874;
    HIRectD b = { 0, 0, W, H };
    CGContextSaveGState(cg);
    double baseline = b.h / 2 + 4;   // vertically centred 11pt baseline, from top
    // The control bounds span the LABEL (e.g. "Lens Flare:") on the left plus the
    // popup box on the right (nib titleJustification -2 = right-aligned label).
    // Reserve the rightmost portion for the box; draw the label right-aligned just
    // to its left so the classic colon-aligned column look is preserved.
    CGFloat box_w = b.w * 0.52; if (box_w > 150) box_w = 150; if (box_w < 60 && b.w > 70) box_w = b.w - 8;
    CGFloat box_x = b.w - box_w;
    if (pu->label[0])
        draw_text(cg, pu->label, box_x - 6, baseline, H, 11, 0.1, 0.1, 0.12, 1 /*right-align*/);
    // bezel: rounded light-gray box (right portion)
    CGRect box = CGRectMake(box_x + 0.5, 0.5, box_w - 1, b.h - 1);
    CGPathRef path = CGPathCreateWithRoundedRect(box, 4, 4, NULL);
    CGContextSetRGBFillColor(cg, 0.96, 0.96, 0.97, 1.0);
    CGContextAddPath(cg, path); CGContextFillPath(cg);
    CGContextSetRGBStrokeColor(cg, 0.5, 0.5, 0.53, 1.0); CGContextSetLineWidth(cg, 1);
    CGContextAddPath(cg, path); CGContextStrokePath(cg); CGPathRelease(path);
    // current selection text (inside the box)
    const char *sel = (pu->selected >= 1 && pu->selected <= pu->nitems)
                      ? pu->items[pu->selected - 1] : (pu->nitems ? pu->items[0] : "");
    draw_text(cg, sel, box_x + 8, baseline, H, 11, 0.1, 0.1, 0.12, 0);
    // disclosure double-triangle at the right of the box
    CGFloat tx = b.w - 14, ty = b.h / 2;
    CGContextSetRGBFillColor(cg, 0.25, 0.25, 0.3, 1);
    CGContextMoveToPoint(cg, tx, ty + 1);
    CGContextAddLineToPoint(cg, tx + 8, ty + 1);
    CGContextAddLineToPoint(cg, tx + 4, ty + 5);
    CGContextClosePath(cg); CGContextFillPath(cg);
    CGContextMoveToPoint(cg, tx, ty - 1);
    CGContextAddLineToPoint(cg, tx + 8, ty - 1);
    CGContextAddLineToPoint(cg, tx + 4, ty - 5);
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
    if (item >= 1 && item <= pu->nitems && item != pu->selected) {
        pu->selected = item;
        if (n_HIViewSetNeedsDisplay && pu->view) n_HIViewSetNeedsDisplay(pu->view, 1);
        // Fire the popup's HICommand so the app's command handler runs its
        // settings logic (e.g. the value-changed harvest) exactly as a real
        // popup button would when the user picks a new item.
        if (pu->command) {
            static OSStatus (*phc)(const void *, void *);
            if (!phc) phc = (void *)dlsym(RTLD_DEFAULT, "ProcessHICommand");
            if (phc) { HICommandLite hc = { 0, pu->command }; phc(&hc, NULL); }
        }
    }
    return noErr;
}

// kEventControlHitTest: a base com.apple.hiview is not hit-testable by default, so
// mouse-downs pass straight through and it never gets kEventControlTrack/SetData.
// Report the whole view as a single clickable part (return kEventParamControlPart)
// so HIToolbox routes the click to this control and then dispatches Hit/Track.
static OSStatus sd_hittest(void *call, void *ev, void *ud) {
    (void)call; (void)ud;
    static OSStatus (*gep)(void *, uint32_t, uint32_t, uint32_t *, unsigned long, unsigned long *, void *);
    static OSStatus (*sep)(void *, uint32_t, uint32_t, unsigned long, const void *);
    if (!gep) gep = (void *)dlsym(RTLD_DEFAULT, "GetEventParameter");
    if (!sep) sep = (void *)dlsym(RTLD_DEFAULT, "SetEventParameter");
    // report the point is inside -> part 1 (kControlButtonPart / a generic hot part)
    int16_t part = 1;
    if (sep) sep(ev, 'cprt' /*kEventParamControlPart*/, 'cprt', sizeof part, &part);
    return noErr;
}

// Build a self-drawn popup at r with the menu items nested in [lo,hi) of the nib.
static ControlRef make_popup(const char *x, long lo, long hi, const CRect *r,
                             ControlRef parent, WindowRef win) {
    ControlRef c = NULL;
    if (!n_HIObjectCreate) return NULL;
    n_HIObjectCreate(CFSTR("com.apple.hiview"), NULL, (HIObjectRef *)&c);
    if (!c) return NULL;
    set_frame(c, r);
    struct popup *pu = (struct popup *)calloc(1, sizeof *pu);
    if (!pu) { if (parent) n_HIViewAddSubview(parent, c); return c; }
    pu->view = c;
    if (r) pu->r = *r;
    pu->enabled = 1;
    pu->win = win;
    { uint32_t cmd = 0; if (nibx_ostype(x, lo, hi, "command", &cmd)) pu->command = cmd; }
    sd_register(c, SD_POPUP, pu);
    pu->menuID = g_popup_menu_id++;
    if (n_CreateNewMenu) n_CreateNewMenu(pu->menuID, 0, &pu->menu);
    if (pu->menu && n_SetMenuID) n_SetMenuID(pu->menu, pu->menuID);
    // the popup's own title (e.g. "Lens Flare:") — drawn as a left label. It sits
    // at the popup object's top level; the nested IBCarbonMenu's own title is the
    // "Popup:" placeholder, which nibx_own_title skips over (past the items array).
    char lbl[64] = "";
    if (nibx_str(x, lo, hi, "title", lbl, sizeof lbl)) {
        nibx_unescape(lbl);
        if (strcmp(lbl, "Popup:") != 0) strncpy(pu->label, lbl, sizeof pu->label - 1);
    }
    // parse nested IBCarbonMenuItem titles in order; a menu item flagged
    // checked=TRUE is the nib's default selection (Halo may override at runtime).
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
                int chk = 0;
                if (nibx_bool(x, os, obje, "checked", &chk) && chk)
                    pu->selected = pu->nitems + 1;      /* 1-based */
                pu->nitems++;
            }
        }
        p = obje;
    }
    if (pu->selected < 1 || pu->selected > pu->nitems)
        pu->selected = pu->nitems ? 1 : 0;              /* default to first */
    if (n_InstallEventHandler2 && n_GetControlEventTarget2) {
        void *tgt = (void *)n_GetControlEventTarget2(c);
        struct { uint32_t cls, kind; } dr = { 'cntl', 4  /*kEventControlDraw*/    };
        struct { uint32_t cls, kind; } ht = { 'cntl', 3  /*kEventControlHitTest*/ };
        struct { uint32_t cls, kind; } hk = { 'cntl', 1  /*kEventControlHit*/     };
        struct { uint32_t cls, kind; } tk = { 'cntl', 51 /*kEventControlTrack*/   };
        n_InstallEventHandler2(tgt, (void *)popup_draw,  1, &dr, pu, NULL);
        n_InstallEventHandler2(tgt, (void *)sd_hittest,  1, &ht, pu, NULL);
        n_InstallEventHandler2(tgt, (void *)popup_track, 1, &hk, pu, NULL);
        n_InstallEventHandler2(tgt, (void *)popup_track, 1, &tk, pu, NULL);
    }
    if (n_HIViewSetVisible) n_HIViewSetVisible(c, 1);   // base hiview starts hidden
    if (parent) n_HIViewAddSubview(parent, c);
    return c;
}

// ---- self-drawn edit-text field ----------------------------------------
// IBCarbonEditText -> the classic MLTE/edit-text control was removed from 64-bit
// HIToolbox, so materialize it as a self-drawn com.apple.hiview that (a) draws a
// classic sunken white field with its current text, and (b) captures the value the
// app pushes at runtime.  Halo sets the port/IP text with
// SetControlData(ctl, part, kControlEditTextCFStringTag|kControlEditTextTextTag,
// size, &value) after resolving the field by its ControlID (signature 'Sprt' etc.).
// On a compositing HIView SetControlData is delivered as a kEventControlSetData
// carbon event to the view's own handler, so we intercept it there, store the
// string, and redraw — no SetControlData interposition (which would need a
// retranslate) required.  Universal for any IBCarbonEditText.
struct edit { char text[128]; void *view; CRect r; int enabled; int focused; };

static OSStatus edit_draw(void *call, void *ev, void *ud) {
    (void)call;
    struct edit *e = (struct edit *)ud;
    CGContextRef cg = NULL; double W = 0, H = 0;
    if (!e || !draw_ctx(ev, e->view, &e->r, &cg, &W, &H)) return (OSStatus)-9874;
    HIRectD b = { 0, 0, W, H };
    CGContextSaveGState(cg);
    // sunken white field with a 1px gray inset border
    CGRect box = CGRectMake(0.5, 0.5, b.w - 1, b.h - 1);
    CGContextSetRGBFillColor(cg, 1.0, 1.0, 1.0, 1.0);
    CGContextFillRect(cg, box);
    CGContextSetRGBStrokeColor(cg, 0.55, 0.55, 0.58, 1.0);
    CGContextSetLineWidth(cg, 1.0);
    CGContextStrokeRect(cg, box);
    // top inner shadow line for the recessed look
    CGContextSetRGBStrokeColor(cg, 0.78, 0.78, 0.80, 1.0);
    CGContextBeginPath(cg);
    CGContextMoveToPoint(cg, 1.5, b.h - 1.5);
    CGContextAddLineToPoint(cg, b.w - 1.5, b.h - 1.5);
    CGContextStrokePath(cg);
    if (e && e->text[0])
        draw_text(cg, e->text, 5, b.h / 2 + 4, H, 11, 0.05, 0.05, 0.08, 0);
    CGContextRestoreGState(cg);
    return noErr;
}

// kEventControlSetData: capture the CFString/text the app pushes into the field.
static OSStatus edit_setdata(void *call, void *ev, void *ud) {
    (void)call;
    struct edit *e = (struct edit *)ud;
    if (!e) return (OSStatus)-9874;
    static OSStatus (*gep)(void *, uint32_t, uint32_t, uint32_t *, unsigned long, unsigned long *, void *);
    if (!gep) gep = (void *)dlsym(RTLD_DEFAULT, "GetEventParameter");
    if (!gep) return (OSStatus)-9874;
    uint32_t tag = 0;
    void *buf = NULL; unsigned long bufsz = 0;
    gep(ev, 'cdtg' /*kEventParamControlDataTag*/,        'enum', NULL, sizeof tag,   NULL, &tag);
    gep(ev, 'cdbf' /*kEventParamControlDataBuffer*/,     'ptr ', NULL, sizeof buf,   NULL, &buf);
    gep(ev, 'cdbs' /*kEventParamControlDataBufferSize*/, 'lcnt', NULL, sizeof bufsz, NULL, &bufsz);
    int handled = 0;
    if (tag == 'cfst' /*kControlEditTextCFStringTag*/ && buf) {
        // buffer holds a CFStringRef*
        CFStringRef cf = *(CFStringRef *)buf;
        if (cf && CFGetTypeID(cf) == CFStringGetTypeID()) {
            e->text[0] = 0;
            CFStringGetCString(cf, e->text, sizeof e->text, kCFStringEncodingUTF8);
            handled = 1;
        }
    } else if (tag == 'text' /*kControlEditTextTextTag*/ && buf) {
        unsigned long n = bufsz < sizeof e->text - 1 ? bufsz : sizeof e->text - 1;
        memcpy(e->text, buf, n); e->text[n] = 0; handled = 1;
    }
    if (handled) {
        if (n_HIViewSetNeedsDisplay && e->view) n_HIViewSetNeedsDisplay(e->view, 1);
        return noErr;
    }
    return (OSStatus)-9874;  /* eventNotHandledErr: let HIToolbox handle other tags */
}

// kEventControlSetFocusPart: accept keyboard focus so the field can be typed into.
// The event's part is >0 to focus, 0 to unfocus; echo it back as accepted.
static OSStatus edit_focus(void *call, void *ev, void *ud) {
    (void)call;
    struct edit *e = (struct edit *)ud;
    static OSStatus (*gep)(void *, uint32_t, uint32_t, uint32_t *, unsigned long, unsigned long *, void *);
    static OSStatus (*sep)(void *, uint32_t, uint32_t, unsigned long, const void *);
    if (!gep) gep = (void *)dlsym(RTLD_DEFAULT, "GetEventParameter");
    if (!sep) sep = (void *)dlsym(RTLD_DEFAULT, "SetEventParameter");
    int16_t part = 0;
    if (gep) gep(ev, 'cprt', 'cprt', NULL, sizeof part, NULL, &part);
    if (e) { e->focused = part != 0; if (e->view && n_HIViewSetNeedsDisplay) n_HIViewSetNeedsDisplay(e->view, 1); }
    if (sep) sep(ev, 'cprt', 'cprt', sizeof part, &part);   /* accept the focus */
    return noErr;
}

// kEventControlKeyDown / kEventTextInputUnicodeForKeyEvent: edit the field text.
static OSStatus edit_key(void *call, void *ev, void *ud) {
    (void)call;
    struct edit *e = (struct edit *)ud;
    if (!e || !e->enabled) return (OSStatus)-9874;
    static OSStatus (*gep)(void *, uint32_t, uint32_t, uint32_t *, unsigned long, unsigned long *, void *);
    if (!gep) gep = (void *)dlsym(RTLD_DEFAULT, "GetEventParameter");
    if (!gep) return (OSStatus)-9874;
    char ch = 0;
    // kEventParamKeyMacCharCodes 'kchr' typeChar 'TEXT'
    if (gep(ev, 'kchr', 'TEXT', NULL, sizeof ch, NULL, &ch) != 0) return (OSStatus)-9874;
    int n = (int)strlen(e->text);
    if (ch == 8 || ch == 127) {            /* backspace / delete */
        if (n > 0) e->text[n - 1] = 0;
    } else if (ch == 13 || ch == 3 || ch == 9) {
        return (OSStatus)-9874;            /* return/enter/tab: let the app handle */
    } else if (ch >= 32 && ch < 127 && n < (int)sizeof e->text - 1) {
        e->text[n] = ch; e->text[n + 1] = 0;
    } else {
        return (OSStatus)-9874;
    }
    if (e->view && n_HIViewSetNeedsDisplay) n_HIViewSetNeedsDisplay(e->view, 1);
    return noErr;
}

static ControlRef make_edit_field(const char *x, long lo, long hi, const CRect *r,
                                  ControlRef parent) {
    (void)x; (void)lo; (void)hi;
    ControlRef c = NULL;
    if (!n_HIObjectCreate) return NULL;
    n_HIObjectCreate(CFSTR("com.apple.hiview"), NULL, (HIObjectRef *)&c);
    if (!c) return NULL;
    set_frame(c, r);
    struct edit *e = (struct edit *)calloc(1, sizeof *e);
    if (e) { e->view = c; if (r) e->r = *r; e->enabled = 1; sd_register(c, SD_EDIT, e); }
    if (e && n_InstallEventHandler2 && n_GetControlEventTarget2) {
        void *tgt = n_GetControlEventTarget2(c);
        struct { uint32_t cls, kind; } dr = { 'cntl', 4  /*kEventControlDraw*/     };
        struct { uint32_t cls, kind; } sd = { 'cntl', 20 /*kEventControlSetData*/  };
        struct { uint32_t cls, kind; } ht = { 'cntl', 3  /*kEventControlHitTest*/  };
        struct { uint32_t cls, kind; } fp = { 'cntl', 4013/*kEventControlSetFocusPart*/ };
        struct { uint32_t cls, kind; } kd = { 'cntl', 11 /*kEventControlKeyDown*/  };
        n_InstallEventHandler2(tgt, (void *)edit_draw,    1, &dr, e, NULL);
        n_InstallEventHandler2(tgt, (void *)edit_setdata, 1, &sd, e, NULL);
        n_InstallEventHandler2(tgt, (void *)sd_hittest,   1, &ht, e, NULL);
        n_InstallEventHandler2(tgt, (void *)edit_focus,   1, &fp, e, NULL);
        n_InstallEventHandler2(tgt, (void *)edit_key,     1, &kd, e, NULL);
    }
    if (n_HIViewSetVisible) n_HIViewSetVisible(c, 1);   // base hiview starts hidden
    if (parent) n_HIViewAddSubview(parent, c);
    return c;
}

// ==================== self-drawn Control Manager bridge ====================
// Called from the Control-Manager MTSHIM shims (carbon_ui_shim.c /
// carbon_control_shim.c).  Each returns 1 if `ctrl` is one of our self-drawn
// controls (and it serviced the request), 0 otherwise so the shim forwards to
// native HIToolbox for the real (checkbox/button/static-text) controls.
static void sd_redraw(void *view) {
    if (view && n_HIViewSetNeedsDisplay) n_HIViewSetNeedsDisplay(view, 1);
}

// SetControl32BitValue(ctrl, value): for a popup, `value` selects the 1-based menu
// item (Halo pushes the detected/saved selection here at window-load).
int sd_ctrl_set_value(void *ctrl, int32_t value) {
    struct sd_entry *e = sd_find(ctrl);
    if (!e) return 0;
    if (e->kind == SD_POPUP) {
        struct popup *pu = (struct popup *)e->rec;
        if (value >= 1 && value <= pu->nitems) pu->selected = value;
        else if (value == 0 && pu->nitems) pu->selected = 1;  /* clamp */
        sd_redraw(pu->view);
    }
    return 1;
}

// GetControl32BitValue(ctrl): return the popup's current 1-based selection.
int sd_ctrl_get_value(void *ctrl, int32_t *out) {
    struct sd_entry *e = sd_find(ctrl);
    if (!e) return 0;
    if (out) *out = (e->kind == SD_POPUP) ? ((struct popup *)e->rec)->selected : 0;
    return 1;
}

// SetControlData(ctrl, ..., kControlEditTextCFStringTag, &cfstr): edit field text.
int sd_ctrl_set_cfstring(void *ctrl, const void *cfstr) {
    struct sd_entry *e = sd_find(ctrl);
    if (!e || e->kind != SD_EDIT) return 0;
    struct edit *ed = (struct edit *)e->rec;
    ed->text[0] = 0;
    CFStringRef cf = (CFStringRef)cfstr;
    if (cf && CFGetTypeID(cf) == CFStringGetTypeID())
        CFStringGetCString(cf, ed->text, sizeof ed->text, kCFStringEncodingUTF8);
    sd_redraw(ed->view);
    return 1;
}

// SetControlData(ctrl, ..., kControlEditTextTextTag, ptr,len): raw (MacRoman) bytes.
int sd_ctrl_set_text(void *ctrl, const char *buf, int len) {
    struct sd_entry *e = sd_find(ctrl);
    if (!e || e->kind != SD_EDIT) return 0;
    struct edit *ed = (struct edit *)e->rec;
    int n = len; if (n < 0) n = 0; if (n > (int)sizeof ed->text - 1) n = sizeof ed->text - 1;
    if (buf && n) memcpy(ed->text, buf, n);
    ed->text[n] = 0;
    sd_redraw(ed->view);
    return 1;
}

// GetControlData(ctrl, ..., kControlEditTextTextTag): copy the current text out.
// Returns the text length (>=0) if ours, or -1 if not one of our controls.
int sd_ctrl_get_text(void *ctrl, char *buf, int bufsz) {
    struct sd_entry *e = sd_find(ctrl);
    if (!e || e->kind != SD_EDIT) return -1;
    struct edit *ed = (struct edit *)e->rec;
    int n = (int)strlen(ed->text);
    if (buf && bufsz > 0) { int c = n < bufsz ? n : bufsz - 1; memcpy(buf, ed->text, c); buf[c] = 0; }
    return n;
}

// GetControlData(ctrl, ..., kControlEditTextCFStringTag): create a CFString of the
// current text.  Returns 1 (and *out = a +1 CFStringRef the caller releases) if ours.
int sd_ctrl_get_cfstring(void *ctrl, const void **out) {
    struct sd_entry *e = sd_find(ctrl);
    if (!e || e->kind != SD_EDIT) return 0;
    struct edit *ed = (struct edit *)e->rec;
    if (out) *out = CFStringCreateWithCString(NULL, ed->text, kCFStringEncodingUTF8);
    return 1;
}

// (De)ActivateControl / enable state: dim + make the control inert.
int sd_ctrl_set_enabled(void *ctrl, int enabled) {
    struct sd_entry *e = sd_find(ctrl);
    if (!e) return 0;
    if (e->kind == SD_POPUP) { ((struct popup *)e->rec)->enabled = enabled; sd_redraw(((struct popup *)e->rec)->view); }
    else if (e->kind == SD_EDIT) { ((struct edit *)e->rec)->enabled = enabled; sd_redraw(((struct edit *)e->rec)->view); }
    return 1;
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
            // Wire its ControlID so Halo's GetControlByID({'Lens',0})/
            // SetControl32BitValue resolves the popup at runtime.
            ControlRef pc = make_popup(x, il, ih, &r, parent, win);
            if (pc) wire_ids(x, il, ih, pc);
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
            // Use the group's OWN title (after its subviews array) — nibx_str would
            // return the first *child's* title ("Lens Flare:") instead of the frame
            // label ("Rendering Pipeline").
            char gt[512] = "";
            if (nibx_own_title(x, il, ih, gt, sizeof gt)) nibx_unescape(gt);
            make_group_frame(win, &r, gt[0] ? gt : NULL, parent);
            build_children(x, il, ih, parent, win);   // children flat on root
            p = oe; continue;
        } else if (!strcmp(cls, "IBCarbonUserPane")) {
            // user pane: transparent container. Same flat-coord rule as groups —
            // a base hiview here would double-offset its kids; keep them on root.
            build_children(x, il, ih, parent, win);
            p = oe; continue;
        } else if (!strcmp(cls, "IBCarbonEditText")) {
            // self-drawn edit field (MLTE/edit-text removed on modern macOS): draws a
            // classic sunken box and captures the text the app pushes via
            // SetControlData. Wire its ControlID so GetControlByID({'Sprt',0}) etc.
            // resolves the field for the app's runtime value set.
            ControlRef ec = make_edit_field(x, il, ih, &r, parent);
            if (ec) wire_ids(x, il, ih, ec);
            p = oe; continue;
        } else if (!strcmp(cls, "IBCarbonSeparator") ||
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

// Set the title on the AppKit NSWindow backing a Carbon WindowRef.  On modern
// macOS a Carbon window is hosted by an NSWindow (carbon_appkit_host.c loads
// AppKit), and SetWindowTitleWithCFString on a movable-modal window does NOT paint
// a title in the frame (the window-server title stays empty).  Locate the backing
// NSWindow by its CGWindowID via [NSApp windowWithWindowNumber:] and setTitle:, so
// the title bar shows the nib's window title ("Halo Graphics Settings").  All via
// the objc runtime C API to keep this a pure-C TU.
static void set_nswindow_title(WindowRef win, CFStringRef title) {
    if (!win || !title) return;
    static uint32_t (*HIWindowGetCGWindowID)(WindowRef);
    if (!HIWindowGetCGWindowID)
        HIWindowGetCGWindowID = (uint32_t (*)(WindowRef))dlsym(RTLD_DEFAULT, "HIWindowGetCGWindowID");
    if (!HIWindowGetCGWindowID) return;
    uint32_t cgid = HIWindowGetCGWindowID(win);
    if (!cgid) return;
    Class NSApplication = objc_getClass("NSApplication");
    if (!NSApplication) return;
    id nsapp = ((id (*)(id, SEL))objc_msgSend)((id)NSApplication, sel_getUid("sharedApplication"));
    if (!nsapp) return;
    id nswin = ((id (*)(id, SEL, long))objc_msgSend)(nsapp, sel_getUid("windowWithWindowNumber:"), (long)cgid);
    if (!nswin) return;
    ((void (*)(id, SEL, id))objc_msgSend)(nswin, sel_getUid("setTitle:"), (id)title);
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
    CFStringRef titleCF = cfs(title);
    if (n_SetWindowTitleWithCFString) n_SetWindowTitleWithCFString(win, titleCF);
    if (title[0]) set_nswindow_title(win, titleCF);   // paint the title-bar text

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
    int trace = getenv("ABICONV_NIB_TRACE") != NULL;
    char wname[128] = "?";
    if (trace && wn) CFStringGetCString(wn, wname, sizeof wname, kCFStringEncodingUTF8);
    if (trace) fprintf(stderr, "[nib] CreateWindowFromNib('%s') native st=%d w=%p\n", wname, (int)st, w);

    if ((st != 0 || !w)) {                       // native gutted path failed (e.g. -5601)
        const char *xib = nib_lookup(ref);
        if (trace) fprintf(stderr, "[nib]   -> falling back to build_window (xib=%s)\n", xib ? xib : "(none)");
        if (xib && wn) {
            WindowRef bw = build_window(xib, wn);
            if (bw) { w = bw; st = 0; }
            if (trace) fprintf(stderr, "[nib]   -> build_window returned %p\n", (void *)bw);
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
