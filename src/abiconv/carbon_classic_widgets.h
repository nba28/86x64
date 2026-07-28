/*
 * carbon_classic_widgets.h — THE shared classic-Carbon widget substrate.
 *
 * ONE JOB: hand every classic-UI shim the same set of REAL classic controls —
 * a real Carbon compositing window plus SELF-DRAWN classic HIViews
 * (com.apple.hiview + kEventControlDraw/HitTest/Track through CoreGraphics +
 * CoreText, Lucida Grande) — so that the classic alert (carbon_classic_alert.c),
 * the classic Dialog Manager (carbon_classic_dialog.c) and the IBCarbon nib
 * materializer all draw the SAME authentic classic chrome instead of each
 * growing its own private copy (or falling back to modern AppKit).
 *
 * This is internal to libabiconv: nothing here is an exported ABI, and no symbol
 * here is a shim entry point.  It is the classic substrate, not a shim.
 *
 * WHY IT EXISTS: 64-bit macOS deleted the classic Control Manager's drawn
 * controls, TextEdit, and the non-compositing window model, and the surviving
 * HIToolbox draws MODERN chrome.  Re-materializing the classic look therefore
 * means drawing it ourselves on the real classic substrate — which is project
 * doctrine (the authentic-original-look rule), not a preference.  AppKit is only
 * ever a fallback for when this substrate cannot be materialized at all.
 */
#ifndef CARBON_CLASSIC_WIDGETS_H
#define CARBON_CLASSIC_WIDGETS_H

#include <stdint.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t CCWStatus;
typedef void *CCWWindowRef, *CCWViewRef, *CCWEventRef;

/* Classic Rect, in the classic order. */
typedef struct { int16_t top, left, bottom, right; } CCWRect;
/* HIRect as HIToolbox actually lays it out (2 CGFloat origin + 2 CGFloat size). */
typedef struct { double x, y, w, h; } CCWRectD;

#define kCCWCompositing  (1u << 19)   /* kWindowCompositingAttribute           */
#define kCCWStdHandler   (1u << 25)   /* kWindowStandardHandlerAttribute       */
#define kCCWCloseBox     (1u << 0)
#define kCCWAlertWindow          1u
#define kCCWMovableAlertWindow   2u
#define kCCWModalWindow          3u
#define kCCWMovableModalWindow   4u
#define kCCWDocumentWindow       6u
#define ccwEventNotHandled ((CCWStatus)-9874)

/* ---- native HIToolbox entry points -------------------------------------
 * All dlsym'd: the C wrappers are header-gated out on a modern SDK but the
 * symbols are live in the shared HIToolbox.  NB GetControlEventTarget and
 * friends return 64-bit POINTERS — never declare them CCWStatus (that
 * truncation once faulted HIToolbox's own handler walk). */
extern CCWStatus  (*ccw_CreateNewWindow)(uint32_t, uint32_t, const CCWRect *, CCWWindowRef *);
extern uint32_t   (*ccw_GetAvailableWindowAttributes)(uint32_t);
extern CCWStatus  (*ccw_SetWindowTitleWithCFString)(CCWWindowRef, CFStringRef);
extern void       (*ccw_ShowWindow)(CCWWindowRef);
extern void       (*ccw_SelectWindow)(CCWWindowRef);
extern void       (*ccw_DisposeWindow)(CCWWindowRef);
extern CCWStatus  (*ccw_RepositionWindow)(CCWWindowRef, CCWWindowRef, uint32_t);
extern CCWStatus  (*ccw_GetWindowBounds)(CCWWindowRef, uint32_t, CCWRect *);
extern CCWStatus  (*ccw_SetWindowBounds)(CCWWindowRef, uint32_t, const CCWRect *);
extern CCWStatus  (*ccw_BeginAppModalStateForWindow)(CCWWindowRef);
extern CCWStatus  (*ccw_EndAppModalStateForWindow)(CCWWindowRef);
extern CCWViewRef (*ccw_HIViewGetRoot)(CCWWindowRef);
extern CCWViewRef (*ccw_HIViewGetFirstSubview)(CCWViewRef);
extern CCWViewRef (*ccw_HIViewGetNextView)(CCWViewRef);
extern CCWStatus  (*ccw_HIViewGetBounds)(CCWViewRef, CCWRectD *);
extern CCWStatus  (*ccw_HIViewSetFrame)(CCWViewRef, const CCWRectD *);
extern CCWStatus  (*ccw_HIViewAddSubview)(CCWViewRef, CCWViewRef);
extern CCWStatus  (*ccw_HIViewSetVisible)(CCWViewRef, uint8_t);
extern CCWStatus  (*ccw_HIViewSetNeedsDisplay)(CCWViewRef, uint8_t);
extern CCWStatus  (*ccw_HIViewChangeFeatures)(CCWViewRef, uint64_t, uint64_t);
extern CCWStatus  (*ccw_HIObjectCreate)(CFStringRef, void *, void **);
extern void      *(*ccw_GetControlEventTarget)(CCWViewRef);
extern void      *(*ccw_GetWindowEventTarget)(CCWWindowRef);
extern void      *(*ccw_GetEventDispatcherTarget)(void);
extern CCWStatus  (*ccw_InstallEventHandler)(void *, void *, uint32_t, const void *, void *, void *);
extern CCWStatus  (*ccw_GetEventParameter)(CCWEventRef, uint32_t, uint32_t, uint32_t *, unsigned long, unsigned long *, void *);
extern CCWStatus  (*ccw_SetEventParameter)(CCWEventRef, uint32_t, uint32_t, unsigned long, const void *);
extern CCWStatus  (*ccw_ReceiveNextEvent)(uint32_t, const void *, double, uint8_t, CCWEventRef *);
extern CCWStatus  (*ccw_SendEventToEventTarget)(CCWEventRef, void *);
extern void       (*ccw_ReleaseEvent)(CCWEventRef);
extern uint32_t   (*ccw_GetEventClass)(CCWEventRef);
extern uint32_t   (*ccw_GetEventKind)(CCWEventRef);

