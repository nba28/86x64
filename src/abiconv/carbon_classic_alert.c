/*
 * carbon_classic_alert.c — one job: materialize and run an AUTHENTIC CLASSIC
 * Carbon alert (real Carbon window + self-drawn classic HIViews) and return the
 * classic item number the user picked.
 *
 * The classic widgets themselves (window, buttons, text, modal pump) now live in
 * carbon_classic_widgets.c, shared with the classic Dialog Manager
 * (carbon_classic_dialog.c) and the IBCarbon nib materializer.  What is left
 * here is exactly the ALERT: its classic layout, its icon, and its 1/2/3 button
 * contract.
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
 * CoreText, Lucida Grande), the same live machinery that draws Halo's EULA
 * scrolling text box, the settings group frames, popups and edit fields, whose
 * clicks and drags the tester has confirmed working on screen.
 *
 * WHAT IT DRAWS: the classic Aqua alert — movable-modal window, 64x64 alert icon
 * at the left, Lucida Grande Bold 13 message, Lucida Grande 11 informative text,
 * classic bevelled push buttons bottom-right in the classic order
 * [Other] [Cancel] [OK], default button in the classic aqua-blue gradient.
 *
 * UNIVERSAL: nothing keys on an app name or an alert string — it triggers on the
 * standard-alert API family, so every translated Carbon app gets the classic
 * alert.  SINGLE PURPOSE: this file only builds+runs the classic alert; the i386
 * argument marshalling stays in carbon_standardalert_shim.m, the Dialog Manager
 * in carbon_dialog_shim.m + carbon_classic_dialog.c, nibs in carbon_nib_shim.c.
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

#include "carbon_classic_widgets.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* AppKit window-management + foreground bootstrap (carbon_appkit_host.c). */
extern void carbon_ensure_window_host(void);
extern void carbon_ensure_foreground(void);

/* If the classic window is up but the pump delivers NOTHING for this long and
 * the alert never even drew, we conclude this OS will not drive it and fall back
 * to the caller's modal. */
#define CLASSIC_ALERT_PROOF_SECS 3.0

#define CA(...) CCW_LOG(__VA_ARGS__)

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

/* ------------------------------ alert state ------------------------------ */
struct alert {
    int          type;
    char         message[1024], info[2048];
    ccw_button   btn[AL_MAXBTN];
    int          nbtn;
    int          result;              /* classic item hit, 0 = none yet */
    int          done;
    int          drew;                /* our body view actually drew at least once */
    CCWWindowRef win;
    CCWViewRef   body;
    double       contentW, contentH, msgH, infoH;
};

/* Body view: classic alert background + icon + message + informative text. */
static CCWStatus body_draw(void *call, CCWEventRef ev, void *ud) {
    (void)call;
    struct alert *a = (struct alert *)ud;
    CGContextRef cg = ccw_draw_cg(ev);
    double W = 0, H = 0;
    if (!a || !cg || !ccw_view_size(a->body, &W, &H)) return ccwEventNotHandled;
    a->drew = 1;
    CGContextSaveGState(cg);
    /* the classic flat platinum/aqua alert background */
    CGContextSetRGBFillColor(cg, 0.929, 0.929, 0.929, 1.0);
    CGContextFillRect(cg, CGRectMake(0, 0, W, H));
    ccw_draw_alert_icon(cg, a->type, AL_MARGIN, AL_MARGIN, AL_ICON);
    ccw_draw_wrapped(cg, a->message, AL_TEXT_X, AL_MARGIN, AL_TEXT_W, a->msgH, H,
                     AL_MSG_SIZE, 1, 0.0, 0.0, 0.0);
    if (a->info[0])
        ccw_draw_wrapped(cg, a->info, AL_TEXT_X, AL_MARGIN + a->msgH + 8, AL_TEXT_W,
                         a->infoH, H, AL_INFO_SIZE, 0, 0.0, 0.0, 0.0);
    CGContextRestoreGState(cg);
    return 0;
}

/* Return/Enter = default button, Escape = cancel — the classic alert keyboard
 * contract, so an alert is never un-dismissable. */
static CCWStatus key_handler(void *call, CCWEventRef ev, void *ud) {
    (void)call;
    struct alert *a = (struct alert *)ud;
    if (!a || !ccw_GetEventParameter) return ccwEventNotHandled;
    char ch = 0;
    if (ccw_GetEventParameter(ev, 'kchr' /*kEventParamKeyMacCharCodes*/, 'TEXT',
                              NULL, sizeof ch, NULL, &ch) != 0)
        return ccwEventNotHandled;
    int want = 0;
    if (ch == 13 || ch == 3)  want = 1;          /* Return / Enter -> default */
    else if (ch == 27)        want = 2;          /* Escape -> cancel          */
    if (!want) return ccwEventNotHandled;
    for (int i = 0; i < a->nbtn; i++) {
        if ((want == 1 && a->btn[i].isDefault) || (want == 2 && a->btn[i].isCancel)) {
            ccw_button_fire(&a->btn[i]);
            CA("key %d -> item %d\n", ch, a->result);
            return 0;
        }
    }
    return ccwEventNotHandled;
}

