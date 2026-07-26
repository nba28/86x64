/*
 * carbon_classic_alert.c — one job: materialize and run an AUTHENTIC CLASSIC
 * Carbon alert (real Carbon window + self-drawn classic HIViews) and return the
 * classic item number the user picked.
 *
 * WHY (measured, not assumed — probe transcript in the commit message; guard in
 * tests-i386/carbon_classic_alert_test.sh)
 * -------------------------------------------------------------------------
 * FIRST we fixed and re-tested the SUBSTRATE, because that is the deep universal
 * fix: a translated pure-Carbon process was NOT a foreground GUI app, so no
 * Carbon window could become key and every HIToolbox modal loop starved.  That
 * is real and is now fixed in carbon_appkit_host.c (carbon_ensure_foreground:
 * NSApplicationLoad + TransformProcessType(kProcessTransformToForeground-
 * Application) + SetFrontProcess + activateIgnoringOtherApps).  MEASURED, on a
 * natively-compiled x86_64 probe:
 *      before: [NSApp activationPolicy] = Prohibited, NSApp isActive = NO,
 *              keyWindow = nil, ActiveNonFloatingWindow() = NULL,
 *              IsWindowActive(alert) = false            -> starved, as theorised
 *      after : activationPolicy = Regular, isActive = YES, keyWindow = the alert,
 *              ActiveNonFloatingWindow() = the alert, IsWindowActive = true,
 *              and an InstallEventLoopTimer callback firing continuously (0.2s)
 *              throughout -> the Carbon event loop IS pumping.
 * So the substrate defect is real and is fixed.  It is NOT, however, what kills
 * the standard alert:
 *
 *   (1) THE NATIVE STANDARD ALERT IS INERT ANYWAY.  With all of the above true,
 *       RunStandardAlert STILL never returns.  Not for a synthetic HID click on
 *       the OK button, not for a synthetic Return, and not for
 *       AXUIElementPerformAction(kAXPressAction) on the alert's own AXButton
 *       "OK" issued from a separate Accessibility-trusted process (it reports
 *       kAXErrorSuccess and has no effect).  Replacing RunAppModalLoopForWindow
 *       with BeginAppModalStateForWindow + RunApplicationEventLoop does not help.
 *       CONTROL: the identical synthetic input dismisses an NSAlert instantly, so
 *       the input path itself is good.  Reproduced in a pristine, code-signed
 *       .app bundle launched by LaunchServices with NO translator in the picture,
 *       so this is a 64-bit HIToolbox limitation, not an 86x64 defect.  It is
 *       exactly the DEAD native alert the tester reported for Civ IV.
 *
 *   (2) AND IT IS NOT AUTHENTIC.  Screenshotted: modern system font, a modern
 *       rounded blue "pill" default button, modern control chrome.  Only the
 *       LAYOUT is classic.  Routing to it buys no fidelity.
 *
 * So the authentic path is the one the project already uses everywhere else
 * (the authentic-original-look rule): re-materialize the classic UI on the REAL
 * classic substrate — a real Carbon window plus SELF-DRAWN classic HIViews
 * (com.apple.hiview + kEventControlDraw/HitTest/Track through CoreGraphics +
 * CoreText), the same live machinery that draws Halo's EULA scrolling text box,
 * the settings group frames, popups and edit fields, whose clicks and drags the tester
 * has confirmed working on screen.
 *
 * WHAT IT DRAWS: the classic Aqua alert — movable-modal window, 64x64 alert icon
 * at the left, Lucida Grande Bold 13 message, Lucida Grande 11 informative text,
 * classic bevelled push buttons bottom-right in the classic order
 * [Other] [Cancel] [OK], default button in the classic aqua-blue gradient.
 * Lucida Grande (still shipped in /System/Library/Fonts) is the real classic
 * system font, so the text matches the era rather than the modern UI font.
 *
 * UNIVERSAL: nothing keys on an app name or an alert string — it triggers on the
 * standard-alert API family, so every translated Carbon app gets the classic
 * alert.  SINGLE PURPOSE: this file only builds+runs the classic alert; the i386
 * argument marshalling stays in carbon_standardalert_shim.m, the Dialog Manager
 * in carbon_dialog_shim.m, nibs in carbon_nib_shim.c.
 *
 * NEVER REGRESSES FUNCTION, two ways:
 *   - returns 0 if the classic window cannot be materialized, and the caller
 *     falls back to its existing working AppKit modal;
 *   - the modal loop is OUR OWN ReceiveNextEvent/SendEventToEventTarget pump
 *     (re-entrancy-safe: it does not Quit an outer RunApplicationEventLoop the
 *     app may already be inside), and if it observes no events at all within
 *     CLASSIC_ALERT_PROOF_SECS and nothing ever drew, it tears the window down
 *     and returns 0 so the caller still falls back.  A dead un-dismissable modal
 *     is never an outcome.
 */

#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

/* AppKit window-management + foreground bootstrap (carbon_appkit_host.c). */
extern void carbon_ensure_window_host(void);
extern void carbon_ensure_foreground(void);

typedef int32_t OSStatus;
typedef void *WindowRef, *ControlRef, *HIViewRef, *HIObjectRef, *EventRef, *EventTargetRef;
typedef struct { int16_t top, left, bottom, right; } CRect;
typedef struct { double x, y, w, h; } HIRectD;

