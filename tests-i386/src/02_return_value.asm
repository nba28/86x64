;;; 02_return_value — the photocd output.c:98 pattern.
;;;
;;; A callee returns a fixed value in eax. The caller does
;;;   call _callee
;;;   dec  eax
;;;   je   .ok
;;; mirroring photocd's `if (foo() != 1) errfun(98);` shape. If the translated
;;; call thunk fails to preserve eax across the i386-call-simulation
;;; (`leaq r11,[rip+d]; push r11 (low 32); jmp callee`), eax on return is
;;; wrong and the test prints "BAD" instead of "OK".
;;;
;;; Exercises: i386 call thunk, eax-as-return preservation, conditional
;;; branch after call.
   bits 32
   global _main
   extern _printf
   extern _exit

   section __TEXT,__cstring
_ok_msg:  db "OK",10,0
_bad_msg: db "BAD eax=%d",10,0

   section __TEXT,__text
_callee:
   push   ebp
   mov    ebp, esp
   mov    eax, 1            ; return 1 — must round-trip through the call
   leave
   ret

_main:
   push   ebp
   mov    ebp, esp
   sub    esp, 16
   call   _callee
   dec    eax                ; eax-- ; if was 1, eax==0 → ZF=1
   je     .ok
   inc    eax                ; restore: print what we actually got
   mov    [esp+4], eax
   mov    dword [esp], _bad_msg
   call   _printf
   mov    dword [esp], 1
   call   _exit
   ud2
.ok:
   mov    dword [esp], _ok_msg
   call   _printf
   mov    dword [esp], 0
   call   _exit
   ud2
