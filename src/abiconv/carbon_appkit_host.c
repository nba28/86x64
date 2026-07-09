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

#include <dlfcn.h>

void carbon_ensure_window_host(void) {
    static int done;
    if (done) return;
    done = 1;
    unsigned char (*NSApplicationLoad)(void) =
        (unsigned char (*)(void))dlsym(RTLD_DEFAULT, "NSApplicationLoad");
    if (NSApplicationLoad) NSApplicationLoad();
}
