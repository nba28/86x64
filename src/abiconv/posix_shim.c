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

/* open(const char *path, int oflag, ...):
 *   a[0] = path (low-4GB char*), a[1] = oflag, a[2] = mode (used iff O_CREAT). */
int32_t shim_open(uint32_t *a) {
   const char *path = (const char *)(uintptr_t)a[0];
   int   oflag = (int)a[1];
   int   mode  = (int)a[2];   /* ignored by open() unless O_CREAT/O_TMPFILE */
   return (int32_t)open(path, oflag, mode);
}

/* fcntl(int fd, int cmd, ...):
 *   a[0] = fd, a[1] = cmd, a[2] = int arg OR low-4GB struct pointer. */
int32_t shim_fcntl(uint32_t *a) {
   int fd  = (int)a[0];
   int cmd = (int)a[1];
   /* Pass the third slot as a pointer-width value: it carries either a small
    * integer (F_SETFD/F_SETFL/F_DUPFD) or a low-4GB struct pointer
    * (F_GETLK/F_SETLK flock*), both correct zero-extended. */
   return (int32_t)fcntl(fd, cmd, (void *)(uintptr_t)a[2]);
}
