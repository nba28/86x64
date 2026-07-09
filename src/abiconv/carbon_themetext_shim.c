/*
 * carbon_themetext_shim.c — REAL Appearance Manager theme text via CoreText.
 *
 * DrawThemeTextBox / DrawThemeText / GetThemeTextDimensions are the classic
 * Carbon way to draw and measure UI text (Halo links the pair; the whole
 * classic Carbon fleet uses them). The natives are gone from 64-bit macOS and
 * the previous shims were silent no-ops: DrawThemeTextBox returned noErr and
 * drew NOTHING (text-shaped holes in otherwise-working windows), and
 * GetThemeTextDimensions returned a canned 16pt line, wrecking any caller's
 * wrap/scroll layout math.
 *
 * REAL implementation:
 *   - Measure with a CTFramesetter (wrap-to-width honored via the caller's
 *     ioBounds->h, exactly the classic contract).
 *   - Draw with CTFrameDraw into the caller-provided CGContext. The context
 *     arrives from the i386 side as an arena handle -> unwrap. When the
 *     caller passes NULL (classic "draw into the current port"), we have no
 *     port plumbing here — return without drawing (same visible result as
 *     the old stub, but only for that legacy sub-case).
 *   - ThemeFontID -> CoreText UI font mapping for the classic IDs (system /
 *     small / emphasized / views / application / label); styled sizes follow
 *     the classic metrics.
 *   - Justification: teFlushLeft(0)/teCenter(1)/teFlushRight(-1) -> a CoreText
 *     paragraph-style alignment attribute.
 *   - Contexts handed to Carbon draw paths are QuickDraw-oriented (origin
 *     top-left, y down) — flip around the target rect for CoreText, same
 *     recipe as carbon_scrolltext_shim.c's control drawing.
 *
 * MTSHIM convention: rdi -> &i386 args[0] (4-byte cdecl slots), uint32_t
 * (OSStatus) in eax. ___DrawThemeTextBox / ___DrawThemeText /
 * ___GetThemeTextDimensions are already wired in maptable_tramp.asm (the
 * no-op bodies lived in carbon_ui_shim.c / qt_hitoolbox_shim.c and moved
 * here), so targets bind them already — validates on a RESYNC.
 */

#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

extern uint64_t x64_objc_unwrap(uint32_t h);

typedef struct { int16_t top, left, bottom, right; } CRect;
typedef struct { int16_t v, h; } QDPoint;

/* ThemeFontID (Appearance.h) -> CoreText UI font. */
static CTFontRef theme_font(int16_t id) {
    CTFontUIFontType t = kCTFontUIFontSystem;
    double size = 0;                       /* 0 = the type's default size */
    int bold = 0;
    switch (id) {
    case 0:  t = kCTFontUIFontSystem; break;               /* kThemeSystemFont */
    case 1:  t = kCTFontUIFontSmallSystem; break;          /* kThemeSmallSystemFont */
    case 2:  t = kCTFontUIFontSmallEmphasizedSystem; break;/* small emphasized */
    case 3:  t = kCTFontUIFontViews; break;                /* kThemeViewsFont */
    case 4:  t = kCTFontUIFontEmphasizedSystem; break;     /* emphasized system */
    case 5:  t = kCTFontUIFontApplication; break;          /* kThemeApplicationFont */
    case 6:  t = kCTFontUIFontLabel; break;                /* kThemeLabelFont */
    case 100: t = kCTFontUIFontMenuItem; break;            /* kThemeMenuItemFont */
    case 101: t = kCTFontUIFontMenuTitle; break;           /* kThemeMenuTitleFont */
    case 103: t = kCTFontUIFontToolTip; break;             /* kThemeToolTipFont */
    case 104: t = kCTFontUIFontPushButton; break;          /* kThemePushButtonFont */
    case 105: t = kCTFontUIFontUtilityWindowTitle; break;  /* utility window title */
    case 106: t = kCTFontUIFontAlertHeader; break;         /* alert header */
    default: t = kCTFontUIFontSystem; break;
    }
    CTFontRef f = CTFontCreateUIFontForLanguage(t, size, NULL);
    if (!f) f = CTFontCreateWithName(CFSTR("Helvetica"), 13, NULL);
    (void)bold;
    return f;
}

