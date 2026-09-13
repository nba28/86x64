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
      /* ★A FAILED dlopen MUST REPORT dlerror(). Without it a module that cannot
       * load is indistinguishable from one that was never asked for, and the app
       * just quietly gives up: Portal 2's CSourceAppSystemGroup::Create() failed
       * solely because vguimatsurface.dylib returned NULL here, and with no error
       * text the whole startup looked like an idle-park in shutdown instead.
       * dlerror() also CLEARS the error, so read it only on the failure path --
       * never speculatively, or a later genuine dlerror() comes back empty. */
      if (h == NULL) {
         const char *e = dlerror();
         fprintf(stderr, "[posix]   dlerror: %s\n", e ? e : "(none)");
      }
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

/* ---- a bridge must DEFER to a TRANSLATED provider -------------------------
 * ★ The MIRROR IMAGE of dlsym_shim_for's own reason to exist.
 *
 * An abigen legacy bridge `___X` is an i386-callable entry that marshals the
 * i386 cdecl frame up to the x86_64 SysV ABI and `call`s a NATIVE `_X`:
 *     ___PyInt_AsLong.l1:  mov edi,[rbp+0xc]      ; i386 stack slot -> SysV reg
 *                          call  _PyInt_AsLong    ; 8-byte return address
 *                          mov r11d,[rsp]; add rsp,4; jmp r11   ; i386 4-byte ret
 * That is exactly right while a NATIVE `_X` exists. When the implementation is
 * ITSELF TRANSLATED, the bridge is not redundant — it is WRONG. Translated code
 * keeps the i386 convention (arguments in 4-byte stack slots, a 4-byte return
 * address), so the bridge's native-convention call hands it registers it never
 * reads and a return address twice the width it pops: the callee reads stack
 * garbage as its argument and returns to a fused PC. A DOUBLE CONVERSION.
 *
 * MEASURED, Civilization IV (2026-08-08), SIGSEGV KERN_INVALID_ADDRESS at 0x11:
 *     #0  ?+0x100a338c8                    (native-convention call debris)
 *     #1  libabiconv+0x22db86  ___Py_Initialize+0x33      <- this bridge
 * Apple removed Python 2 in macOS 12.3, so there is no native `_Py_Initialize`
 * left; the bundle vendors our OWN TRANSLATED Python 2.6 (__TEXT vmaddr
 * 0x10000000, `_Py_Initialize` at 0x10109e02). `Civilization IV.dylib` binds
 * `Python/_Py_Initialize` CORRECTLY and static-interpose never touched it — the
 * bridge is reached at RUN TIME instead, through the by-name lookup family:
 *     Civilization IV.dylib -> ___NSLookupAndBindSymbol("_Py_Initialize")
 *       -> ns_symbol_resolve -> dlsym_shim_for -> ___Py_Initialize
 * and the bridge's own `<flat-namespace>/_Py_Initialize` lazy bind then flat-
 * resolves straight back into the translated Python. dyld_info sees nothing
 * wrong anywhere: the whole detour is minted at run time by us.
 *
 * THE RULE (universal, structural, keyed on nothing but Mach-O shape): when the
 * image that DEFINES `_X` in this process is itself a TRANSLATED image, hand the
 * i386 caller that definition directly and suppress the bridge. An image is
 * translated iff it loads libabiconv — every artifact of the translate pipeline
 * does, and no native system image ever does.
 *
 * ⚠It must NOT break the normal case. Deferral fires ONLY when a translated
 * image in THIS PROCESS actually defines the symbol; with no such provider
 * (every other target, and most symbols in every target) the shim is returned
 * exactly as before. That is why the decision is made here, at run time, from
 * the loaded image list, rather than from a build-time list: whether a
 * translated provider exists is a property of the BUNDLE, not of libabiconv,
 * and the same libabiconv.dylib is deployed into every one of them.
 *
 * Kill switch M64_NO_TRANSLATED_PROVIDER_DEFER=1 restores the double-converting
 * behaviour so the defect can be reproduced on demand. */
static int translated_provider_defer_disabled(void) {
   static int t = -1;
   if (t < 0) { t = getenv("M64_NO_TRANSLATED_PROVIDER_DEFER") ? 1 : 0; }
   return t;
}

/* Structural test: does this loaded image carry an LC_LOAD_DYLIB naming
 * libabiconv? Every product of the translate pipeline does (static-interpose
 * adds the load command before rewriting binds into it); no native system image
 * does. Reads the mapped header only — no dlopen, no file I/O. */
