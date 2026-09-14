   segment .text
   global ___sigaction
   extern _sigaction
   extern __dyld_stub_binder_flag

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

   ;; ARGS
   movsxd rdi, dword [rbp + 12]         ; int sig
   
   mov esi, dword [rbp + 16]    ; const struct sigaction *act (i386 ptr, may be NULL)
   sub rsp, 16                          ; sizeof(struct sigaction) -- reserved unconditionally
                                        ; so the stack layout (and call alignment) is identical
                                        ; whether or not act is NULL.
   test esi, esi                        ; act == NULL ? (POSIX: query-only, do not deref)
   jz .act_null                         ; -> leave rsi = 0 (NULL) for native sigaction
   mov eax, [rsi + 0]
   mov [rsp + 0], rax           ; union __sigaction_u
   mov eax, [rsi + 4]
   mov [rsp + 8], eax           ; sigset_t sa_mask
   mov eax, [rsi + 8]
   mov [rsp + 12], eax          ; int sa_flags
   mov rsi, rsp
.act_null:

   sub rsp, 16      
   mov rdx, rsp                 ; struct sigaction *oact

   call _sigaction

   ;; convert back struct
   mov edi, [rbp + 20]          ; struct sigaction *oact
   or edi, edi
   jz .l2
   mov rdx, [rsp + 0]
   mov [rdi + 0], edx
   mov edx, [rsp + 8]
   mov [rdi + 4], edx
   mov edx, [rsp + 12]
   mov [rdi + 8], edx
.l2:
   lea rsp, [rbp - 0x10]
   
   pop rsi
   pop rdi

   leave
   mov r11d, [rsp]
   add rsp, 4                   ; POP the i386 4-byte return address; without
                                ; this the caller's esp stays 4 low forever
   jmp r11
