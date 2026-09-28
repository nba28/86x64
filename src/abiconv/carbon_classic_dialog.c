/*
 * carbon_classic_dialog.c — the classic Dialog Manager, drawn as REAL classic
 * Carbon instead of modern AppKit.
 *
 * WHY
 * ---
 * A translated i386 Carbon app that opens a classic modal dialog ('DLOG'+'DITL'
 * run by ModalDialog) used to get a modern AppKit NSWindow full of NSButton /
 * NSTextField.  It WORKED, but it looked like 2026, not like the app's own era —
 * and project doctrine (the authentic-original-look rule) is that every window
 * should look the way it did on its original macOS.  The classic substrate to do
 * it properly already existed and was already proven on screen (Halo's EULA
 * scroll box, the settings controls, the classic alert), so the Dialog Manager
 * had no business being the one holdout on AppKit.
 *
 * WHAT
 * ----
 * A real Carbon window (compositing — 64-bit HIToolbox DELETED the
 * non-compositing model, so every window request without
 * kWindowCompositingAttribute returns -5601) whose contents are SELF-DRAWN
 * classic HIViews from carbon_classic_widgets.c:
 *   ctrlItem+btnCtrl  -> classic Aqua bevelled push button (default = aqua blue)
 *   ctrlItem+chkCtrl  -> classic check box
 *   ctrlItem+radCtrl  -> classic radio button
 *   statText          -> classic Lucida Grande static text, wrapped in its rect
 *   editText          -> the classic sunken edit field with a real blinking
 *                        caret, click-to-place, drag selection and Tab focus
 *                        (64-bit macOS deleted TextEdit AND the Control
 *                        Manager's edit text, so it is drawn and driven by us)
 * driven by a real Carbon modal session (BeginAppModalStateForWindow + our own
 * ReceiveNextEvent/SendEventToEventTarget pump, NOT RunApplicationEventLoop:
 * the app may already be inside one and QuitApplicationEventLoop would tear
 * down the app's loop rather than ours).
 *
 * The classic keyboard contract is preserved exactly: Return/Enter = default
 * item, Escape / Cmd-. = cancel item, Tab / Shift-Tab cycles the edit fields,
 * and ModalDialog returns the classic item number so the app's own validate
 * loop (e.g. Halo's gcdkey checksum) runs UNCHANGED.  We never validate or
 * bypass anything the app checks.
 *
 * NEVER REGRESSES FUNCTION: ccd_create returns NULL if the classic window
 * cannot be materialized, and ccd_run_modal returns 0 if the pump proves dead
 * (nothing drew and no event arrived) — in both cases carbon_dialog_shim.m
 * falls back to its existing, working AppKit dialog.  A dead un-dismissable
 * modal is never an outcome.
 *
 * UNIVERSAL: triggers on the DITL item TYPE and rect, never on an app name, a
 * resource id or an item string.  SINGLE PURPOSE: materialize + run; the i386
 * marshalling and the DLOG/DITL parsing stay in carbon_dialog_shim.m, the
 * standard alert in carbon_classic_alert.c, nibs in carbon_nib_shim.c.
 */

#include "carbon_classic_dialog.h"
#include "carbon_classic_widgets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* AppKit window-management + foreground bootstrap (carbon_appkit_host.c): a
 * pure-Carbon translated process is not a foreground GUI app, so no Carbon
 * window can become key and every modal loop starves. */
extern void carbon_ensure_window_host(void);
extern void carbon_ensure_foreground(void);

/* ---- DITL item types (classic Dialogs.h), absolute type-byte values ------ */
enum {
    kDItemUser     = 0,
    kDItemHelp     = 1,
    kDItemBtn      = 4,     /* ctrlItem + btnCtrl */
    kDItemChk      = 5,     /* ctrlItem + chkCtrl */
    kDItemRad      = 6,     /* ctrlItem + radCtrl */
    kDItemResCtl   = 7,     /* ctrlItem + resCtrl */
    kDItemStatText = 8,
    kDItemEditText = 16,
    kDItemIcon     = 32,
    kDItemPicture  = 64,
};

/* The classic Dialog Manager's STANDARD item numbers, verbatim from the real
 * 10.6 SDK HIToolbox/Dialogs.h:
 *     enum { kStdOkItemIndex = 1, kStdCancelItemIndex = 2,
 *            ok = kStdOkItemIndex, cancel = kStdCancelItemIndex };
 * A format-level invariant of every classic DITL, not a per-app guess. */