static int image_is_translated(const void *base) {
   const struct mach_header_64 *mh = (const struct mach_header_64 *)base;
   if (mh == NULL || mh->magic != MH_MAGIC_64) { return 0; }
   const struct load_command *lc = (const struct load_command *)(mh + 1);
   for (uint32_t i = 0; i < mh->ncmds; i++) {
      if (lc->cmdsize < sizeof *lc) { return 0; }   /* malformed: refuse to walk */
      if (lc->cmd == LC_LOAD_DYLIB || lc->cmd == LC_LOAD_WEAK_DYLIB ||
          lc->cmd == LC_REEXPORT_DYLIB || lc->cmd == LC_LOAD_UPWARD_DYLIB) {
         const struct dylib_command *dc = (const struct dylib_command *)lc;
         if (dc->dylib.name.offset < dc->cmdsize) {
            const char *nm = (const char *)lc + dc->dylib.name.offset;
            if (strstr(nm, "libabiconv") != NULL) { return 1; }
         }
      }
      lc = (const struct load_command *)((const char *)lc + lc->cmdsize);
   }
   return 0;
}

/* Is `_<bare>` defined by a TRANSLATED image in this process, at an address the
 * i386 caller can hold and call directly? Returns that address, else 0.
 *
 * The low-4GB requirement is not decoration: a translated definition is
 * i386-callable ONLY because it lives where a 4-byte pointer can reach it (the
 * vendored Python's __TEXT sits at 0x10000000). If a provider somehow landed
 * above 4GB we could not hand it over anyway, and the shim remains the best
 * available answer — so the gate stays shut rather than guessing. */
