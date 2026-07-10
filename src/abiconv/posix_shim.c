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
#include <os/lock.h>
#include <mach/vm_prot.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <CoreFoundation/CoreFoundation.h>

/* proxy arena (objc_shim.c): bridge a 64-bit handle/ptr <-> a 32-bit token the
 * i386 caller can hold, and bounce a 64-bit C-string into low-4GB. */
extern uint32_t x64_objc_wrap(uint64_t real);
extern uint64_t x64_objc_unwrap(uint32_t h);
extern uint32_t x64_objc_bounce_cstr(const char *s);

/* dlsym callable-thunk pool (dlsym_tramp.asm): low-4GB i386-callable stubs that
 * marshal an i386 cdecl call into a captured native function. g_dlsym_target[k]
 * holds the native target for slot k (0 = free); x64_dlsym_thunk_table[k] is the
 * stub address the i386 caller invokes; x64_dlsym_nslots is the pool size. */
extern uint64_t g_dlsym_target[];
extern uint64_t x64_dlsym_thunk_table[];
extern uint64_t x64_dlsym_nslots;

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

/* True if `v` lies in an executable Mach-O segment (i.e. it is code, a function),
 * as opposed to a data symbol. dlsym() loses the function-vs-data distinction, so
 * we recover it from the owning segment's protection: a callable thunk is only
 * correct for a function; a data symbol must keep the arena-handle path (it would
 * be read, not called).
 *
 * We must NOT use the live VM protection (mach_vm_region): under Rosetta the
 * native x86_64 framework pages are mapped read-only (Rosetta translates them in
 * a separate JIT region), so they never carry VM_PROT_EXECUTE and every function
 * would look like data. The Mach-O segment header's `initprot` carries
 * VM_PROT_EXECUTE for __TEXT regardless of how Rosetta maps the pages. */
static int addr_is_executable(uint64_t v) {
   Dl_info di;
   if (dladdr((void *)(uintptr_t)v, &di) == 0 || di.dli_fbase == NULL) { return 0; }
   const struct mach_header_64 *mh = (const struct mach_header_64 *)di.dli_fbase;
   if (mh->magic != MH_MAGIC_64) { return 0; }
   const struct load_command *lc =
      (const struct load_command *)((const char *)mh + sizeof(*mh));

   /* slide = runtime header address - the __TEXT segment's link-time vmaddr */
   intptr_t slide = 0;
   const struct load_command *l = lc;
   for (uint32_t i = 0; i < mh->ncmds; i++) {
      if (l->cmd == LC_SEGMENT_64) {
         const struct segment_command_64 *s = (const struct segment_command_64 *)l;
         if (strcmp(s->segname, SEG_TEXT) == 0) {
            slide = (intptr_t)mh - (intptr_t)s->vmaddr;
            break;
         }
      }
      l = (const struct load_command *)((const char *)l + l->cmdsize);
   }
   /* find the segment containing v; report its initprot EXECUTE bit */
   l = lc;
   for (uint32_t i = 0; i < mh->ncmds; i++) {
      if (l->cmd == LC_SEGMENT_64) {
         const struct segment_command_64 *s = (const struct segment_command_64 *)l;
         uint64_t lo = (uint64_t)((intptr_t)s->vmaddr + slide);
         if (v >= lo && v < lo + s->vmsize) {
            return (s->initprot & VM_PROT_EXECUTE) != 0;
         }
      }
      l = (const struct load_command *)((const char *)l + l->cmdsize);
   }
   return 0;
}

/* Bind a >4GB native FUNCTION to a low-4GB i386-callable thunk slot and return
 * its stub address (a valid 32-bit pointer the i386 caller can call). Slots are
 * reused per native target so repeated dlsym()s of the same entry share one. */
static uint32_t dlsym_make_thunk(uint64_t native, const char *name) {
   static os_unfair_lock lk = OS_UNFAIR_LOCK_INIT;
   uint64_t n = x64_dlsym_nslots;
   os_unfair_lock_lock(&lk);
   for (uint64_t i = 0; i < n; i++) {                 /* reuse existing binding */
      if (g_dlsym_target[i] == native) {
         os_unfair_lock_unlock(&lk);
         return (uint32_t)x64_dlsym_thunk_table[i];
      }
   }
   for (uint64_t i = 0; i < n; i++) {                 /* allocate a free slot */
      if (g_dlsym_target[i] == 0) {
         g_dlsym_target[i] = native;
         uint32_t stub = (uint32_t)x64_dlsym_thunk_table[i];
         os_unfair_lock_unlock(&lk);
         if (posix_trace()) {
            fprintf(stderr, "[posix] dlsym: %s native=0x%llx -> callable thunk "
                    "slot %llu @0x%x\n", name ? name : "?",
                    (unsigned long long)native, (unsigned long long)i, stub);
            fflush(stderr);
         }
         return stub;
      }
   }
   os_unfair_lock_unlock(&lk);
   fprintf(stderr, "[posix] dlsym: thunk slots exhausted for %s\n",
           name ? name : "?");
   return 0;
}

