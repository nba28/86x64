/*
 * carbon_scrolltext_shim.c — REAL CreateScrollingTextBoxControl: the classic
 * 'TEXT'/'styl' resource text box (Halo's EULA license text), re-implemented
 * as a SELF-DRAWING compositing HIView rendered with CoreText.
 *
 * WHAT THE CLASSIC CONTROL DID: CreateScrollingTextBoxControl(window, bounds,
 * contentResID, ...) loaded the 'TEXT' resource (+ its 'styl' style runs) with
 * id contentResID from the CURRENT RESOURCE FILE (the app has just
 * FSOpenResFile'd its .rsrc — Halo: EULA.rsrc, opened two calls earlier) and
 * displayed it in a scrollable read-only text box. The control class was
 * removed from 64-bit HIToolbox (32-bit-only), and no HIToolbox text view
 * survives (HITextView/MLTE gutted: com.apple.HITextView -> -50,
 * com.apple.HIScrollView -> -9870 — probed 2026-07-09).
 *
 * THE BRIDGE (all steps ground-truthed on the live translated Halo):
 *   1. Resource load is REAL and native: Get1Resource('TEXT'/'styl', resID)
 *      still works on modern macOS and honors the app's own FSOpenResFile
 *      resource chain (Halo's restored 815758-byte EULA.rsrc fork -> 20875-byte
 *      "MACSOFT LICENSE AGREEMENT" text + styl runs, ids 1024-1026 US/DE/FR).
 *   2. The control is a genuine base HIView ("com.apple.hiview", still
 *      creatable via HIObjectCreate) embedded at `bounds`, so the caller's
 *      ShowControl/ActivateControl/DisposeControl calls all operate on a real
 *      control, and it participates in the SAME compositing draw pipeline that
 *      demonstrably renders (the nib windows' buttons draw on screen).
 *   3. Rendering: a kEventControlDraw handler paints the styled text with
 *      CoreText into the event's CGContext. (A first attempt hosted an
 *      NSTextView via -[NSWindow initWithWindowRef:] — AppKit wrapper windows
 *      panic in NSCGSWindow _createContext under the app's own event loop and
 *      the overlay never composited; drawing INSIDE the HIView pipeline is the
 *      mechanism that actually reaches the screen.) The draw context follows
 *      the compositing-HIView convention: origin top-left, y down — flipped
 *      back around the view rect for CoreText.
 *   4. Scrolling: a kEventMouseWheelMoved handler on the host WINDOW target
 *      adjusts the scroll offset and invalidates the view (one text box per
 *      modal window in practice — Halo's EULA). Wheel/trackpad is the natural
 *      modern scroll surface; the classic auto-scroll marquee args are
 *      accepted but not re-animated.
 *
 * 'styl' runs (big-endian ScrpSTElement: startChar/height/ascent/fontID/face/
 * size/RGB) become CoreText attributes: point size, bold/italic traits,
 * underline, RGB color. Classic font IDs have no faithful modern mapping, so
 * the system UI font carries the styled sizes.
 *
 * MTSHIM convention (maptable_tramp.asm): rdi -> &i386 args[0] (4-byte cdecl
 * slots), uint32_t (OSStatus) in eax. ___CreateScrollingTextBoxControl is
 * already wired -> _shim_CreateScrollingTextBoxControl (this replaces the old
 * -9999 stub from carbon_control_shim.c), so translated targets bind it
 * already and this validates on a RESYNC — no retranslate.
 */

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

extern uint64_t x64_objc_unwrap(uint32_t h);
extern uint32_t x64_objc_wrap(uint64_t real);
void carbon_ensure_window_host(void);            /* carbon_appkit_host.c */

typedef int32_t OSStatus;
typedef struct { int16_t top, left, bottom, right; } CRect;
typedef struct { double x, y; } HIPointD;
typedef struct { double x, y, w, h; } HIRectD;

#define WRAPH(p) ((p) ? (((uint64_t)(uintptr_t)(p) >> 32) ? x64_objc_wrap((uint64_t)(uintptr_t)(p)) : (uint32_t)(uintptr_t)(p)) : 0)
#define DL(fn, ret, args) static ret (*fn) args; if (!fn) fn = (ret (*) args)dlsym(RTLD_DEFAULT, #fn)