/* Window close -> the cancel button (classic behaviour), never a dead modal. */
static CCWStatus close_handler(void *call, CCWEventRef ev, void *ud) {
    (void)call; (void)ev;
    struct alert *a = (struct alert *)ud;
    if (!a || a->nbtn <= 0) return ccwEventNotHandled;
    if (!a->result) {
        a->result = a->btn[0].item;
        for (int i = 0; i < a->nbtn; i++) if (a->btn[i].isCancel) a->result = a->btn[i].item;
    }
    a->done = 1;
    return 0;
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
    if (!ccw_available()) return 0;
    if (!ccw_BeginAppModalStateForWindow) return 0;
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
        ccw_button *b = &a->btn[a->nbtn++];
        strncpy(b->title, txt[i], sizeof b->title - 1);
        b->item = i + 1;
        b->kind = CCW_BTN_PUSH;
        b->resultOut = &a->result;
        b->doneOut = &a->done;
    }
    int dflt = (defaultButton >= 1 && defaultButton <= 3) ? defaultButton : 1;
    for (int i = 0; i < a->nbtn; i++) {
        a->btn[i].isDefault = (a->btn[i].item == dflt);
        a->btn[i].isCancel  = (cancelButton >= 1 && a->btn[i].item == cancelButton);
    }
    if (cancelButton < 1)                       /* classic: Cancel is the Esc button */
        for (int i = 0; i < a->nbtn; i++) if (a->btn[i].item == 2) a->btn[i].isCancel = 1;

    /* ---- classic layout ---- */
    a->msgH  = ccw_wrap_height(a->message, AL_TEXT_W, AL_MSG_SIZE, 1);
    a->infoH = a->info[0] ? ccw_wrap_height(a->info, AL_TEXT_W, AL_INFO_SIZE, 0) : 0;
    double textBottom = AL_MARGIN + a->msgH + (a->infoH ? 8 + a->infoH : 0);
    double iconBottom = AL_MARGIN + AL_ICON;
    double btnTop = (textBottom > iconBottom ? textBottom : iconBottom) + 16;
    a->contentW = AL_W;
    a->contentH = btnTop + AL_BTN_H + AL_MARGIN;

    double x = AL_W - AL_MARGIN;
    for (int i = 0; i < a->nbtn; i++) {          /* OK is rightmost */
        double w = ccw_line_width(a->btn[i].title, AL_INFO_SIZE + 1, 0) + 28;
        if (w < AL_BTN_MINW) w = AL_BTN_MINW;
        a->btn[i].frame = CGRectMake(x - w, btnTop, w, AL_BTN_H);
        x -= w + AL_BTN_GAP;
    }

    /* ---- real Carbon window (movable modal = classic draggable alert) ---- */
    CCWWindowRef win = ccw_create_window(kCCWMovableModalWindow, 0,
                                         a->contentW, a->contentH, "");
    if (!win) { CA("CreateNewWindow failed for every window class\n"); free(a); return 0; }
    a->win = win;
    for (int i = 0; i < a->nbtn; i++) a->btn[i].win = win;

    CCWViewRef root = ccw_HIViewGetRoot ? ccw_HIViewGetRoot(win) : NULL;
    CCWViewRef parent = ccw_content_view_of(win, root);
    if (!parent) { if (ccw_DisposeWindow) ccw_DisposeWindow(win); free(a); return 0; }

    a->body = ccw_make_view(CGRectMake(0, 0, a->contentW, a->contentH),
                            (void *)body_draw, NULL, NULL, NULL, a, parent);
    if (!a->body) { if (ccw_DisposeWindow) ccw_DisposeWindow(win); free(a); return 0; }
    for (int i = 0; i < a->nbtn; i++) ccw_button_install(&a->btn[i], parent);

    if (ccw_InstallEventHandler && ccw_GetWindowEventTarget) {
        void *wt = ccw_GetWindowEventTarget(win);
        struct { uint32_t cls, kind; } kd = { 'keyb', 1 };  /* kEventRawKeyDown  */
        struct { uint32_t cls, kind; } wc = { 'wind', 72 }; /* kEventWindowClose */
        ccw_InstallEventHandler(wt, (void *)key_handler,   1, &kd, a, NULL);
        ccw_InstallEventHandler(wt, (void *)close_handler, 1, &wc, a, NULL);
    }

    ccw_ShowWindow(win);
    if (ccw_SelectWindow) ccw_SelectWindow(win);
    ccw_BeginAppModalStateForWindow(win);
    int live = ccw_run_modal_pump(&a->done, &a->drew, NULL, CLASSIC_ALERT_PROOF_SECS);
    if (ccw_EndAppModalStateForWindow) ccw_EndAppModalStateForWindow(win);
    if (ccw_DisposeWindow) ccw_DisposeWindow(win);

    int item = live ? (a->result ? a->result : a->btn[0].item) : 0;
    CA("classic alert -> item %d (live=%d)\n", item, live);
    free(a);
    return item;
}
