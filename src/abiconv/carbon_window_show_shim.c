/*
 * carbon_window_show_shim.c — ShowWindow / IsWindowVisible / HideWindow
 * for translated callers: unwrap the i386 WindowRef handle (CreateNewWindow
 * hands out an arena handle for a >4GB WindowRef) and forward to native.
 */

#include <stdint.h>
#include <dlfcn.h>

#include "carbon_shim.h"

extern uint64_t x64_objc_unwrap(uint32_t h);

typedef void (*dsw_void_fn)(void *);
typedef unsigned char (*dsw_bool_fn)(void *);

/* void ShowWindow(WindowRef) */
uint32_t shim_ShowWindow(uint32_t *args) {
   void *win = (void *)(uintptr_t)x64_objc_unwrap(args[0]);
   static dsw_void_fn f;
   if (!f) f = (dsw_void_fn)dlsym(RTLD_DEFAULT, "ShowWindow");
   if (f && win) f(win);
   extern void agl_window_fit(void *);   /* agl_drawable_shim.c: too big for the screen? */
   agl_window_fit(win);
   return 0;
}

/* Boolean IsWindowVisible(WindowRef) */
uint32_t shim_IsWindowVisible(uint32_t *args) {
   void *win = (void *)(uintptr_t)x64_objc_unwrap(args[0]);
   static dsw_bool_fn f;
   if (!f) f = (dsw_bool_fn)dlsym(RTLD_DEFAULT, "IsWindowVisible");
   if (!f || !win) return 0;
   return f(win) ? 1u : 0u;
}

/* void HideWindow(WindowRef) */
uint32_t shim_HideWindow(uint32_t *args) {
   void *win = (void *)(uintptr_t)x64_objc_unwrap(args[0]);
   static dsw_void_fn f;
   if (!f) f = (dsw_void_fn)dlsym(RTLD_DEFAULT, "HideWindow");
   if (f && win) f(win);
   return 0;
}