static int st_debug(void) {
    static int v = -1;
    if (v < 0) v = getenv("ABICONV_SCROLLTEXT_DEBUG") != NULL;
    return v;
}
#define STLOG(...) do { if (st_debug()) fprintf(stderr, "[scrolltext] " __VA_ARGS__); } while (0)

/* ---- text-box registry (single-threaded UI) ---- */
#define NBOX 8
static struct box {
    void *ctrl;                   /* the HIView we returned                 */
    void *win;                    /* host Carbon window                     */
    CTFramesetterRef fs;          /* retained                               */
    CTFrameRef frame;             /* full-text frame, lazily built per size */
    double frame_w, frame_h;      /* geometry the cached frame was built for*/
    double text_h;                /* measured total text height             */
    double maxscroll;             /* max(0, text_h - view_h), set at draw    */
    double scroll;                /* 0 .. maxscroll                          */
} g_box[NBOX];

static struct box *box_for_ctrl(void *c) {
    for (int i = 0; i < NBOX; i++) if (c && g_box[i].ctrl == c) return &g_box[i];
    return NULL;
}
static struct box *box_for_win(void *w) {
    for (int i = 0; i < NBOX; i++) if (w && g_box[i].win == w) return &g_box[i];
    return NULL;
}
static struct box *box_alloc(void) {
    for (int i = 0; i < NBOX; i++) if (!g_box[i].ctrl) return &g_box[i];
    struct box *b = &g_box[0];                 /* recycle; window long dead  */
    if (b->frame) CFRelease(b->frame);
    if (b->fs) CFRelease(b->fs);
    memset(b, 0, sizeof *b);
    return b;
}

/* ---- 'TEXT' + 'styl' -> CFAttributedString (resource data is big-endian) ---- */
static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

/* Classic legible text sizes only: a garbage 'styl' size (see styl-parse note
 * below) must never produce a giant font that explodes layout to millions of
 * pixels. 6..48pt covers every real UI text run; anything else -> the default. */
static double clamp_pt(int size) {
    if (size < 6 || size > 48) return 11;
    return (double)size;
}

static CTFontRef ui_font(double size, int bold, int italic) {
    CTFontRef base = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, size, NULL);
    if (!base) base = CTFontCreateWithName(CFSTR("Helvetica"), size, NULL);
    uint32_t traits = (bold ? kCTFontTraitBold : 0) | (italic ? kCTFontTraitItalic : 0);
    if (traits && base) {
        CTFontRef t = CTFontCreateCopyWithSymbolicTraits(base, size, NULL, traits, traits);
        if (t) { CFRelease(base); return t; }
    }
    return base;
}

static CFAttributedStringRef styled_text(const uint8_t *txt, long tlen,
                                         const uint8_t *sty, long slen) {
    CFStringRef s = CFStringCreateWithBytes(NULL, txt, tlen,
                                            kCFStringEncodingMacRoman, false);
    if (!s) s = CFRetain(CFSTR(""));
    CFMutableAttributedStringRef as =
        CFAttributedStringCreateMutable(NULL, 0);
    CFAttributedStringReplaceString(as, CFRangeMake(0, 0), s);
    CFIndex n = CFStringGetLength(s);
    CFRelease(s);

    CTFontRef deffont = ui_font(11, 0, 0);
    CFAttributedStringSetAttribute(as, CFRangeMake(0, n),
                                   kCTFontAttributeName, deffont);
    CFRelease(deffont);

    /* Apply the 'styl' style runs (ScrpSTElement, big-endian, 20 bytes each:
     * startChar@0(4) height@4(2) ascent@6(2) font@8(2) face@10(1)+pad size@12(2)
     * color@14(6)).
     *
     * ROBUSTNESS (ground-truthed): the modern Resource Manager does NOT hand
     * back the classic 'styl' bytes verbatim — it applies its own normalization
     * so the master-pointer data is shifted/mangled vs the on-disk resource
     * (Halo's EULA styl 1024 is `00 01 ...` on disk but `01 00 ...` from
     * Get1Resource, making a naive parse read runCount=256 and size=3072pt ->
     * a 10-million-pixel frame that pushed all text off-view = the BLANK EULA
     * body). Every field is therefore VALIDATED: an implausible run count, a
     * startChar past the text, or a non-legible point size is skipped, and the
     * default 11pt run already covers the whole string. The EULA text itself
     * (a single plain default run) renders correctly from the default alone. */
    if (sty && slen >= 2) {
        int runs = (int)be16(sty);
        if (runs < 1 || (long)runs * 20 + 2 > slen + 20) runs = 0;  /* sane count */
        const uint8_t *r = sty + 2;
        for (int i = 0; i < runs && (r - sty) + 20 <= slen; i++, r += 20) {
            uint32_t start = be32(r);
            uint32_t end = (i + 1 < runs && (r + 20 - sty) + 4 <= slen)
                           ? be32(r + 20) : (uint32_t)n;
            uint8_t  face = r[10];
            int      size = (int16_t)be16(r + 12);
            uint16_t red = be16(r + 14), grn = be16(r + 16), blu = be16(r + 18);
            if (start >= (uint32_t)n) continue;         /* bogus offset: skip */
            if (end > (uint32_t)n) end = (uint32_t)n;
            if (end <= start) continue;
            CFRange rg = CFRangeMake(start, end - start);
            CTFontRef f = ui_font(clamp_pt(size), face & 1, face & 2);
            CFAttributedStringSetAttribute(as, rg, kCTFontAttributeName, f);
            CFRelease(f);
            if (face & 4) {
                int32_t u = kCTUnderlineStyleSingle;
                CFNumberRef un = CFNumberCreate(NULL, kCFNumberSInt32Type, &u);
                CFAttributedStringSetAttribute(as, rg, kCTUnderlineStyleAttributeName, un);
                CFRelease(un);
            }
            if (red | grn | blu) {
                CGColorRef c = CGColorCreateGenericRGB(red / 65535.0, grn / 65535.0,
                                                       blu / 65535.0, 1.0);
                CFAttributedStringSetAttribute(as, rg, kCTForegroundColorAttributeName, c);
                CGColorRelease(c);
            }
        }
    }
    return as;
}