#define kWinCompositing (1u << 19)
#define kWinStdHandler  (1u << 25)
#define kMovableModalWindowClass 4u
#define kAlertWindowClass        1u
#define eventNotHandledErr   ((OSStatus)-9874)

/* If the classic window is up but the pump delivers NOTHING for this long and
 * the alert never even drew, we conclude this OS will not drive it and fall back
 * to the caller's modal. */
#define CLASSIC_ALERT_PROOF_SECS 3.0

/* ---- native entry points (all dlsym: the C wrappers are header-gated on a
 *      modern SDK but the symbols are live in the shared HIToolbox) ---------- */
static OSStatus (*n_CreateNewWindow)(uint32_t, uint32_t, const CRect *, WindowRef *);
static uint32_t (*n_GetAvailableWindowAttributes)(uint32_t);
static OSStatus (*n_SetWindowTitleWithCFString)(WindowRef, CFStringRef);
static void     (*n_ShowWindow)(WindowRef);
static void     (*n_SelectWindow)(WindowRef);
static void     (*n_DisposeWindow)(WindowRef);
static OSStatus (*n_RepositionWindow)(WindowRef, WindowRef, uint32_t);
static OSStatus (*n_GetWindowBounds)(WindowRef, uint32_t, CRect *);
static OSStatus (*n_BeginAppModalStateForWindow)(WindowRef);
static OSStatus (*n_EndAppModalStateForWindow)(WindowRef);
static HIViewRef(*n_HIViewGetRoot)(WindowRef);
static HIViewRef(*n_HIViewGetFirstSubview)(HIViewRef);
static HIViewRef(*n_HIViewGetNextView)(HIViewRef);
static OSStatus (*n_HIViewGetBounds)(HIViewRef, HIRectD *);
static OSStatus (*n_HIViewSetFrame)(HIViewRef, const HIRectD *);
static OSStatus (*n_HIViewAddSubview)(HIViewRef, HIViewRef);
static OSStatus (*n_HIViewSetVisible)(HIViewRef, uint8_t);
static OSStatus (*n_HIViewSetNeedsDisplay)(HIViewRef, uint8_t);
static OSStatus (*n_HIObjectCreate)(CFStringRef, void *, HIObjectRef *);
/* GetControlEventTarget / GetWindowEventTarget / GetEventDispatcherTarget return
 * 64-bit POINTERS — never declare them OSStatus (that truncation once faulted
 * HIToolbox's handler walk; see carbon_nib_shim.c). */
static void *   (*n_GetControlEventTarget)(ControlRef);
static void *   (*n_GetWindowEventTarget)(WindowRef);
static void *   (*n_GetEventDispatcherTarget)(void);
static OSStatus (*n_InstallEventHandler)(void *, void *, uint32_t, const void *, void *, void *);
static OSStatus (*n_GetEventParameter)(EventRef, uint32_t, uint32_t, uint32_t *, unsigned long, unsigned long *, void *);
static OSStatus (*n_SetEventParameter)(EventRef, uint32_t, uint32_t, unsigned long, const void *);
static OSStatus (*n_ReceiveNextEvent)(uint32_t, const void *, double, uint8_t, EventRef *);
static OSStatus (*n_SendEventToEventTarget)(EventRef, void *);
static void     (*n_ReleaseEvent)(EventRef);

static int g_resolved;
static void resolve_once(void) {
    if (g_resolved) return;
    g_resolved = 1;
#define R(f) n_##f = (void *)dlsym(RTLD_DEFAULT, #f)
    R(CreateNewWindow); R(GetAvailableWindowAttributes); R(SetWindowTitleWithCFString);
    R(ShowWindow); R(SelectWindow); R(DisposeWindow); R(RepositionWindow);
    R(GetWindowBounds); R(BeginAppModalStateForWindow); R(EndAppModalStateForWindow);
    R(HIViewGetRoot); R(HIViewGetFirstSubview); R(HIViewGetNextView); R(HIViewGetBounds);
    R(HIViewSetFrame); R(HIViewAddSubview); R(HIViewSetVisible); R(HIViewSetNeedsDisplay);
    R(HIObjectCreate); R(GetControlEventTarget); R(GetWindowEventTarget);
    R(GetEventDispatcherTarget); R(InstallEventHandler);
    R(GetEventParameter); R(SetEventParameter);
    R(ReceiveNextEvent); R(SendEventToEventTarget); R(ReleaseEvent);
#undef R
}

static int ca_trace(void) {
    static int v = -1;
    if (v < 0) v = getenv("CARBON_ALERT_TRACE") != NULL || getenv("CARBON_DIALOG_TRACE") != NULL;
    return v;
}
#define CA(...) do { if (ca_trace()) { fprintf(stderr, "[classicalert] " __VA_ARGS__); fflush(stderr); } } while (0)

