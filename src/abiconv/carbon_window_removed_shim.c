/*
 * carbon_window_removed_shim.c — Window Manager calls that 64-bit HIToolbox
 * dropped, for 32-bit Carbon callers (PvZ's PopCap window code). Without these
 * the legacy bridges jumped to NULL.
 *
 *  SetWindowAlpha      -> CGSSetWindowAlpha on the window's CGWindowID (the
 *                         same server call HIToolbox made).
 *  ChangeWindowGroupAttributes / SetWindowGroupParent -> no 64-bit equivalent;
 *                         the group keeps its defaults. noErr, logged once.
 *  Create/ReleaseQDContextForCollapsedWindowDockTile -> no QuickDraw dock
 *                         tiles; a clean error, so the caller skips the
 *                         custom minimized-tile drawing.
 */
#include <stdint.h>
#include <stdio.h>
#include <dlfcn.h>
#include "gap.h"

extern uint64_t x64_objc_unwrap(uint32_t h);

static void *win_native(uint32_t w) { return (void *)(uintptr_t)x64_objc_unwrap(w); }

static void once(const char *what) {
   fprintf(stderr, "carbon_window_removed_shim: %s has no 64-bit equivalent; ignored\n", what);
}

/* OSStatus SetWindowAlpha(WindowRef, CGFloat alpha) — i386 CGFloat is float */
uint32_t shim_SetWindowAlpha(uint32_t *a) {
   static uint32_t (*getid)(void *);
   static int32_t (*conn)(void);
   static int32_t (*setalpha)(int32_t, uint32_t, float);
   if (!setalpha) {
      getid    = (uint32_t (*)(void *))dlsym(RTLD_DEFAULT, "HIWindowGetCGWindowID");
      conn     = (int32_t (*)(void))dlsym(RTLD_DEFAULT, "CGSMainConnectionID");
      setalpha = (int32_t (*)(int32_t, uint32_t, float))dlsym(RTLD_DEFAULT, "CGSSetWindowAlpha");
   }
   void *w = win_native(a[0]);
   if (!w || !getid || !conn || !setalpha) { return (uint32_t)-50; }  /* paramErr */
   float alpha;
   __builtin_memcpy(&alpha, &a[1], sizeof alpha);
   return (uint32_t)setalpha(conn(), getid(w), alpha);
}

uint32_t shim_ChangeWindowGroupAttributes(uint32_t *a) {
   static int n; (void)a; if (!n++) once("ChangeWindowGroupAttributes"); return 0;
}
uint32_t shim_SetWindowGroupParent(uint32_t *a) {
   static int n; (void)a; if (!n++) once("SetWindowGroupParent"); return 0;
}
/* CGrafPtr CreateQDContextForCollapsedWindowDockTile(WindowRef) */
uint32_t shim_CreateQDContextForCollapsedWindowDockTile(uint32_t *a) { GAP_STUB(a); return 0; }
/* OSStatus ReleaseQDContextForCollapsedWindowDockTile(WindowRef, CGrafPtr) */
uint32_t shim_ReleaseQDContextForCollapsedWindowDockTile(uint32_t *a) { (void)a; return 0; }