/* Build (or rebuild) the cached full-text CTFrame for a given wrap width. */
static void box_layout(struct box *b, double w, double h) {
    (void)h;
    if (b->frame && b->frame_w == w) return;
    if (b->frame) { CFRelease(b->frame); b->frame = NULL; }
    CFRange fit;
    CGSize sz = CTFramesetterSuggestFrameSizeWithConstraints(
        b->fs, CFRangeMake(0, 0), NULL, CGSizeMake(w, CGFLOAT_MAX), &fit);
    b->text_h = sz.height + 4;
    CGMutablePathRef p = CGPathCreateMutable();
    CGPathAddRect(p, NULL, CGRectMake(0, 0, w, b->text_h));
    b->frame = CTFramesetterCreateFrame(b->fs, CFRangeMake(0, 0), p, NULL);
    CGPathRelease(p);
    b->frame_w = w;
    STLOG("layout w=%.0f text_h=%.0f\n", w, b->text_h);
}

/* ---- kEventControlDraw: paint the text with CoreText ---- */
static OSStatus box_draw_handler(void *call, void *event, void *user) {
    (void)call;
    struct box *b = (struct box *)user;
    DL(GetEventParameter, OSStatus, (void *, uint32_t, uint32_t, uint32_t *, unsigned long, unsigned long *, void *));
    DL(HIViewGetBounds, OSStatus, (void *, HIRectD *));
    CGContextRef ctx = NULL;
    if (!GetEventParameter ||
        GetEventParameter(event, 'cntx', 'cntx', NULL, sizeof ctx, NULL, &ctx) || !ctx) {
        STLOG("draw: no CGContext param\n");
        return -9874;
    }
    HIRectD vb = { 0, 0, 0, 0 };
    if (HIViewGetBounds) HIViewGetBounds(b->ctrl, &vb);
    double w = vb.w > 8 ? vb.w : 8, h = vb.h > 8 ? vb.h : 8;
    const double pad = 4, scroller = 14;
    STLOG("draw: view %.0fx%.0f scroll=%.0f\n", w, h, b->scroll);

    CGContextSaveGState(ctx);
    /* background + border (the classic box drew a 1px frame) */
    CGContextSetRGBFillColor(ctx, 1, 1, 1, 1);
    CGContextFillRect(ctx, CGRectMake(0, 0, w, h));
    CGContextSetRGBStrokeColor(ctx, 0.55, 0.55, 0.55, 1);
    CGContextStrokeRectWithWidth(ctx, CGRectMake(0.5, 0.5, w - 1, h - 1), 1);

    box_layout(b, w - 2 * pad - scroller, h);
    double maxscroll = b->text_h - h; if (maxscroll < 0) maxscroll = 0;
    b->maxscroll = maxscroll;                     /* wheel handler clamps to this */
    if (b->scroll > maxscroll) b->scroll = maxscroll;

    /* text, clipped inside the border */
    CGContextClipToRect(ctx, CGRectMake(pad, 1, w - 2 * pad - scroller, h - 2));
    /* compositing HIView contexts are QuickDraw-oriented (origin top-left,
     * y down); flip around the view rect for CoreText, then slide by scroll */
    CGContextTranslateCTM(ctx, 0, h);
    CGContextScaleCTM(ctx, 1, -1);
    /* frame rect is {0,0,w',text_h}; put its TOP at the view top: */
    CGContextTranslateCTM(ctx, pad, h - b->text_h + b->scroll);
    CTFrameDraw(b->frame, ctx);
    CGContextRestoreGState(ctx);

    /* scroll indicator (simple, honest): a thumb sized/positioned by ratio */
    if (maxscroll > 0) {
        double th = h * (h / b->text_h); if (th < 20) th = 20;
        double ty = (h - th - 2) * (b->scroll / maxscroll) + 1;
        CGContextSetRGBFillColor(ctx, 0.75, 0.75, 0.75, 1);
        CGContextFillRect(ctx, CGRectMake(w - scroller + 3, ty, scroller - 6, th));
    }
    return 0;
}