/* ================= classic metrics (Mac OS X HIG, Aqua alert) ============== */
enum {
    AL_W          = 420,   /* classic alert width                          */
    AL_MARGIN     = 20,
    AL_ICON       = 64,    /* the alert icon is 64x64                       */
    AL_TEXT_X     = 100,   /* icon (20..84) + a 16pt gutter                 */
    AL_BTN_H      = 20,    /* classic Aqua push-button height               */
    AL_BTN_MINW   = 68,    /* classic minimum push-button width             */
    AL_BTN_GAP    = 12,
    AL_MSG_SIZE   = 13,    /* Lucida Grande Bold 13 = classic message       */
    AL_INFO_SIZE  = 11,    /* Lucida Grande 11 = classic informative text   */
    AL_MAXBTN     = 3,
};
#define AL_TEXT_W (AL_W - AL_TEXT_X - AL_MARGIN)

/* ---------------------------- classic text ------------------------------- */
/* Lucida Grande is the real classic system font and is still shipped; fall back
 * to Helvetica — never the modern system font, which is what makes an alert read
 * as "new". */
static CTFontRef classic_font(double size, int bold) {
    CFStringRef want = bold ? CFSTR("LucidaGrande-Bold") : CFSTR("LucidaGrande");
    CTFontRef f = CTFontCreateWithName(want, size, NULL);
    if (f) {
        CFStringRef nm = CTFontCopyPostScriptName(f);
        int ok = nm && CFStringCompare(nm, want, 0) == kCFCompareEqualTo;
        if (nm) CFRelease(nm);
        if (ok) return f;
        CFRelease(f);
    }
    return CTFontCreateWithName(bold ? CFSTR("Helvetica-Bold") : CFSTR("Helvetica"), size, NULL);
}