/* Attributed string for (str, fontID, just); caller releases. */
static CFAttributedStringRef theme_astr(CFStringRef str, int16_t fontID,
                                        int just, int dim) {
    CTFontRef font = theme_font(fontID);
    CTTextAlignment al = kCTTextAlignmentLeft;
    if (just == 1) al = kCTTextAlignmentCenter;
    else if (just == -1) al = kCTTextAlignmentRight;
    CTParagraphStyleSetting ps[1] = {
        { kCTParagraphStyleSpecifierAlignment, sizeof al, &al }
    };
    CTParagraphStyleRef para = CTParagraphStyleCreate(ps, 1);
    CGColorRef color = CGColorCreateGenericGray(dim ? 0.5 : 0.0, 1.0);
    const void *keys[] = { kCTFontAttributeName, kCTParagraphStyleAttributeName,
                           kCTForegroundColorAttributeName };
    const void *vals[] = { font, para, color };
    CFDictionaryRef attrs = CFDictionaryCreate(NULL, keys, vals, 3,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef as = CFAttributedStringCreate(NULL, str, attrs);
    CFRelease(attrs); CGColorRelease(color); CFRelease(para); CFRelease(font);
    return as;
}

/* OSStatus DrawThemeTextBox(CFStringRef inString, ThemeFontID inFontID,
 *     ThemeDrawState inState, Boolean inWrapToWidth, const Rect *inBoundingBox,
 *     SInt16 inJust, void *inContext)
 * slots: a[0]=str a[1]=fontID a[2]=state a[3]=wrap a[4]=bounds a[5]=just a[6]=ctx */
uint32_t shim_DrawThemeTextBox(uint32_t *a) {
    CFStringRef str = (CFStringRef)(uintptr_t)x64_objc_unwrap(a[0]);
    const CRect *rc = (const CRect *)(uintptr_t)a[4];
    CGContextRef ctx = (CGContextRef)(uintptr_t)x64_objc_unwrap(a[6]);
    if (!str || !rc) return (uint32_t)-50;            /* paramErr */
    if (!ctx) return 0;   /* classic "current port" form: no port plumbing here */
    int dim = (a[2] == 0);                            /* kThemeStateInactive */
    CFAttributedStringRef as = theme_astr(str, (int16_t)a[1], (int16_t)a[5], dim);
    if (!as) return (uint32_t)-50;
    CTFramesetterRef fs = CTFramesetterCreateWithAttributedString(as);
    CFRelease(as);
    if (!fs) return (uint32_t)-50;

    double x = rc->left, y = rc->top;
    double w = rc->right - rc->left, h = rc->bottom - rc->top;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    CGMutablePathRef p = CGPathCreateMutable();
    CGPathAddRect(p, NULL, CGRectMake(0, 0, w, h + 4));
    CTFrameRef fr = CTFramesetterCreateFrame(fs, CFRangeMake(0, 0), p, NULL);
    CGPathRelease(p); CFRelease(fs);
    if (!fr) return (uint32_t)-50;

    CGContextSaveGState(ctx);
    /* flip the QuickDraw-oriented context around the target rect (top at y) */
    CGContextTranslateCTM(ctx, x, y + h + 4);
    CGContextScaleCTM(ctx, 1, -1);
    CTFrameDraw(fr, ctx);
    CGContextRestoreGState(ctx);
    CFRelease(fr);
    return 0;
}

/* OSStatus DrawThemeText(...) — QuickTime-era alias with the same shape used
 * by qt_hitoolbox surface; route to the same implementation. */
uint32_t shim_DrawThemeText(uint32_t *a) { return shim_DrawThemeTextBox(a); }

/* OSStatus GetThemeTextDimensions(CFStringRef inString, ThemeFontID inFontID,
 *     ThemeDrawState inState, Boolean inWrapToWidth, Point *ioBounds,
 *     SInt16 *outBaseline)
 * ioBounds IN: h = wrap width (when inWrapToWidth); OUT: {v=height, h=width}. */
uint32_t shim_GetThemeTextDimensions(uint32_t *a) {
    CFStringRef str = (CFStringRef)(uintptr_t)x64_objc_unwrap(a[0]);
    QDPoint *io = (QDPoint *)(uintptr_t)a[4];
    int16_t *baseline = (int16_t *)(uintptr_t)a[5];
    if (!str || !io) return (uint32_t)-50;
    int wrap = (a[3] & 0xff) != 0;
    double width = (wrap && io->h > 0) ? io->h : CGFLOAT_MAX;

    CFAttributedStringRef as = theme_astr(str, (int16_t)a[1], 0, 0);
    if (!as) return (uint32_t)-50;
    CTFramesetterRef fs = CTFramesetterCreateWithAttributedString(as);
    if (!fs) { CFRelease(as); return (uint32_t)-50; }
    CFRange fit;
    CGSize sz = CTFramesetterSuggestFrameSizeWithConstraints(
        fs, CFRangeMake(0, 0), NULL, CGSizeMake(width, CGFLOAT_MAX), &fit);
    CFRelease(fs);

    io->h = (int16_t)(sz.width + 0.999);
    io->v = (int16_t)(sz.height + 0.999);
    if (baseline) {
        /* classic contract: baseline of the FIRST line, relative to the TOP
         * of the box, negated (Appearance.h: "distance from the bottom edge
         * ... a negative value"). Approximate with the font's ascent. */
        CTFontRef f = theme_font((int16_t)a[1]);
        double ascent = CTFontGetAscent(f);
        CFRelease(f);
        *baseline = (int16_t)-(sz.height - ascent);
    }
    CFRelease(as);
    return 0;
}
