;
; sigaction.asm — i386-cdecl -> C trampoline for sigaction (sigaction_shim.c).
; The i386 struct sigaction is {handler, sa_mask, sa_flags} (12 bytes), and the
; handler is i386 code: the shim wraps it in a native callback trampoline.
;
   segment .text
   extern __dyld_stub_binder_flag
   global ___sigaction
   extern _shim_sigaction

___sigaction:
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
   and rsp, ~0xf
   lea rdi, [rbp + 12]          ; &args[0]
   call _shim_sigaction         ; eax = int result
   lea rsp, [rbp - 0x10]
   pop rsi
   pop rdi
   leave
   mov r11d, [rsp]              ; i386 4-byte return address
   add rsp, 4
   jmp r11