static CFAttributedStringRef make_attr(const char *utf8, double size, int bold,
                                       double r, double g, double b) {
    CFStringRef s = CFStringCreateWithCString(NULL, utf8 ? utf8 : "", kCFStringEncodingUTF8);
    if (!s) s = CFStringCreateWithCString(NULL, utf8 ? utf8 : "", kCFStringEncodingMacRoman);
    if (!s) return NULL;
    CTFontRef font = classic_font(size, bold);
    CGColorRef col = CGColorCreateGenericRGB(r, g, b, 1);
    CFStringRef k[] = { kCTFontAttributeName, kCTForegroundColorAttributeName };
    CFTypeRef v[] = { font, col };
    CFDictionaryRef a = CFDictionaryCreate(NULL, (const void **)k, (const void **)v, 2,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef as = CFAttributedStringCreate(NULL, s, a);
    CFRelease(a); CFRelease(col); CFRelease(font); CFRelease(s);
    return as;
}

/* Height of `utf8` wrapped to `width`, in points (0 for empty). */
static double wrap_height(const char *utf8, double width, double size, int bold) {
    if (!utf8 || !utf8[0]) return 0;
    CFAttributedStringRef as = make_attr(utf8, size, bold, 0, 0, 0);
    if (!as) return 0;
    CTFramesetterRef fs = CTFramesetterCreateWithAttributedString(as);
    CFRange fit;
    CGSize sz = CTFramesetterSuggestFrameSizeWithConstraints(
        fs, CFRangeMake(0, 0), NULL, CGSizeMake(width, 100000), &fit);
    CFRelease(fs); CFRelease(as);
    return sz.height;
}

/* Draw `utf8` wrapped into (x,yTop,w,h) of a FLIPPED (y-down) HIView draw
 * context whose total height is H.  CoreText renders y-up, so flip a saved CTM
 * about H — the carbon_nib_shim.c / carbon_scrolltext_shim.c recipe. */
static void draw_wrapped(CGContextRef cg, const char *utf8, double x, double yTop,
                         double w, double h, double H, double size, int bold,
                         double r, double g, double b) {
    if (!utf8 || !utf8[0] || h <= 0) return;
    CFAttributedStringRef as = make_attr(utf8, size, bold, r, g, b);
    if (!as) return;
    CTFramesetterRef fs = CTFramesetterCreateWithAttributedString(as);
    CGMutablePathRef p = CGPathCreateMutable();
    CGPathAddRect(p, NULL, CGRectMake(x, H - yTop - h, w, h));
    CTFrameRef fr = CTFramesetterCreateFrame(fs, CFRangeMake(0, 0), p, NULL);
    CGContextSaveGState(cg);
    CGContextTranslateCTM(cg, 0, H);
    CGContextScaleCTM(cg, 1, -1);
    CGContextSetTextMatrix(cg, CGAffineTransformIdentity);
    if (fr) CTFrameDraw(fr, cg);
    CGContextRestoreGState(cg);
    if (fr) CFRelease(fr);
    CGPathRelease(p); CFRelease(fs); CFRelease(as);
}

/* One line, centred in (x,yTop,w,h) — button titles. */
static void draw_centered_line(CGContextRef cg, const char *utf8, double x, double yTop,
                               double w, double h, double H, double size, int bold,
                               double r, double g, double b) {
    if (!utf8 || !utf8[0]) return;
    CFAttributedStringRef as = make_attr(utf8, size, bold, r, g, b);
    if (!as) return;
    CTLineRef line = CTLineCreateWithAttributedString(as);
    CGFloat asc = 0, desc = 0;
    double tw = CTLineGetTypographicBounds(line, &asc, &desc, NULL);
    double tx = x + (w - tw) / 2.0;
    double baselineFromTop = yTop + (h - (asc + desc)) / 2.0 + asc;
    CGContextSaveGState(cg);
    CGContextTranslateCTM(cg, 0, H);
    CGContextScaleCTM(cg, 1, -1);
    CGContextSetTextMatrix(cg, CGAffineTransformIdentity);
    CGContextSetTextPosition(cg, tx, H - baselineFromTop);
    CTLineDraw(line, cg);
    CGContextRestoreGState(cg);
    CFRelease(line); CFRelease(as);
}

static double line_width(const char *utf8, double size, int bold) {
    if (!utf8 || !utf8[0]) return 0;
    CFAttributedStringRef as = make_attr(utf8, size, bold, 0, 0, 0);
    if (!as) return 0;
    CTLineRef l = CTLineCreateWithAttributedString(as);
    double w = CTLineGetTypographicBounds(l, NULL, NULL, NULL);
    CFRelease(l); CFRelease(as);
    return w;
}

/* --------------------------- classic drawing ----------------------------- */
static void stroke_round(CGContextRef cg, CGRect r, double rad) {
    CGPathRef p = CGPathCreateWithRoundedRect(r, rad, rad, NULL);
    CGContextAddPath(cg, p); CGContextStrokePath(cg); CGPathRelease(p);
}
/* Classic Aqua vertical gradient clipped to a rounded rect. */
static void grad_round(CGContextRef cg, CGRect r, double rad,
                       const CGFloat top[4], const CGFloat bot[4]) {
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGFloat comps[8] = { top[0], top[1], top[2], top[3], bot[0], bot[1], bot[2], bot[3] };
    CGFloat locs[2] = { 0, 1 };
    CGGradientRef gr = CGGradientCreateWithColorComponents(cs, comps, locs, 2);
    CGPathRef p = CGPathCreateWithRoundedRect(r, rad, rad, NULL);
    CGContextSaveGState(cg);
    CGContextAddPath(cg, p); CGContextClip(cg);
    CGContextDrawLinearGradient(cg, gr, CGPointMake(0, r.origin.y),
                                CGPointMake(0, r.origin.y + r.size.height), 0);
    CGContextRestoreGState(cg);
    CGPathRelease(p); CGGradientRelease(gr); CGColorSpaceRelease(cs);
}
static void grad_ellipse(CGContextRef cg, CGRect box, const CGFloat top[4], const CGFloat bot[4]) {
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGFloat comps[8] = { top[0], top[1], top[2], top[3], bot[0], bot[1], bot[2], bot[3] };
    CGFloat locs[2] = { 0, 1 };
    CGGradientRef gr = CGGradientCreateWithColorComponents(cs, comps, locs, 2);
    CGContextSaveGState(cg);
    CGContextAddEllipseInRect(cg, box); CGContextClip(cg);
    CGContextDrawLinearGradient(cg, gr, CGPointMake(0, box.origin.y),
                                CGPointMake(0, box.origin.y + box.size.height), 0);
    CGContextRestoreGState(cg);
    CGGradientRelease(gr); CGColorSpaceRelease(cs);
}

/* Classic alert icons, self-drawn (the classic 'stop'/'caution'/'note' icon
 * resources are long gone from the OS).  Drawn in a y-DOWN context. */
static void draw_alert_icon(CGContextRef cg, int type, double x, double y, double s) {
    CGRect box = CGRectInset(CGRectMake(x, y, s, s), 2, 2);
    CGContextSaveGState(cg);
    if (type == 2) {                                   /* caution: yellow triangle */
        CGContextSetRGBFillColor(cg, 0.98, 0.80, 0.09, 1);
        CGContextMoveToPoint(cg, x + s / 2, y + 2);
        CGContextAddLineToPoint(cg, x + s - 2, y + s - 4);
        CGContextAddLineToPoint(cg, x + 2, y + s - 4);
        CGContextClosePath(cg); CGContextFillPath(cg);
        CGContextSetRGBStrokeColor(cg, 0.62, 0.48, 0.03, 1);
        CGContextSetLineWidth(cg, 1.5);
        CGContextMoveToPoint(cg, x + s / 2, y + 2);
        CGContextAddLineToPoint(cg, x + s - 2, y + s - 4);
        CGContextAddLineToPoint(cg, x + 2, y + s - 4);
        CGContextClosePath(cg); CGContextStrokePath(cg);
        CGContextSetRGBFillColor(cg, 0.15, 0.12, 0.02, 1);
        CGContextFillRect(cg, CGRectMake(x + s / 2 - 2.5, y + s * 0.34, 5, s * 0.32));
        CGContextFillRect(cg, CGRectMake(x + s / 2 - 2.5, y + s * 0.72, 5, 5));
        CGContextRestoreGState(cg);
        return;
    }
    if (type == 1) {                                   /* note: blue badge */
        CGFloat t[4] = { 0.55, 0.72, 0.95, 1 }, b[4] = { 0.20, 0.40, 0.76, 1 };
        grad_ellipse(cg, box, t, b);
        CGContextSetRGBFillColor(cg, 1, 1, 1, 1);
        CGContextFillRect(cg, CGRectMake(x + s / 2 - 3, y + s * 0.26, 6, 6));
        CGContextFillRect(cg, CGRectMake(x + s / 2 - 3, y + s * 0.42, 6, s * 0.34));
    } else {                                           /* stop (0) / plain: red badge */
        CGFloat t[4] = { 0.92, 0.32, 0.28, 1 }, b[4] = { 0.68, 0.10, 0.08, 1 };
        grad_ellipse(cg, box, t, b);
        CGContextSetRGBFillColor(cg, 1, 1, 1, 1);
        CGContextFillRect(cg, CGRectMake(x + s / 2 - 3, y + s * 0.24, 6, s * 0.40));
        CGContextFillRect(cg, CGRectMake(x + s / 2 - 3, y + s * 0.70, 6, 6));
    }
    CGContextSetRGBStrokeColor(cg, 0.35, 0.35, 0.38, 0.55);
    CGContextSetLineWidth(cg, 1);
    CGContextStrokeEllipseInRect(cg, box);
    CGContextRestoreGState(cg);
}

/* ------------------------------ alert state ------------------------------ */
struct alert;
struct albtn {
    char   title[128];
    int    item;                /* classic 1/2/3 */
    CGRect frame;               /* content coords, y DOWN */
    int    isDefault, isCancel;
    HIViewRef view;
    int    pressed;
    struct alert *owner;
};
struct alert {
    int    type;
    char   message[1024], info[2048];
    struct albtn btn[AL_MAXBTN];
    int    nbtn;
    int    result;              /* classic item hit, 0 = none yet */
    int    done;
    int    drew;                /* our body view actually drew at least once */
    WindowRef win;
    HIViewRef body;
    double contentW, contentH, msgH, infoH;
};

static CGContextRef draw_cg(EventRef ev) {
    CGContextRef cg = NULL;
    if (n_GetEventParameter)
        n_GetEventParameter(ev, 'cntx', 'cntx', NULL, sizeof(CGContextRef), NULL, &cg);
    return cg;
}
static int view_size(HIViewRef v, double *w, double *h) {
    HIRectD b = { 0, 0, 0, 0 };
    if (!v || !n_HIViewGetBounds || n_HIViewGetBounds(v, &b) != 0) return 0;
    if (b.w <= 0 || b.h <= 0) return 0;
    *w = b.w; *h = b.h; return 1;
}

/* Body view: classic alert background + icon + message + informative text. */
static OSStatus body_draw(void *call, EventRef ev, void *ud) {
    (void)call;
    struct alert *a = (struct alert *)ud;
    CGContextRef cg = draw_cg(ev);
    double W = 0, H = 0;
    if (!a || !cg || !view_size(a->body, &W, &H)) return eventNotHandledErr;
    a->drew = 1;
    CGContextSaveGState(cg);
    /* the classic flat platinum/aqua alert background */
    CGContextSetRGBFillColor(cg, 0.929, 0.929, 0.929, 1.0);
    CGContextFillRect(cg, CGRectMake(0, 0, W, H));
    draw_alert_icon(cg, a->type, AL_MARGIN, AL_MARGIN, AL_ICON);
    draw_wrapped(cg, a->message, AL_TEXT_X, AL_MARGIN, AL_TEXT_W, a->msgH, H,
                 AL_MSG_SIZE, 1, 0.0, 0.0, 0.0);
    if (a->info[0])
        draw_wrapped(cg, a->info, AL_TEXT_X, AL_MARGIN + a->msgH + 8, AL_TEXT_W,
                     a->infoH, H, AL_INFO_SIZE, 0, 0.0, 0.0, 0.0);
    CGContextRestoreGState(cg);
    return 0;
}

/* Push button: classic Aqua bevel; the default button in classic aqua blue. */
static OSStatus btn_draw(void *call, EventRef ev, void *ud) {
    (void)call;
    struct albtn *b = (struct albtn *)ud;
    CGContextRef cg = draw_cg(ev);
    double W = 0, H = 0;
    if (!b || !cg || !view_size(b->view, &W, &H)) return eventNotHandledErr;
    CGRect r = CGRectMake(0.5, 0.5, W - 1, H - 1);
    double rad = H / 2.0;
    CGContextSaveGState(cg);
    if (b->isDefault) {
        CGFloat top[4]  = { 0.42, 0.64, 0.94, 1 }, bot[4]  = { 0.13, 0.38, 0.82, 1 };
        CGFloat ptop[4] = { 0.20, 0.44, 0.80, 1 }, pbot[4] = { 0.08, 0.26, 0.62, 1 };
        grad_round(cg, r, rad, b->pressed ? ptop : top, b->pressed ? pbot : bot);
        CGContextSetRGBStrokeColor(cg, 0.10, 0.28, 0.60, 1);
        CGContextSetLineWidth(cg, 1);
        stroke_round(cg, r, rad);
        draw_centered_line(cg, b->title, 0, 0, W, H, H, AL_INFO_SIZE + 1, 0, 1, 1, 1);
    } else {
        CGFloat top[4]  = { 1.00, 1.00, 1.00, 1 }, bot[4]  = { 0.86, 0.86, 0.87, 1 };
        CGFloat ptop[4] = { 0.80, 0.80, 0.82, 1 }, pbot[4] = { 0.66, 0.66, 0.68, 1 };
        grad_round(cg, r, rad, b->pressed ? ptop : top, b->pressed ? pbot : bot);
        CGContextSetRGBStrokeColor(cg, 0.58, 0.58, 0.60, 1);
        CGContextSetLineWidth(cg, 1);
        stroke_round(cg, r, rad);
        draw_centered_line(cg, b->title, 0, 0, W, H, H, AL_INFO_SIZE + 1, 0, 0.05, 0.05, 0.07);
    }
    CGContextRestoreGState(cg);
    return 0;
}

/* A base com.apple.hiview is not hit-testable by default, so mouse-downs pass
 * straight through and it never receives Hit/Track.  Claim the whole view as one
 * clickable part — the carbon_nib_shim.c sd_hittest recipe. */
static OSStatus btn_hittest(void *call, EventRef ev, void *ud) {
    (void)call; (void)ud;
    int16_t part = 1;                       /* kControlButtonPart */
    if (n_SetEventParameter) n_SetEventParameter(ev, 'cprt', 'cprt', sizeof part, &part);
    return 0;
}

/* Classic press-and-track: keep the button drawn "pressed" while the mouse is
 * held, follow the pointer in and out of the button exactly as the classic
 * Control Manager did, and fire only on a mouse-UP inside.  The classic
 * TrackMouseLocation is gone from 64-bit HIToolbox, so poll the live pointer
 * through CoreGraphics — the tracking itself then needs no event pump. */
static OSStatus btn_track(void *call, EventRef ev, void *ud) {
    (void)call; (void)ev;
    struct albtn *b = (struct albtn *)ud;
    if (!b || !b->owner) return eventNotHandledErr;
    struct alert *a = b->owner;

    CRect wb = { 0, 0, 0, 0 };
    if (n_GetWindowBounds) n_GetWindowBounds(a->win, 33 /*kWindowContentRgn*/, &wb);
    int inside = 1, was = -1;
    for (;;) {
        CGEventRef e = CGEventCreate(NULL);
        CGPoint g = e ? CGEventGetLocation(e) : CGPointMake(-1, -1);
        if (e) CFRelease(e);
        double lx = g.x - wb.left, ly = g.y - wb.top;
        inside = (lx >= b->frame.origin.x && lx <= b->frame.origin.x + b->frame.size.width &&
                  ly >= b->frame.origin.y && ly <= b->frame.origin.y + b->frame.size.height);
        if (inside != was) {
            was = inside;
            b->pressed = inside;
            if (n_HIViewSetNeedsDisplay) n_HIViewSetNeedsDisplay(b->view, 1);
        }
        if (!CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kCGMouseButtonLeft))
            break;
        usleep(15000);
    }
    b->pressed = 0;
    if (n_HIViewSetNeedsDisplay) n_HIViewSetNeedsDisplay(b->view, 1);
    if (inside) {
        a->result = b->item;
        a->done = 1;
        CA("button '%s' -> item %d\n", b->title, b->item);
    }
    return 0;
}