/* Map a NATIVE address resolved by a lookup-by-name API (dlsym,
 * CFBundleGetFunctionPointerForName, ...) to a value the i386 caller can hold
 * and call. The caller hands the result straight to an i386 indirect call/jmp,
 * which is only 32 bits wide: a raw >4GB native pointer truncates to garbage
 * and the call faults (Civ IV: CFBundleGetFunctionPointerForName(System,
 * "chdir") = 0x7ff8`1b1010c4, truncated to 0x1b1010c4, jmp *eax -> SIGSEGV in
 * internal_chdir). Bind such a FUNCTION to a low-4GB i386-callable thunk that
 * marshals the i386 cdecl frame into the native call. A low (translated)
 * address is already callable and passes through; a >4GB DATA symbol keeps the
 * arena-handle path (it is dereferenced, not called). Shared by every
 * lookup-by-name shim so the function-vs-data discipline stays in one place. */
static int32_t fnptr_lookup_result(uint64_t v, const char *name) {
   if (v == 0) { return 0; }
   if (v < 0x100000000ULL) { return (int32_t)(uint32_t)v; }  /* translated/low: callable */
   /* High native address. A FUNCTION (executable page) can't be called by i386
    * code directly; hand back a low-4GB callable thunk. A DATA symbol keeps the
    * arena-handle path (it is dereferenced, not called). */
   if (addr_is_executable(v)) {
      uint32_t thunk = dlsym_make_thunk(v, name);
      if (thunk) { return (int32_t)thunk; }
      /* pool exhausted: fall through to the handle (better a later fault than
       * silently returning 0 / a wrong call) */
   }
   if (posix_trace()) {
      fprintf(stderr, "[posix] lookup: high DATA symbol %s=0x%llx wrapped as handle\n",
              name ? name : "?", (unsigned long long)v);
      fflush(stderr);
   }
   return (int32_t)x64_objc_wrap(v);
}

/* Prefer libabiconv's own interpose shim when one exists for the requested
 * symbol. static-interpose binds a translated i386 import "_<name>" to the shim
 * exported as "__" + "_<name>" (PREFIX "__"); a direct i386 call thus reaches the
 * shim, which marshals the i386 cdecl frame AND reverse-wraps any callback
 * argument. A symbol obtained via dlsym() must get the SAME shim: the generic
 * marshalling thunk (dlsym_make_thunk) only forwards the call, so a CALLBACK
 * passed through it is handed RAW to the native callee, which later invokes it
 * with the x86_64 ABI -> the i386 cdecl frame is read as registers -> fault.
 * (Civ IV: check_cxa_atexit dlsym's __cxa_atexit, registers the i386 callback
 * cxa_atexit_check_1 raw, then native __cxa_finalize calls it x86_64-ABI ->
 * jmp *0 at pc=0. With the shim, shim_cxa_atexit wraps the callback via
 * x64_cb_wrap so native __cxa_finalize invokes a native trampoline that marshals
 * back to the i386 callee.) Universal: triggers on "a shim is exported for this
 * name", not on an app.
 *
 * dlsym(name) resolves nlist "_name"; the shim's nlist is "__" + "_name" =
 * "___name". dlsym() prepends one '_', so we query "__" + name. The shim lives
 * only in libabiconv, so we restrict the search to libabiconv's own image and
 * require a low-4GB (i386-callable) result. */
static uint32_t dlsym_shim_for(const char *name) {
   if (!name || !*name) { return 0; }
   static void *abi = NULL;
   static int abi_ready = 0;
   static os_unfair_lock lk = OS_UNFAIR_LOCK_INIT;
   os_unfair_lock_lock(&lk);
   if (!abi_ready) {
      Dl_info di;
      if (dladdr((void *)&dlsym_shim_for, &di) && di.dli_fname) {
         abi = dlopen(di.dli_fname, RTLD_NOLOAD | RTLD_LAZY);
      }
      /* RTLD_NOLOAD by resolved abspath FAILS when libabiconv was loaded under a
       * different name than dladdr's realpath — the co-located multi-copy deploy
       * loads each copy via `@loader_path/libabiconv.dylib`, and dyld keys the
       * image on that install name, so dlopen(realpath, RTLD_NOLOAD) returns NULL
       * even though the image is mapped. Falling through then hands dlsym the RAW
       * marshalling thunk for a shimmed symbol (e.g. __cxa_atexit): the i386
       * callback it registers is later invoked by native __cxa_finalize with the
       * x86_64 ABI -> the 4-byte-truncated native arg derefs a NULL fn-ptr ->
       * jmp *0 (SFTabular check_cxa_atexit, crash pc=0). Fall back to the GLOBAL
       * namespace: every libabiconv copy exports the same interpose shims, so
       * RTLD_DEFAULT finds one, and the low-4GB guard below keeps only a callable
       * (this-image or any-copy) shim. */
      if (!abi) { abi = RTLD_DEFAULT; }
      abi_ready = 1;
   }
   void *h = abi;
   os_unfair_lock_unlock(&lk);
   if (!h) { return 0; }
   char q[256];
   int n = snprintf(q, sizeof q, "__%s", name);
   if (n < 0 || (size_t)n >= sizeof q) { return 0; }
   void *shim = dlsym(h, q);
   if (!shim) { return 0; }
   uint64_t v = (uint64_t)(uintptr_t)shim;
   if (v >= 0x100000000ULL) { return 0; }   /* shim must be low-4GB callable */
   return (uint32_t)v;
}

