#!/bin/bash
#
# carbon_classic_dialog_test.sh — regression guard for the AUTHENTIC CLASSIC
# Carbon Dialog Manager and, above all, for THE CLASSIC EDIT FIELD
# (src/abiconv/carbon_classic_widgets.c + carbon_classic_dialog.c).
#
# WHAT IT GUARDS
# --------------
# Project rule (the authentic-original-look rule): every window must look the way
# it did on its original macOS.  A translated i386 app that opens a classic modal
# dialog ('DLOG'+'DITL' run by ModalDialog) used to get a MODERN AppKit NSWindow
# full of NSButton/NSTextField — it worked, but it was the wrong decade.  It is
# now materialized as a REAL classic Carbon window whose contents are self-drawn
# classic HIViews, with the AppKit window kept only as the fallback.
#
# The load-bearing new component is the classic EDIT TEXT field: 64-bit macOS
# deleted TextEdit AND the Control Manager's edit-text control outright, so the
# sunken frame, the blinking caret, click-to-place, drag/shift selection and the
# classic editing keys are all drawn and driven by us.  Halo's "Enter Your Halo
# Product Key" gate and every IBCarbonEditText in a nib depend on it.
#
# WHAT IT PROVES
#   (1) EDITING MODEL (always runs — no window server, no Accessibility rights):
#       ccw_edit_key implements the classic contract — typing inserts at the
#       caret, Backspace/Delete, arrow keys move the caret, Shift-arrow extends
#       a selection, typing REPLACES a selection, Cmd-A selects all, the maxLen
#       cap holds, and Return/Enter/Tab/Esc are DECLINED so the dialog (not the
#       field) gets them.  This half is the real regression guard: it fails
#       loudly if the editing model ever breaks.
#   (2) MATERIALIZES: ccd_create builds a real Carbon window with the DLOG's
#       content geometry, and a TITLED dialog is movable-modal (structure region
#       taller than content = it has a title bar).
#   (3) A/B KILL SWITCH: with M64_NO_CLASSIC_DIALOG=1 ccd_create must DECLINE
#       (return NULL) so carbon_dialog_shim.m falls back to AppKit — the negative
#       half, which proves the test is exercising the new code.
#   (4) TYPING + CLICK, END TO END (only where synthetic input is permitted):
#       real CGEventPost keystrokes reach the focused classic field and
#       ccd_get_text returns exactly what was typed, then a real synthetic click
#       on the OK button ends the modal with that classic item number.  SKIPs
#       cleanly when there is no window server / no Accessibility rights.
#
# Runs NATIVE x86_64 (no i386 sysroot needed).
set -u
cd "$(dirname "$0")"

W="../src/abiconv/carbon_classic_widgets.c"
D="../src/abiconv/carbon_classic_dialog.c"
HOST="../src/abiconv/carbon_appkit_host.c"
for f in "$W" "$D" "$HOST"; do
    [ -f "$f" ] || { echo "classicdialog: SKIP (missing $f)"; exit 0; }
done

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/h.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>
#include <ApplicationServices/ApplicationServices.h>
#include "carbon_classic_widgets.h"
#include "carbon_classic_dialog.h"

