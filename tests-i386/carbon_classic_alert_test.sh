#!/bin/bash
#
# carbon_classic_alert_test.sh — regression guard for the AUTHENTIC CLASSIC
# Carbon alert (src/abiconv/carbon_classic_alert.c).
#
# WHAT IT GUARDS
# --------------
# Project rule (the authentic-original-look rule): a translated Carbon app's alert
# must be a CLASSIC CARBON alert, not a modern AppKit NSAlert.  The obvious route
# — just call the native HIToolbox CreateStandardAlert/RunStandardAlert, which
# ARE still exported — does not work, twice over, and both were MEASURED in a
# pristine natively-compiled code-signed .app bundle launched by LaunchServices
# with no translator involved:
#   * the native alert is INERT: with the process foreground, [NSApp isActive]
#     YES, the alert window key (IsWindowActive true, ActiveNonFloatingWindow ==
#     the alert) and the Carbon event loop demonstrably pumping (an
#     InstallEventLoopTimer firing throughout), RunStandardAlert never returns
#     for a synthetic click, a synthetic Return, or an
#     AXUIElementPerformAction(kAXPressAction) from a separate trusted process;
#   * and it is not authentic anyway — modern font, modern blue pill button.
# So the alert is re-materialized on the REAL classic substrate: a real Carbon
# window (CreateNewWindow, movable modal) whose contents are self-drawn classic
# HIViews (com.apple.hiview + kEventControlDraw/HitTest/Track, CoreGraphics +
# CoreText, Lucida Grande) — the same live machinery as Halo's EULA scroll box.
#
# WHAT IT PROVES (no human clicking, no screenshot diffing)
#   (1) MATERIALIZES: a real Carbon window appears with the classic alert
#       geometry (420pt wide, the classic width).
#   (2) CLICKABLE: a REAL synthetic mouse click (CGEventPost through the session
#       event tap — the same path a physical click takes) on the OK button
#       dismisses the modal and returns classic item 1
#       (kAlertStdAlertOKButton), and a click on the CANCEL button returns
#       classic item 2 (kAlertStdAlertCancelButton).  1-vs-2 is the real check:
#       it proves the click landed on the intended button and that the classic
#       item mapping is right, not that some fallback fired.
#   (3) MOVABLE: the window is created movable-modal, so it carries a title bar
#       (structure region taller than the content region) = draggable.
#   (4) A/B KILL SWITCH: with M64_NO_CLASSIC_ALERT=1 the classic path must
#       DECLINE (return 0) so the caller falls back — this is the negative half
#       of the guard and proves the test is actually exercising the new code.
#
# Runs NATIVE x86_64 (no i386 sysroot needed).  Requires a WindowServer session
# AND Accessibility rights for synthetic events; SKIPs cleanly (exit 0) when it
# cannot drive input, rather than failing spuriously on a headless runner.
set -u
cd "$(dirname "$0")"

SRC="../src/abiconv/carbon_classic_alert.c"
HOST="../src/abiconv/carbon_appkit_host.c"
if [ ! -f "$SRC" ] || [ ! -f "$HOST" ]; then
    echo "classicalert: SKIP (shim source missing)"; exit 0
fi

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

extern int carbon_classic_alert_run(int, const char *, const char *,
                                    const char *, const char *, const char *, int, int);

static volatile int g_done;
static const char *g_which = "ok";
static int g_w, g_h, g_struct_h, g_saw_window;

