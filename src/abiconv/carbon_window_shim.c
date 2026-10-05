// carbon_window_shim.c — the 64-bit Window Manager's COMPOSITING contract for
// PROGRAMMATIC Carbon window creation.
//
// PROBLEM (structural; affects every 32-bit-era Carbon app that makes its own
// window).  Apple deleted the non-compositing (classic, GrafPort-drawn) window
// model when HIToolbox went 64-bit.  The check is unconditional and sits at the
// very top of the one surviving creation path:
//
//     HIToolbox`NewWindowCommon+23:   btl $0x13, %r14d     ; attributes bit 19
//     HIToolbox`NewWindowCommon+28:   jae <+1347>          ; -> -5601
//     HIToolbox`NewWindowCommon+1347: movl $0xffffea1f, %r14d
//
// Bit 19 is kWindowCompositingAttribute.  A caller that does not set it gets
// errUnsupportedWindowAttributesForClass (-5601) for EVERY class, even with no
// other attribute requested.  That attribute did not exist until Mac OS X 10.2
// and stayed opt-in through 10.6, so essentially all pre-2005 Carbon code omits
// it and every window it asks for fails on modern macOS.  Measured, x86_64:
//
//     CreateNewWindow(kPlainWindowClass,    0x80000000)  -> -5601
//     CreateNewWindow(kDocumentWindowClass, 0x82000009)  -> -5601
//     CreateNewWindow(kDocumentWindowClass, 0)           -> -5601
//     ... each of the three + (1<<19)                    -> noErr, real window
//
// Live failure this repairs: Halo CE's render-window setup (i386 0x2c2522) calls
// CreateNewWindow(kPlainWindowClass, kWindowNoConstrainAttribute) for fullscreen
// and CreateNewWindow(kDocumentWindowClass, closeBox|collapseBox|stdHandler|
// noConstrain) for windowed, then `test eax,eax / jne` on the returned WindowRef
// and takes its error path when it is NULL -> the empty alert the user sees
// after clicking OK in the graphics-settings window.  No window was ever made.
//
// FIX (real functionality, universal): OR kWindowCompositingAttribute into the
// attributes of every legacy programmatic window request.  It triggers on a
// STRUCTURAL property — a 32-bit-era attribute set reaching a 64-bit Window
// Manager that implements exactly one window model — never on an app, class or
// window name, and it is the only faithful translation available: the model the
// caller asked for does not exist in the target framework, and compositing is
// what all of that framework's remaining window surface (HIView, the standard
// handler, the frame view) is built on.
//
// SCOPE.  CreateNewWindow is the ONLY programmatic window-creation entry point
// that survives in 64-bit HIToolbox — CreateCustomWindow, NewWindow, NewCWindow,
// GetNewWindow, GetNewCWindow, CreateWindowFromResource, CreateWindowFromCollection,
// HIWindowCreate and GetAvailableWindowAttributes are all ABSENT from the modern
// dyld namespace (measured with dlsym(RTLD_DEFAULT)).  So this one shim covers
// the entire live surface of the rule.  The nib-loading half of the same family
// is already handled inside carbon_nib_shim.c, which forces the same bit when it
// builds a window from a parsed .xib; this file is its programmatic counterpart,
// for the apps and code paths that never go through a nib.
//
// DEGRADE.  With compositing on, the 64-bit attribute/class table can still
// reject a decoration the class does not offer (measured: class 13 +
// closeBox|collapseBox -> -5601), where 10.6 would have produced a plain window
// rather than an error.  On that error ONLY, retry with a bare compositing
// window of the same class, preserving the caller's standard-handler choice.  A
// window with fewer decorations is a far better translation than no window at
// all, and the retry cannot turn a succeeding call into a failing one.  (Same
// last-resort shape carbon_nib_shim.c already uses for nib windows.)
//
// NOT DONE, deliberately: ChangeWindowAttributes is left native.  Clearing the
// injected compositing bit returns errWindowAttributeImmutable (-5612) — but so
// does clearing several bits we do NOT inject, so masking ours out would be a
// partial cure for a hazard no target has hit.  Recorded rather than half-fixed.
//
// A/B kill-switch: M64_NO_CARBON_COMPOSITING=1 disarms the injection and the
// degrade retry, restoring raw native behaviour (-5601).  Used by
// tests-i386/carbon_window_compositing_test.sh to prove the guard bites.
//
// MTSHIM convention (maptable_tramp.asm): rdi -> &i386 args[0] (4-byte cdecl
// slots), uint32_t (OSStatus) result in eax.  Wired ___CreateNewWindow ->
// _shim_CreateNewWindow; that MTSHIM line also removes _CreateNewWindow from the
// abigen legacy consider-set (shimdb_legacy_considerset.py scans this asm), so
// the generated bridge is no longer emitted and there is no duplicate symbol.
// Since translated targets already bind ___CreateNewWindow (it existed as an
// abigen bridge), swapping the implementation validates on a RESYNC.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "gap.h"

