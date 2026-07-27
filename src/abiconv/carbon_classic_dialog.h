/*
 * carbon_classic_dialog.h — the AUTHENTIC classic Dialog Manager surface.
 *
 * ONE JOB: turn an ALREADY-PARSED 'DLOG'+'DITL' into a REAL classic Carbon
 * dialog — a compositing Carbon window whose contents are self-drawn classic
 * HIViews (carbon_classic_widgets.c) — and run it with a real Carbon modal
 * session that returns the classic item numbers.
 *
 * The resource PARSING stays in carbon_dialog_shim.m (which owns the i386
 * Dialog Manager entry points); this file owns only the materialization and the
 * modal run.  Every call is generic over the DITL structure: nothing here keys
 * on an app, a resource id, or an item string.
 */
#ifndef CARBON_CLASSIC_DIALOG_H
#define CARBON_CLASSIC_DIALOG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ccd_dialog ccd_dialog;

/* One parsed DITL item, in classic local (top-left origin) coordinates.
 * `type` is the raw DITL type byte with the 0x80 disable bit stripped
 * (4=button, 5=checkbox, 6=radio, 7=resource control, 8=statText,
 * 16=editText, 32=icon, 64=picture, 0=userItem). */
typedef struct {
    int         type;
    int         disabled;
    short       top, left, bottom, right;
    const char *text;          /* pascal-string body already converted to C */
} ccd_item;

/* Materialize the dialog.  Returns NULL if the classic substrate cannot build
 * the window, in which case the caller must fall back (never regress function).
 * `contentW/H` are the DLOG's content size in points. */
ccd_dialog *ccd_create(const char *utf8Title, int contentW, int contentH,
                       const ccd_item *items, int nitems);

/* Run one classic modal round.  Returns the classic 1-based item number the
 * user hit, or 0 if the modal pump proved DEAD on this OS (nothing ever drew
 * and no event arrived) — the caller must then dispose and fall back. */
int  ccd_run_modal(ccd_dialog *d);

/* Live item text (what the user actually typed).  Returns 1 if the item exists. */
int  ccd_get_text(ccd_dialog *d, int item, char *out, size_t outsz);
int  ccd_set_text(ccd_dialog *d, int item, const char *utf8);
/* Classic Return / Esc wiring. */
void ccd_set_default_item(ccd_dialog *d, int item);
void ccd_set_cancel_item(ccd_dialog *d, int item);
/* Classic control value (checkbox/radio) — GetControlValue on a DITL control. */
int  ccd_get_value(ccd_dialog *d, int item, int *outValue);
int  ccd_set_value(ccd_dialog *d, int item, int value);
void ccd_clear_focus(ccd_dialog *d);
void ccd_dispose(ccd_dialog *d);

#ifdef __cplusplus
}
#endif
#endif /* CARBON_CLASSIC_DIALOG_H */