/* Return/Enter = default button, Escape = cancel — the classic alert keyboard
 * contract, so an alert is never un-dismissable. */
static OSStatus key_handler(void *call, EventRef ev, void *ud) {
    (void)call;
    struct alert *a = (struct alert *)ud;
    if (!a || !n_GetEventParameter) return eventNotHandledErr;
    char ch = 0;
    if (n_GetEventParameter(ev, 'kchr' /*kEventParamKeyMacCharCodes*/, 'char',
                            NULL, sizeof ch, NULL, &ch) != 0)
        return eventNotHandledErr;
    int want = 0;
    if (ch == 13 || ch == 3)  want = 1;          /* Return / Enter -> default */
    else if (ch == 27)        want = 2;          /* Escape -> cancel          */
    if (!want) return eventNotHandledErr;
    for (int i = 0; i < a->nbtn; i++) {
        if ((want == 1 && a->btn[i].isDefault) || (want == 2 && a->btn[i].isCancel)) {
            a->result = a->btn[i].item;
            a->done = 1;
            CA("key %d -> item %d\n", ch, a->result);
            return 0;
        }
    }
    return eventNotHandledErr;
}

/* Window close -> the cancel button (classic behaviour), never a dead modal. */
static OSStatus close_handler(void *call, EventRef ev, void *ud) {
    (void)call; (void)ev;
    struct alert *a = (struct alert *)ud;
    if (!a || a->nbtn <= 0) return eventNotHandledErr;
    if (!a->result) {
        a->result = a->btn[0].item;
        for (int i = 0; i < a->nbtn; i++) if (a->btn[i].isCancel) a->result = a->btn[i].item;
    }
    a->done = 1;
    return 0;
}

