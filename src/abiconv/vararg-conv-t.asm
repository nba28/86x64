   segment .text
   global ___%+SYMBOL
   extern _%+SYMBOL%+_conversion_f
   extern _%+SYMBOL
   extern __dyld_stub_binder_flag

;; Slot capacity, shared with printf-conv.cc VARARG_SLOTS. It was 16: an
;; 18-argument sscanf (Portal 2 materialsystem CMaterialSubRect::ParseMaterialVars,
;; a 16-float matrix) wrote args 17/18 past args64 into argtypes and their type
;; tags (Q = 3) past argtypes onto the saved rsi below -> the caller's %esi came
;; back as 3 (KeyValues* = 3 -> SIGSEGV at 0x15). Guard sscanf-many-args.
%define ARGS64_COUNT 64
%define  ARGS64_SIZE (ARGS64_COUNT * 8)

___%+SYMBOL:
   cmp qword [rel __dyld_stub_binder_flag], 0
   je .l1
   mov rsp, qword [rel __dyld_stub_binder_flag]
   add rsp, 16
   mov qword [rel __dyld_stub_binder_flag], 0
.l1:
   push rbp
   mov rbp, rsp
   
   push rdi
   push rsi
   
   ;; sizeof(rdi, rsi, rdx, rcx, r8, r9) == 8 * 6 = 42, 16-byte aligned

   and rsp, ~0xf
   sub rsp, ARGS64_COUNT * 4
   mov rdx, rsp                 ; reg_width_t argtypes[]
   sub rsp, ARGS64_SIZE
   mov rsi, rsp                 ; void *args64
   lea rdi, [rbp + 12]          ; const void *args32

   call _%+SYMBOL%+_conversion_f
   ;; eax -- arg count
   pop rdi
   pop rsi
   pop rdx
   pop rcx
   pop r8
   pop r9

   xor eax, eax

   call _%+SYMBOL

   lea rsp, [rbp - 16]
   pop rsi
   pop rdi

   leave

   mov r11d, dword [rsp]
   add rsp, 4
   jmp r11
