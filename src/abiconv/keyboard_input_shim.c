/*
 * keyboard_input_shim.c — restore the user's system keyboard input source when a
 * translated app exits.
 *
 * PROBLEM (Halo, 2026-07-10): during the game Halo forces the system
 * keyboard/input source to U.S. (so its key handling is consistent) and LEAVES
 * it there — after quitting, the user (Hungarian layout) is stuck on U.S. and
 * has to switch back by hand. Confirmed (headless AppKit repro) that our own
 * Dialog Manager windowing does NOT change the source; the switch is Halo's OWN
 * game code. Per the user: forcing the layout DURING the game is fine — we just
 * must not leave it changed AFTER Halo exits.
 *
 * FIX (minimal, universal, non-invasive): snapshot the user's current input
 * source ID once at load (BEFORE the app's main runs, so we capture the real
 * pre-launch layout), and re-select it at process exit. We do NOT suppress or
 * interpose the app's in-run layout forcing — the app behaves exactly as before
 * while running; only the FINAL state is restored.
 *
 * Cross-copy safe: libabiconv is loaded once per co-located framework, so the
 * constructor runs in each copy. The FIRST one to run records the pre-launch
 * source ID in the process environment (ABICONV_USER_INPUT_SRC); later copies
 * (and any that load mid-run after the app already switched) see the flag and do
 * NOT overwrite it. Every copy's atexit handler re-selects that same recorded
 * source — idempotent.
 *
 * Best-effort: atexit runs on a normal quit / ExitToShell (which calls exit());
 * it does not run on _exit()/SIGKILL/crash. That is the correct scope for "put
 * the user's keyboard back when the app finishes."
 *
 * GENERIC: any revived i386 app that leaves the system input source changed
 * benefits. Uses only the SDK's Text Input Sources API (Carbon/HIToolbox).
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Carbon/Carbon.h>          /* TIS* input-source API + kTIS* properties */

#define ENV_KEY "ABICONV_USER_INPUT_SRC"

/* Re-select the recorded pre-launch input source. Runs at process exit. */
static void restore_user_source(void) {
    const char *want = getenv(ENV_KEY);
    if (!want || !*want) return;

    CFStringRef wantID = CFStringCreateWithCString(NULL, want, kCFStringEncodingUTF8);
    if (!wantID) return;

    /* If it is already current, nothing to do. */
    TISInputSourceRef cur = TISCopyCurrentKeyboardInputSource();
    if (cur) {
        CFStringRef curID = (CFStringRef)TISGetInputSourceProperty(cur, kTISPropertyInputSourceID);
        int same = (curID && CFEqual(curID, wantID));
        CFRelease(cur);
        if (same) { CFRelease(wantID); return; }
    }

    /* Find the enabled input source with that ID and select it. */
    const void *keys[]   = { kTISPropertyInputSourceID };
    const void *values[] = { wantID };
    CFDictionaryRef filter = CFDictionaryCreate(NULL, keys, values, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFArrayRef list = filter ? TISCreateInputSourceList(filter, false) : NULL;
    if (list && CFArrayGetCount(list) > 0) {
        TISInputSourceRef src = (TISInputSourceRef)CFArrayGetValueAtIndex(list, 0);
        (void)TISSelectInputSource(src);
    }
    if (list) CFRelease(list);
    if (filter) CFRelease(filter);
    CFRelease(wantID);
}

/* Snapshot the pre-launch source ID + arm the restore. Runs at libabiconv load,
 * before the translated app's main. */
__attribute__((constructor))
static void kbinput_init(void) {
    if (getenv(ENV_KEY)) {                 /* another copy already snapshotted */
        atexit(restore_user_source);       /* still register OUR exit restore */
        return;
    }
    TISInputSourceRef s = TISCopyCurrentKeyboardInputSource();
    if (s) {
        CFStringRef id = (CFStringRef)TISGetInputSourceProperty(s, kTISPropertyInputSourceID);
        char buf[256];
        if (id && CFStringGetCString(id, buf, sizeof buf, kCFStringEncodingUTF8) && buf[0]) {
            setenv(ENV_KEY, buf, 1);
        }
        CFRelease(s);
    }
    atexit(restore_user_source);
}