enum {
    kStdOkItemIndex     = 1,
    kStdCancelItemIndex = 2,
};

#define CCD_MAX_ITEMS 64
/* Classic dialog body colour: the era's light platinum/aqua window fill. */
#define CCD_BG 0.929

struct ccd_item_live {
    int         type, disabled;
    CGRect      frame;                 /* content coords, y DOWN */
    char        text[512];             /* statText body / control title */
    ccw_button *btn;                   /* ctrlItem items */
    ccw_edit   *ed;                    /* editText items */
};

struct ccd_dialog {
    CCWWindowRef win;
    CCWViewRef   body;
    int          nitems;
    struct ccd_item_live it[CCD_MAX_ITEMS];
    ccw_button   btnpool[CCD_MAX_ITEMS];
    ccw_edit     edpool[CCD_MAX_ITEMS];
    int          nbtn, ned;
    int          defaultItem, cancelItem;
    int          result;               /* classic item hit, 0 = none yet   */
    int          done;                 /* pump exit flag                    */
    int          drew;                 /* the body view actually drew once  */
    int          ranOnce;              /* a modal round already completed   */
    double       contentW, contentH;
    ccw_edit    *focus;                /* caret-blink slot for the pump     */
};

/* If the window is up but the pump delivers NOTHING this long and nothing ever
 * drew, this OS will not drive it: tear down and let the caller fall back. */
#define CCD_PROOF_SECS 3.0

/* ---------------------------- body drawing ------------------------------- */
/* The classic dialog background plus every item that has no control of its own:
 * static text, and a visible placeholder frame for icon/picture items whose
 * classic resources we cannot draw. */
static CCWStatus body_draw(void *call, CCWEventRef ev, void *ud) {
    (void)call;
    ccd_dialog *d = (ccd_dialog *)ud;
    CGContextRef cg = ccw_draw_cg(ev);
    double W = 0, H = 0;
    if (!d || !cg || !ccw_view_size(d->body, &W, &H)) return ccwEventNotHandled;
    d->drew = 1;
    CGContextSaveGState(cg);
    CGContextSetRGBFillColor(cg, CCD_BG, CCD_BG, CCD_BG, 1.0);
    CGContextFillRect(cg, CGRectMake(0, 0, W, H));
    for (int i = 0; i < d->nitems; i++) {
        struct ccd_item_live *it = &d->it[i];
        if (it->type == kDItemStatText && it->text[0]) {
            /* classic static text: Lucida Grande 11, wrapped inside its own rect */
            ccw_draw_wrapped(cg, it->text, it->frame.origin.x, it->frame.origin.y,
                             it->frame.size.width, it->frame.size.height, H,
                             11, 0, 0.0, 0.0, 0.0);
        } else if (it->type == kDItemIcon || it->type == kDItemPicture) {
            /* the classic 'ICON'/'PICT' body is a resource id we do not render
             * yet: draw the classic empty-well frame rather than nothing, so the
             * layout still reads correctly. */
            CGContextSetRGBStrokeColor(cg, 0.62, 0.62, 0.65, 1.0);
            CGContextSetLineWidth(cg, 1.0);
            CGContextStrokeRect(cg, CGRectMake(it->frame.origin.x + 0.5,
                                               it->frame.origin.y + 0.5,
                                               it->frame.size.width - 1,
                                               it->frame.size.height - 1));
        }
    }
    CGContextRestoreGState(cg);
    return 0;
}

/* ------------------------------- keyboard -------------------------------- */
static struct ccd_item_live *item_at(ccd_dialog *d, int item) {
    if (!d || item < 1 || item > d->nitems) return NULL;
    return &d->it[item - 1];
}

static void fire_item(ccd_dialog *d, int item) {
    struct ccd_item_live *it = item_at(d, item);
    if (!it || it->disabled) return;
    if (it->btn) { ccw_button_fire(it->btn); return; }
    d->result = item;
    d->done = 1;
}

/* The classic modal keyboard contract, at the WINDOW target so it works no
 * matter which HIView HIToolbox thinks is focused: Return/Enter fire the
 * default item, Escape and Cmd-. fire the cancel item, Tab cycles the edit
 * fields, and every other key goes to the focused classic edit field. */