int32_t shim_dlsym(uint32_t *a) {
   void *h = dl_handle(a[0]);
   const char *name = a[1] ? (const char *)(uintptr_t)a[1] : NULL;
   /* A symbol libabiconv shims must come back as the shim (correct ABI +
    * callback reverse-wrapping), not a raw native marshalling thunk. */
   uint32_t shimaddr = dlsym_shim_for(name);
   if (shimaddr) {
      if (posix_trace()) {
         fprintf(stderr, "[posix] dlsym(\"%s\") -> libabiconv interpose shim "
                 "@0x%x\n", name ? name : "(null)", shimaddr);
         fflush(stderr);
      }
      return (int32_t)shimaddr;
   }
   void *sym = dlsym(h, name);
   if (posix_trace()) {
      fprintf(stderr, "[posix] dlsym(%p, \"%s\") = %p\n",
              h, name ? name : "(null)", sym);
      fflush(stderr);
   }
   /* Bridge the resolved address to an i386-callable value: a >4GB native
    * FUNCTION becomes a low-4GB callable thunk (wrapping it as a handle made the
    * indirect call jump into proxy-arena data -> SIGBUS, e.g. iPhoto's
    * BackupWrapper dlsym'ing BURegisterStartTimeMachineFromDock); a DATA symbol
    * keeps the arena-handle path. */
   return fnptr_lookup_result((uint64_t)(uintptr_t)sym, name);
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

/* ---- CoreFoundation bundle function lookup -------------------------------
 * CFBundleGetFunctionPointerForName(bundle, name) is the CF analogue of dlsym:
 * it returns a raw native function pointer the i386 caller then calls directly
 * (CFBundle-as-dynamic-loader, the classic Carbon idiom). abigen's generated
 * bridge marshalled the CFBundleRef/CFStringRef args correctly but returned the
 * >4GB native pointer untouched, so the i386 caller truncated it to 32 bits and
 * jumped to garbage — Civ IV's Aspyr BSD-compat layer (GetBSDProcAddress ->
 * CFBundleGetFunctionPointerForName(System, "chdir") = 0x7ff8`1b1010c4, stored
 * 32-bit in _bsd_chdir, then `jmp *eax` in internal_chdir -> 0x1b1010c4 ->
 * EXC_BAD_ACCESS; whether the truncated low half lands unmapped (crash) or
 * mapped (hang) just depends on the per-boot shared-cache slide).
 *
 * Hand-shim it as the exact CF counterpart of shim_dlsym: resolve the two
 * object args through the SAME full resolver the abigen bridge used
 * (_86x64_unwrap_obj_arg: arena handle / CFSTR constant / already-low ptr),
 * call the real CF API, and route the returned native function pointer through
 * fnptr_lookup_result so it comes back as a low-4GB i386-callable thunk.
 * Universal: triggers on the structural property (a lookup-by-name API returned
 * a native function pointer for i386 to call), not on any app — benefits every
 * i386 target using this idiom (Civ IV, Halo, iPhoto). The other native
 * function-pointer-by-name lookups (NSAddressOfSymbol,
 * CFBundleGetFunctionPointersForNames) have the identical latent defect; see
 * todo_gaps. */
extern uint64_t _86x64_unwrap_obj_arg(uint32_t a);  /* objc_shim.c: full resolver */

int32_t shim_CFBundleGetFunctionPointerForName(uint32_t *a) {
   CFBundleRef bundle = (CFBundleRef)(uintptr_t)_86x64_unwrap_obj_arg(a[0]);
   CFStringRef fname  = (CFStringRef)(uintptr_t)_86x64_unwrap_obj_arg(a[1]);
   char nm[256];
   nm[0] = '\0';
   if (fname) { CFStringGetCString(fname, nm, sizeof nm, kCFStringEncodingUTF8); }
   void *fp = bundle ? CFBundleGetFunctionPointerForName(bundle, fname) : NULL;
   if (posix_trace()) {
      fprintf(stderr, "[posix] CFBundleGetFunctionPointerForName(\"%s\") = %p\n",
              nm[0] ? nm : "?", fp);
      fflush(stderr);
   }
   return fnptr_lookup_result((uint64_t)(uintptr_t)fp,
                              nm[0] ? nm : "CFBundleGetFunctionPointerForName");
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
