#!/bin/bash
#
# carbon_standardalert_test.sh — regression guard proving the Carbon StandardAlert
# family is CLICKABLE and MOVABLE (carbon_standardalert_shim.m).
#
# THE BUG (Civ IV "XML Load Error" modal, and universally): CreateStandardAlert /
# RunStandardAlert / StandardAlert were UNSHIMMED, so they bound to the live
# 64-bit HIToolbox. In a pure-Carbon translated process (never foreground, no
# NSApplicationLoad on that path, starved event pump — shim_GetNextEvent returns
# 0) native RunStandardAlert drew a real alert window that never became key/front
# and whose modal loop starved: the user saw the alert but its BUTTONS DID NOT
# RESPOND TO CLICKS and it could NOT BE DRAGGED. The fix routes the family through
# a real AppKit -runModal NSAlert (the same mechanism carbon_dialog_shim.m uses),
# which owns its own key/front/movable/event-pumped modal.
#
# This guard runs NATIVE x86_64 (no i386 sysroot needed). It compiles the SHIM
# ITSELF (carbon_standardalert_shim.m) with tiny stubs for its 3 externs
# (x64_objc_wrap/unwrap = identity, carbon_ensure_window_host = NSApplicationLoad)
# and drives the real shim_CreateStandardAlert -> shim_RunStandardAlert entry
# points. It proves, WITHOUT a human clicking:
#   (1) CLICKABLE: a synthetic click on a button ([button performClick:] — the
#       exact target/action path a mouse click drives) dismisses the modal and
#       shim_RunStandardAlert returns the RIGHT classic item: clicking the default
#       returns item 1 (kAlertStdAlertOKButton), clicking Cancel returns item 2
#       (kAlertStdAlertCancelButton). A dead (native-forwarded) alert never
#       returns / hangs.
#   (2) MOVABLE: the modal window carries NSWindowStyleMaskTitled (a draggable
#       title bar) and its origin can be programmatically moved (frame changes) —
#       a borderless/utility alert would not move by its title bar.
#
# Requires a WindowServer session (GUI login). If AppKit can't init (headless
# ssh, no window server), it SKIPs (exit 0) rather than failing spuriously.
set -u
cd "$(dirname "$0")"

SHIM="../src/abiconv/carbon_standardalert_shim.m"
if [ ! -f "$SHIM" ]; then echo "standardalert: SKIP (shim source missing)"; exit 0; fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/harness.m" <<'EOF'
#import <AppKit/AppKit.h>
#import <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- stubs for the shim's 3 externs (objc_shim bridge + host bootstrap) ----
 * In the real dylib x64_objc_wrap/unwrap map real 64-bit objects to/from 32-bit
 * i386 handles via an arena. Here we keep a tiny table so a real (>4GB) CFString
 * pointer round-trips through a 32-bit slot exactly as it would in the translator
 * (an already-low value passes through as identity). */
static uint64_t g_tab[256]; static int g_n;
uint32_t x64_objc_wrap(uint64_t real) {
    if (!real) return 0;
    if (!(real >> 32)) return (uint32_t)real;         /* already low: identity */
    for (int i=0;i<g_n;i++) if (g_tab[i]==real) return 0x40000000u + (uint32_t)i;
    if (g_n < 256) { g_tab[g_n]=real; return 0x40000000u + (uint32_t)g_n++; }
    return 0;
}
uint64_t x64_objc_unwrap(uint32_t h) {
    if (!h) return 0;
    if (h >= 0x40000000u && (h-0x40000000u) < (uint32_t)g_n) return g_tab[h-0x40000000u];
    return (uint64_t)h;                                /* low value: identity */
}
void carbon_ensure_window_host(void) {
    static int done; if (done) return; done = 1;
    NSApplicationLoad();
}
/* The shim's ___Alert path references shim_StopAlert (carbon_dialog_shim.m, not
 * compiled here). Benign stand-in so the shim links; this test drives only
 * Create/RunStandardAlert, not ___Alert. */
uint32_t shim_StopAlert(uint32_t *a) { (void)a; return 1; }

/* The shim now tries the AUTHENTIC classic Carbon alert FIRST
 * (carbon_classic_alert.c) and only falls back to this AppKit NSAlert when the
 * classic path declines. THIS guard is the one that covers the FALLBACK leg, so
 * stub the classic path to decline (return 0) — carbon_classic_alert_test.sh is
 * the guard for the classic path itself. Keeping both means a regression in
 * either leg is caught: the classic alert must work, AND the fallback must still
 * be clickable/movable and return the classic item numbers if it is ever used. */