/* ---------- (1) the classic editing model, headless ---------- */
static int fails;
static void eq(const char *what, const char *got, const char *want) {
    if (strcmp(got, want)) { printf("  EDIT FAIL %s: '%s' want '%s'\n", what, got, want); fails++; }
}
static void eqi(const char *what, int got, int want) {
    if (got != want) { printf("  EDIT FAIL %s: %d want %d\n", what, got, want); fails++; }
}
static void type(ccw_edit *e, const char *s, uint32_t mods) {
    for (; *s; s++) ccw_edit_key(e, (unsigned char)*s, mods);
}
static int edit_model_checks(void) {
    ccw_edit e; memset(&e, 0, sizeof e);
    e.enabled = 1; e.maxLen = 16;

    type(&e, "HELLO", 0);
    eq("insert", e.text, "HELLO");
    eqi("caret after insert", e.caret, 5);

    ccw_edit_key(&e, 8, 0);                       /* Backspace */
    eq("backspace", e.text, "HELL");

    ccw_edit_key(&e, 28, 0); ccw_edit_key(&e, 28, 0);   /* Left, Left */
    eqi("caret after 2x left", e.caret, 2);
    type(&e, "X", 0);
    eq("insert mid-string", e.text, "HEXLL");

    ccw_edit_key(&e, 1, 0);                       /* Home */
    eqi("home", e.caret, 0);
    ccw_edit_key(&e, 4, 0);                       /* End  */
    eqi("end", e.caret, 5);
    ccw_edit_key(&e, 127, 0);                     /* fwd Delete at end = no-op */
    eq("fwd delete at end", e.text, "HEXLL");

    /* Shift-Left extends a selection; typing replaces it (classic) */
    ccw_edit_key(&e, 28, 0x0200); ccw_edit_key(&e, 28, 0x0200);
    type(&e, "Z", 0);
    eq("typing replaces selection", e.text, "HEXZ");

    /* Cmd-A select all, then type replaces everything */
    ccw_edit_key(&e, 'a', 0x0100);
    type(&e, "Q", 0);
    eq("cmd-A select all + type", e.text, "Q");

    /* maxLen cap holds */
    type(&e, "0123456789ABCDEFGHIJ", 0);
    eqi("maxLen cap", (int)strlen(e.text), 16);

    /* Return / Enter / Tab / Esc must be DECLINED so the dialog handles them */
    eqi("Return declined", ccw_edit_key(&e, 13, 0), 0);
    eqi("Enter declined",  ccw_edit_key(&e, 3, 0), 0);
    eqi("Tab declined",    ccw_edit_key(&e, 9, 0), 0);
    eqi("Esc declined",    ccw_edit_key(&e, 27, 0), 0);

    /* a disabled field takes nothing */
    ccw_edit d2; memset(&d2, 0, sizeof d2); d2.enabled = 0;
    ccw_edit_key(&d2, 'x', 0);
    eq("disabled field ignores keys", d2.text, "");
    return fails;
}

/* ---------- (2)(4) the real classic window ---------- */
/* A miniature DITL: statText prompt, editText field, Cancel, OK. */
#define DW 352
#define DH 152
static ccd_item g_items[] = {
    { 8 /*statText*/, 0, 12,  16, 44,  336, "Enter a value:" },
    { 16 /*editText*/,0, 52,  16, 74,  336, ""               },
    { 4 /*button*/,   0, 112, 168, 132, 240, "Cancel"        },
    { 4 /*button*/,   0, 112, 252, 132, 336, "OK"            },
};
#define NITEMS ((int)(sizeof g_items / sizeof g_items[0]))
#define OK_ITEM 4

/* A Halo-shaped DITL: item 1 = OK, item 2 = the cancel button, then a key field.
 * This is the exact shape of Halo's real DITL 10001, and it is what exercises
 * the classic STANDARD item numbers (10.6 HIToolbox/Dialogs.h:
 * kStdOkItemIndex = 1, kStdCancelItemIndex = 2) that ccd_create applies for the
 * many classic apps that never call SetDialogCancelItem. */
#define HW  352
#define HH2 120
static ccd_item g_hitems[] = {
    { 4  /*button*/,   0, 88, 268, 108, 338, "OK"   },   /* kStdOkItemIndex     */
    { 4  /*button*/,   0, 88, 184, 108, 254, "Quit" },   /* kStdCancelItemIndex */
    { 16 /*editText*/, 0, 48,  20,  65,  80, ""     },
};
#define NHITEMS ((int)(sizeof g_hitems / sizeof g_hitems[0]))

static volatile int g_done;
static int g_w, g_h, g_struct_h;
static char g_typed[128];
static ccd_dialog *g_dlg;
static const char *g_mode = "click";

