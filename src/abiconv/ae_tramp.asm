;; ae_tramp.asm — i386-cdecl -> C trampolines for the Apple Event Manager
;; native-shadow bridge (ae_shim.c). static-interpose renames the translated
;; app's `_AEGetParamPtr` / `_AEInstallEventHandler` imports to ___AEGetParamPtr
;; / ___AEInstallEventHandler; these provide them (custom.syms lists the base
;; names so abigen's modern/legacy passes never dup-emit a colliding global).
;;
;; Identical MTSHIM frame + lazy-stub-binder dance as wchar_tramp.asm: after
;; `push rbp; mov rbp,rsp; push rdi; push rsi` the first i386 arg is [rbp+12] and
;; the 4-byte i386 return address is at [rbp+8] (its low half at [rsp] after
;; `leave`). The C impl takes &args[0] in rdi and returns the OSErr in eax
;; (sign-extended by the impl). i386 esi/edi = callee-saved x86_64 rsi/rdi are
;; preserved around the SysV call.

   segment .text
   extern __dyld_stub_binder_flag

%macro AE_STUB 0
   cmp qword [rel __dyld_stub_binder_flag], 0
   je %%l1
   mov rsp, qword [rel __dyld_stub_binder_flag]
   add rsp, 16
   mov qword [rel __dyld_stub_binder_flag], 0
%%l1:
%endmacro

;; result in eax (OSErr, 32-bit-representable).  %1 = export, %2 = C impl.
%macro AE_MTSHIM 2
   global %1
   extern %2
%1:
   AE_STUB
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

   AE_MTSHIM ___AEGetParamPtr,         _shim_AEGetParamPtr
   AE_MTSHIM ___AEInstallEventHandler, _shim_AEInstallEventHandler
