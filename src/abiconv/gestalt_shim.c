/*
 * gestalt_shim.c — report removed-capability versions to legacy Gestalt callers.
 *
 * WHY THIS EXISTS (Civ IV s26)
 * ---------------------------
 * A legacy app gates itself on a system capability's version via Gestalt(). The
 * classic pattern is a QuickTime version check:
 *
 *     Gestalt(gestaltQuickTimeVersion == 'qtim', &ver);
 *     if (err || ver <= MINIMUM) { showAlert("requires QuickTime N"); exit(); }
 *
 * QuickTime was removed from macOS, so the modern CoreServices Gestalt returns
 * gestaltUndefSelectorErr (-5551) with ver==0 for the 'qtim' selector (verified
 * on macOS 15). The version reads as 0 -> below the app's minimum -> the app
 * shows its "requires QuickTime" nag and quits. (Civ IV s26: the exact check is
 * `testw %ax,%ax; jne alert; cmpl $0x05FFFFFF, ver; jg pass` — err must be 0 and
 * ver must exceed 0x05FFFFFF, i.e. QuickTime > 6.0.)
 *
 * abigen generates ___Gestalt as a straight forward to the native _Gestalt, so
 * the failing selector reaches the modern (QuickTime-less) system Gestalt.
 *
 * FIX.  Define _Gestalt here (the same in-image interception malloc_shim.c /
 * mmap_shim.c use: abigen's ___Gestalt does the i386->x86_64 lift then
 * `call _Gestalt`, which binds to this definition). For the removed-capability
 * selectors we answer with a safe, high final version so any reasonable
 * ">= minimum" gate passes; every other selector forwards unchanged to the real
 * Gestalt, so genuine queries (system version 'sysv', physical RAM, CPU, …) are
 * untouched.
 *
 * Universal: triggers on the SELECTOR (a capability whose version the modern OS
 * no longer reports), never on an app name — any legacy app version-checking
 * QuickTime through Gestalt benefits. It is the Gestalt analogue of a legacy
 * capability probe returning a "new enough" answer (cf. Halo's CPU/renderer
 * gates).
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

/* gestaltQuickTimeVersion. NumVersion-style: 0xVV RR SS BB (major/minor.bugfix/
 * stage/nonRelRev). 0x07668000 = QuickTime 7.6.6 final — the ACTUAL last-shipped
 * Mac QuickTime 7 (Snow Leopard); comfortably above any real minimum a legacy
 * title checks (6.0 = 0x06.., 7.0 = 0x07..) and positive as a signed 32-bit
 * compare. Reporting the true final 7.x matters: iMovie '11 gates launch on
 * `Gestalt('qtim') >= 7.6.6` and puts up a modal "QuickTime update required"
 * critical alert (blocking its main window) for anything lower — 7.6.3 failed it.
 * Higher is strictly safer for the universal `>=` version gate (Civ IV also does
 * a 'qtim' check); no legacy title checks for an exact/upper-bound version. */
#define GESTALT_QUICKTIME_VERSION 'qtim'
#define QUICKTIME_VERSION_REPORT  0x07668000

/* Resolve the real system Gestalt once, load-order-independently: RTLD_NEXT can
 * miss CoreServices if it was mapped before libabiconv, so open the framework by
 * path (already resident -> just yields a handle) and read Gestalt from it. */
static int (*resolve_real_gestalt(void))(uint32_t, int32_t *) {
   static int (*fn)(uint32_t, int32_t *) = NULL;
   static int tried = 0;
   if (!fn && !tried) {
      tried = 1;
      void *h = dlopen(
         "/System/Library/Frameworks/CoreServices.framework/CoreServices",
         RTLD_LAZY | RTLD_NOLOAD);
      if (!h) {
         h = dlopen(
            "/System/Library/Frameworks/CoreServices.framework/CoreServices",
            RTLD_LAZY);
      }
      if (h) { fn = (int (*)(uint32_t, int32_t *))dlsym(h, "Gestalt"); }
      if (!fn) { fn = (int (*)(uint32_t, int32_t *))dlsym(RTLD_NEXT, "Gestalt"); }
   }
   return fn;
}

/* OSErr Gestalt(OSType selector, SInt32 *response). Return type widened to int
 * (the i386 caller tests only the low 16 bits / AX = OSErr). */
int Gestalt(uint32_t selector, int32_t *response) {
   if (selector == GESTALT_QUICKTIME_VERSION) {
      if (response) { *response = QUICKTIME_VERSION_REPORT; }
      return 0;  /* noErr */
   }
   int (*real)(uint32_t, int32_t *) = resolve_real_gestalt();
   if (real) { return real(selector, response); }
   /* No real Gestalt available: report gestaltUndefSelectorErr (-5551), the
    * same answer the caller would otherwise get, rather than a false success. */
   if (response) { *response = 0; }
   return -5551;
}
