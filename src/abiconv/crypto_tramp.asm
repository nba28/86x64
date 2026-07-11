;; crypto_tramp.asm — i386-cdecl -> C trampolines for the OpenSSL 0.9.7 CD-key
;; verification surface hand-shimmed in crypto_shim.c. static-interpose renames
;; the translated app's `_CRYPTO_set_mem_functions` / `_RSA_*` / `_RIPEMD160*` /
;; `_BN_bin2bn` imports to `___...`; these provide them.
;;
;; WHY THIS EXISTS: those symbols resolve to the LIVE native libcrypto.0.9.7 in
;; the shared cache, so abigen left them as raw weak binds. Calling native from
;; i386-translated code without a frame fixup lets the native 8-byte `ret`
;; over-pop the i386 4-byte cdecl return frame -> fused PC crash (Halo
;; Halo-2026-07-11-233750.ips, EXC_BAD_ACCESS at 0x02086cb00203564a, the fault
;; was _CRYPTO_set_mem_functions, the first crypto call).
;;
;; Frame + lazy-stub-binder dance identical to maptable_tramp.asm / wchar_tramp.asm
;; MTSHIM: after `push rbp; mov rbp,rsp; push rdi; push rsi` the first i386 arg
;; slot is [rbp+12] and the 4-byte i386 return address lands at [rsp] after
;; `leave`. i386 esi/edi = x86_64 rsi/rdi are callee-saved, so preserved around
;; the SysV call. Every crypto shim returns a 32-bit value in eax (int / a
;; low-4GB pointer/token), so one MTSHIM variant covers all of them.

   segment .text
   extern __dyld_stub_binder_flag

%macro CRYPTO_MTSHIM 2          ; %1 = export symbol, %2 = C impl (uint32 -> eax)
   global %1
   extern %2
%1:
   cmp qword [rel __dyld_stub_binder_flag], 0
   je %%l1
   mov rsp, qword [rel __dyld_stub_binder_flag]
   add rsp, 16
   mov qword [rel __dyld_stub_binder_flag], 0
%%l1:
   push rbp
   mov rbp, rsp
   push rdi
   push rsi
   and rsp, ~0xf
   lea rdi, [rbp + 12]          ; &args[0]  (first i386 cdecl slot)
   call %2                      ; eax = i386 return value
   lea rsp, [rbp - 0x10]
   pop rsi
   pop rdi
   leave
   mov r11d, [rsp]              ; 4-byte i386 return address
   add rsp, 4
   jmp r11
%endmacro

   CRYPTO_MTSHIM ___CRYPTO_set_mem_functions, _shim_CRYPTO_set_mem_functions
   CRYPTO_MTSHIM ___RIPEMD160_Init,           _shim_RIPEMD160_Init
   CRYPTO_MTSHIM ___RIPEMD160_Update,         _shim_RIPEMD160_Update
   CRYPTO_MTSHIM ___RIPEMD160_Final,          _shim_RIPEMD160_Final
   CRYPTO_MTSHIM ___RIPEMD160,                _shim_RIPEMD160
   CRYPTO_MTSHIM ___BN_bin2bn,                _shim_BN_bin2bn
   CRYPTO_MTSHIM ___RSA_new,                  _shim_RSA_new
   CRYPTO_MTSHIM ___RSA_verify,               _shim_RSA_verify
   CRYPTO_MTSHIM ___RSA_free,                 _shim_RSA_free
