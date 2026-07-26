/*
 * carbon_appkit_host.c — one job: make Carbon windowing WORK in a translated
 * process by bootstrapping AppKit's window-management bridge exactly once.
 *
 * On modern macOS every on-screen window is materialized through AppKit
 * (NSCGSWindow); HIToolbox reaches it through the private WindowManagement
 * "delegate" that AppKit installs during application load. A pure-Carbon
 * translated app (Halo: CreateWindowFromNib/ShowWindow, no AppKit) never
 * initializes AppKit, so the delegate is missing. Observed on Halo (ground
 * truth, 2026-07-09):
 *     "Unable to bridge WindowManagement interface - no delegate has been set"
 * and then, once the window is half-bridged anyway, an AppKit PANIC inside
 * RunApplicationEventLoop's display cycle:
 *     NSCGSPanic <- -[NSCGSWindow _createContext] (SLSSetWindowLayerContext
 *     rc=1000) <- NSWindowUpdateLayerTree <- displayIfNeeded
 * killing the app before the window can draw. Injecting NSApplicationLoad()
 * once before the window flow (lldb experiment) cured both: the real EULA
 * window materialized on screen with live buttons.
 *
 * NSApplicationLoad() is Apple's DOCUMENTED bootstrap for exactly this
 * ("startup function to call when running Cocoa code from a Carbon
 * application"). It is idempotent and cheap. We trigger it lazily from the
 * window-creating shim paths (nib windows, classic NewCWindow, scroll-text
 * box), NOT unconditionally at libabiconv init — translated non-GUI processes
 * must not suddenly grow an AppKit connection.
 */

/*
 * SECOND JOB OF THIS FILE'S FAMILY (same one purpose — "make the translated
 * process a real GUI host so Carbon windowing works"): carbon_ensure_foreground.
 *
 * NSApplicationLoad alone is NOT enough for a Carbon window to become KEY. A
 * translated process that LaunchServices did not start as a foreground
 * application (a bare executable, a helper re-exec, a tool-launched run) gets
 * NSApplicationActivationPolicyProhibited, and a Prohibited app can never
 * activate: its windows are visible but never key, so every HIToolbox modal loop
 * starves — the app draws a window whose buttons do nothing.
 *
 * MEASURED on a natively-compiled x86_64 probe driving a real HIToolbox window
 * (no translator involved, so the numbers are the OS's, not ours):
 *      NSApplicationLoad only        -> activationPolicy = Prohibited(2),
 *                                       [NSApp isActive] = NO, keyWindow = nil,
 *                                       ActiveNonFloatingWindow() = NULL,
 *                                       IsWindowActive(win) = false
 *      + TransformProcessType(fg)
 *        + SetFrontProcess
 *        + activateIgnoringOtherApps -> activationPolicy = Regular(0),
 *                                       isActive = YES, keyWindow = the window,
 *                                       ActiveNonFloatingWindow() = the window,
 *                                       IsWindowActive(win) = true
 * i.e. this is exactly the missing "become a real foreground GUI app" step, and
 * it is UNIVERSAL: every Carbon window, dialog, menu and control in every
 * translated app needs the process to be foreground before it can take input.
 *
 * It triggers on a STRUCTURAL condition — "this translated process is about to
 * put a Carbon window on screen" — never on an app name, and is idempotent and a
 * no-op for a process that LaunchServices already started as foreground (a
 * normal .app bundle launch), where TransformProcessType simply returns with the
 * app already in that state. Non-GUI translated processes never call it.
 */

#include <dlfcn.h>
#include <stdint.h>

void carbon_ensure_window_host(void) {
    static int done;
    if (done) return;
    done = 1;
    unsigned char (*NSApplicationLoad)(void) =
        (unsigned char (*)(void))dlsym(RTLD_DEFAULT, "NSApplicationLoad");
    if (NSApplicationLoad) NSApplicationLoad();
}

/* ProcessSerialNumber / kCurrentProcess (MacTypes.h, Processes.h). */
struct abiconv_psn { uint32_t highLongOfPSN, lowLongOfPSN; };
#define ABICONV_kCurrentProcess 2
#define ABICONV_kProcessTransformToForegroundApplication 1

void carbon_ensure_foreground(void) {
    static int done;
    if (done) return;
    done = 1;

    carbon_ensure_window_host();          /* NSApp must exist before we activate */

    struct abiconv_psn psn = { 0, ABICONV_kCurrentProcess };
    int32_t (*TransformProcessType)(const struct abiconv_psn *, uint32_t) =
        (int32_t (*)(const struct abiconv_psn *, uint32_t))
            dlsym(RTLD_DEFAULT, "TransformProcessType");
    int32_t (*SetFrontProcess)(const struct abiconv_psn *) =
        (int32_t (*)(const struct abiconv_psn *))dlsym(RTLD_DEFAULT, "SetFrontProcess");
    if (TransformProcessType)
        TransformProcessType(&psn, ABICONV_kProcessTransformToForegroundApplication);
    if (SetFrontProcess)
        SetFrontProcess(&psn);

    /* [NSApp activateIgnoringOtherApps:YES] via the ObjC runtime — this TU is
     * plain C on purpose (it is linked into every translated process). */
    void *(*getClass)(const char *) = (void *(*)(const char *))dlsym(RTLD_DEFAULT, "objc_getClass");
    void *(*sel)(const char *) = (void *(*)(const char *))dlsym(RTLD_DEFAULT, "sel_registerName");
    void *msgsend = dlsym(RTLD_DEFAULT, "objc_msgSend");
    if (getClass && sel && msgsend) {
        void *(*msg_id)(void *, void *) = (void *(*)(void *, void *))msgsend;
        void (*msg_b)(void *, void *, signed char) =
            (void (*)(void *, void *, signed char))msgsend;
        void *cls = getClass("NSApplication");
        if (cls) {
            void *app = msg_id(cls, sel("sharedApplication"));
            if (app) msg_b(app, sel("activateIgnoringOtherApps:"), 1);
        }
    }
}