static uint32_t translated_provider_for(const char *bare) {
   if (translated_provider_defer_disabled()) { return 0; }
   if (bare == NULL || *bare == '\0') { return 0; }
   void *sym = dlsym(RTLD_DEFAULT, bare);
   if (sym == NULL) { return 0; }
   if ((uintptr_t)sym >= 0x100000000ULL) { return 0; }  /* not i386-callable */
   Dl_info di;
   if (!dladdr(sym, &di) || di.dli_fbase == NULL) { return 0; }
   if (!image_is_translated(di.dli_fbase)) { return 0; }
   return (uint32_t)(uintptr_t)sym;
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

/* THE by-name-lookup answer, for every member of the family (dlsym,
 * CFBundleGetFunctionPointerForName, NSLookupAndBindSymbol/NSAddressOfSymbol).
 * Two rules, in this order:
 *   1. DEFER to a TRANSLATED provider. Its code already speaks the i386
 *      convention, so the caller must reach it DIRECTLY; putting the bridge in
 *      front of it double-converts (see translated_provider_for above).
 *   2. otherwise PREFER libabiconv's interpose shim over a raw marshalling
 *      thunk, which is why this family consults libabiconv at all.
 * 0 = neither applies; the caller falls through to its own native resolution.
 *
 * The shim is looked up FIRST and the provider question asked only when there
 * is a shim to suppress. With no shim the answer cannot change, so this costs
 * nothing (no extra RTLD_DEFAULT search) and alters no existing behaviour on
 * the overwhelming majority of names. */
static uint32_t lookup_by_name_target(const char *name) {
   uint32_t shimaddr = dlsym_shim_for(name);
   if (!shimaddr) { return 0; }
   uint32_t provider = translated_provider_for(name);
   if (provider) {
      if (posix_trace()) {
         fprintf(stderr, "[posix] lookup(\"%s\") -> TRANSLATED provider @0x%x "
                 "(suppressed libabiconv bridge @0x%x: it would double-convert "
                 "the ABI)\n", name ? name : "(null)", provider, shimaddr);
         fflush(stderr);
      }
      return provider;
   }
   return shimaddr;
}

int32_t shim_dlsym(uint32_t *a) {
   void *h = dl_handle(a[0]);
   const char *name = a[1] ? (const char *)(uintptr_t)a[1] : NULL;
   /* A symbol libabiconv shims must come back as the shim (correct ABI +
    * callback reverse-wrapping), not a raw native marshalling thunk — unless a
    * TRANSLATED image already provides it, in which case the shim would
    * double-convert and we hand back that provider instead. */
   uint32_t shimaddr = lookup_by_name_target(name);
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

/* ---- dladdr ---------------------------------------------------------------
 * ★ MEASURED on Civilization IV 2026-08-09/10. The dl* family was shimmed for
 * dlopen/dlsym/dlclose/dlerror and NOT for dladdr, so static-interpose had no
 * `___dladdr` to redirect to and the translated bind stayed on NATIVE libSystem.
 * The translated i386 `call dladdr` puts its arguments in 4-byte STACK slots;
 * native dladdr reads them from %rdi/%rsi. Under lldb, at the fault:
 *     frame #0  dyld`dyld4::APIs::dladdr + 635
 *     ->  movq 0x8(%rbx), %rcx      rbx = 0x4e   (0x4e + 8 = 0x56 = fault addr)
 *         rdi = 0x8aee8378 (a stack address, read as `addr`)
 *         rsi = 0x8        (read as `Dl_info *`)
 * CPython 2.6 calls dladdr during interpreter start-up, so this landed inside
 * Py_Initialize and looked for a long time like a Python-bridge defect. It is
 * not: it is the plain "no shim -> native call with the i386 ABI" family, the
 * same one documented above for dlopen/dlsym and for the NSSymbol trio.
 *
 * It presents as a RACE (11 of 11 unhosted runs, intermittent under a debugger)
 * because the native callee reads whatever happens to be in %rdi/%rsi, and
 * whether that garbage is an unmapped pointer depends on stack contents and
 * scheduling.
 *
 * ⚠NOT a pass-through. `Dl_info` is FOUR POINTER FIELDS, so it is 16 bytes on
 * i386 and 32 on x86_64:
 *     i386   +0 dli_fname  +4 dli_fbase  +8 dli_sname  +12 dli_saddr
 *     x86_64 +0 dli_fname  +8 dli_fbase +16 dli_sname  +24 dli_saddr
 * Handing the caller's 16-byte buffer to native dladdr would both overrun it by
 * 16 bytes and leave every field at the wrong offset. The shim calls native
 * dladdr into a NATIVE Dl_info and writes back four 4-byte slots, bouncing the
 * two strings into low-4GB (the same x64_objc_bounce_cstr path shim_dlerror
 * uses) and wrapping any >4GB base/symbol address as an arena handle.
 *
 * Kill switch M64_NO_DLADDR_MARSHAL=1 skips the layout conversion and copies the
 * native record verbatim, which is the deterministic half of the defect (the
 * caller then reads dli_fbase out of the high half of dli_fname). */
static int dladdr_marshal_disabled(void) {
   static int t = -1;
   if (t < 0) { t = getenv("M64_NO_DLADDR_MARSHAL") ? 1 : 0; }
   return t;
}

/* An i386 slot can hold a low-4GB pointer as-is; anything above needs the arena
 * handle, exactly as fnptr_lookup_result does for DATA. */
static uint32_t dladdr_slot(const void *p) {
   uint64_t v = (uint64_t)(uintptr_t)p;
   if (v == 0) { return 0; }
   if (v < 0x100000000ULL) { return (uint32_t)v; }
   return x64_objc_wrap(v);
}

int32_t shim_dladdr(uint32_t *a) {
   const void *addr = (const void *)(uintptr_t)a[0];
   uint32_t   *out  = a[1] ? (uint32_t *)(uintptr_t)a[1] : NULL;
   Dl_info info;
   memset(&info, 0, sizeof info);
   int r = dladdr(addr, &info);
   if (posix_trace()) {
      fprintf(stderr, "[posix] dladdr(%p, i386 Dl_info@0x%x) = %d  fname=%s\n",
              addr, a[1], r, (r && info.dli_fname) ? info.dli_fname : "(none)");
      fflush(stderr);
   }
   if (!out) { return (int32_t)r; }
   if (dladdr_marshal_disabled()) {
      memcpy(out, &info, sizeof info);       /* KILL SWITCH: unmarshalled */
      return (int32_t)r;
   }
   if (!r) { out[0] = out[1] = out[2] = out[3] = 0; return 0; }
   out[0] = info.dli_fname ? x64_objc_bounce_cstr(info.dli_fname) : 0;
   out[1] = dladdr_slot(info.dli_fbase);
   out[2] = info.dli_sname ? x64_objc_bounce_cstr(info.dli_sname) : 0;
   out[3] = dladdr_slot(info.dli_saddr);
   return (int32_t)r;
}

int32_t shim_dlclose(uint32_t *a) {
   void *h = dl_handle(a[0]);
   /* Trace the CLOSE as well as the open: for an app that decides to quit, the
    * FIRST dlclose is the start of teardown, so the last module opened before it
    * is how far startup actually got. (Portal 2 2026-09-13: the loaded-image list
    * at the hang was missing engine.dylib and materialsystem.dylib, which is what
    * revealed the process was in CAppSystemGroup::OnShutdown rather than parked in
    * startup.) */
   if (posix_trace()) {
      Dl_info info;
      const char *name = "(unknown)";
      if (h && dladdr(h, &info) && info.dli_fname) { name = info.dli_fname; }
      fprintf(stderr, "[posix] dlclose(%p)  %s\n", h, name);
      fflush(stderr);
   }
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

/* ★ The generic marshalling thunk fnptr_lookup_result mints is NOT enough for a
 * symbol libabiconv also INTERPOSES. The thunk lifts the i386 cdecl frame to the
 * x86_64 ABI and calls the native function — correct for the call — but it
 * returns whatever the native callee returned in %eax, i.e. TRUNCATED TO 32
 * BITS. For a native function that returns an OPAQUE >4GB POINTER the i386
 * caller must keep (FILE*, DIR*, iconv_t, …) that is fatal, and it is exactly
 * what libabiconv's own interpose exists to prevent: `_fopen` in file_shim.c
 * wraps the real FILE* in a low-4GB `struct shim_FILE`, so the 4-byte i386 slot
 * holds it losslessly and `resolve_file`/`is_shim` can unwrap it again.
 *
 * shim_dlsym and ns_symbol_resolve already prefer the interpose shim
 * ("__"+name -> nlist "___name") over a raw thunk; this CF twin was the one
 * member of the by-name lookup family that never got that step. Measured on
 * Halo CE (MacSoft 2.0.4), which has NO `_fopen` import at all and instead does
 *     g_fopen = CFBundleGetFunctionPointerForName(bundle, CFSTR("fopen"));
 *     ...  calll *g_fopen                                     (i386 0x29e72e)
 * then hands the result to the imported `_ftell`. Without this step the stored
 * handle is (uint32_t)(uintptr_t)<native FILE*>, so
 * `___ftell.l1 -> ftell -> resolve_file -> is_shim` dereferences fp+8 on an
 * address that is in no VM region at all: SIGSEGV, KERN_INVALID_ADDRESS.
 *
 * Universal — triggers on the structural property "libabiconv exports an
 * interpose shim for this name", never on an app. Kill switch
 * M64_NO_CFBUNDLE_FNPTR_SHIM=1 restores the raw-thunk behaviour. */
static int cfbundle_fnptr_shim_disabled(void) {
   static int t = -1;
   if (t < 0) { t = getenv("M64_NO_CFBUNDLE_FNPTR_SHIM") ? 1 : 0; }
   return t;
}

int32_t shim_CFBundleGetFunctionPointerForName(uint32_t *a) {
   CFBundleRef bundle = (CFBundleRef)(uintptr_t)_86x64_unwrap_obj_arg(a[0]);
   CFStringRef fname  = (CFStringRef)(uintptr_t)_86x64_unwrap_obj_arg(a[1]);
   char nm[256];
   nm[0] = '\0';
   if (fname) { CFStringGetCString(fname, nm, sizeof nm, kCFStringEncodingUTF8); }
   if (nm[0] && !cfbundle_fnptr_shim_disabled()) {
      uint32_t shimaddr = lookup_by_name_target(nm);
      if (shimaddr) {
         if (posix_trace()) {
            fprintf(stderr, "[posix] CFBundleGetFunctionPointerForName(\"%s\") -> "
                    "libabiconv interpose shim @0x%x\n", nm, shimaddr);
            fflush(stderr);
         }
         return (int32_t)shimaddr;
      }
   }
   void *fp = bundle ? CFBundleGetFunctionPointerForName(bundle, fname) : NULL;
   if (posix_trace()) {
      fprintf(stderr, "[posix] CFBundleGetFunctionPointerForName(\"%s\") = %p\n",
              nm[0] ? nm : "?", fp);
      fflush(stderr);
   }
   return fnptr_lookup_result((uint64_t)(uintptr_t)fp,
                              nm[0] ? nm : "CFBundleGetFunctionPointerForName");
}

/* ---- classic NeXT dyld NSSymbol lookup API --------------------------------
 * NSIsSymbolNameDefined / NSLookupAndBindSymbol / NSAddressOfSymbol (+
 * NSLookupSymbolInImage, NSNameOfSymbol): the pre-dlopen dynamic-loader API
 * old Mac ports use as their dlsym (the classic idiom builds "_"+name into a
 * stack buffer, probes NSIsSymbolNameDefined, then binds + takes the address).
 * abigen gives these no shim (modern <mach-o/dyld.h> marks them unavailable),
 * so the translated i386 `call NSIsSymbolNameDefined` reached NATIVE libdyld:
 * it read its arg from %rdi (garbage — the i386 caller put it on the stack)
 * and, fatally, returned with a 64-bit `ret` that over-popped the i386 4-byte
 * return slot, fusing the adjacent slot (arg0, a stack buffer pointer) into
 * the high 32 bits of the popped PC (Civ IV Steam: deterministic
 * EXC_BAD_ACCESS at 0x877ffaf0`09babb13 = &stackbuf<<32 | retaddr, the first
 * runtime symbol probe right after the embedded Python loads; same latent
 * defect in iPhoto/iWeb via NSLookupSymbolInImage and dbRepair via the trio).
 *
 * Exactly the shim_dlsym semantics apply: prefer libabiconv's own interpose
 * shim for the name (correct ABI + callback reverse-wrapping), else resolve
 * the native symbol and route it through fnptr_lookup_result so a >4GB native
 * FUNCTION comes back as a low-4GB i386-callable thunk. NSSymbol is opaque to
 * the caller; ours is a low-heap record (libabiconv's malloc is the low-4GB
 * shim heap) so the 4-byte i386 slot holds it losslessly, deduped per name so
 * repeated probes (GL-extension loops) don't grow without bound. */
#define NS_SYMBOL_MAGIC 0x4e53796dU              /* 'NSym' */
struct ns_symbol_rec {
   uint32_t magic;
   uint32_t addr;                /* i386-callable value (shim/thunk/handle) */
   char    *name;                /* low-heap copy, nlist spelling ("_puts") */
   struct ns_symbol_rec *next;
};
static struct ns_symbol_rec *g_ns_symbols = NULL;
static os_unfair_lock g_ns_symbols_lk = OS_UNFAIR_LOCK_INIT;

/* Resolve an nlist-spelled symbol name ("_puts") to an i386-callable value:
 * the libabiconv interpose shim if one exists, else the native symbol bridged
 * by fnptr_lookup_result. 0 = undefined. */
static uint32_t ns_symbol_resolve(const char *symbolName) {
   if (symbolName == NULL || symbolName[0] == '\0') { return 0; }
   const char *bare = (symbolName[0] == '_') ? symbolName + 1 : symbolName;
   uint32_t shimaddr = lookup_by_name_target(bare);
   if (shimaddr) { return shimaddr; }
   void *sym = dlsym(RTLD_DEFAULT, bare);
   if (sym == NULL) { return 0; }
   return (uint32_t)fnptr_lookup_result((uint64_t)(uintptr_t)sym, bare);
}

/* Bind symbolName into a (deduped) NSSymbol record; 0 if undefined. */
static int32_t ns_symbol_bind(const char *symbolName, const char *api) {
   if (symbolName == NULL || symbolName[0] == '\0') { return 0; }
   os_unfair_lock_lock(&g_ns_symbols_lk);
   for (struct ns_symbol_rec *r = g_ns_symbols; r != NULL; r = r->next) {
      if (strcmp(r->name, symbolName) == 0) {
         os_unfair_lock_unlock(&g_ns_symbols_lk);
         return (int32_t)(uint32_t)(uintptr_t)r;
      }
   }
   os_unfair_lock_unlock(&g_ns_symbols_lk);

   uint32_t addr = ns_symbol_resolve(symbolName);   /* outside the lock: may
                                                     * dlopen/mint a thunk */
   if (addr == 0) {
      if (posix_trace()) {
         fprintf(stderr, "[posix] %s(\"%s\") -> undefined\n", api, symbolName);
         fflush(stderr);
      }
      return 0;
   }
   struct ns_symbol_rec *rec = malloc(sizeof *rec);  /* low-4GB shim heap */
   size_t len = strlen(symbolName) + 1;
   char *copy = malloc(len);                         /* NOT strdup: that would
                                                      * allocate from libc's
                                                      * high heap */
   if (rec == NULL || copy == NULL ||
       ((uintptr_t)rec >> 32) || ((uintptr_t)copy >> 32)) {
      free(rec); free(copy);
      return 0;
   }
   memcpy(copy, symbolName, len);
   rec->magic = NS_SYMBOL_MAGIC;
   rec->addr  = addr;
   rec->name  = copy;
   os_unfair_lock_lock(&g_ns_symbols_lk);
   for (struct ns_symbol_rec *r = g_ns_symbols; r != NULL; r = r->next) {
      if (strcmp(r->name, symbolName) == 0) {        /* lost a bind race */
         os_unfair_lock_unlock(&g_ns_symbols_lk);
         free(copy); free(rec);
         return (int32_t)(uint32_t)(uintptr_t)r;
      }
   }
   rec->next = g_ns_symbols;
   g_ns_symbols = rec;
   os_unfair_lock_unlock(&g_ns_symbols_lk);
   if (posix_trace()) {
      fprintf(stderr, "[posix] %s(\"%s\") -> NSSymbol %p addr=0x%x\n",
              api, symbolName, (void *)rec, addr);
      fflush(stderr);
   }
   return (int32_t)(uint32_t)(uintptr_t)rec;
}

int32_t shim_NSIsSymbolNameDefined(uint32_t *a) {
   const char *symbolName = a[0] ? (const char *)(uintptr_t)a[0] : NULL;
   if (symbolName == NULL || symbolName[0] == '\0') { return 0; }
   const char *bare = (symbolName[0] == '_') ? symbolName + 1 : symbolName;
   /* Existence probe only — never mints a thunk (extension probe loops would
    * drain the pool on symbols the caller never binds). */
   int defined = dlsym_shim_for(bare) != 0 ||
                 dlsym(RTLD_DEFAULT, bare) != NULL;
   if (posix_trace()) {
      fprintf(stderr, "[posix] NSIsSymbolNameDefined(\"%s\") = %d\n",
              symbolName, defined);
      fflush(stderr);
   }
   return defined;
}

int32_t shim_NSLookupAndBindSymbol(uint32_t *a) {
   const char *symbolName = a[0] ? (const char *)(uintptr_t)a[0] : NULL;
   return ns_symbol_bind(symbolName, "NSLookupAndBindSymbol");
}

/* NSLookupSymbolInImage(mh, symbolName, options): resolve in the GLOBAL
 * namespace rather than scoping to `mh` — the image arg is a translated-side
 * mach_header the shim layer can't hand to any modern per-image lookup, and
 * the callers of record (iPhoto/iWeb probing their own frameworks) want the
 * same shim-first routing anyway. Options (BIND_NOW/RETURN_ON_ERROR) don't
 * change anything for an eager, non-aborting resolver. */
int32_t shim_NSLookupSymbolInImage(uint32_t *a) {
   const char *symbolName = a[1] ? (const char *)(uintptr_t)a[1] : NULL;
   return ns_symbol_bind(symbolName, "NSLookupSymbolInImage");
}

int32_t shim_NSAddressOfSymbol(uint32_t *a) {
   const struct ns_symbol_rec *rec =
      (const struct ns_symbol_rec *)(uintptr_t)a[0];
   if (rec == NULL || rec->magic != NS_SYMBOL_MAGIC) { return 0; }
   return (int32_t)rec->addr;
}

int32_t shim_NSNameOfSymbol(uint32_t *a) {
   const struct ns_symbol_rec *rec =
      (const struct ns_symbol_rec *)(uintptr_t)a[0];
   if (rec == NULL || rec->magic != NS_SYMBOL_MAGIC) { return 0; }
   return (int32_t)(uint32_t)(uintptr_t)rec->name;
}

/* ---- fortify _chk family (mem/str) ----------------------------------------
 * __memcpy_chk / __memset_chk / __strcat_chk / ... — the checked variants
 * clang emits for _FORTIFY_SOURCE builds (default since 10.6, so most late
 * i386 binaries import them). Modern headers expose them only as compiler
 * builtins (<secure/_string.h>), so abigen has no prototype to shim from and
 * translated code reached NATIVE libsystem_c: it read its args from
 * registers (garbage — the i386 caller put them on the stack; observed
 * __strcat_chk faulting in _platform_strlen on rdi = the dstlen arg) and its
 * 64-bit `ret` over-popped the i386 4-byte return slot -> fused-PC crash.
 * Civ IV (Steam) binds __memcpy_chk/__memset_chk/__strcat_chk/__strcpy_chk/
 * __strncat_chk natively; the vendored Python 2.6 adds __memmove_chk/
 * __sprintf_chk/__strncpy_chk. Implemented directly (not forwarded to the
 * native _chk entries) so the i386 "unknown size" sentinel 0xffffffff maps
 * to no-check rather than a bogus 4GB bound. All pointers are low-4GB i386
 * buffers and pass straight through. */
static void chk_fail(const char *fn) {
   fprintf(stderr, "[posix] %s: buffer overflow detected (fortify)\n", fn);
   fflush(stderr);
   abort();
}
#define CHK_KNOWN(dstlen) ((dstlen) != 0xffffffffU)

int32_t shim_memcpy_chk(uint32_t *a) {     /* (dst, src, n, dstlen) -> dst */
   if (CHK_KNOWN(a[3]) && a[2] > a[3]) { chk_fail("__memcpy_chk"); }
   memcpy((void *)(uintptr_t)a[0], (const void *)(uintptr_t)a[1], a[2]);
   return (int32_t)a[0];
}

int32_t shim_memmove_chk(uint32_t *a) {    /* (dst, src, n, dstlen) -> dst */
   if (CHK_KNOWN(a[3]) && a[2] > a[3]) { chk_fail("__memmove_chk"); }
   memmove((void *)(uintptr_t)a[0], (const void *)(uintptr_t)a[1], a[2]);
   return (int32_t)a[0];
}

int32_t shim_memset_chk(uint32_t *a) {     /* (b, c, n, dstlen) -> b */
   if (CHK_KNOWN(a[3]) && a[2] > a[3]) { chk_fail("__memset_chk"); }
   memset((void *)(uintptr_t)a[0], (int)a[1], a[2]);
   return (int32_t)a[0];
}

int32_t shim_strcpy_chk(uint32_t *a) {     /* (dst, src, dstlen) -> dst */
   const char *src = (const char *)(uintptr_t)a[1];
   if (CHK_KNOWN(a[2]) && strlen(src) + 1 > a[2]) { chk_fail("__strcpy_chk"); }
   strcpy((char *)(uintptr_t)a[0], src);
   return (int32_t)a[0];
}

int32_t shim_stpcpy_chk(uint32_t *a) {     /* (dst, src, dstlen) -> dst+len */
   const char *src = (const char *)(uintptr_t)a[1];
   size_t len = strlen(src);
   if (CHK_KNOWN(a[2]) && len + 1 > a[2]) { chk_fail("__stpcpy_chk"); }
   memcpy((char *)(uintptr_t)a[0], src, len + 1);
   return (int32_t)(a[0] + (uint32_t)len);
}

int32_t shim_strncpy_chk(uint32_t *a) {    /* (dst, src, n, dstlen) -> dst */
   if (CHK_KNOWN(a[3]) && a[2] > a[3]) { chk_fail("__strncpy_chk"); }
   strncpy((char *)(uintptr_t)a[0], (const char *)(uintptr_t)a[1], a[2]);
   return (int32_t)a[0];
}

int32_t shim_strcat_chk(uint32_t *a) {     /* (dst, src, dstlen) -> dst */
   char       *dst = (char *)(uintptr_t)a[0];
   const char *src = (const char *)(uintptr_t)a[1];
   if (CHK_KNOWN(a[2]) && strlen(dst) + strlen(src) + 1 > a[2]) {
      chk_fail("__strcat_chk");
   }
   strcat(dst, src);
   return (int32_t)a[0];
}

int32_t shim_strncat_chk(uint32_t *a) {    /* (dst, src, n, dstlen) -> dst */
   char       *dst = (char *)(uintptr_t)a[0];
   const char *src = (const char *)(uintptr_t)a[1];
   if (CHK_KNOWN(a[3])) {
      size_t sl = strlen(src);
      size_t add = sl < a[2] ? sl : a[2];
      if (strlen(dst) + add + 1 > a[3]) { chk_fail("__strncat_chk"); }
   }
   strncat(dst, src, a[2]);
   return (int32_t)a[0];
}

/* libgcc __popcountsi2(int) — libSystem re-exports compiler-rt, so the bind
 * resolves and the native 64-bit ret over-pops just like any other unshimmed
 * C entry (Civ IV binds it). */
int32_t shim_popcountsi2(uint32_t *a) {
   return (int32_t)__builtin_popcount(a[0]);
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