static void tap_key(CGKeyCode k) {
    CGEventRef d = CGEventCreateKeyboardEvent(NULL, k, true);
    if (d) { CGEventPost(kCGSessionEventTap, d); CFRelease(d); }
    usleep(60000);
    CGEventRef u = CGEventCreateKeyboardEvent(NULL, k, false);
    if (u) { CGEventPost(kCGSessionEventTap, u); CFRelease(u); }
    usleep(60000);
}

static void *watchdog(void *arg) {
    (void)arg;
    usleep(1600000);
    void *(*FrontWindow)(void) = dlsym(RTLD_DEFAULT, "FrontWindow");
    int32_t (*GetWindowBounds)(void *, uint32_t, short *) = dlsym(RTLD_DEFAULT, "GetWindowBounds");
    void *w = FrontWindow ? FrontWindow() : NULL;
    short c[4] = {0,0,0,0}, s[4] = {0,0,0,0};       /* top,left,bottom,right */
    if (w && GetWindowBounds) {
        GetWindowBounds(w, 33 /*kWindowContentRgn*/,   c);
        GetWindowBounds(w, 32 /*kWindowStructureRgn*/, s);
    }
    if (!w || c[3] <= c[1]) { fprintf(stderr, "no window\n"); _exit(3); }
    g_w = c[3] - c[1]; g_h = c[2] - c[0];
    g_struct_h = s[2] - s[0];
    fprintf(stderr, "  window content %dx%d structure_h=%d\n", g_w, g_h, g_struct_h);

    /* type A B C into the focused classic edit field */
    tap_key(0 /*A*/); tap_key(11 /*B*/); tap_key(8 /*C*/);
    usleep(150000);
    ccd_get_text(g_dlg, 2, g_typed, sizeof g_typed);

    /* click the centre of the OK item's rect (classic local coords -> global) */
    double x = c[1] + (g_items[OK_ITEM - 1].left + g_items[OK_ITEM - 1].right) / 2.0;
    double y = c[0] + (g_items[OK_ITEM - 1].top + g_items[OK_ITEM - 1].bottom) / 2.0;
    CGPoint pt = CGPointMake(x, y);
    CGEventRef mv = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, pt, kCGMouseButtonLeft);
    if (mv) { CGEventPost(kCGSessionEventTap, mv); CFRelease(mv); }
    usleep(200000);
    CGEventRef md = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDown, pt, kCGMouseButtonLeft);
    if (md) { CGEventPost(kCGSessionEventTap, md); CFRelease(md); }
    usleep(250000);
    CGEventRef mu = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseUp, pt, kCGMouseButtonLeft);
    if (mu) { CGEventPost(kCGSessionEventTap, mu); CFRelease(mu); }

    usleep(3500000);
    if (!g_done) { fprintf(stderr, "HUNG trusted=%d\n", (int)AXIsProcessTrusted()); _exit(9); }
    return NULL;
}

/* Locate the live dialog's content rect; _exit(3) if there is no window. */
static void front_content(short c[4]) {
    void *(*FrontWindow)(void) = dlsym(RTLD_DEFAULT, "FrontWindow");
    int32_t (*GetWindowBounds)(void *, uint32_t, short *) = dlsym(RTLD_DEFAULT, "GetWindowBounds");
    void *w = FrontWindow ? FrontWindow() : NULL;
    c[0] = c[1] = c[2] = c[3] = 0;
    if (w && GetWindowBounds) GetWindowBounds(w, 33 /*kWindowContentRgn*/, c);
    if (!w || c[3] <= c[1]) { fprintf(stderr, "no window\n"); _exit(3); }
}

/* Watchdog for the KEYBOARD arms — it never clicks anything.
 *   "esc":     Escape must end the modal with the classic cancel item.
 *   "phantom": park the pointer over the OK button with the mouse UP and prove
 *              the modal does NOT end.  A tracking loop wired to
 *              kEventControlHit fires on pointer position alone, so the modal
 *              would self-dismiss here with no input at all — the exact
 *              phantom-click shape that made a real dialog look like it
 *              auto-dismissed.  Then Escape, so the arm always terminates. */