static HIViewRef make_view(const CRect *frame, void *drawFn, void *hitFn,
                           void *trackFn, void *ud, HIViewRef parent) {
    HIViewRef v = NULL;
    if (!n_HIObjectCreate) return NULL;
    n_HIObjectCreate(CFSTR("com.apple.hiview"), NULL, (HIObjectRef *)&v);
    if (!v) return NULL;
    if (n_HIViewSetFrame && frame) {
        HIRectD r = { (double)frame->left, (double)frame->top,
                      (double)(frame->right - frame->left),
                      (double)(frame->bottom - frame->top) };
        n_HIViewSetFrame(v, &r);
    }
    if (n_InstallEventHandler && n_GetControlEventTarget) {
        void *tgt = n_GetControlEventTarget(v);
        struct { uint32_t cls, kind; } dr = { 'cntl', 4  };  /* kEventControlDraw    */
        struct { uint32_t cls, kind; } ht = { 'cntl', 3  };  /* kEventControlHitTest */
        struct { uint32_t cls, kind; } hk = { 'cntl', 1  };  /* kEventControlHit     */
        struct { uint32_t cls, kind; } tk = { 'cntl', 51 };  /* kEventControlTrack   */
        if (drawFn)  n_InstallEventHandler(tgt, drawFn,  1, &dr, ud, NULL);
        if (hitFn)   n_InstallEventHandler(tgt, hitFn,   1, &ht, ud, NULL);
        if (trackFn) n_InstallEventHandler(tgt, trackFn, 1, &tk, ud, NULL);
        if (trackFn) n_InstallEventHandler(tgt, trackFn, 1, &hk, ud, NULL);
    }
    if (n_HIViewSetVisible) n_HIViewSetVisible(v, 1);   /* base hiview starts hidden */
    if (parent && n_HIViewAddSubview) n_HIViewAddSubview(parent, v);
    return v;
}

