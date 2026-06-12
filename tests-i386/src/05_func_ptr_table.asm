;;; 05_func_ptr_table — exercise __DATA function-pointer array (REBASE path).
;;;
;;; In i386, a global array of function pointers in __DATA holds absolute
;;; vmaddrs that the dyld rebase opcodes adjust at load time. After M32→M64
;;; transform, the rebase opcodes need to point at the new function vmaddrs.
;;; If any of that pipeline drops a rebase, we call a stale pointer and
;;; SIGSEGV. If the rebase opcode count is wrong (e.g., a ULEB-ordinal bug
;;; like the one fixed 2026-05-27 dropped half of iPhoto's binds), the
;;; output prints `?` for the un-rebased slot.
;;;
;;; Exercises: __DATA rebase, indirect call `call [reg]`, ABI return values.
   bits 32
   global _main
   extern _printf
   extern _exit

   section __TEXT,__text
_add1:
   mov eax, 1
   ret
_add2:
   mov eax, 2
   ret
_add3:
   mov eax, 3
   ret

   section __DATA,__data
   align 4
_ops:
   dd _add1
   dd _add2
   dd _add3

   section __TEXT,__cstring
_fmt: db "ops[0]=%d ops[1]=%d ops[2]=%d",10,0

   section __TEXT,__text
_main:
   push ebp
   mov  ebp, esp
   sub  esp, 32
   call [_ops + 0]
   mov  [esp+4],  eax
   call [_ops + 4]
   mov  [esp+8],  eax
   call [_ops + 8]
   mov  [esp+12], eax
   mov  dword [esp], _fmt
   call _printf
   mov  dword [esp], 0
   call _exit
   ud2