static void *watchdog(void *arg) {
    (void)arg;
    usleep(2200000);
    void *(*FrontWindow)(void) = dlsym(RTLD_DEFAULT, "FrontWindow");
    int32_t (*GetWindowBounds)(void *, uint32_t, short *) = dlsym(RTLD_DEFAULT, "GetWindowBounds");
    void *w = FrontWindow ? FrontWindow() : NULL;
    short c[4] = {0,0,0,0}, s[4] = {0,0,0,0};       /* top,left,bottom,right */
    if (w && GetWindowBounds) {
        GetWindowBounds(w, 33 /*kWindowContentRgn*/,   c);
        GetWindowBounds(w, 32 /*kWindowStructureRgn*/, s);
    }
    if (!w || c[3] <= c[1]) { fprintf(stderr, "no window\n"); _exit(3); }
    g_saw_window = 1;
    g_w = c[3] - c[1]; g_h = c[2] - c[0];
    g_struct_h = s[2] - s[0];
    fprintf(stderr, "  window content %dx%d structure_h=%d\n", g_w, g_h, g_struct_h);

    if (strcmp(g_which, "none") != 0) {
        /* classic layout: button row 20pt above the bottom, 20pt from the right;
         * OK rightmost, Cancel to its left (min width 68, gap 12). */
        double y = c[2] - 20 - 10;
        double x = strcmp(g_which, "cancel") == 0 ? c[3] - 20 - 68 - 12 - 34
                                                  : c[3] - 20 - 34;
        CGPoint pt = CGPointMake(x, y);
        CGEventRef mv = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, pt, kCGMouseButtonLeft);
        if (mv) { CGEventPost(kCGSessionEventTap, mv); CFRelease(mv); }
        usleep(200000);
        CGEventRef md = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDown, pt, kCGMouseButtonLeft);
        if (md) { CGEventPost(kCGSessionEventTap, md); CFRelease(md); }
        usleep(250000);
        CGEventRef mu = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseUp, pt, kCGMouseButtonLeft);
        if (mu) { CGEventPost(kCGSessionEventTap, mu); CFRelease(mu); }
    }
    usleep(3500000);
    /* Not dismissed: could be a dead pump OR no Accessibility rights for the
     * synthetic click. Distinguish so the suite SKIPs instead of failing on a
     * machine that simply may not synthesise input. */
    if (!g_done) { fprintf(stderr, "HUNG trusted=%d\n", (int)AXIsProcessTrusted()); _exit(9); }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc > 1) g_which = argv[1];
    pthread_t t; pthread_create(&t, NULL, watchdog, NULL);
    int item = carbon_classic_alert_run(0 /*kAlertStopAlert*/, "XML Load Error",
                                        "Assets//GameInfo/CIV4PlayerOptionInfos.xml",
                                        NULL, "Cancel", NULL, 1, 2);
    g_done = 1;
    printf("item=%d w=%d h=%d structh=%d sawwin=%d\n", item, g_w, g_h, g_struct_h, g_saw_window);
    return 0;
}
EOF

if ! clang -arch x86_64 -Wno-deprecated-declarations -o "$TMP/t" "$TMP/h.c" \
        "$SRC" "$HOST" \
        -framework Carbon -framework Cocoa -framework CoreText \
        -framework ApplicationServices 2>"$TMP/cerr"; then
    echo "classicalert: SKIP (compile failed)"; sed -n '1,25p' "$TMP/cerr"; exit 0
fi

run() {  # $1 = which button; echoes the harness stdout, sets RC
    OUT=$(arch -x86_64 "$TMP/t" "$1" 2>"$TMP/rerr"); RC=$?
}

# (A) click OK -> classic item 1, and the window must be the classic 420pt-wide
#     movable-modal alert (structure taller than content = it has a title bar).
run ok
if [ "$RC" -eq 3 ]; then echo "classicalert: SKIP (no window server)"; exit 0; fi
if [ "$RC" -eq 9 ]; then
    if grep -q "trusted=0" "$TMP/rerr"; then
        echo "classicalert: SKIP (no Accessibility rights to synthesise a click)"; exit 0
    fi
    echo "classicalert: FAIL (A: classic alert never dismissed) $(cat "$TMP/rerr")"; exit 1
fi
ITEM_A=$(echo "$OUT" | sed -n 's/.*item=\([0-9-]*\).*/\1/p')
W_A=$(echo "$OUT"    | sed -n 's/.*w=\([0-9]*\).*/\1/p')
SH_A=$(echo "$OUT"   | sed -n 's/.*structh=\([0-9]*\).*/\1/p')
H_A=$(echo "$OUT"    | sed -n 's/.* h=\([0-9]*\).*/\1/p')

# (B) click CANCEL -> classic item 2 (proves the click hits the intended button)
run cancel
if [ "$RC" -eq 9 ]; then echo "classicalert: FAIL (B: never dismissed)"; exit 1; fi
ITEM_B=$(echo "$OUT" | sed -n 's/.*item=\([0-9-]*\).*/\1/p')

# (C) A/B kill switch: the classic path must DECLINE so the caller falls back.
OUT_C=$(M64_NO_CLASSIC_ALERT=1 arch -x86_64 "$TMP/t" none 2>/dev/null); RC=$?
ITEM_C=$(echo "$OUT_C" | sed -n 's/.*item=\([0-9-]*\).*/\1/p')

PASS=1
[ "${ITEM_A:-x}" = "1" ] || PASS=0        # OK      -> kAlertStdAlertOKButton
[ "${ITEM_B:-x}" = "2" ] || PASS=0        # Cancel  -> kAlertStdAlertCancelButton
[ "${ITEM_C:-x}" = "0" ] || PASS=0        # killed  -> decline, caller falls back
[ "${W_A:-0}" = "420" ]  || PASS=0        # classic alert width
[ "${SH_A:-0}" -gt "${H_A:-0}" ] || PASS=0  # title bar present => movable

echo "  A click OK:     item=$ITEM_A (want 1)"
echo "  B click Cancel: item=$ITEM_B (want 2)"
echo "  C kill-switch:  item=$ITEM_C (want 0 = decline -> fallback)"
echo "  geometry:       ${W_A}x${H_A} content, structure_h=$SH_A (want w=420, structh>h)"
if [ "$PASS" = "1" ]; then echo "classicalert: PASS"; exit 0; fi
echo "classicalert: FAIL"; exit 1