static CCWStatus key_handler(void *call, CCWEventRef ev, void *ud) {
    (void)call;
    ccd_dialog *d = (ccd_dialog *)ud;
    if (!d || !ccw_GetEventParameter) return ccwEventNotHandled;
    unsigned char ch = 0;
    uint32_t mods = 0;
    if (ccw_GetEventParameter(ev, 'kchr' /*kEventParamKeyMacCharCodes*/, 'TEXT',
                              NULL, sizeof ch, NULL, &ch) != 0)
        return ccwEventNotHandled;
    ccw_GetEventParameter(ev, 'kmod' /*kEventParamKeyModifiers*/, 'magn',
                          NULL, sizeof mods, NULL, &mods);
    const uint32_t cmdKeyMask = 0x0100, shiftKeyMask = 0x0200;

    if (ch == 13 || ch == 3) {                      /* Return / Enter */
        if (d->defaultItem) { fire_item(d, d->defaultItem); return 0; }
        return ccwEventNotHandled;
    }
    if (ch == 27 || (ch == '.' && (mods & cmdKeyMask))) {   /* Escape / Cmd-. */
        if (d->cancelItem) { fire_item(d, d->cancelItem); return 0; }
        return ccwEventNotHandled;
    }
    if (ch == 9) {                                  /* Tab / Shift-Tab */
        if (ccw_focus_next(d->win, (mods & shiftKeyMask) ? 1 : 0)) {
            d->focus = ccw_focused_edit(d->win);
            return 0;
        }
        return ccwEventNotHandled;
    }
    ccw_edit *f = ccw_focused_edit(d->win);
    d->focus = f;
    if (f && ccw_edit_key(f, ch, mods)) return 0;
    return ccwEventNotHandled;
}

/* A click anywhere may have moved focus between fields; keep the pump's caret
 * slot in step.  (kEventWindowHandleContentClick, after the views tracked.) */
static CCWStatus click_handler(void *call, CCWEventRef ev, void *ud) {
    (void)call; (void)ev;
    ccd_dialog *d = (ccd_dialog *)ud;
    if (d) d->focus = ccw_focused_edit(d->win);
    return ccwEventNotHandled;                      /* observe only, never eat */
}

/* Window close box -> the CANCEL item, and only ever the cancel item.
 *
 * It used to fall back to `defaultItem` when no cancel item was known, which
 * reported the OK item from a gesture that means the exact opposite — the app
 * then acted on an empty/unconfirmed dialog as if the user had accepted it.  A
 * classic modal dialog with no cancel item has no close box at all, so with
 * nothing to cancel to the right answer is to ignore the close and leave the
 * modal running (still never a DEAD modal: Return/Enter and the buttons work). */
static CCWStatus close_handler(void *call, CCWEventRef ev, void *ud) {
    (void)call; (void)ev;
    ccd_dialog *d = (ccd_dialog *)ud;
    if (!d) return ccwEventNotHandled;
    if (!d->cancelItem) return 0;      /* never report OK for a close gesture */
    fire_item(d, d->cancelItem);
    if (!d->done) { d->result = d->cancelItem; d->done = 1; }
    return 0;
}

