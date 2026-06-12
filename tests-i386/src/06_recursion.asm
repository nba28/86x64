;;; 06_recursion — deep recursion exercises the i386 call/ret thunk under
;;; sustained stack growth. fib(10)=55.
;;;
;;; Exercises: recursive `call` (the translator's `lea-rip; push r11d; jmp`
;;; thunk pattern) under stack pressure, eax return preservation across
;;; many calls, conditional branches.
   bits 32
   global _main
   extern _printf
   extern _exit

   section __TEXT,__cstring
_fmt: db "fib(10)=%d",10,0

   section __TEXT,__text
_fib:
   push ebp
   mov  ebp, esp
   sub  esp, 8
   mov  eax, [ebp+8]
   cmp  eax, 1
   jbe  .base
   ;; fib(n-1)
   mov  eax, [ebp+8]
   dec  eax
   mov  [esp], eax
   call _fib
   mov  [ebp-4], eax
   ;; fib(n-2)
   mov  eax, [ebp+8]
   sub  eax, 2
   mov  [esp], eax
   call _fib
   add  eax, [ebp-4]
   leave
   ret
.base:
   leave
   ret

_main:
   push ebp
   mov  ebp, esp
   sub  esp, 16
   mov  dword [esp], 10
   call _fib
   mov  [esp+4], eax
   mov  dword [esp], _fmt
   call _printf
   mov  dword [esp], 0
   call _exit
   ud2