/* Resolve the natives once.  Returns 1 if the substrate can be used at all
 * (window creation + HIView creation + a modal state are all present). */
int  ccw_available(void);
/* CARBON_ALERT_TRACE / CARBON_DIALOG_TRACE tracing, shared by all users. */
int  ccw_trace(void);
void ccw_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#define CCW_LOG(...) do { if (ccw_trace()) ccw_log(__VA_ARGS__); } while (0)

/* ---- classic text (Lucida Grande — the real classic system font) -------- */
CTFontRef ccw_font(double size, int bold);
double    ccw_wrap_height(const char *utf8, double width, double size, int bold);
double    ccw_line_width(const char *utf8, double size, int bold);
/* All drawing takes H = the total height of the FLIPPED (y-down) HIView draw
 * context, because CoreText renders y-up and we flip a saved CTM about H. */
void ccw_draw_wrapped(CGContextRef cg, const char *utf8, double x, double yTop,
                      double w, double h, double H, double size, int bold,
                      double r, double g, double b);
void ccw_draw_centered_line(CGContextRef cg, const char *utf8, double x, double yTop,
                            double w, double h, double H, double size, int bold,
                            double r, double g, double b);
void ccw_draw_left_line(CGContextRef cg, const char *utf8, double x, double baselineFromTop,
                        double H, double size, int bold, double r, double g, double b);

/* ---- classic drawing primitives ---------------------------------------- */
void ccw_stroke_round(CGContextRef cg, CGRect r, double rad);
void ccw_grad_round(CGContextRef cg, CGRect r, double rad, const CGFloat top[4], const CGFloat bot[4]);
void ccw_grad_ellipse(CGContextRef cg, CGRect box, const CGFloat top[4], const CGFloat bot[4]);
/* type: 0/other = stop (red), 1 = note (blue), 2 = caution (yellow triangle). */
void ccw_draw_alert_icon(CGContextRef cg, int type, double x, double y, double s);

/* ---- HIView plumbing ---------------------------------------------------- */
CGContextRef ccw_draw_cg(CCWEventRef ev);
int          ccw_view_size(CCWViewRef v, double *w, double *h);
/* Build a self-drawn com.apple.hiview with the given handlers (any may be NULL)
 * and add it to `parent`.  `frameYDown` is in classic y-DOWN content coords. */
CCWViewRef   ccw_make_view(CGRect frameYDown, void *drawFn, void *hitFn, void *trackFn,
                           void *keyFn, void *ud, CCWViewRef parent);
/* Content view of a compositing window (its root may include the title band). */
CCWViewRef   ccw_content_view_of(CCWWindowRef win, CCWViewRef root);
/* Create a compositing Carbon window of `cls` with content size w x h, honouring
 * the 64-bit rule that kWindowCompositingAttribute is MANDATORY.  Falls back
 * through movable-modal -> modal -> document.  Returns NULL on total failure. */
CCWWindowRef ccw_create_window(uint32_t cls, uint32_t extraAttrs, double w, double h,
                               const char *utf8Title);