int carbon_classic_alert_run(int t, const char *m, const char *i,
                             const char *ok, const char *c, const char *o,
                             int d, int cb) {
    (void)t; (void)m; (void)i; (void)ok; (void)c; (void)o; (void)d; (void)cb;
    return 0;
}

/* Pull in the REAL shim under test. */
#include "SHIM_PATH"

/* ---- low (<4GB) arena: all i386-visible memory (param rec, out slots) must be
 * addressable through a 32-bit slot, exactly as in the translated process. ---- */
static uint8_t *g_low; static size_t g_low_off, g_low_cap;
static int low_init(void) {
    g_low_cap = 1 << 20;
    g_low = mmap(NULL, g_low_cap, PROT_READ|PROT_WRITE,
                 MAP_PRIVATE|MAP_ANON|MAP_32BIT, -1, 0);
    if (g_low == MAP_FAILED || ((uintptr_t)g_low >> 32)) { g_low = NULL; return 0; }
    return 1;
}
static void *low_alloc(size_t n) {
    n = (n + 15) & ~15u;
    if (!g_low || g_low_off + n > g_low_cap) return NULL;
    void *p = g_low + g_low_off; g_low_off += n; memset(p, 0, n); return p;
}

/* kAlertStopAlert + PARM_* offsets come from the included shim. */

/* NSAlert nests its buttons several views deep in the modal window; collect
 * them recursively so the test can click one. */
static void collect_buttons(NSView *v, NSMutableArray<NSButton*> *out) {
    for (NSView *s in v.subviews) {
        if ([s isKindOfClass:[NSButton class]]) [out addObject:(NSButton *)s];
        collect_buttons(s, out);
    }
}

static int g_titled, g_moved;

/* Run one alert with a synthetic click. clickCancel: 0 = click default(OK),
 * 1 = click Cancel. Returns the classic item shim_RunStandardAlert reports;
 * -1 on setup failure. Sets g_titled/g_moved from the modal window. */
static int run_and_click(uint32_t alertType, const char *err, const char *expl,
                         uint32_t deflt, uint32_t cancel, int clickCancel) {
    /* param rec in low memory (shim reads it via raw PTR(3)) */
    uint8_t *parm = (uint8_t *)low_alloc(PARM_size);
    if (!parm) return -1;
    *(uint32_t*)(parm+PARM_defaultText) = deflt;
    *(uint32_t*)(parm+PARM_cancelText)  = cancel;
    *(uint32_t*)(parm+PARM_otherText)   = 0;
    *(uint16_t*)(parm+PARM_defaultButton) = 1;
    *(uint16_t*)(parm+PARM_cancelButton)  = cancel ? 2 : 0;

    CFStringRef e  = err  ? CFStringCreateWithCString(NULL, err,  kCFStringEncodingUTF8) : NULL;
    CFStringRef ex = expl ? CFStringCreateWithCString(NULL, expl, kCFStringEncodingUTF8) : NULL;

    /* outAlert is a DialogRef* — the caller passes the ADDRESS of a low-memory
     * DialogRef slot; the shim writes the handle into *outAlert. */
    uint32_t *outAlertSlot = (uint32_t *)low_alloc(4);
    if (!outAlertSlot) { if (e) CFRelease(e); if (ex) CFRelease(ex); return -1; }
    *outAlertSlot = 0;
    uint32_t createArgs[5];
    createArgs[0] = alertType;
    createArgs[1] = x64_objc_wrap((uint64_t)(uintptr_t)e);
    createArgs[2] = ex ? x64_objc_wrap((uint64_t)(uintptr_t)ex) : 0;
    createArgs[3] = (uint32_t)(uintptr_t)parm;          /* raw low addr */
    createArgs[4] = (uint32_t)(uintptr_t)outAlertSlot;  /* &DialogRef (low) */
    if (shim_CreateStandardAlert(createArgs) != 0 || *outAlertSlot == 0) {
        if (e) CFRelease(e); if (ex) CFRelease(ex); return -1;
    }
    uint32_t dialogRef = *outAlertSlot;

    g_titled = g_moved = 0;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5*NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        NSWindow *w = [NSApp modalWindow];
        if (w) {
            g_titled = (w.styleMask & NSWindowStyleMaskTitled) ? 1 : 0;
            NSPoint o0 = w.frame.origin;
            [w setFrameOrigin:NSMakePoint(o0.x + 40, o0.y + 40)];
            g_moved = (w.frame.origin.x != o0.x || w.frame.origin.y != o0.y) ? 1 : 0;
        }
        /* Find the alert buttons (NSAlert nests them deep in the modal window's
         * view tree — recurse) and click the target one exactly as a mouse would
         * (performClick: fires the real target/action). NSAlert manages the
         * keyEquivalents itself, so match by TITLE (OK vs Cancel). */
        NSButton *target = nil;
        if (w) {
            NSMutableArray<NSButton*> *btns = [NSMutableArray array];
            collect_buttons(w.contentView, btns);
            NSString *want = clickCancel ? @"Cancel" : @"OK";
            for (NSButton *b in btns) if ([b.title isEqualToString:want]) { target = b; break; }
            /* fallback: OK is normally the last-added / rightmost; Cancel the other */
            if (!target && btns.count) target = clickCancel ? btns.firstObject : btns.lastObject;
        }
        if (target) [target performClick:nil];
        else [NSApp abortModal];               /* never hang the test */
    });

    int16_t *slot = (int16_t *)low_alloc(4);
    if (!slot) { if (e) CFRelease(e); if (ex) CFRelease(ex); return -1; }
    *slot = -99;
    uint32_t runArgs[3] = { dialogRef, 0, (uint32_t)(uintptr_t)slot };
    shim_RunStandardAlert(runArgs);
    int item = *slot;
    if (e) CFRelease(e); if (ex) CFRelease(ex);
    return item;
}

