;; dyld_func_lookup.asm — neutralize the classic __DATA,__dyld crt bootstrap.
;;
;; THE BUG (see the Halo target notes). Pre-10.5 i386 binaries (Halo CE 2.0.4, Civ IV)
;; bootstrap from crt1.o's __start through the classic __DATA,__dyld section: two
;; 4-byte slots, [+0]=lazy-symbol-binder and [+4]=dyld_func_lookup. The crt builds
;; an i386 cdecl frame and tail-jumps through the func_lookup slot to ask dyld for
;; __dyld_make_delayed_module_initializer_calls / __dyld_mod_term_funcs:
;;
;;     ; i386 cdecl frame at the lookup site:
;;     ;   [rsp+0] = 4-byte return address (into the crt)
;;     ;   [rsp+4] = name    (const char *, i386 4-byte ptr)
;;     ;   [rsp+8] = address (void **,      i386 4-byte ptr — &local fp slot)
;;     movl  (__dyld+4),%eax        ; eax = func_lookup slot (32-bit read)
;;     jmpq  *%rax                  ; tail-jump to func_lookup
;;
;; For a legacy image dyld points the slot at native
;; legacyDyldLookup4OldBinaries(const char *name, void **address), which reads its
;; args from rdi/rsi (x86_64 ABI) — but the crt passed them on the STACK (i386
;; cdecl), so rdi/rsi are garbage. It then does `*address = fp` through the garbage
;; `address` -> SIGSEGV writing to 0x0. (Crash backtrace: findAndRunAllInitializers
;; -> objc_slide_init -> add-image callback -> abiconv_init_trampoline -> crt ->
;; legacyDyldLookup4OldBinaries -> NULL write.)
;;
;; THE FIX. objc_slide.c (patch_dyld_section) overwrites the two slots so the crt's
;; func_lookup tail-jump lands HERE instead of in dyld's native handler. These
;; shims honor the i386 cdecl stack ABI and return harmlessly: our runtime
;; (wrapper + slide_objc + ABICONV_RUN_INITS) already performs the legacy bootstrap
;; the crt was asking dyld to do, so neutralizing the lookup is correct, not a
;; regression. Universal: triggers on the structural presence of __DATA,__dyld in
;; any translated image, not on an app name.
;;
;; libabiconv is guaranteed to map <4GB (objc_slide.c verifies before patching), so
;; these shims' addresses fit the crt's 32-bit `movl (slot),%eax` / `jmpq *%rax`.

   segment .text
   global __86x64_dyld_func_lookup
   global __86x64_dyld_noop

;; void _86x64_dyld_noop(void)   [i386 cdecl callee]
;; The fp the crt receives from the func-lookup (we make it point here). When the
;; crt invokes it, pop the i386 4-byte return address and jump back to the i386
;; caller — a native `ret` (0xC3) would pop 8 bytes and over-pop the i386 frame,
;; fusing the return addr with an adjacent stack word into a wild PC. Args (if any)
;; are caller-cleaned in cdecl, so we leave them.
__86x64_dyld_noop:
   mov r11d, [rsp]                 ; load i386 4-byte return address
   lea rsp, [rsp + 4]              ; pop it
   jmp r11                         ; return to the i386 caller

;; void _86x64_dyld_func_lookup(const char *name, void **address)  [i386 cdecl]
;; Entered via the crt's `movl (__dyld+4),%eax ; jmpq *%rax` tail-jump, so the
;; i386 cdecl args are on the stack (see the layout in the header comment). We
;; emulate dyld's func-lookup out-parameter: write the address of our no-op into
;; *address (and return it in eax for crt variants that use the return value), so
;; the crt's subsequent `jmpq *fp` lands in _86x64_dyld_noop. We service EVERY name
;; with the no-op because our runtime owns init/term; reading `name` from [rsp+4]
;; to special-case the __dyld_* helpers is unnecessary.
__86x64_dyld_func_lookup:
   mov eax, [rsp + 8]              ; eax = address (zero-extended; i386 ptr <4GB)
   lea r11, [rel __86x64_dyld_noop]; r11 = &noop (libabiconv <4GB => r11d == full)
   mov dword [rax], r11d           ; *address = (uint32_t)&noop
   mov eax, r11d                   ; also return &noop (return-value-using crts)
   ;; i386 cdecl return: pop the 4-byte return address, jump back to the crt.
   mov r11d, [rsp]
   lea rsp, [rsp + 4]
   jmp r11