/* Content view of a compositing window (the root may include the title band). */
static HIViewRef content_view_of(WindowRef win, HIViewRef root) {
    if (!root || !n_HIViewGetFirstSubview || !n_HIViewGetBounds) return root;
    double ch = 0;
    CRect cr = { 0, 0, 0, 0 };
    if (n_GetWindowBounds && n_GetWindowBounds(win, 33 /*kWindowContentRgn*/, &cr) == 0)
        ch = (double)(cr.bottom - cr.top);
    HIRectD rb = { 0, 0, 0, 0 };
    n_HIViewGetBounds(root, &rb);
    if (ch > 0 && rb.h <= ch + 1.0) return root;
    HIViewRef first = n_HIViewGetFirstSubview(root);
    if (ch > 0 && n_HIViewGetNextView)
        for (HIViewRef v = first; v; v = n_HIViewGetNextView(v)) {
            HIRectD vb = { 0, 0, 0, 0 };
            n_HIViewGetBounds(v, &vb);
            if (vb.h >= ch - 1.0 && vb.h <= ch + 1.0) return v;
        }
    return first ? first : root;
}

/* Our own modal pump.  Deliberately NOT RunApplicationEventLoop: the translated
 * app may already be inside one, and QuitApplicationEventLoop would tear down
 * the app's loop rather than ours.  Returns 1 if the alert was dismissed, 0 if
 * the pump proved dead (no events at all for CLASSIC_ALERT_PROOF_SECS and the
 * alert never even drew) so the caller can fall back. */
static int run_modal_pump(struct alert *a) {
    if (!n_ReceiveNextEvent || !n_SendEventToEventTarget || !n_GetEventDispatcherTarget)
        return 0;
    void *disp = n_GetEventDispatcherTarget();
    if (!disp) return 0;
    double idle = 0;
    while (!a->done) {
        EventRef ev = NULL;
        OSStatus s = n_ReceiveNextEvent(0, NULL, 0.05 /*seconds*/, 1 /*pull*/, &ev);
        if (s == 0 && ev) {
            idle = 0;
            n_SendEventToEventTarget(ev, disp);
            if (n_ReleaseEvent) n_ReleaseEvent(ev);
            continue;
        }
        idle += 0.05;
        if (!a->drew && idle >= CLASSIC_ALERT_PROOF_SECS) {
            CA("pump proved dead (no events, never drew) -> fall back\n");
            return 0;
        }
    }
    return 1;
}

/* =============================== public API =============================== */
/*
 * Build + run the classic alert.  Button texts: NULL/"" = that button is absent
 * (the OK button always exists).  defaultButton / cancelButton are classic 1..3
 * item numbers (0 = OK default / no explicit cancel).  Returns the classic item
 * hit (1 = kAlertStdAlertOKButton, 2 = Cancel, 3 = Other), or 0 if the classic
 * alert could NOT be materialized/driven — the caller must then fall back.
 */