static int g_phantom_ok = -1;
static void *watchdog_key(void *arg) {
    const char *mode = (const char *)arg;
    usleep(1600000);
    if (!strcmp(mode, "phantom")) {
        short c[4]; front_content(c);
        CGWarpMouseCursorPosition(CGPointMake(
            c[1] + (g_hitems[0].left + g_hitems[0].right) / 2.0,
            c[0] + (g_hitems[0].top  + g_hitems[0].bottom) / 2.0));
        usleep(2500000);
        g_phantom_ok = g_done ? 0 : 1;        /* 1 = no phantom fire (correct) */
    }
    tap_key(53 /*Escape*/);                   /* the classic cancel key */
    usleep(2500000);
    if (!g_done) { fprintf(stderr, "HUNG trusted=%d\n", (int)AXIsProcessTrusted()); _exit(9); }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc > 1) g_mode = argv[1];

    /* Park the cursor away from where the dialog will appear.  Same reason as
     * carbon_classic_alert_test.sh: an ambient pointer resting over the button
     * area leaves the control pre-hovered and the synthetic click never
     * dismisses, so the arm fails for a reason that has nothing to do with the
     * code under test.  The phantom arm warps deliberately later; this only
     * fixes the STARTING position. */
    CGWarpMouseCursorPosition(CGPointMake(20, 20));
    usleep(150000);

    if (!strcmp(g_mode, "model")) {               /* headless editing model only */
        int f = edit_model_checks();
        printf("editmodel fails=%d\n", f);
        return f ? 1 : 0;
    }

    if (!strcmp(g_mode, "items")) {   /* the classic standard item numbers */
        ccd_dialog *a = ccd_create("A", DW, DH, g_items, NITEMS);
        if (!a) { fprintf(stderr, "ccd_create failed\n"); return 3; }
        printf("A default=%d cancel=%d\n", ccd_default_item(a), ccd_cancel_item(a));
        ccd_dispose(a);
        ccd_dialog *b = ccd_create("B", HW, HH2, g_hitems, NHITEMS);
        if (!b) { fprintf(stderr, "ccd_create failed\n"); return 3; }
        printf("B default=%d cancel=%d\n", ccd_default_item(b), ccd_cancel_item(b));
        ccd_dispose(b);
        return 0;
    }

    if (!strcmp(g_mode, "esc") || !strcmp(g_mode, "phantom")) {
        /* No ccd_set_cancel_item call: the classic kStdCancelItemIndex default
         * is what must wire Escape here. */
        g_dlg = ccd_create("Enter Your Product Key", HW, HH2, g_hitems, NHITEMS);
        if (!g_dlg) { fprintf(stderr, "ccd_create failed\n"); return 3; }
        pthread_t t2; pthread_create(&t2, NULL, watchdog_key, (void *)g_mode);
        int it = ccd_run_modal(g_dlg);
        g_done = 1;
        printf("item=%d phantom_ok=%d\n", it, g_phantom_ok);
        ccd_dispose(g_dlg);
        return 0;
    }

    g_dlg = ccd_create("Enter a value", DW, DH, g_items, NITEMS);
    if (!strcmp(g_mode, "kill")) {                 /* A/B: must DECLINE */
        printf("created=%d\n", g_dlg != NULL);
        return 0;
    }
    if (!g_dlg) { fprintf(stderr, "ccd_create failed\n"); return 3; }
    ccd_set_cancel_item(g_dlg, 3);
    ccd_set_default_item(g_dlg, OK_ITEM);

    pthread_t t; pthread_create(&t, NULL, watchdog, NULL);
    int item = ccd_run_modal(g_dlg);
    g_done = 1;
    printf("item=%d typed='%s' w=%d h=%d structh=%d\n", item, g_typed, g_w, g_h, g_struct_h);
    ccd_dispose(g_dlg);
    return 0;
}
EOF

