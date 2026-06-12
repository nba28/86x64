;;; test32.asm — i386 test program that exercises 86x64 transform features.
;;;
;;; Patterns exercised:
;;;   - Variadic libc call (printf)
;;;   - Multi-arg syscall (write via libc) 
;;;   - Local stack frame with allocas
;;;   - Function pointer table in __DATA (rebase test)
;;;   - Internal data references (cstring address)
;;;   - Jump table from switch (disassembler robustness — bytes between
;;;     functions interleaved with data)
;;;   - Indirect call via memory
;;;   - Recursive call (stack growth)
;;;
;;; Build: nasm -f macho32 test32.asm -o test32.o
;;;        ld -arch i386 -macos_version_min 10.6 -no_pie -o test32 test32.o <i386_libSystem.dylib>
;;;        (we synthesize a libSystem stub since modern SDK lacks i386 slice)
   bits 32

   section .text
   global _main

   extern _printf
   extern _puts
   extern _strlen
   extern _getpid
   extern _exit

;;; ---- helper: 32-bit add and multiply, for the function-pointer-array test ----
_add:
   push ebp
   mov ebp, esp
   mov eax, [ebp+8]
   add eax, [ebp+12]
   pop ebp
   ret

_mul:
   push ebp
   mov ebp, esp
   mov eax, [ebp+8]
   imul eax, [ebp+12]
   pop ebp
   ret

;;; ---- recursive fibonacci — exercises call/ret translation + stack ----
_fib:
   push ebp
   mov ebp, esp
   sub esp, 8
   mov eax, [ebp+8]
   cmp eax, 1
   jbe .base
   ;; fib(n-1)
   mov eax, [ebp+8]
   dec eax
   mov [esp], eax
   call _fib
   mov [ebp-4], eax
   ;; fib(n-2)
   mov eax, [ebp+8]
   sub eax, 2
   mov [esp], eax
   call _fib
   add eax, [ebp-4]
   leave
   ret
.base:
   leave
   ret

;;; ---- switch-like jump table — branches via computed jmp ----
;;; Forces the disassembler to navigate a table of code addresses
_describe:
   push ebp
   mov ebp, esp
   mov eax, [ebp+8]
   cmp eax, 4
   ja .default
   ;; jmp [_describe_table + eax*4]
   jmp dword [_describe_table + eax*4]

.case0:
   push dword .str_zero
   call _puts
   add esp, 4
   jmp .done
.case1:
   push dword .str_one
   call _puts
   add esp, 4
   jmp .done
.case2:
   push dword .str_two
   call _puts
   add esp, 4
   jmp .done
.case3:
   push dword .str_three
   call _puts
   add esp, 4
   jmp .done
.case4:
   push dword .str_four
   call _puts
   add esp, 4
   jmp .done
.default:
   push eax
   push dword .str_default
   call _printf
   add esp, 8
.done:
   leave
   ret

   section __DATA,__data
   global _describe_table
_describe_table:
   dd _describe.case0
   dd _describe.case1
   dd _describe.case2
   dd _describe.case3
   dd _describe.case4

;;; Function pointer array (rebase test)
   global _ops
_ops:
   dd _add
   dd _mul

   section __TEXT,__cstring
_describe.str_zero:    db "zero",0
_describe.str_one:     db "one",0
_describe.str_two:     db "two",0
_describe.str_three:   db "three",0
_describe.str_four:    db "four",0
_describe.str_default: db "default (%d)",10,0

_hello_fmt:    db "Hello from i386, pid=%d argc=%d",10,0
_argv_fmt:     db "  argv[%d] = %s",10,0
_op_fmt:       db "ops[%d](%d, %d) = %d",10,0
_fib_fmt:      db "fib(%d) = %d",10,0
_done_msg:     db "test32 done",0

   section .text

;;; ---- main(int argc, char **argv) ----
_main:
   push ebp
   mov ebp, esp
   sub esp, 16

   ;; printf("Hello ... pid=%d argc=%d\n", getpid(), argc);
   call _getpid
   mov edi, eax
   push dword [ebp+8]            ; argc
   push edi                      ; pid
   push dword _hello_fmt
   call _printf
   add esp, 12

   ;; print each argv[i]
   xor ebx, ebx
.argv_loop:
   mov eax, [ebp+8]
   cmp ebx, eax
   jae .argv_done
   mov ecx, [ebp+12]
   mov edx, [ecx + ebx*4]
   push edx
   push ebx
   push dword _argv_fmt
   call _printf
   add esp, 12
   inc ebx
   jmp .argv_loop
.argv_done:

   ;; ops[0](3, 4) and ops[1](3, 4)
   ;; printf via _op_fmt for each
   mov esi, 0
.op_loop:
   cmp esi, 2
   jae .op_done
   ;; call ops[esi] with (3, 4)
   push dword 4
   push dword 3
   call dword [_ops + esi*4]
   add esp, 8
   ;; printf("ops[%d](3,4) = %d\n", esi, eax)
   push eax
   push dword 4
   push dword 3
   push esi
   push dword _op_fmt
   call _printf
   add esp, 20
   inc esi
   jmp .op_loop
.op_done:

   ;; fib(10) via recursion
   push dword 10
   call _fib
   add esp, 4
   push eax
   push dword 10
   push dword _fib_fmt
   call _printf
   add esp, 12

   ;; describe each of 0..5 (exercises jump table)
   mov ebx, 0
.desc_loop:
   cmp ebx, 6
   jae .desc_done
   push ebx
   call _describe
   add esp, 4
   inc ebx
   jmp .desc_loop
.desc_done:

   push dword _done_msg
   call _puts
   add esp, 4

   xor eax, eax
   leave
   ret
