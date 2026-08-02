/* windowspy.m — WHO still owns a window when a translated app switches display mode?
 *
 * The NSCGSPanic bisect (probes/displayswitch.m, journal 2026-08-03) settled the
 * mechanism: modern AppKit cannot re-create an NSCarbonWindow's CGS context during
 * ANY display reconfiguration. Only a window that never had a context
 * ("carbonhidden") or no longer exists ("carbondispose") survives. Hiding does NOT
 * work, and the modern successor API panics identically.
 *
 * So the remaining question is not "how do we make the switch safe" but:
 *
 *      WHY is a Carbon window still alive in the process at the switch?
 *
 * This dylib answers it by measurement rather than inference. It interposes the
 * whole classic display-mode entry family and, on the way in, dumps every window
 * the process owns — class, title, visibility, frame, and whether AppKit thinks it
 * has a CGS context yet. Then it calls through, so the app behaves exactly as it
 * would have (the panic, if any, still happens and is still ours to read).
 *
 * WHY AN INTERPOSE AND NOT A SHIM: CGDisplaySwitchToMode reaches native through an
 * abigen-GENERATED bridge (___CGDisplaySwitchToMode.l1). Instrumenting that would
 * mean hand-shimming a function whose marshalling is already verified correct —
 * a change to shipped code purely to observe it. DYLD_INSERT_LIBRARIES observes
 * the same call with zero effect on the deployed artifact.
 *
 * ⚠HEISENBUG DISCIPLINE (this target has bitten us twice): an observer can hide the
 * very thing it observes. This particular failure is deterministic and lives in
 * AppKit rather than in stack garbage, so printing should be safe — but VERIFY the
 * panic still reproduces with this loaded before trusting anything it says. If the
 * panic disappears, the observer is the finding, not the evidence.
 *
 * Build (must be x86_64 — the translated app is x86_64 under Rosetta):
 *   clang -arch x86_64 -dynamiclib -framework Cocoa -framework CoreGraphics \
 *         -o windowspy.dylib windowspy.m
 * Use:
 *   DYLD_INSERT_LIBRARIES=/path/windowspy.dylib open -a <App>   (or a direct exec)
 */

#import <Cocoa/Cocoa.h>
#include <ApplicationServices/ApplicationServices.h>
#include <objc/runtime.h>
#include <stdio.h>

extern CGError CGDisplaySwitchToMode(CGDirectDisplayID d, CFDictionaryRef mode);

#define SPY(...) do { fprintf(stderr, "[winspy] " __VA_ARGS__); } while (0)

/* Dump every window the process owns. The interesting column is `class`: an
 * NSCarbonWindow is one AppKit cannot re-create a CGS context for. `ctx` reports
 * whether a context already exists — a window that never had one survives the
 * reconfiguration, which is why "created but never shown" is safe. */
static void dump_windows(const char *when)
{
   NSArray *ws = [NSApp windows];
   SPY("=== %s: NSApp windows=%lu ===\n", when, (unsigned long)[ws count]);
   if (![ws count]) { SPY("    (none — this configuration SURVIVES the switch)\n"); return; }
   NSUInteger i = 0;
   for (NSWindow *w in ws) {
      const char *cls = class_getName([w class]);
      NSRect f = [w frame];
      /* -windowNumber > 0 means a CGS window really exists behind it. */
      NSInteger num = [w windowNumber];
      SPY("    [%lu] class=%-20s visible=%d num=%ld frame=%.0fx%.0f@%.0f,%.0f title='%s'%s\n",
          (unsigned long)i++, cls, (int)[w isVisible], (long)num,
          f.size.width, f.size.height, f.origin.x, f.origin.y,
          [[w title] UTF8String] ?: "",
          strstr(cls, "Carbon") ? "   <<< CARBON — THIS IS THE PANIC CANDIDATE" : "");
   }
}

static CGError spy_CGDisplaySwitchToMode(CGDirectDisplayID d, CFDictionaryRef mode)
{
   SPY("CGDisplaySwitchToMode(display=%u, mode=%p) ENTERED\n", (unsigned)d, (void *)mode);
   dump_windows("at switch");
   SPY("calling through to native now — a panic after this line is the AppKit walk\n");
   CGError e = CGDisplaySwitchToMode(d, mode);
   SPY("CGDisplaySwitchToMode -> %d (SURVIVED)\n", (int)e);
   dump_windows("after switch");
   return e;
}

__attribute__((used)) static struct {
   const void *replacement;
   const void *replacee;
} interposers[] __attribute__((section("__DATA,__interpose"))) = {
   { (const void *)&spy_CGDisplaySwitchToMode, (const void *)&CGDisplaySwitchToMode },
};

__attribute__((constructor)) static void windowspy_init(void)
{
   setvbuf(stderr, NULL, _IONBF, 0);
   SPY("loaded — watching the classic display-mode family\n");
}