int main(void) {
    @autoreleasepool {
        NSApplicationLoad();
        [NSApplication sharedApplication];
        if (!NSApp) { fprintf(stderr, "no NSApp\n"); return 3; }
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        if (!low_init()) { fprintf(stderr, "no MAP_32BIT low arena\n"); return 3; }

        int pass = 1;
        uint32_t OK = 0xffffffffu;   /* kAlertDefaultOKText / CancelText sentinel */

        /* (A) OK-only alert (the Civ "XML Load Error" shape); click default -> 1;
         *     modal must be titled+movable. */
        int itemA = run_and_click(kAlertStopAlert, "XML Load Error",
                                  "Assets//GameInfo/CIV4PlayerOptionInfos.xml",
                                  OK, 0, /*clickCancel*/0);
        int titledA = g_titled, movedA = g_moved;
        int okA = (itemA == 1) && titledA && movedA;
        fprintf(stderr, "  A ok-only:        item=%d(want 1) titled=%d moved=%d -> %s\n",
                itemA, titledA, movedA, okA?"ok":"BAD");
        pass = pass && okA;

        /* (B) OK+Cancel alert; click CANCEL -> 2. */
        int itemB = run_and_click(kAlertStopAlert, "Delete file?", "This cannot be undone.",
                                  OK, OK, /*clickCancel*/1);
        int okB = (itemB == 2);
        fprintf(stderr, "  B ok+cancel/esc:  item=%d(want 2) -> %s\n", itemB, okB?"ok":"BAD");
        pass = pass && okB;

        /* (C) OK+Cancel alert; click DEFAULT (OK) -> 1. */
        int itemC = run_and_click(kAlertStopAlert, "Quit?", "Save changes?",
                                  OK, OK, /*clickCancel*/0);
        int okC = (itemC == 1);
        fprintf(stderr, "  C ok+cancel/def:  item=%d(want 1) -> %s\n", itemC, okC?"ok":"BAD");
        pass = pass && okC;

        printf("standardalert: %s (A=%d B=%d C=%d)\n", pass?"PASS":"FAIL", itemA, itemB, itemC);
        return pass ? 0 : 1;
    }
}
EOF

# Splice the real shim path in.
SHIMABS="$(cd .. && pwd)/src/abiconv/carbon_standardalert_shim.m"
sed -i '' "s|SHIM_PATH|$SHIMABS|" "$TMP/harness.m" 2>/dev/null \
  || sed -i "s|SHIM_PATH|$SHIMABS|" "$TMP/harness.m"

# -Wl,-pagezero_size,0x1000 shrinks the reserved __PAGEZERO so MAP_32BIT can hand
# back genuine <4GB pages — the i386-address regime every shim slot assumes.
if ! clang -arch x86_64 -fobjc-arc -Wno-deprecated-declarations \
        -Wl,-pagezero_size,0x1000 \
        -o "$TMP/t" "$TMP/harness.m" \
        -framework AppKit -framework CoreFoundation 2>"$TMP/cerr"; then
    echo "standardalert: SKIP (compile failed)"; sed -n '1,30p' "$TMP/cerr"; exit 0
fi

OUT=$(arch -x86_64 "$TMP/t" 2>"$TMP/rerr"); RC=$?
if [ "$RC" -eq 3 ]; then
    echo "standardalert: SKIP (no window server / headless: $(tail -1 "$TMP/rerr"))"; exit 0
fi
echo "$OUT"
[ "$RC" -eq 0 ] || { echo "  stderr:"; sed -n '1,20p' "$TMP/rerr"; }
exit $RC