/* ------------------------------ construction ----------------------------- */
ccd_dialog *ccd_create(const char *utf8Title, int contentW, int contentH,
                       const ccd_item *items, int nitems) {
    if (!ccw_available()) { return NULL; }
    if (getenv("M64_NO_CLASSIC_DIALOG")) return NULL;   /* A/B kill switch */
    if (contentW < 80)  contentW = 360;
    if (contentH < 40)  contentH = 160;

    carbon_ensure_window_host();
    carbon_ensure_foreground();

    ccd_dialog *d = (ccd_dialog *)calloc(1, sizeof *d);
    if (!d) return NULL;
    d->contentW = contentW;
    d->contentH = contentH;

    /* A titled classic dialog is a MOVABLE modal; an untitled one is the plain
     * modal dBoxProc.  Both must be compositing on 64-bit. */
    uint32_t cls = (utf8Title && utf8Title[0]) ? kCCWMovableModalWindow : kCCWModalWindow;
    d->win = ccw_create_window(cls, 0, contentW, contentH, utf8Title);
    if (!d->win) { free(d); return NULL; }

    CCWViewRef root = ccw_HIViewGetRoot ? ccw_HIViewGetRoot(d->win) : NULL;
    CCWViewRef parent = ccw_content_view_of(d->win, root);
    if (!parent) { ccw_DisposeWindow(d->win); free(d); return NULL; }

    d->body = ccw_make_view(CGRectMake(0, 0, contentW, contentH),
                            (void *)body_draw, NULL, NULL, NULL, d, parent);
    if (!d->body) { ccw_DisposeWindow(d->win); free(d); return NULL; }

    int n = nitems > CCD_MAX_ITEMS ? CCD_MAX_ITEMS : nitems;
    d->nitems = n;
    for (int i = 0; i < n; i++) {
        const ccd_item *src = &items[i];
        struct ccd_item_live *it = &d->it[i];
        it->type = src->type;
        it->disabled = src->disabled;
        it->frame = CGRectMake(src->left, src->top,
                               src->right - src->left, src->bottom - src->top);
        if (it->frame.size.width < 0)  it->frame.size.width = 0;
        if (it->frame.size.height < 0) it->frame.size.height = 0;
        if (src->text) { strncpy(it->text, src->text, sizeof it->text - 1); }

        if (it->type == kDItemBtn || it->type == kDItemChk ||
            it->type == kDItemRad || it->type == kDItemResCtl) {
            ccw_button *b = &d->btnpool[d->nbtn++];
            strncpy(b->title, it->text, sizeof b->title - 1);
            b->item = i + 1;
            b->frame = it->frame;
            b->kind = (it->type == kDItemChk) ? CCW_BTN_CHECK
                    : (it->type == kDItemRad) ? CCW_BTN_RADIO : CCW_BTN_PUSH;
            b->disabled = it->disabled;
            b->win = d->win;
            b->resultOut = &d->result;
            /* a check box / radio does NOT end a classic modal; only a real
             * push button (or a keyboard equivalent) does. */
            b->doneOut = (b->kind == CCW_BTN_PUSH) ? &d->done : NULL;
            it->btn = b;
            ccw_button_install(b, parent);
        } else if (it->type == kDItemEditText) {
            ccw_edit *e = &d->edpool[d->ned++];
            strncpy(e->text, it->text, sizeof e->text - 1);
            e->frame = it->frame;
            e->enabled = !it->disabled;
            e->win = d->win;
            e->owner = d;
            it->ed = e;
            ccw_edit_install(e, parent);
        }
        /* statText / icon / picture / userItem are painted by body_draw */
    }

    /* classic default: the FIRST push button is the default item until the app
     * says otherwise with SetDialogDefaultItem. */
    for (int i = 0; i < n && !d->defaultItem; i++)
        if (d->it[i].btn && d->it[i].btn->kind == CCW_BTN_PUSH && !d->it[i].disabled)
            ccd_set_default_item(d, i + 1);

    /* ...and DITL item 2 is the cancel item, because that is what the classic
     * standard item numbers MEAN (kStdCancelItemIndex, above).  Honour it when
     * item 2 really is an enabled push button and is not already the default:
     * without this, Escape / Cmd-. are dead and the close box has nothing to
     * cancel to for the many classic apps that never call SetDialogCancelItem
     * (Halo's DITL 10001 is exactly that shape — item 1 'OK', item 2 'Quit').
     * SetDialogCancelItem still overrides it. */
    if (n >= kStdCancelItemIndex) {
        struct ccd_item_live *c = &d->it[kStdCancelItemIndex - 1];
        if (c->btn && c->btn->kind == CCW_BTN_PUSH && !c->disabled &&
            d->defaultItem != kStdCancelItemIndex)
            ccd_set_cancel_item(d, kStdCancelItemIndex);
    }

    if (ccw_InstallEventHandler && ccw_GetWindowEventTarget) {
        void *wt = ccw_GetWindowEventTarget(d->win);
        struct { uint32_t cls, kind; } kd[2] = { { 'keyb', 1 },   /* RawKeyDown   */
                                                 { 'keyb', 2 } }; /* RawKeyRepeat */
        struct { uint32_t cls, kind; } wc = { 'wind', 72 };       /* WindowClose  */
        struct { uint32_t cls, kind; } cc = { 'wind', 25 };       /* ContentClick */
        ccw_InstallEventHandler(wt, (void *)key_handler,   2, kd,  d, NULL);
        ccw_InstallEventHandler(wt, (void *)close_handler, 1, &wc, d, NULL);
        ccw_InstallEventHandler(wt, (void *)click_handler, 1, &cc, d, NULL);
    }

    /* first edit field takes the caret, classic-style */
    for (int i = 0; i < n; i++)
        if (d->it[i].ed) { ccw_edit_set_focus(d->it[i].ed, 1);
                           ccw_edit_select_all(d->it[i].ed);
                           d->focus = d->it[i].ed; break; }

    return d;
}