/* ---- kEventMouseWheelMoved on the host window: scroll + invalidate ---- */
static OSStatus box_wheel_handler(void *call, void *event, void *user) {
    (void)call;
    struct box *b = (struct box *)user;
    DL(GetEventParameter, OSStatus, (void *, uint32_t, uint32_t, uint32_t *, unsigned long, unsigned long *, void *));
    DL(HIViewSetNeedsDisplay, OSStatus, (void *, uint8_t));
    int16_t axis = 0; int32_t delta = 0;
    if (GetEventParameter) {
        GetEventParameter(event, 'mwax', 'mwax', NULL, sizeof axis, NULL, &axis);
        GetEventParameter(event, 'mwdl', 'long', NULL, sizeof delta, NULL, &delta);
    }
    if (axis != 1 /*kEventMouseWheelAxisY*/ || !delta) return -9874;
    /* clamp to maxscroll from the last draw (0 until first draw; re-clamped
     * there anyway). delta<0 = wheel down = show LATER text = scroll++. */
    b->scroll -= delta * 24.0;
    if (b->scroll < 0) b->scroll = 0;
    if (b->maxscroll > 0 && b->scroll > b->maxscroll) b->scroll = b->maxscroll;
    if (HIViewSetNeedsDisplay && b->ctrl) HIViewSetNeedsDisplay(b->ctrl, 1);
    STLOG("wheel delta=%d scroll=%.0f/%.0f\n", delta, b->scroll, b->maxscroll);
    return 0;
}

/* kEventControlHitTest: report part 1 so the view is hittable (wheel routing) */
static OSStatus box_hit_handler(void *call, void *event, void *user) {
    (void)call; (void)user;
    DL(SetEventParameter, OSStatus, (void *, uint32_t, uint32_t, unsigned long, const void *));
    int16_t part = 1;
    if (SetEventParameter)
        SetEventParameter(event, 'cprt' /*kEventParamControlPart*/,
                          'cprt' /*typeControlPartCode*/, sizeof part, &part);
    return 0;
}

/* Called by shim_DisposeControl (carbon_control_shim.c) for every disposed
 * control: drop this control's text state if it was one of ours. */
void carbon_scrolltext_control_disposed(void *ctrl) {
    struct box *b = box_for_ctrl(ctrl);
    if (!b) return;
    if (b->frame) CFRelease(b->frame);
    if (b->fs) CFRelease(b->fs);
    memset(b, 0, sizeof *b);
    STLOG("disposed\n");
}

/* OSStatus CreateScrollingTextBoxControl(WindowRef window, const Rect *bounds,
 *     SInt16 contentResID, Boolean autoScroll, UInt32 delayBeforeAutoScroll,
 *     UInt32 delayBetweenAutoScroll, UInt16 autoScrollAmount,
 *     ControlRef *outControl)
 * i386 slots: a[0]=window a[1]=bounds a[2]=resID a[3]=autoScroll a[4..5]=delays
 *             a[6]=amount a[7]=outControl. */
