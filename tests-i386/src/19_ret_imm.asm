;;; 19_ret_imm — exercise `ret imm16` (callee-cleanup return; the i386
;;; struct-return convention pops its hidden sret pointer with `ret $4`).
;;;
;;; Caught by the width guard 2026-06-11 (28 sites in iPhoto): the
;;; byte-identical x86_64 `ret imm16` pops an 8-byte return address where
;;; i386 pops 4, jumping to the return address joined with the adjacent
;;; stack slot. Rewrite: mov r11d,[rsp]; lea rsp,[rsp+4+imm16]; jmp r11.
;;;
;;; The sentinel push verifies the callee released EXACTLY 4+8 bytes.
   bits 32
   global _main
   extern _exit

   section __TEXT,__text
_callee:                       ; int callee(int a, int b) -- cleans 8 bytes
   mov  eax, [esp+4]
   add  eax, [esp+8]
   ret  8
   ud2

_main:
   push ebp
   mov  ebp, esp
   push 0x5a5a5a5a              ; sentinel
   push 30
   push 12
   call _callee                 ; eax = 42; callee pops both args
   pop  ecx                     ; must be the sentinel again
   cmp  ecx, 0x5a5a5a5a
   jne  _bad
   push eax
   call _exit
   ud2
_bad:
   push 99
   call _exit
   ud2