if ! clang -arch x86_64 -Wno-deprecated-declarations -I../src/abiconv \
        -o "$TMP/t" "$TMP/h.c" "$W" "$D" "$HOST" \
        -framework Carbon -framework Cocoa -framework CoreText \
        -framework ApplicationServices 2>"$TMP/cerr"; then
    echo "classicdialog: SKIP (compile failed)"; sed -n '1,25p' "$TMP/cerr"; exit 0
fi

PASS=1

# Synthetic HID input goes to whichever process is FRONTMOST, so consecutive GUI
# arms must not overlap: let each harness fully exit and the window server settle
# before the next one opens its window, or keystrokes bleed between arms.
settle() { sleep 1.5; }

# (1) the editing model — ALWAYS runs, no window server needed.
MODEL=$(arch -x86_64 "$TMP/t" model 2>&1); MRC=$?
echo "$MODEL" | sed -n '/EDIT FAIL/p'
[ "$MRC" -eq 0 ] || PASS=0
echo "  1 editing model: $(echo "$MODEL" | tail -1) (want fails=0)"

# (3) A/B kill switch: ccd_create must decline.
KILL=$(M64_NO_CLASSIC_DIALOG=1 arch -x86_64 "$TMP/t" kill 2>/dev/null)
CREATED=$(echo "$KILL" | sed -n 's/.*created=\([0-9]*\).*/\1/p')
[ "${CREATED:-x}" = "0" ] || PASS=0
echo "  3 kill switch:   created=$CREATED (want 0 = decline -> AppKit fallback)"

# (2)(4) the real classic window + synthetic typing and click.
# RETRIED: this arm briefly takes key focus and reads back whatever landed in the
# field, so anything else typing at that moment goes into it too — most often the
# HUMAN AT THE KEYBOARD (confirmed live: 'tabc' / 'a bc' / 'nc' were the tester typing
# while the arm ran), and equally any other process posting synthetic input.
# The assertion stays EXACT ('abc', nothing else); only the SCHEDULING is
# tolerant, so a real regression still fails every attempt and fails the arm.
CLICK_TRIES=3
RC=0
for _try in $(seq $CLICK_TRIES); do
    settle; OUT=$(arch -x86_64 "$TMP/t" click 2>"$TMP/rerr"); RC=$?
    [ "$RC" -eq 0 ] || break
    echo "$OUT" | grep -q "typed='abc'" && break
    [ "$_try" = "$CLICK_TRIES" ] || echo "  4 retry $_try: foreign input ($(echo "$OUT" | sed -n "s/.*\(typed='[^']*'\).*/\1/p"))"
done
if [ "$RC" -eq 3 ]; then
    echo "  2/4 window:      SKIP (no window server)"
elif [ "$RC" -eq 9 ] && grep -q "trusted=0" "$TMP/rerr"; then
    echo "  2/4 window:      SKIP (no Accessibility rights to synthesise input)"
elif [ "$RC" -eq 9 ]; then
    echo "  2/4 window:      FAIL (classic dialog never dismissed) $(cat "$TMP/rerr")"; PASS=0
else
    ITEM=$(echo "$OUT"  | sed -n 's/.*item=\([0-9-]*\).*/\1/p')
    TYPED=$(echo "$OUT" | sed -n "s/.*typed='\([^']*\)'.*/\1/p")
    WW=$(echo "$OUT"    | sed -n 's/.* w=\([0-9]*\).*/\1/p')
    HH=$(echo "$OUT"    | sed -n 's/.* h=\([0-9]*\).*/\1/p')
    SH=$(echo "$OUT"    | sed -n 's/.*structh=\([0-9]*\).*/\1/p')
    [ "${ITEM:-x}"  = "4" ]   || PASS=0     # the OK item, hit by a real click
    # keycodes 0/11/8 with no shift = the classic MacRoman chars 'a','b','c'
    [ "${TYPED:-x}" = "abc" ] || PASS=0     # real keystrokes reached the field
    [ "${WW:-0}"    = "352" ] || PASS=0     # the DLOG's content geometry
    [ "${HH:-0}"    = "152" ] || PASS=0
    [ "${SH:-0}" -gt "${HH:-0}" ] || PASS=0 # title bar => movable modal
    echo "  2 geometry:      ${WW}x${HH} content, structure_h=$SH (want 352x152, structh>h)"
    echo "  4 typing+click:  typed='$TYPED' item=$ITEM (want 'abc', 4)"