uint32_t shim_CreateScrollingTextBoxControl(uint32_t *a) {
    DL(Get1Resource, void *, (uint32_t, int16_t));
    DL(GetResource, void *, (uint32_t, int16_t));
    DL(GetHandleSize, long, (void *));
    DL(ReleaseResource, void, (void *));
    DL(HIObjectCreate, OSStatus, (CFStringRef, void *, void **));
    DL(HIViewSetFrame, OSStatus, (void *, const HIRectD *));
    DL(HIViewAddSubview, OSStatus, (void *, void *));
    DL(HIViewGetRoot, void *, (void *));
    DL(GetWindowEventTarget, void *, (void *));
    DL(GetControlEventTarget, void *, (void *));
    DL(InstallEventHandler, OSStatus, (void *, void *, unsigned long, const void *, void *, void *));

    carbon_ensure_window_host();

    void *win = (void *)(uintptr_t)x64_objc_unwrap(a[0]);
    const CRect *rect = (const CRect *)(uintptr_t)a[1];
    int16_t resID = (int16_t)a[2];
    uint32_t *out = (uint32_t *)(uintptr_t)a[7];
    if (out) *out = 0;
    STLOG("create win=%p rect=%p resID=%d\n", win, (void *)rect, resID);
    if (!win || !rect || !Get1Resource || !HIObjectCreate) return (uint32_t)-50;

    /* 1. the classic resource load — from the app's own current res file
     *    (Get1Resource; fall back to the whole chain like the classic CDEF) */
    void *th = Get1Resource('TEXT', resID);
    if (!th && GetResource) th = GetResource('TEXT', resID);
    if (!th) { STLOG("TEXT %d not found\n", resID); return (uint32_t)-192; }
    long tlen = GetHandleSize ? GetHandleSize(th) : 0;
    void *sh = Get1Resource('styl', resID);
    long slen = sh && GetHandleSize ? GetHandleSize(sh) : 0;
    CFAttributedStringRef text = styled_text(*(const uint8_t **)th, tlen,
                                             sh ? *(const uint8_t **)sh : NULL, slen);
    if (ReleaseResource) { ReleaseResource(th); if (sh) ReleaseResource(sh); }
    STLOG("TEXT %d loaded: %ld bytes, styl %ld bytes\n", resID, tlen, slen);

    /* 2. a real compositing HIView at `rect` */
    void *ctrl = NULL;
    HIObjectCreate(CFSTR("com.apple.hiview"), NULL, &ctrl);
    if (!ctrl) { CFRelease(text); return (uint32_t)-50; }
    if (HIViewSetFrame) {
        HIRectD fr = { rect->left, rect->top,
                       rect->right - rect->left, rect->bottom - rect->top };
        HIViewSetFrame(ctrl, &fr);
    }
    if (HIViewAddSubview && HIViewGetRoot) {
        void *root = HIViewGetRoot(win);
        if (root) HIViewAddSubview(root, ctrl);
    }

    /* 3. register + self-drawing/scroll handlers */
    struct box *b = box_alloc();
    b->ctrl = ctrl; b->win = win; b->scroll = 0;
    b->fs = CTFramesetterCreateWithAttributedString(text);
    CFRelease(text);
    if (!b->fs) { memset(b, 0, sizeof *b); return (uint32_t)-50; }

    if (InstallEventHandler && GetControlEventTarget) {
        struct { uint32_t cls, kind; } dr = { 'cntl', 4 /*kEventControlDraw*/ };
        struct { uint32_t cls, kind; } ht = { 'cntl', 3 /*kEventControlHitTest*/ };
        void *t = GetControlEventTarget(ctrl);
        InstallEventHandler(t, (void *)box_draw_handler, 1, &dr, b, NULL);
        InstallEventHandler(t, (void *)box_hit_handler, 1, &ht, b, NULL);
    }
    if (InstallEventHandler && GetWindowEventTarget) {
        struct { uint32_t cls, kind; } wh = { 'mous', 10 /*kEventMouseWheelMoved*/ };
        InstallEventHandler(GetWindowEventTarget(win), (void *)box_wheel_handler,
                            1, &wh, b, NULL);
    }

    if (out) *out = WRAPH(ctrl);
    STLOG("created ctrl=%p\n", ctrl);
    return 0;
}

/* box_for_win is used by the wheel handler indirectly; keep -Wunused quiet. */
__attribute__((unused)) static void *st_unused_ref = (void *)box_for_win;
