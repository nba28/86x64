/*
 * carbon_classic_widgets.c — THE shared classic-Carbon widget substrate.
 * See carbon_classic_widgets.h for what this is and why it exists.
 *
 * ONE JOB: draw and drive authentic classic Carbon controls on real Carbon
 * compositing HIViews.  No shim entry points live here, nothing keys on an app
 * name, and every widget triggers purely on the STRUCTURE it is handed (a rect,
 * a title, an item number), so every translated Carbon app gets the same classic
 * chrome from whichever shim materialized the UI.
 *
 * HISTORY: the button/text/window primitives were proven inside
 * carbon_classic_alert.c (confirmed on screen for Halo's EULA scroll box,
 * settings controls and the classic alert).  They are factored out here
 * UNCHANGED in behaviour so the classic Dialog Manager and the IBCarbon nib
 * materializer can share them instead of each growing a private copy — and so
 * the ONE piece that was still missing everywhere, a real classic EDIT TEXT
 * field with a caret and a selection, exists exactly once.
 */

#include "carbon_classic_widgets.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

/* ---------------------------- native binding ----------------------------- */
CCWStatus  (*ccw_CreateNewWindow)(uint32_t, uint32_t, const CCWRect *, CCWWindowRef *);
uint32_t   (*ccw_GetAvailableWindowAttributes)(uint32_t);
CCWStatus  (*ccw_SetWindowTitleWithCFString)(CCWWindowRef, CFStringRef);
void       (*ccw_ShowWindow)(CCWWindowRef);
void       (*ccw_SelectWindow)(CCWWindowRef);
void       (*ccw_DisposeWindow)(CCWWindowRef);
CCWStatus  (*ccw_RepositionWindow)(CCWWindowRef, CCWWindowRef, uint32_t);
CCWStatus  (*ccw_GetWindowBounds)(CCWWindowRef, uint32_t, CCWRect *);
CCWStatus  (*ccw_SetWindowBounds)(CCWWindowRef, uint32_t, const CCWRect *);
CCWStatus  (*ccw_BeginAppModalStateForWindow)(CCWWindowRef);
CCWStatus  (*ccw_EndAppModalStateForWindow)(CCWWindowRef);
CCWViewRef (*ccw_HIViewGetRoot)(CCWWindowRef);
CCWViewRef (*ccw_HIViewGetFirstSubview)(CCWViewRef);
CCWViewRef (*ccw_HIViewGetNextView)(CCWViewRef);
CCWStatus  (*ccw_HIViewGetBounds)(CCWViewRef, CCWRectD *);
CCWStatus  (*ccw_HIViewSetFrame)(CCWViewRef, const CCWRectD *);
CCWStatus  (*ccw_HIViewAddSubview)(CCWViewRef, CCWViewRef);
CCWStatus  (*ccw_HIViewSetVisible)(CCWViewRef, uint8_t);
CCWStatus  (*ccw_HIViewSetNeedsDisplay)(CCWViewRef, uint8_t);
CCWStatus  (*ccw_HIViewChangeFeatures)(CCWViewRef, uint64_t, uint64_t);
CCWStatus  (*ccw_HIObjectCreate)(CFStringRef, void *, void **);
void      *(*ccw_GetControlEventTarget)(CCWViewRef);
void      *(*ccw_GetWindowEventTarget)(CCWWindowRef);
void      *(*ccw_GetEventDispatcherTarget)(void);
CCWStatus  (*ccw_InstallEventHandler)(void *, void *, uint32_t, const void *, void *, void *);
CCWStatus  (*ccw_GetEventParameter)(CCWEventRef, uint32_t, uint32_t, uint32_t *, unsigned long, unsigned long *, void *);
CCWStatus  (*ccw_SetEventParameter)(CCWEventRef, uint32_t, uint32_t, unsigned long, const void *);
CCWStatus  (*ccw_ReceiveNextEvent)(uint32_t, const void *, double, uint8_t, CCWEventRef *);
CCWStatus  (*ccw_SendEventToEventTarget)(CCWEventRef, void *);
void       (*ccw_ReleaseEvent)(CCWEventRef);

int ccw_available(void) {
    static int state = -1;
    if (state >= 0) return state;
#define R(f) ccw_##f = (void *)dlsym(RTLD_DEFAULT, #f)
    R(CreateNewWindow); R(GetAvailableWindowAttributes); R(SetWindowTitleWithCFString);
    R(ShowWindow); R(SelectWindow); R(DisposeWindow); R(RepositionWindow);
    R(GetWindowBounds); R(SetWindowBounds);
    R(BeginAppModalStateForWindow); R(EndAppModalStateForWindow);
    R(HIViewGetRoot); R(HIViewGetFirstSubview); R(HIViewGetNextView); R(HIViewGetBounds);
    R(HIViewSetFrame); R(HIViewAddSubview); R(HIViewSetVisible); R(HIViewSetNeedsDisplay);
    R(HIViewChangeFeatures); R(HIObjectCreate);
    R(GetControlEventTarget); R(GetWindowEventTarget); R(GetEventDispatcherTarget);
    R(InstallEventHandler); R(GetEventParameter); R(SetEventParameter);
    R(ReceiveNextEvent); R(SendEventToEventTarget); R(ReleaseEvent);
#undef R
    state = (ccw_CreateNewWindow && ccw_HIObjectCreate && ccw_ShowWindow &&
             ccw_HIViewSetFrame && ccw_InstallEventHandler && ccw_GetControlEventTarget &&
             ccw_ReceiveNextEvent && ccw_SendEventToEventTarget &&
             ccw_GetEventDispatcherTarget) ? 1 : 0;
    return state;
}

