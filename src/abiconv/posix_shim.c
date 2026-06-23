/*
 * Hand-written i386->x86_64 shims for the VARIADIC POSIX primitives abigen
 * cannot generate (it skips functions with a `...` parameter): open() and
 * fcntl(). Without a shim the translated i386 `call open` reaches NATIVE
 * libSystem open() directly — it reads its args from registers (the i386
 * caller put them on the stack) and, fatally, does a 64-bit `ret` that
 * over-pops the i386 caller's 4-byte return slot, fusing stale stack garbage
 * into the high 32 bits of the popped PC (observed: iPhoto building its
 * library path then `open()`ing it -> EXC_BAD_ACCESS at 0x8a8a0f40`01780e04,
 * the bounce-cstr handle fused over a real return address).
 *
 * The MTSHIM trampoline (maptable_tramp.asm) hands us `a = &args[0]` — the
 * i386-cdecl arg block on the stack, each slot 4 bytes — and returns our
 * uint32_t result in eax while correctly unwinding the i386 4-byte frame.
 *
 * Arg pointers (open's path, fcntl's optional struct) are already low-4GB
 * pointers in the translated address space (raw i386 strings or forward-bridge
 * bounce buffers), so they pass straight through zero-extended. struct flock
 * (the only fcntl pointer arg iPhoto uses) has an identical field layout on
 * i386 and x86_64 (Darwin off_t is 64-bit on both), so no struct translation
 * is needed. Generic to any i386 target.
 */

#include <stdint.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <dlfcn.h>

/* proxy arena (objc_shim.c): bridge a 64-bit handle/ptr <-> a 32-bit token the
 * i386 caller can hold, and bounce a 64-bit C-string into low-4GB. */
extern uint32_t x64_objc_wrap(uint64_t real);
extern uint64_t x64_objc_unwrap(uint32_t h);
extern uint32_t x64_objc_bounce_cstr(const char *s);

static int posix_trace(void) {
   static int t = -1;
   if (t < 0) { t = getenv("POSIX_TRACE") ? 1 : 0; }
   return t;
}

/* ---- dynamic loader (dlopen/dlsym/dlclose/dlerror) -----------------------
 * abigen gives these no shim, so a translated i386 `call dlopen` reaches NATIVE
 * dlopen with the i386 cdecl ABI (args on the stack, not in SysV regs) -> the
 * native callee reads garbage registers and faults (observed: Portal 2
 * portal2_osx Sys_LoadModule -> dlopen(path=0x1)). Source-style plugin loaders
 * (dlopen a module, dlsym its entry, call it) depend on this.
 *
 * Handle bridging: native dlopen returns a 64-bit handle the i386 caller can
 * only hold in 32 bits — wrap it into a low arena token (the same convention as
 * the objc bridge) and unwrap on dlsym/dlclose. dlsym's returned address, for a
 * TRANSLATED module (loaded in the low-4GB window, as all our outputs are),
 * fits in 32 bits and is called translated->translated with the emulated i386
 * ABI; a high (native) address can't be called by i386 code directly and is
 * flagged. The RTLD_* pseudo-handles (-1..-5) sign-extend from their i386
 * 0xFFFFFFFx form. */
int32_t shim_dlopen(uint32_t *a) {
   const char *path = a[0] ? (const char *)(uintptr_t)a[0] : NULL;
   int mode = (int)a[1];
   void *h = dlopen(path, mode);
   if (posix_trace()) {
      fprintf(stderr, "[posix] dlopen(\"%s\", 0x%x) = %p\n",
              path ? path : "(null)", mode, h);
      fflush(stderr);
   }
   return (int32_t)x64_objc_wrap((uint64_t)(uintptr_t)h);
}

static void *dl_handle(uint32_t h32) {
   /* RTLD_NEXT(-1)/DEFAULT(-2)/SELF(-3)/MAIN_ONLY(-5): pass the sign-extended
    * pseudo-handle straight through. Real handles round-trip via the arena. */
   if (h32 >= 0xFFFFFFF0u) { return (void *)(intptr_t)(int32_t)h32; }
   return (void *)(uintptr_t)x64_objc_unwrap(h32);
}

int32_t shim_dlsym(uint32_t *a) {
   void *h = dl_handle(a[0]);
   const char *name = a[1] ? (const char *)(uintptr_t)a[1] : NULL;
   void *sym = dlsym(h, name);
   if (posix_trace()) {
      fprintf(stderr, "[posix] dlsym(%p, \"%s\") = %p\n",
              h, name ? name : "(null)", sym);
      fflush(stderr);
   }
   uint64_t v = (uint64_t)(uintptr_t)sym;
   if (v == 0) { return 0; }
   if (v < 0x100000000ULL) { return (int32_t)(uint32_t)v; }  /* translated/low: callable */
   /* High native address: not directly callable by i386 code. Wrap so it at
    * least round-trips for data symbols; a native FUNCTION target would need a
    * callback bridge (none of Source's dlsym'd entries are native). */
   fprintf(stderr, "[posix] dlsym: WARNING high native symbol %s=%p wrapped "
           "(not callable by i386 if a function)\n", name ? name : "?", sym);
   return (int32_t)x64_objc_wrap(v);
}

int32_t shim_dlclose(uint32_t *a) {
   void *h = dl_handle(a[0]);
   return (int32_t)dlclose(h);
}

int32_t shim_dlerror(uint32_t *a) {
   (void)a;
   const char *e = dlerror();
   return (int32_t)(e ? x64_objc_bounce_cstr(e) : 0);
}

/* open(const char *path, int oflag, ...):
 *   a[0] = path (low-4GB char*), a[1] = oflag, a[2] = mode (used iff O_CREAT). */
int32_t shim_open(uint32_t *a) {
   const char *path = (const char *)(uintptr_t)a[0];
   int   oflag = (int)a[1];
   int   mode  = (int)a[2];   /* ignored by open() unless O_CREAT/O_TMPFILE */
   int r = open(path, oflag, mode);
   if (posix_trace()) {
      fprintf(stderr, "[posix] open(\"%s\", 0x%x, 0%o) = %d%s\n",
              path ? path : "(null)", oflag, mode, r,
              r < 0 ? strerror(errno) : "");
      fflush(stderr);
   }
   return (int32_t)r;
}

/* fcntl(int fd, int cmd, ...):
 *   a[0] = fd, a[1] = cmd, a[2] = int arg OR low-4GB struct pointer. */
int32_t shim_fcntl(uint32_t *a) {
   int fd  = (int)a[0];
   int cmd = (int)a[1];
   /* Pass the third slot as a pointer-width value: it carries either a small
    * integer (F_SETFD/F_SETFL/F_DUPFD) or a low-4GB struct pointer
    * (F_GETLK/F_SETLK flock*), both correct zero-extended. */
   int r = fcntl(fd, cmd, (void *)(uintptr_t)a[2]);
   if (posix_trace()) {
      fprintf(stderr, "[posix] fcntl(fd=%d, cmd=%d, arg=0x%x) = %d%s\n",
              fd, cmd, a[2], r, r < 0 ? strerror(errno) : "");
      fflush(stderr);
   }
   return (int32_t)r;
}
