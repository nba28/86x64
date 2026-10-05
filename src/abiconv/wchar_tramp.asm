;; wchar_tramp.asm — i386-cdecl -> C trampolines for the <wchar.h> wide-string
;; family hand shims (wchar_shim.c). static-interpose renames the translated
;; app's `_wcs*` imports to `___wcs*`; these provide them (a PRESENT native
;; surface abigen's modern pass did not shim -> jt-weak-import `ud2` when called;
;; Civ IV `wcslen`). wchar_t is 4 bytes on both arches, so the only marshalling
;; is pointer widening (done in wchar_shim.c) plus the i386 return-register
;; convention (here).
;;
;; Frame + lazy-stub-binder dance identical to maptable_tramp.asm's MTSHIM: after
;; `push rbp; mov rbp,rsp; push rdi; push rsi` the first i386 arg slot is
;; [rbp+12] and the 4-byte i386 return address is at [rbp+8] (its low half lands
;; at [rsp] after `leave`). i386 esi/edi = x86_64 rsi/rdi are callee-saved, so
;; preserved around the SysV call.

   segment .text
   extern __dyld_stub_binder_flag

%macro WSTUB 0
   cmp qword [rel __dyld_stub_binder_flag], 0
   je %%l1
   mov rsp, qword [rel __dyld_stub_binder_flag]
   add rsp, 16
   mov qword [rel __dyld_stub_binder_flag], 0
%%l1:
%endmacro

;; result in eax (int / size_t / wchar_t* — 32-bit-representable in low-4GB).
%macro WCHAR_MTSHIM 2           ; %1 = export, %2 = C impl (uint32 -> eax)
   global %1
   extern %2
%1:
   WSTUB
   push rbp
   mov rbp, rsp
   push rdi
   push rsi
   and rsp, ~0xf
   lea rdi, [rbp + 12]          ; &args[0]
   call %2
   lea rsp, [rbp - 0x10]
   pop rsi
   pop rdi
   leave
   mov r11d, [rsp]              ; 4-byte i386 return address
   add rsp, 4
   jmp r11
%endmacro

;; 64-bit result in edx:eax (wcstoll). C impl returns uint64 in rax.
%macro WCHAR_MTSHIM64 2
   global %1
   extern %2
%1:
   WSTUB
   push rbp
   mov rbp, rsp
   push rdi
   push rsi
   and rsp, ~0xf
   lea rdi, [rbp + 12]
   call %2                      ; rax = uint64 result
   mov rcx, rax                 ; survive the register restore
   lea rsp, [rbp - 0x10]
   pop rsi
   pop rdi
   leave
   mov eax, ecx                 ; low 32
   shr rcx, 32
   mov edx, ecx                 ; high 32
   mov r11d, [rsp]
   add rsp, 4
   jmp r11
%endmacro

;; floating result in st0 (wcstod/wcstof, i386 cdecl FP return). C impl returns
;; the value in xmm0; fld it onto the x87 stack (integer epilogue leaves x87
;; untouched, so st0 survives to the i386 caller).
%macro WCHAR_MTSHIM_FP 2
   global %1
   extern %2
%1:
   WSTUB
   push rbp
   mov rbp, rsp
   push rdi
   push rsi
   and rsp, ~0xf
   lea rdi, [rbp + 12]
   call %2                      ; xmm0 = double result
   sub rsp, 16
   movsd [rsp], xmm0
   fld qword [rsp]              ; st0 = result
   add rsp, 16
   lea rsp, [rbp - 0x10]
   pop rsi
   pop rdi
   leave
   mov r11d, [rsp]
   add rsp, 4
   jmp r11                      ; st0 holds the fp result
%endmacro

   WCHAR_MTSHIM    ___wcslen,   _shim_wcslen
   WCHAR_MTSHIM    ___wcscmp,   _shim_wcscmp
   WCHAR_MTSHIM    ___wcsncmp,  _shim_wcsncmp
   WCHAR_MTSHIM    ___wcscpy,   _shim_wcscpy
   WCHAR_MTSHIM    ___wcsncpy,  _shim_wcsncpy
   WCHAR_MTSHIM    ___wcscat,   _shim_wcscat
   WCHAR_MTSHIM    ___wcschr,   _shim_wcschr
   WCHAR_MTSHIM    ___wcsrchr,  _shim_wcsrchr
   WCHAR_MTSHIM    ___wcsstr,   _shim_wcsstr
   WCHAR_MTSHIM    ___wcstok,   _shim_wcstok
   WCHAR_MTSHIM    ___wcstol,   _shim_wcstol
   WCHAR_MTSHIM    ___wcsftime, _shim_wcsftime
   WCHAR_MTSHIM    ___mbrtowc,  _shim_mbrtowc
   WCHAR_MTSHIM    ___wcrtomb,  _shim_wcrtomb
   WCHAR_MTSHIM64  ___wcstoll,  _shim_wcstoll
   WCHAR_MTSHIM_FP ___wcstod,   _shim_wcstod
   WCHAR_MTSHIM_FP ___wcstof,   _shim_wcstof