/* Global (screen) point -> window content-local, y DOWN. */
void ccw_global_to_content(CCWWindowRef win, double gx, double gy, double *lx, double *ly);
/* Where the mouse is right now, in content-local y-DOWN coords. */
void ccw_mouse_in_content(CCWWindowRef win, double *lx, double *ly);

/* ================================ BUTTON ================================= */
enum { CCW_BTN_PUSH = 0, CCW_BTN_CHECK = 1, CCW_BTN_RADIO = 2 };
typedef struct ccw_button {
    char         title[192];
    int          item;              /* classic 1-based item number            */
    CGRect       frame;             /* content coords, y DOWN                 */
    int          kind;              /* CCW_BTN_*                              */
    int          isDefault, isCancel, disabled;
    int          value;             /* check/radio on/off                     */
    int          pressed;
    CCWViewRef   view;
    CCWWindowRef win;
    int         *resultOut;         /* item number written here on a real hit */
    int         *doneOut;           /* set to 1 on a real hit (may be NULL)   */
} ccw_button;

/* Create + install a classic button.  The struct is owned by the caller and
 * must outlive the window. */
CCWViewRef ccw_button_install(ccw_button *b, CCWViewRef parent);
/* Draw a classic button into an ALREADY-FLIPPED y-down context of size W x H.
 * Exposed so a shim can draw a button inside a bigger self-drawn view. */
void ccw_button_draw_into(CGContextRef cg, const ccw_button *b, double W, double H);
/* Fire the button as if clicked (Return/Esc/space keyboard equivalents). */
void ccw_button_fire(ccw_button *b);

/* ================================= EDIT ================================== */
/* The classic edit-text field: sunken white box, classic 1px inset frame,
 * a real blinking caret, click-to-place caret, drag + shift-click selection,
 * and the classic editing keys.  This is the piece 64-bit macOS deleted
 * outright (TextEdit / the Control Manager's edit text), so it is drawn and
 * driven entirely here. */
typedef struct ccw_edit {
    char         text[512];         /* UTF-8; classic callers use MacRoman ASCII */
    int          caret;             /* insertion point, byte index               */
    int          selAnchor;         /* other end of the selection (== caret: none)*/
    int          focused, enabled, caretOn, password;
    int          maxLen;            /* 0 = sizeof text - 1                       */
    CGRect       frame;             /* content coords, y DOWN                    */
    CCWViewRef   view;
    CCWWindowRef win;
    void        *owner;             /* opaque back-pointer for the shim          */
} ccw_edit;

CCWViewRef ccw_edit_install(ccw_edit *e, CCWViewRef parent);
/* Focus bookkeeping is owned here so that clicking a field un-focuses its
 * siblings without every shim reimplementing it.  `win` scopes the group. */
ccw_edit *ccw_focused_edit(CCWWindowRef win);
/* Tab / Shift-Tab through the fields of `win`.  Returns 1 if focus moved. */
int  ccw_focus_next(CCWWindowRef win, int backwards);
/* Forget every field belonging to `win` (call before disposing the window). */
void ccw_forget_window(CCWWindowRef win);
/* Give the field one key.  `ch` is the classic MacRoman char code.  Returns 1 if
 * the field consumed it, 0 if the caller should handle it (Return/Enter/Tab). */
int  ccw_edit_key(ccw_edit *e, unsigned char ch, uint32_t modifiers);
void ccw_edit_set_text(ccw_edit *e, const char *utf8);
void ccw_edit_set_focus(ccw_edit *e, int on);
void ccw_edit_select_all(ccw_edit *e);
/* Draw the field into an ALREADY-FLIPPED y-down context of size W x H. */
void ccw_edit_draw_into(CGContextRef cg, ccw_edit *e, double W, double H);

/* ============================== MODAL PUMP =============================== */
/* Our OWN modal pump.  Deliberately NOT RunApplicationEventLoop: the translated
 * app may already be inside one, and QuitApplicationEventLoop would tear down
 * the app's loop rather than ours.
 *
 *   done   — the pump runs until *done != 0.
 *   drew   — set by the caller's draw handler; if nothing ever draws AND no
 *            event at all arrives within proofSecs, the pump reports 0 so the
 *            caller can fall back to AppKit rather than hang a dead modal.
 *   caret  — optional focused edit field to blink (may be NULL, and may be
 *            changed by handlers while the pump runs: it is re-read each tick).
 * Returns 1 if the modal completed, 0 if the pump proved dead. */
int ccw_run_modal_pump(int *done, const int *drew, ccw_edit *const *caretSlot,
                       double proofSecs);

#ifdef __cplusplus
}
#endif
#endif /* CARBON_CLASSIC_WIDGETS_H */