int ccw_trace(void) {
    static int v = -1;
    if (v < 0) v = (getenv("CARBON_ALERT_TRACE") != NULL ||
                    getenv("CARBON_DIALOG_TRACE") != NULL) ? 1 : 0;
    return v;
}
void ccw_log(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fprintf(stderr, "[classic] ");
    vfprintf(stderr, fmt, ap);
    fflush(stderr);
    va_end(ap);
}

/* ------------------------------ classic text ------------------------------
 * Lucida Grande is the real classic system font and is still shipped in
 * /System/Library/Fonts; fall back to Helvetica — never the modern system font,
 * which is exactly what makes a re-created classic window read as "new". */
CTFontRef ccw_font(double size, int bold) {
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

/* Classic resource text is MacRoman, not UTF-8 (a curly apostrophe is 0xD5).
 * Decode UTF-8 first and fall back to MacRoman so no string is ever dropped. */
static CFStringRef ccw_cfstr(const char *utf8) {
    if (!utf8) utf8 = "";
    CFStringRef s = CFStringCreateWithCString(NULL, utf8, kCFStringEncodingUTF8);
    if (!s) s = CFStringCreateWithCString(NULL, utf8, kCFStringEncodingMacRoman);
    return s;
}

static CFAttributedStringRef make_attr(const char *utf8, double size, int bold,
                                       double r, double g, double b) {
    CFStringRef s = ccw_cfstr(utf8);
    if (!s) return NULL;
    CTFontRef font = ccw_font(size, bold);
    CGColorRef col = CGColorCreateGenericRGB(r, g, b, 1);
    CFStringRef k[] = { kCTFontAttributeName, kCTForegroundColorAttributeName };
    CFTypeRef v[] = { font, col };
    CFDictionaryRef a = CFDictionaryCreate(NULL, (const void **)k, (const void **)v, 2,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef as = CFAttributedStringCreate(NULL, s, a);
    CFRelease(a); CFRelease(col); CFRelease(font); CFRelease(s);
    return as;
}

double ccw_wrap_height(const char *utf8, double width, double size, int bold) {
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

double ccw_line_width(const char *utf8, double size, int bold) {
    if (!utf8 || !utf8[0]) return 0;
    CFAttributedStringRef as = make_attr(utf8, size, bold, 0, 0, 0);
    if (!as) return 0;
    CTLineRef l = CTLineCreateWithAttributedString(as);
    double w = l ? CTLineGetTypographicBounds(l, NULL, NULL, NULL) : 0;
    if (l) CFRelease(l);
    CFRelease(as);
    return w;
}

void ccw_draw_wrapped(CGContextRef cg, const char *utf8, double x, double yTop,
                      double w, double h, double H, double size, int bold,
                      double r, double g, double b) {
    if (!cg || !utf8 || !utf8[0] || h <= 0) return;
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

void ccw_draw_centered_line(CGContextRef cg, const char *utf8, double x, double yTop,
                            double w, double h, double H, double size, int bold,
                            double r, double g, double b) {
    if (!cg || !utf8 || !utf8[0]) return;
    CFAttributedStringRef as = make_attr(utf8, size, bold, r, g, b);
    if (!as) return;
    CTLineRef line = CTLineCreateWithAttributedString(as);
    if (!line) { CFRelease(as); return; }
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

void ccw_draw_left_line(CGContextRef cg, const char *utf8, double x, double baselineFromTop,
                        double H, double size, int bold, double r, double g, double b) {
    if (!cg || !utf8 || !utf8[0]) return;
    CFAttributedStringRef as = make_attr(utf8, size, bold, r, g, b);
    if (!as) return;
    CTLineRef line = CTLineCreateWithAttributedString(as);
    if (!line) { CFRelease(as); return; }
    CGContextSaveGState(cg);
    CGContextTranslateCTM(cg, 0, H);
    CGContextScaleCTM(cg, 1, -1);
    CGContextSetTextMatrix(cg, CGAffineTransformIdentity);
    CGContextSetTextPosition(cg, x, H - baselineFromTop);
    CTLineDraw(line, cg);
    CGContextRestoreGState(cg);
    CFRelease(line); CFRelease(as);
}

/* --------------------------- classic drawing ----------------------------- */
void ccw_stroke_round(CGContextRef cg, CGRect r, double rad) {
    CGPathRef p = CGPathCreateWithRoundedRect(r, rad, rad, NULL);
    CGContextAddPath(cg, p); CGContextStrokePath(cg); CGPathRelease(p);
}

void ccw_grad_round(CGContextRef cg, CGRect r, double rad,
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

void ccw_grad_ellipse(CGContextRef cg, CGRect box, const CGFloat top[4], const CGFloat bot[4]) {
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

/* The classic 'stop'/'caution'/'note' icon resources are long gone from the OS,
 * so draw them.  y-DOWN context. */
void ccw_draw_alert_icon(CGContextRef cg, int type, double x, double y, double s) {
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
        ccw_grad_ellipse(cg, box, t, b);
        CGContextSetRGBFillColor(cg, 1, 1, 1, 1);
        CGContextFillRect(cg, CGRectMake(x + s / 2 - 3, y + s * 0.26, 6, 6));
        CGContextFillRect(cg, CGRectMake(x + s / 2 - 3, y + s * 0.42, 6, s * 0.34));
    } else {                                           /* stop (0) / plain: red badge */
        CGFloat t[4] = { 0.92, 0.32, 0.28, 1 }, b[4] = { 0.68, 0.10, 0.08, 1 };
        ccw_grad_ellipse(cg, box, t, b);
        CGContextSetRGBFillColor(cg, 1, 1, 1, 1);
        CGContextFillRect(cg, CGRectMake(x + s / 2 - 3, y + s * 0.24, 6, s * 0.40));
        CGContextFillRect(cg, CGRectMake(x + s / 2 - 3, y + s * 0.70, 6, 6));
    }
    CGContextSetRGBStrokeColor(cg, 0.35, 0.35, 0.38, 0.55);
    CGContextSetLineWidth(cg, 1);
    CGContextStrokeEllipseInRect(cg, box);
    CGContextRestoreGState(cg);
}

/* ----------------------------- HIView plumbing --------------------------- */
CGContextRef ccw_draw_cg(CCWEventRef ev) {
    CGContextRef cg = NULL;
    if (ccw_GetEventParameter)
        ccw_GetEventParameter(ev, 'cntx', 'cntx', NULL, sizeof(CGContextRef), NULL, &cg);
    return cg;
}

int ccw_view_size(CCWViewRef v, double *w, double *h) {
    CCWRectD b = { 0, 0, 0, 0 };
    if (!v || !ccw_HIViewGetBounds || ccw_HIViewGetBounds(v, &b) != 0) return 0;
    if (b.w <= 0 || b.h <= 0) return 0;
    *w = b.w; *h = b.h; return 1;
}

CCWViewRef ccw_make_view(CGRect f, void *drawFn, void *hitFn, void *trackFn,
                         void *keyFn, void *ud, CCWViewRef parent) {
    CCWViewRef v = NULL;
    if (!ccw_HIObjectCreate) return NULL;
    ccw_HIObjectCreate(CFSTR("com.apple.hiview"), NULL, (void **)&v);
    if (!v) return NULL;
    if (ccw_HIViewSetFrame) {
        CCWRectD r = { f.origin.x, f.origin.y, f.size.width, f.size.height };
        ccw_HIViewSetFrame(v, &r);
    }
    if (ccw_InstallEventHandler && ccw_GetControlEventTarget) {
        void *tgt = ccw_GetControlEventTarget(v);
        struct { uint32_t cls, kind; } dr = { 'cntl', 4    };  /* kEventControlDraw         */
        struct { uint32_t cls, kind; } ht = { 'cntl', 3    };  /* kEventControlHitTest      */
        struct { uint32_t cls, kind; } hk = { 'cntl', 1    };  /* kEventControlHit          */
        struct { uint32_t cls, kind; } tk = { 'cntl', 51   };  /* kEventControlTrack        */
        struct { uint32_t cls, kind; } kd = { 'cntl', 11   };  /* kEventControlKeyDown      */
        struct { uint32_t cls, kind; } fp = { 'cntl', 4013 };  /* kEventControlSetFocusPart */
        if (drawFn)  ccw_InstallEventHandler(tgt, drawFn,  1, &dr, ud, NULL);
        if (hitFn)   ccw_InstallEventHandler(tgt, hitFn,   1, &ht, ud, NULL);
        if (trackFn) ccw_InstallEventHandler(tgt, trackFn, 1, &tk, ud, NULL);
        if (trackFn) ccw_InstallEventHandler(tgt, trackFn, 1, &hk, ud, NULL);
        if (keyFn)   ccw_InstallEventHandler(tgt, keyFn,   1, &kd, ud, NULL);
        if (keyFn)   ccw_InstallEventHandler(tgt, keyFn,   1, &fp, ud, NULL);
    }
    if (ccw_HIViewSetVisible) ccw_HIViewSetVisible(v, 1);  /* base hiview starts hidden */
    if (parent && ccw_HIViewAddSubview) ccw_HIViewAddSubview(parent, v);
    return v;
}

CCWViewRef ccw_content_view_of(CCWWindowRef win, CCWViewRef root) {
    if (!root || !ccw_HIViewGetFirstSubview || !ccw_HIViewGetBounds) return root;
    double ch = 0;
    CCWRect cr = { 0, 0, 0, 0 };
    if (ccw_GetWindowBounds && ccw_GetWindowBounds(win, 33 /*kWindowContentRgn*/, &cr) == 0)
        ch = (double)(cr.bottom - cr.top);
    CCWRectD rb = { 0, 0, 0, 0 };
    ccw_HIViewGetBounds(root, &rb);
    if (ch > 0 && rb.h <= ch + 1.0) return root;
    CCWViewRef first = ccw_HIViewGetFirstSubview(root);
    if (ch > 0 && ccw_HIViewGetNextView)
        for (CCWViewRef v = first; v; v = ccw_HIViewGetNextView(v)) {
            CCWRectD vb = { 0, 0, 0, 0 };
            ccw_HIViewGetBounds(v, &vb);
            if (vb.h >= ch - 1.0 && vb.h <= ch + 1.0) return v;
        }
    return first ? first : root;
}

/* 64-bit HIToolbox DELETED the non-compositing window model: EVERY window
 * request without kWindowCompositingAttribute returns -5601 (errInvalidWindow-
 * Attributes), even a bare kDocumentWindowClass with no attributes.  So
 * kCCWCompositing is forced on unconditionally here — never masked away by
 * GetAvailableWindowAttributes, which reports the CLASSIC availability. */
CCWWindowRef ccw_create_window(uint32_t cls, uint32_t extraAttrs, double w, double h,
                               const char *utf8Title) {
    if (!ccw_available()) return NULL;
    const uint32_t must = kCCWCompositing | kCCWStdHandler;
    uint32_t classes[4]; int nc = 0;
    classes[nc++] = cls;
    if (cls != kCCWMovableModalWindow) classes[nc++] = kCCWMovableModalWindow;
    if (cls != kCCWModalWindow)        classes[nc++] = kCCWModalWindow;
    if (cls != kCCWDocumentWindow)     classes[nc++] = kCCWDocumentWindow;

    CCWRect wr = { 200, 300, (int16_t)(200 + h), (int16_t)(300 + w) };
    CCWWindowRef win = NULL;
    for (int i = 0; i < nc && !win; i++) {
        uint32_t attrs = must | extraAttrs;
        if (ccw_GetAvailableWindowAttributes)
            attrs &= ccw_GetAvailableWindowAttributes(classes[i]) | must;
        CCWStatus s = ccw_CreateNewWindow(classes[i], attrs, &wr, &win);
        if (s != 0 || !win) {
            CCW_LOG("CreateNewWindow(class=%u attrs=0x%x) -> %d\n", classes[i], attrs, (int)s);
            win = NULL;
            /* retry that class with the bare mandatory attributes */
            if (ccw_CreateNewWindow(classes[i], must, &wr, &win) != 0) win = NULL;
        }
    }
    if (!win) return NULL;
    if (ccw_SetWindowTitleWithCFString) {
        CFStringRef t = ccw_cfstr(utf8Title ? utf8Title : "");
        if (t) { ccw_SetWindowTitleWithCFString(win, t); CFRelease(t); }
    }
    if (ccw_RepositionWindow)
        ccw_RepositionWindow(win, NULL, 3 /*kWindowAlertPositionOnMainScreen*/);
    return win;
}

void ccw_global_to_content(CCWWindowRef win, double gx, double gy, double *lx, double *ly) {
    CCWRect wb = { 0, 0, 0, 0 };
    if (ccw_GetWindowBounds) ccw_GetWindowBounds(win, 33 /*kWindowContentRgn*/, &wb);
    if (lx) *lx = gx - wb.left;
    if (ly) *ly = gy - wb.top;
}

void ccw_mouse_in_content(CCWWindowRef win, double *lx, double *ly) {
    CGEventRef e = CGEventCreate(NULL);
    CGPoint g = e ? CGEventGetLocation(e) : CGPointMake(-1, -1);
    if (e) CFRelease(e);
    ccw_global_to_content(win, g.x, g.y, lx, ly);
}

static int mouse_is_down(void) {
    return CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState,
                                    kCGMouseButtonLeft) ? 1 : 0;
}

/* ================================= BUTTON ================================ */
#define CCW_BTN_TEXT_SIZE 12    /* classic Aqua push-button title size */

void ccw_button_draw_into(CGContextRef cg, const ccw_button *b, double W, double H) {
    if (!cg || !b) return;
    CGContextSaveGState(cg);
    if (b->kind == CCW_BTN_PUSH) {
        CGRect r = CGRectMake(0.5, 0.5, W - 1, H - 1);
        double rad = H / 2.0;
        if (b->isDefault) {
            CGFloat top[4]  = { 0.42, 0.64, 0.94, 1 }, bot[4]  = { 0.13, 0.38, 0.82, 1 };
            CGFloat ptop[4] = { 0.20, 0.44, 0.80, 1 }, pbot[4] = { 0.08, 0.26, 0.62, 1 };
            ccw_grad_round(cg, r, rad, b->pressed ? ptop : top, b->pressed ? pbot : bot);
            CGContextSetRGBStrokeColor(cg, 0.10, 0.28, 0.60, 1);
            CGContextSetLineWidth(cg, 1);
            ccw_stroke_round(cg, r, rad);
            ccw_draw_centered_line(cg, b->title, 0, 0, W, H, H, CCW_BTN_TEXT_SIZE, 0,
                                   1, 1, 1);
        } else {
            CGFloat top[4]  = { 1.00, 1.00, 1.00, 1 }, bot[4]  = { 0.86, 0.86, 0.87, 1 };
            CGFloat ptop[4] = { 0.80, 0.80, 0.82, 1 }, pbot[4] = { 0.66, 0.66, 0.68, 1 };
            ccw_grad_round(cg, r, rad, b->pressed ? ptop : top, b->pressed ? pbot : bot);
            CGContextSetRGBStrokeColor(cg, 0.58, 0.58, 0.60, 1);
            CGContextSetLineWidth(cg, 1);
            ccw_stroke_round(cg, r, rad);
            double tint = b->disabled ? 0.55 : 0.05;
            ccw_draw_centered_line(cg, b->title, 0, 0, W, H, H, CCW_BTN_TEXT_SIZE, 0,
                                   tint, tint, tint + 0.02);
        }
        CGContextRestoreGState(cg);
        return;
    }
    /* checkbox / radio: classic 14pt box or disc at the left, title beside it */
    double s = 14, by = (H - s) / 2.0;
    CGRect box = CGRectMake(1.5, by + 0.5, s - 1, s - 1);
    CGFloat top[4]  = { 1.00, 1.00, 1.00, 1 }, bot[4]  = { 0.88, 0.88, 0.90, 1 };
    CGFloat ptop[4] = { 0.80, 0.80, 0.82, 1 }, pbot[4] = { 0.68, 0.68, 0.70, 1 };
    if (b->kind == CCW_BTN_RADIO) {
        ccw_grad_ellipse(cg, box, b->pressed ? ptop : top, b->pressed ? pbot : bot);
        CGContextSetRGBStrokeColor(cg, 0.52, 0.52, 0.55, 1);
        CGContextSetLineWidth(cg, 1);
        CGContextStrokeEllipseInRect(cg, box);
        if (b->value) {
            CGContextSetRGBFillColor(cg, 0.15, 0.15, 0.18, 1);
            CGContextFillEllipseInRect(cg, CGRectInset(box, 4, 4));
        }
    } else {
        ccw_grad_round(cg, box, 2.5, b->pressed ? ptop : top, b->pressed ? pbot : bot);
        CGContextSetRGBStrokeColor(cg, 0.52, 0.52, 0.55, 1);
        CGContextSetLineWidth(cg, 1);
        ccw_stroke_round(cg, box, 2.5);
        if (b->value) {                                   /* classic check mark */
            CGContextSetRGBStrokeColor(cg, 0.10, 0.10, 0.14, 1);
            CGContextSetLineWidth(cg, 2);
            CGContextBeginPath(cg);
            CGContextMoveToPoint(cg, box.origin.x + 3, by + s * 0.52);
            CGContextAddLineToPoint(cg, box.origin.x + s * 0.42, by + s - 4);
            CGContextAddLineToPoint(cg, box.origin.x + s - 2.5, by + 3);
            CGContextStrokePath(cg);
        }
    }
    double tint = b->disabled ? 0.55 : 0.05;
    CTFontRef f = ccw_font(CCW_BTN_TEXT_SIZE, 0);
    double asc = f ? CTFontGetAscent(f) : 9, desc = f ? CTFontGetDescent(f) : 3;
    if (f) CFRelease(f);
    ccw_draw_left_line(cg, b->title, s + 6, (H - (asc + desc)) / 2.0 + asc, H,
                       CCW_BTN_TEXT_SIZE, 0, tint, tint, tint + 0.02);
    CGContextRestoreGState(cg);
}

static CCWStatus btn_draw(void *call, CCWEventRef ev, void *ud) {
    (void)call;
    ccw_button *b = (ccw_button *)ud;
    CGContextRef cg = ccw_draw_cg(ev);
    double W = 0, H = 0;
    if (!b || !cg || !ccw_view_size(b->view, &W, &H)) return ccwEventNotHandled;
    ccw_button_draw_into(cg, b, W, H);
    return 0;
}

/* A base com.apple.hiview is not hit-testable by default, so mouse-downs pass
 * straight through and it never receives Hit/Track.  Claim the whole view as one
 * clickable part. */
static CCWStatus ccw_claim_hit(void *call, CCWEventRef ev, void *ud) {
    (void)call; (void)ud;
    int16_t part = 1;                       /* kControlButtonPart */
    if (ccw_SetEventParameter) ccw_SetEventParameter(ev, 'cprt', 'cprt', sizeof part, &part);
    return 0;
}

void ccw_button_fire(ccw_button *b) {
    if (!b || b->disabled) return;
    if (b->kind == CCW_BTN_CHECK) b->value = !b->value;
    else if (b->kind == CCW_BTN_RADIO) b->value = 1;
    if (b->resultOut) *b->resultOut = b->item;
    if (b->doneOut)   *b->doneOut = 1;
    CCW_LOG("button '%s' -> item %d\n", b->title, b->item);
}

/* Classic press-and-track: keep the button drawn "pressed" while the mouse is
 * held, follow the pointer in and out exactly as the classic Control Manager
 * did, and fire only on a mouse-UP inside.  TrackMouseLocation is gone from
 * 64-bit HIToolbox, so poll the live pointer through CoreGraphics — the tracking
 * then needs no event pump of its own. */
static CCWStatus btn_track(void *call, CCWEventRef ev, void *ud) {
    (void)call; (void)ev;
    ccw_button *b = (ccw_button *)ud;
    if (!b) return ccwEventNotHandled;
    CCW_LOG("track enter '%s' (item %d)\n", b->title, b->item);
    if (b->disabled) return 0;
    int inside = 1, was = -1;
    for (;;) {
        double lx = 0, ly = 0;
        ccw_mouse_in_content(b->win, &lx, &ly);
        inside = (lx >= b->frame.origin.x && lx <= b->frame.origin.x + b->frame.size.width &&
                  ly >= b->frame.origin.y && ly <= b->frame.origin.y + b->frame.size.height);
        if (inside != was) {
            was = inside;
            b->pressed = inside;
            if (ccw_HIViewSetNeedsDisplay) ccw_HIViewSetNeedsDisplay(b->view, 1);
        }
        if (!mouse_is_down()) break;
        usleep(15000);
    }
    b->pressed = 0;
    if (ccw_HIViewSetNeedsDisplay) ccw_HIViewSetNeedsDisplay(b->view, 1);
    if (inside) ccw_button_fire(b);
    return 0;
}

CCWViewRef ccw_button_install(ccw_button *b, CCWViewRef parent) {
    if (!b) return NULL;
    b->view = ccw_make_view(b->frame, (void *)btn_draw, (void *)ccw_claim_hit,
                            (void *)btn_track, NULL, b, parent);
    return b->view;
}

/* ================================== EDIT =================================
 * The classic edit-text field.  64-bit macOS deleted TextEdit AND the Control
 * Manager's edit-text control outright, so everything here — the sunken classic
 * frame, the blinking caret, click-to-place, drag/shift selection and the
 * classic editing keys — is drawn and driven by us on a plain compositing
 * HIView.  It is the single implementation shared by the Dialog Manager's
 * 'editText' DITL items and the nib materializer's IBCarbonEditText. */
#define CCW_EDIT_FONT 11.0
#define CCW_EDIT_PADX 4.0

/* Focus group, scoped by window.  Owned here so a click on one field un-focuses
 * its siblings without every shim reimplementing focus. */
#define CCW_FOCUS_MAX 128
static ccw_edit *g_fields[CCW_FOCUS_MAX];
static int       g_nfields;

static void field_register(ccw_edit *e) {
    for (int i = 0; i < g_nfields; i++) if (g_fields[i] == e) return;
    if (g_nfields < CCW_FOCUS_MAX) g_fields[g_nfields++] = e;
}
void ccw_forget_window(CCWWindowRef win) {
    int n = 0;
    for (int i = 0; i < g_nfields; i++)
        if (g_fields[i] && g_fields[i]->win != win) g_fields[n++] = g_fields[i];
    g_nfields = n;
}
ccw_edit *ccw_focused_edit(CCWWindowRef win) {
    for (int i = 0; i < g_nfields; i++)
        if (g_fields[i] && g_fields[i]->win == win && g_fields[i]->focused) return g_fields[i];
    return NULL;
}
void ccw_edit_set_focus(ccw_edit *e, int on) {
    if (!e) return;
    if (on) {
        for (int i = 0; i < g_nfields; i++) {
            ccw_edit *o = g_fields[i];
            if (o && o != e && o->win == e->win && o->focused) {
                o->focused = 0; o->caretOn = 0;
                if (o->view && ccw_HIViewSetNeedsDisplay) ccw_HIViewSetNeedsDisplay(o->view, 1);
            }
        }
    }
    e->focused = on ? 1 : 0;
    e->caretOn = e->focused;
    if (e->view && ccw_HIViewSetNeedsDisplay) ccw_HIViewSetNeedsDisplay(e->view, 1);
}
int ccw_focus_next(CCWWindowRef win, int backwards) {
    int idx[CCW_FOCUS_MAX], n = 0, cur = -1;
    for (int i = 0; i < g_nfields; i++)
        if (g_fields[i] && g_fields[i]->win == win && g_fields[i]->enabled) {
            if (g_fields[i]->focused) cur = n;
            idx[n++] = i;
        }
    if (n == 0) return 0;
    int next = (cur < 0) ? 0 : (backwards ? (cur - 1 + n) % n : (cur + 1) % n);
    ccw_edit *e = g_fields[idx[next]];
    ccw_edit_set_focus(e, 1);
    ccw_edit_select_all(e);
    return 1;
}

static int elen(const ccw_edit *e) { return (int)strlen(e->text); }
static int eclamp(const ccw_edit *e, int i) { int n = elen(e); return i < 0 ? 0 : (i > n ? n : i); }
static void esel(const ccw_edit *e, int *lo, int *hi) {
    int a = eclamp(e, e->caret), b = eclamp(e, e->selAnchor);
    *lo = a < b ? a : b; *hi = a < b ? b : a;
}

/* Width of the first `n` bytes as drawn (password fields measure bullets). */
static double eprefix_w(const ccw_edit *e, int n) {
    char tmp[sizeof e->text];
    n = eclamp(e, n);
    if (n <= 0) return 0;
    if (e->password) { for (int i = 0; i < n; i++) tmp[i] = '*'; tmp[n] = 0; }
    else { memcpy(tmp, e->text, (size_t)n); tmp[n] = 0; }
    return ccw_line_width(tmp, CCW_EDIT_FONT, 0);
}

/* Byte index nearest to local x inside the field. */
static int eindex_at(const ccw_edit *e, double x) {
    int n = elen(e), best = 0;
    double bestd = 1e18;
    for (int i = 0; i <= n; i++) {
        double d = x - (CCW_EDIT_PADX + eprefix_w(e, i));
        if (d < 0) d = -d;
        if (d < bestd) { bestd = d; best = i; }
    }
    return best;
}

void ccw_edit_draw_into(CGContextRef cg, ccw_edit *e, double W, double H) {
    if (!cg || !e) return;
    CGContextSaveGState(cg);
    /* classic sunken white field: white body, 1px mid-gray frame, and a darker
     * inner top/left shadow line — the classic recessed look. */
    CGRect box = CGRectMake(0.5, 0.5, W - 1, H - 1);
    CGContextSetRGBFillColor(cg, 1.0, 1.0, 1.0, 1.0);
    CGContextFillRect(cg, box);
    CGContextSetRGBStrokeColor(cg, 0.55, 0.55, 0.58, 1.0);
    CGContextSetLineWidth(cg, 1.0);
    CGContextStrokeRect(cg, box);
    CGContextSetRGBStrokeColor(cg, 0.72, 0.72, 0.75, 1.0);
    CGContextBeginPath(cg);
    CGContextMoveToPoint(cg, 1.5, 1.5);
    CGContextAddLineToPoint(cg, W - 1.5, 1.5);
    CGContextMoveToPoint(cg, 1.5, 1.5);
    CGContextAddLineToPoint(cg, 1.5, H - 1.5);
    CGContextStrokePath(cg);
    /* classic focus ring: the era's aqua glow around the active field */
    if (e->focused) {
        CGContextSetRGBStrokeColor(cg, 0.35, 0.58, 0.92, 0.85);
        CGContextSetLineWidth(cg, 2.0);
        CGContextStrokeRect(cg, CGRectInset(box, -1.0, -1.0));
    }

    CTFontRef f = ccw_font(CCW_EDIT_FONT, 0);
    double asc = f ? CTFontGetAscent(f) : 9, desc = f ? CTFontGetDescent(f) : 3;
    if (f) CFRelease(f);
    double baseline = (H - (asc + desc)) / 2.0 + asc;   /* from the TOP, y-down */

    int lo, hi; esel(e, &lo, &hi);
    if (e->focused && hi > lo) {                        /* classic selection band */
        double x0 = CCW_EDIT_PADX + eprefix_w(e, lo);
        double x1 = CCW_EDIT_PADX + eprefix_w(e, hi);
        CGContextSetRGBFillColor(cg, 0.70, 0.79, 0.94, 1.0);
        CGContextFillRect(cg, CGRectMake(x0, 2, x1 - x0, H - 4));
    }
    if (e->text[0]) {
        const char *shown = e->text;
        char bullets[sizeof e->text];
        if (e->password) {
            int n = elen(e);
            for (int i = 0; i < n; i++) bullets[i] = '*';
            bullets[n] = 0; shown = bullets;
        }
        double tint = e->enabled ? 0.05 : 0.5;
        ccw_draw_left_line(cg, shown, CCW_EDIT_PADX, baseline, H,
                           CCW_EDIT_FONT, 0, tint, tint, tint + 0.03);
    }
    if (e->focused && e->caretOn && hi == lo) {
        double cx = CCW_EDIT_PADX + eprefix_w(e, e->caret);
        CGContextSetRGBFillColor(cg, 0.0, 0.0, 0.0, 1.0);
        CGContextFillRect(cg, CGRectMake(cx, baseline - asc, 1.0, asc + desc));
    }
    CGContextRestoreGState(cg);
}

static CCWStatus edit_draw(void *call, CCWEventRef ev, void *ud) {
    (void)call;
    ccw_edit *e = (ccw_edit *)ud;
    CGContextRef cg = ccw_draw_cg(ev);
    double W = 0, H = 0;
    if (!e || !cg || !ccw_view_size(e->view, &W, &H)) return ccwEventNotHandled;
    ccw_edit_draw_into(cg, e, W, H);
    return 0;
}

/* Click: focus the field, place the caret, and drag-select while held —
 * the classic TextEdit click behaviour, polled through CoreGraphics because
 * TrackMouseLocation no longer exists. */
static CCWStatus edit_track(void *call, CCWEventRef ev, void *ud) {
    (void)call; (void)ev;
    ccw_edit *e = (ccw_edit *)ud;
    if (!e || !e->enabled) return ccwEventNotHandled;
    ccw_edit_set_focus(e, 1);
    double lx = 0, ly = 0;
    ccw_mouse_in_content(e->win, &lx, &ly);
    int idx = eindex_at(e, lx - e->frame.origin.x);
    e->caret = e->selAnchor = idx;
    if (e->view && ccw_HIViewSetNeedsDisplay) ccw_HIViewSetNeedsDisplay(e->view, 1);
    while (mouse_is_down()) {
        ccw_mouse_in_content(e->win, &lx, &ly);
        int j = eindex_at(e, lx - e->frame.origin.x);
        if (j != e->caret) {
            e->caret = j;
            if (e->view && ccw_HIViewSetNeedsDisplay) ccw_HIViewSetNeedsDisplay(e->view, 1);
        }
        usleep(15000);
    }
    e->caretOn = 1;
    if (e->view && ccw_HIViewSetNeedsDisplay) ccw_HIViewSetNeedsDisplay(e->view, 1);
    return 0;
}

/* kEventControlKeyDown: HIToolbox routes a keystroke to the focused control.
 * The Dialog Manager also runs a window-level handler (it owns Return/Esc/Tab),
 * but a nib window has no such handler, so the field must service its own keys
 * too — that is what makes an IBCarbonEditText genuinely typable. */
static CCWStatus edit_ctrl_key(void *call, CCWEventRef ev, void *ud) {
    (void)call;
    ccw_edit *e = (ccw_edit *)ud;
    if (!e || !ccw_GetEventParameter) return ccwEventNotHandled;
    unsigned char ch = 0;
    uint32_t mods = 0;
    if (ccw_GetEventParameter(ev, 'kchr' /*kEventParamKeyMacCharCodes*/, 'TEXT',
                              NULL, sizeof ch, NULL, &ch) != 0)
        return ccwEventNotHandled;
    ccw_GetEventParameter(ev, 'kmod' /*kEventParamKeyModifiers*/, 'magn',
                          NULL, sizeof mods, NULL, &mods);
    return ccw_edit_key(e, ch, mods) ? 0 : ccwEventNotHandled;
}

/* kEventControlSetFocusPart: accept keyboard focus (echo the part back). */
static CCWStatus edit_focus_ev(void *call, CCWEventRef ev, void *ud) {
    (void)call;
    ccw_edit *e = (ccw_edit *)ud;
    int16_t part = 0;
    if (ccw_GetEventParameter) ccw_GetEventParameter(ev, 'cprt', 'cprt', NULL, sizeof part, NULL, &part);
    if (e) ccw_edit_set_focus(e, part != 0);
    if (ccw_SetEventParameter) ccw_SetEventParameter(ev, 'cprt', 'cprt', sizeof part, &part);
    return 0;
}

void ccw_edit_set_text(ccw_edit *e, const char *utf8) {
    if (!e) return;
    strncpy(e->text, utf8 ? utf8 : "", sizeof e->text - 1);
    e->text[sizeof e->text - 1] = 0;
    e->caret = e->selAnchor = elen(e);
    if (e->view && ccw_HIViewSetNeedsDisplay) ccw_HIViewSetNeedsDisplay(e->view, 1);
}

void ccw_edit_select_all(ccw_edit *e) {
    if (!e) return;
    e->selAnchor = 0; e->caret = elen(e);
    if (e->view && ccw_HIViewSetNeedsDisplay) ccw_HIViewSetNeedsDisplay(e->view, 1);
}

static void edelete_range(ccw_edit *e, int lo, int hi) {
    int n = elen(e);
    if (lo < 0) lo = 0; if (hi > n) hi = n;
    if (hi <= lo) return;
    memmove(e->text + lo, e->text + hi, (size_t)(n - hi + 1));
    e->caret = e->selAnchor = lo;
}

int ccw_edit_key(ccw_edit *e, unsigned char ch, uint32_t modifiers) {
    if (!e || !e->enabled) return 0;
    const uint32_t cmdKeyMask = 0x0100, shiftKeyMask = 0x0200;
    int n = elen(e), lo, hi;
    esel(e, &lo, &hi);
    int cap = e->maxLen > 0 && e->maxLen < (int)sizeof e->text - 1
                  ? e->maxLen : (int)sizeof e->text - 1;

    if (modifiers & cmdKeyMask) {                  /* Cmd-A = Select All (classic) */
        if (ch == 'a' || ch == 'A') { ccw_edit_select_all(e); return 1; }
        return 0;                                  /* other Cmd keys: not ours     */
    }
    switch (ch) {
    case 8:      /* Backspace */
        if (hi > lo) edelete_range(e, lo, hi);
        else if (e->caret > 0) edelete_range(e, e->caret - 1, e->caret);
        break;
    case 127:    /* Forward Delete */
        if (hi > lo) edelete_range(e, lo, hi);
        else if (e->caret < n) edelete_range(e, e->caret, e->caret + 1);
        break;
    case 28:     /* Left arrow  */
        e->caret = eclamp(e, (hi > lo && !(modifiers & shiftKeyMask)) ? lo : e->caret - 1);
        if (!(modifiers & shiftKeyMask)) e->selAnchor = e->caret;
        break;
    case 29:     /* Right arrow */
        e->caret = eclamp(e, (hi > lo && !(modifiers & shiftKeyMask)) ? hi : e->caret + 1);
        if (!(modifiers & shiftKeyMask)) e->selAnchor = e->caret;
        break;
    case 1:      /* Home */ case 30: /* Up */
        e->caret = 0; if (!(modifiers & shiftKeyMask)) e->selAnchor = 0;
        break;
    case 4:      /* End  */ case 31: /* Down */
        e->caret = n; if (!(modifiers & shiftKeyMask)) e->selAnchor = n;
        break;
    case 13: case 3: case 9: case 27:
        return 0;               /* Return / Enter / Tab / Esc belong to the dialog */
    default:
        if (ch < 32) return 0;
        if (hi > lo) { edelete_range(e, lo, hi); n = elen(e); }
        if (n >= cap) { e->caretOn = 1; return 1; }   /* full: swallow, classic beep-less */
        memmove(e->text + e->caret + 1, e->text + e->caret, (size_t)(n - e->caret + 1));
        e->text[e->caret] = (char)ch;
        e->caret++; e->selAnchor = e->caret;
        break;
    }
    e->caretOn = 1;
    if (e->view && ccw_HIViewSetNeedsDisplay) ccw_HIViewSetNeedsDisplay(e->view, 1);
    return 1;
}

CCWViewRef ccw_edit_install(ccw_edit *e, CCWViewRef parent) {
    if (!e) return NULL;
    if (!e->maxLen) e->maxLen = (int)sizeof e->text - 1;
    e->caret = e->selAnchor = elen(e);
    e->view = ccw_make_view(e->frame, (void *)edit_draw, (void *)ccw_claim_hit,
                            (void *)edit_track, NULL, e, parent);
    /* keyboard: kEventControlKeyDown -> our editor, kEventControlSetFocusPart ->
     * accept focus.  Installed here rather than through ccw_make_view's single
     * key slot because the two need DIFFERENT handlers. */
    if (e->view && ccw_InstallEventHandler && ccw_GetControlEventTarget) {
        void *tgt = ccw_GetControlEventTarget(e->view);
        struct { uint32_t cls, kind; } kd = { 'cntl', 11   };  /* KeyDown      */
        struct { uint32_t cls, kind; } fp = { 'cntl', 4013 };  /* SetFocusPart */
        ccw_InstallEventHandler(tgt, (void *)edit_ctrl_key,  1, &kd, e, NULL);
        ccw_InstallEventHandler(tgt, (void *)edit_focus_ev,  1, &fp, e, NULL);
    }
    /* A bare hiview advertises no features, so HIToolbox will not route keyboard
     * focus to it.  Advertise kHIViewFeatureGetsFocusOnClick (1<<8). */
    if (e->view && ccw_HIViewChangeFeatures) ccw_HIViewChangeFeatures(e->view, (1ull << 8), 0);
    if (e->view) field_register(e);
    return e->view;
}

/* =============================== MODAL PUMP ============================== */
int ccw_run_modal_pump(int *done, const int *drew, ccw_edit *const *caretSlot,
                       double proofSecs) {
    if (!ccw_ReceiveNextEvent || !ccw_SendEventToEventTarget || !ccw_GetEventDispatcherTarget)
        return 0;
    if (!done) return 0;
    void *disp = ccw_GetEventDispatcherTarget();
    if (!disp) return 0;
    double idle = 0, blink = 0;
    const double tick = 0.05;
    while (!*done) {
        CCWEventRef ev = NULL;
        CCWStatus s = ccw_ReceiveNextEvent(0, NULL, tick, 1 /*pull*/, &ev);
        if (s == 0 && ev) {
            idle = 0;
            ccw_SendEventToEventTarget(ev, disp);
            if (ccw_ReleaseEvent) ccw_ReleaseEvent(ev);
            continue;
        }
        idle += tick;
        blink += tick;
        if (blink >= 0.5) {                       /* the classic caret blink rate */
            blink = 0;
            ccw_edit *f = caretSlot ? *caretSlot : NULL;
            if (f && f->focused) {
                f->caretOn = !f->caretOn;
                if (ccw_HIViewSetNeedsDisplay) ccw_HIViewSetNeedsDisplay(f->view, 1);
            }
        }
        if (drew && !*drew && proofSecs > 0 && idle >= proofSecs) {
            CCW_LOG("pump proved dead (no events, never drew) -> fall back\n");
            return 0;
        }
    }
    return 1;
}
