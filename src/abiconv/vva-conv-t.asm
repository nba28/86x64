   ;; vva-conv-t.asm — trampoline template for the v-printf family
   ;; (vsnprintf/vsprintf/vfprintf/vprintf/vasprintf/__vsnprintf_chk).
   ;;
   ;; Unlike the inline-varargs printf trampoline (vararg-conv-t.asm), the v*
   ;; functions take a va_list, so there is nothing to distribute into the SysV
   ;; argument registers here. Instead we hand the whole i386 cdecl stack-arg
   ;; block to a C shim (_<SYMBOL>_vshim) that extracts the fixed args, rebuilds
   ;; a proper x86_64 va_list from the i386 one, calls the native v* function,
   ;; and returns the int result in eax. We just forward eax to the i386 caller.

   segment .text
   global ___%+SYMBOL
   extern _%+SYMBOL%+_vshim
   extern __dyld_stub_binder_flag

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

   and rsp, ~0xf

   lea rdi, [rbp + 12]          ; const uint32_t *args32 (i386 stack args)
   call _%+SYMBOL%+_vshim       ; eax = int result

   lea rsp, [rbp - 16]
   pop rsi
   pop rdi

   leave

   mov r11d, dword [rsp]        ; i386 cdecl: pop 4-byte return address
   add rsp, 4
   jmp r11