fi

# (5) the classic STANDARD item numbers: with no SetDialogCancelItem call,
#     ccd_create must take DITL item 2 as the cancel item when it really is an
#     enabled push button, and must NOT invent one when it is not.  Before this,
#     cancelItem stayed 0 forever: Escape/Cmd-. were dead AND the close box fired
#     the DEFAULT item, so a close gesture reported OK.
settle; IT=$(arch -x86_64 "$TMP/t" items 2>"$TMP/ierr"); IRC=$?
if [ "$IRC" -eq 3 ]; then
    echo "  5 std items:     SKIP (no window server)"
else
    ACAN=$(echo "$IT" | sed -n 's/^A .*cancel=\([0-9]*\).*/\1/p')
    BDEF=$(echo "$IT" | sed -n 's/^B default=\([0-9]*\).*/\1/p')
    BCAN=$(echo "$IT" | sed -n 's/^B .*cancel=\([0-9]*\).*/\1/p')
    [ "${ACAN:-x}" = "0" ] || PASS=0      # item 2 is an editText -> no cancel item
    [ "${BDEF:-x}" = "1" ] || PASS=0      # kStdOkItemIndex
    [ "${BCAN:-x}" = "2" ] || PASS=0      # kStdCancelItemIndex
    echo "  5 std items:     item2-not-a-button cancel=$ACAN (want 0);" \
         "Halo-shaped default=$BDEF cancel=$BCAN (want 1, 2)"
fi

# (6) Escape must end the modal with that classic cancel item.
settle; EO=$(arch -x86_64 "$TMP/t" esc 2>"$TMP/eerr"); ERC=$?
if [ "$ERC" -eq 3 ]; then
    echo "  6 Escape:        SKIP (no window server)"
elif [ "$ERC" -eq 9 ] && grep -q "trusted=0" "$TMP/eerr"; then
    echo "  6 Escape:        SKIP (no Accessibility rights to synthesise input)"
elif [ "$ERC" -eq 9 ]; then
    echo "  6 Escape:        FAIL (Escape never dismissed the dialog)"; PASS=0
else
    EITEM=$(echo "$EO" | sed -n 's/.*item=\([0-9-]*\).*/\1/p')
    [ "${EITEM:-x}" = "2" ] || PASS=0
    echo "  6 Escape:        item=$EITEM (want 2 = kStdCancelItemIndex)"
fi

# (7) NO PHANTOM CLICK: the pointer resting over a button with the mouse UP must
#     never fire it.  btn_track used to be installed on kEventControlHit as well
#     as kEventControlTrack; entered with the mouse up its poll loop exits on the
#     first iteration and fires purely on where the cursor happens to be.
settle; PO=$(arch -x86_64 "$TMP/t" phantom 2>"$TMP/perr"); PRC=$?
if [ "$PRC" -eq 3 ]; then
    echo "  7 no phantom:    SKIP (no window server)"
elif [ "$PRC" -eq 9 ] && grep -q "trusted=0" "$TMP/perr"; then
    echo "  7 no phantom:    SKIP (no Accessibility rights to synthesise input)"
elif [ "$PRC" -eq 9 ]; then
    echo "  7 no phantom:    FAIL (dialog never dismissed)"; PASS=0
else
    POK=$(echo "$PO" | sed -n 's/.*phantom_ok=\([0-9-]*\).*/\1/p')
    [ "${POK:-x}" = "1" ] || PASS=0
    echo "  7 no phantom:    phantom_ok=$POK (want 1 = pointer over OK, mouse up, no fire)"
fi

if [ "$PASS" = "1" ]; then echo "classicdialog: PASS"; exit 0; fi
echo "classicdialog: FAIL"; exit 1