int carbon_classic_alert_run(int alertType, const char *message, const char *informative,
                             const char *okText, const char *cancelText, const char *otherText,
                             int defaultButton, int cancelButton) {
    resolve_once();
    if (!n_CreateNewWindow || !n_HIObjectCreate || !n_BeginAppModalStateForWindow || !n_ShowWindow)
        return 0;
    if (getenv("M64_NO_CLASSIC_ALERT")) return 0;   /* A/B kill switch for the guard */

    carbon_ensure_window_host();
    carbon_ensure_foreground();

    struct alert *a = (struct alert *)calloc(1, sizeof *a);
    if (!a) return 0;
    a->type = alertType;
    if (message)     strncpy(a->message, message, sizeof a->message - 1);
    if (informative) strncpy(a->info, informative, sizeof a->info - 1);
    if (!a->message[0]) strncpy(a->message, "Alert", sizeof a->message - 1);

    /* classic button order, right to left: [Other] [Cancel] [OK] */
    const char *txt[3] = { (okText && okText[0]) ? okText : "OK",
                           (cancelText && cancelText[0]) ? cancelText : NULL,
                           (otherText && otherText[0]) ? otherText : NULL };
    for (int i = 0; i < 3; i++) {
        if (!txt[i]) continue;
        struct albtn *b = &a->btn[a->nbtn++];
        strncpy(b->title, txt[i], sizeof b->title - 1);
        b->item = i + 1;
        b->owner = a;
    }
    int dflt = (defaultButton >= 1 && defaultButton <= 3) ? defaultButton : 1;
    for (int i = 0; i < a->nbtn; i++) {
        a->btn[i].isDefault = (a->btn[i].item == dflt);
        a->btn[i].isCancel  = (cancelButton >= 1 && a->btn[i].item == cancelButton);
    }
    if (cancelButton < 1)                       /* classic: Cancel is the Esc button */
        for (int i = 0; i < a->nbtn; i++) if (a->btn[i].item == 2) a->btn[i].isCancel = 1;

    /* ---- classic layout ---- */
    a->msgH  = wrap_height(a->message, AL_TEXT_W, AL_MSG_SIZE, 1);
    a->infoH = a->info[0] ? wrap_height(a->info, AL_TEXT_W, AL_INFO_SIZE, 0) : 0;
    double textBottom = AL_MARGIN + a->msgH + (a->infoH ? 8 + a->infoH : 0);
    double iconBottom = AL_MARGIN + AL_ICON;
    double btnTop = (textBottom > iconBottom ? textBottom : iconBottom) + 16;
    a->contentW = AL_W;
    a->contentH = btnTop + AL_BTN_H + AL_MARGIN;

    double x = AL_W - AL_MARGIN;
    for (int i = 0; i < a->nbtn; i++) {          /* OK is rightmost */
        double w = line_width(a->btn[i].title, AL_INFO_SIZE + 1, 0) + 28;
        if (w < AL_BTN_MINW) w = AL_BTN_MINW;
        a->btn[i].frame = CGRectMake(x - w, btnTop, w, AL_BTN_H);
        x -= w + AL_BTN_GAP;
    }

    /* ---- real Carbon window (movable modal = classic draggable alert) ---- */
    uint32_t cls = kMovableModalWindowClass;
    uint32_t attrs = kWinCompositing | kWinStdHandler;
    if (n_GetAvailableWindowAttributes)
        attrs &= n_GetAvailableWindowAttributes(cls) | kWinCompositing | kWinStdHandler;
    CRect wr = { 200, 300, (int16_t)(200 + a->contentH), (int16_t)(300 + AL_W) };
    WindowRef win = NULL;
    if (n_CreateNewWindow(cls, attrs, &wr, &win) != 0 || !win) {
        cls = kAlertWindowClass; win = NULL;
        if (n_CreateNewWindow(cls, kWinCompositing | kWinStdHandler, &wr, &win) != 0 || !win) {
            CA("CreateNewWindow failed for both window classes\n");
            free(a);
            return 0;
        }
    }
    a->win = win;
    if (n_SetWindowTitleWithCFString) n_SetWindowTitleWithCFString(win, CFSTR(""));
    if (n_RepositionWindow) n_RepositionWindow(win, NULL, 3 /*kWindowAlertPositionOnMainScreen*/);

    HIViewRef root = n_HIViewGetRoot ? n_HIViewGetRoot(win) : NULL;
    HIViewRef parent = content_view_of(win, root);
    if (!parent) { if (n_DisposeWindow) n_DisposeWindow(win); free(a); return 0; }

    CRect bodyR = { 0, 0, (int16_t)a->contentH, (int16_t)a->contentW };
    a->body = make_view(&bodyR, (void *)body_draw, NULL, NULL, a, parent);
    if (!a->body) { if (n_DisposeWindow) n_DisposeWindow(win); free(a); return 0; }
    for (int i = 0; i < a->nbtn; i++) {
        CRect br = { (int16_t)a->btn[i].frame.origin.y, (int16_t)a->btn[i].frame.origin.x,
                     (int16_t)(a->btn[i].frame.origin.y + a->btn[i].frame.size.height),
                     (int16_t)(a->btn[i].frame.origin.x + a->btn[i].frame.size.width) };
        a->btn[i].view = make_view(&br, (void *)btn_draw, (void *)btn_hittest,
                                   (void *)btn_track, &a->btn[i], parent);
    }
    if (n_InstallEventHandler && n_GetWindowEventTarget) {
        void *wt = n_GetWindowEventTarget(win);
        struct { uint32_t cls, kind; } kd = { 'keyb', 1 };  /* kEventRawKeyDown  */
        struct { uint32_t cls, kind; } wc = { 'wind', 72 }; /* kEventWindowClose */
        n_InstallEventHandler(wt, (void *)key_handler,   1, &kd, a, NULL);
        n_InstallEventHandler(wt, (void *)close_handler, 1, &wc, a, NULL);
    }

    n_ShowWindow(win);
    if (n_SelectWindow) n_SelectWindow(win);
    n_BeginAppModalStateForWindow(win);
    int live = run_modal_pump(a);
    if (n_EndAppModalStateForWindow) n_EndAppModalStateForWindow(win);
    if (n_DisposeWindow) n_DisposeWindow(win);

    int item = live ? (a->result ? a->result : a->btn[0].item) : 0;
    CA("classic alert -> item %d (live=%d)\n", item, live);
    free(a);
    return item;
}