// Classic Rect: 4 packed SInt16, identical in the i386 and x86_64 ABIs, so the
// caller's low-4GB pointer is handed to native verbatim.
typedef struct { int16_t top, left, bottom, right; } CRect;
typedef struct OpaqueWindowPtr *WindowRef;

// arena bridge (objc_shim.c): real 64-bit ptr -> low-4GB i386 handle.
extern uint32_t x64_objc_wrap(uint64_t real);

#define kWinCompositing  (1u << 19)   // kWindowCompositingAttribute
#define kWinStdHandler   (1u << 25)   // kWindowStandardHandlerAttribute
#define errUnsupportedWindowAttributesForClass ((int32_t)-5601)
#define kWinParamErr     ((int32_t)-50)

static int32_t (*n_CreateNewWindow)(uint32_t, uint32_t, const CRect *, WindowRef *);

static int cw_disabled(void)
{
   static int v = -1;
   if (v < 0) v = getenv("M64_NO_CARBON_COMPOSITING") != NULL;
   return v;
}

// OSStatus CreateNewWindow(WindowClass, WindowAttributes, const Rect *contentBounds,
//                          WindowRef *outWindow)
uint32_t shim_CreateNewWindow(uint32_t *a)
{
   uint32_t   cls    = a[0];
   uint32_t   attrs  = a[1];
   const CRect *bnds = (const CRect *)(uintptr_t)a[2];
   uint32_t  *out    = (uint32_t *)(uintptr_t)a[3];

   if (!n_CreateNewWindow)
      n_CreateNewWindow = (int32_t (*)(uint32_t, uint32_t, const CRect *, WindowRef *))
                          dlsym(RTLD_DEFAULT, "CreateNewWindow");
   if (!n_CreateNewWindow) return (uint32_t)kWinParamErr;
   if (!out)               return (uint32_t)kWinParamErr;   // native's own -50 case

   *out = 0;
   uint32_t want = cw_disabled() ? attrs : (attrs | kWinCompositing);

   WindowRef w = NULL;
   int32_t   st = n_CreateNewWindow(cls, want, bnds, &w);

   // Class does not offer one of the requested decorations: fall back to a bare
   // compositing window of the same class rather than handing the caller nothing.
   if (st == errUnsupportedWindowAttributesForClass && !cw_disabled()) {
      w = NULL;
      st = n_CreateNewWindow(cls, kWinCompositing | (attrs & kWinStdHandler), bnds, &w);
   }

   if (st == 0 && w) {
      // Mirror the abigen bridge's out-parameter rule: a native ref above 4GB
      // becomes an arena handle, a low ref passes through verbatim.
      uint64_t r = (uint64_t)(uintptr_t)w;
      *out = (r >> 32) ? x64_objc_wrap(r) : (uint32_t)r;
   }
   return (uint32_t)st;
}

// OSStatus GetWindowResizeLimits(WindowRef, HISize *outMinLimits, HISize *outMaxLimits)
// 32-bit only: 64-bit HIToolbox still exports SetWindowResizeLimits but no getter,
// so the abigen bridge called NULL (Call of Duty 4's setup dialog reads the nib's
// limits to pin a fixed-width window). The limits a nib sets live inside HIToolbox
// where no surviving API reads them back, so the honest answer is unimpErr with both
// i386 HISizes (2 floats each) defined; callers of this era treat an error as "leave
// the limits alone". Kill M64_NO_GETWINRESIZE (the old native-NULL forward); guard
// get-window-resize-limits.
uint32_t shim_GetWindowResizeLimits(uint32_t *a)
{
   float *mn = (float *)(uintptr_t)a[1], *mx = (float *)(uintptr_t)a[2];
   if (getenv("M64_NO_GETWINRESIZE")) {
      int32_t (*n)(void *, void *, void *) =
         (int32_t (*)(void *, void *, void *))dlsym(RTLD_DEFAULT, "GetWindowResizeLimits");
      return (uint32_t)n((void *)(uintptr_t)a[0], mn, mx);   /* NULL on 64-bit: the old crash */
   }
   if (mn) { mn[0] = 0; mn[1] = 0; }
   if (mx) { mx[0] = 0; mx[1] = 0; }
   GAP_STUB(a);
   return (uint32_t)(int32_t)-4;   /* unimpErr */
}