/* -------------------------------- modal ---------------------------------- */
int ccd_run_modal(ccd_dialog *d) {
    if (!d || !d->win) return 0;
    d->done = 0;
    d->result = 0;
    if (ccw_ShowWindow) ccw_ShowWindow(d->win);
    if (ccw_SelectWindow) ccw_SelectWindow(d->win);
    carbon_ensure_foreground();
    if (ccw_BeginAppModalStateForWindow) ccw_BeginAppModalStateForWindow(d->win);
    /* Only the FIRST round gets the liveness proof: once a round has completed
     * we know the pump works, and a later round legitimately sits idle for as
     * long as the user takes to type. */
    int live = ccw_run_modal_pump(&d->done, d->ranOnce ? NULL : &d->drew,
                                  (ccw_edit *const *)&d->focus,
                                  d->ranOnce ? 0.0 : CCD_PROOF_SECS);
    if (ccw_EndAppModalStateForWindow) ccw_EndAppModalStateForWindow(d->win);
    if (!live) { return 0; }   /* pump proved dead -> caller falls back */
    d->ranOnce = 1;
    int item = d->result ? d->result : (d->defaultItem ? d->defaultItem : 1);
    return item;
}

/* ------------------------------- item access ----------------------------- */
int ccd_get_text(ccd_dialog *d, int item, char *out, size_t outsz) {
    struct ccd_item_live *it = item_at(d, item);
    if (!it || !out || !outsz) return 0;
    const char *s = it->ed ? it->ed->text : (it->btn ? it->btn->title : it->text);
    strncpy(out, s ? s : "", outsz - 1);
    out[outsz - 1] = 0;
    return 1;
}

int ccd_set_text(ccd_dialog *d, int item, const char *utf8) {
    struct ccd_item_live *it = item_at(d, item);
    if (!it) return 0;
    strncpy(it->text, utf8 ? utf8 : "", sizeof it->text - 1);
    it->text[sizeof it->text - 1] = 0;
    if (it->ed) ccw_edit_set_text(it->ed, it->text);
    else if (it->btn) {
        strncpy(it->btn->title, it->text, sizeof it->btn->title - 1);
        if (ccw_HIViewSetNeedsDisplay) ccw_HIViewSetNeedsDisplay(it->btn->view, 1);
    } else if (ccw_HIViewSetNeedsDisplay) {
        ccw_HIViewSetNeedsDisplay(d->body, 1);     /* statText repaint */
    }
    return 1;
}

void ccd_set_default_item(ccd_dialog *d, int item) {
    struct ccd_item_live *it = item_at(d, item);
    if (!it) return;
    for (int i = 0; i < d->nitems; i++)
        if (d->it[i].btn) d->it[i].btn->isDefault = 0;
    d->defaultItem = item;
    if (it->btn) {
        it->btn->isDefault = 1;
        if (ccw_HIViewSetNeedsDisplay) ccw_HIViewSetNeedsDisplay(it->btn->view, 1);
    }
}

int ccd_default_item(ccd_dialog *d) { return d ? d->defaultItem : 0; }
int ccd_cancel_item(ccd_dialog *d)  { return d ? d->cancelItem  : 0; }

void ccd_set_cancel_item(ccd_dialog *d, int item) {
    struct ccd_item_live *it = item_at(d, item);
    if (!it) return;
    for (int i = 0; i < d->nitems; i++)
        if (d->it[i].btn) d->it[i].btn->isCancel = 0;
    d->cancelItem = item;
    if (it->btn) it->btn->isCancel = 1;
}

int ccd_get_value(ccd_dialog *d, int item, int *outValue) {
    struct ccd_item_live *it = item_at(d, item);
    if (!it || !it->btn) return 0;
    if (outValue) *outValue = it->btn->value;
    return 1;
}

int ccd_set_value(ccd_dialog *d, int item, int value) {
    struct ccd_item_live *it = item_at(d, item);
    if (!it || !it->btn) return 0;
    it->btn->value = value;
    if (ccw_HIViewSetNeedsDisplay) ccw_HIViewSetNeedsDisplay(it->btn->view, 1);
    return 1;
}

void ccd_clear_focus(ccd_dialog *d) {
    if (!d) return;
    ccw_edit *f = ccw_focused_edit(d->win);
    if (f) ccw_edit_set_focus(f, 0);
    d->focus = NULL;
}

void ccd_dispose(ccd_dialog *d) {
    if (!d) return;
    ccw_forget_window(d->win);      /* drop this window's fields from the focus group */
    if (d->win && ccw_DisposeWindow) ccw_DisposeWindow(d->win);
    d->win = NULL;
    free(d);
}
