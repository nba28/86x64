;;; 07_callee_saved — ESI and EDI must survive function calls.
;;;
;;; In i386 SysV, ESI/EDI are callee-saved. In x86_64 SysV they are
;;; caller-saved. The translator currently preserves i386 instructions
;;; verbatim, so any value held in ESI/EDI by translated i386 code is
;;; clobbered by a native x86_64 callee reached through libabiconv
;;; (printf, objc_msgSend, …).
;;;
;;; Loads sentinels into ESI/EDI, calls printf (libabiconv trampoline
;;; → native printf clobbers rsi/rdi), then checks them.
;;;   PASS  → exit 0
;;;   ESI clobbered → exit 1
;;;   EDI clobbered → exit 2
;;;
;;; Exercises: ESI/EDI preservation across a translated→native call.
   bits 32
   global _main
   extern _printf
   extern _exit

   section __TEXT,__cstring
_msg:      db "ping",10,0
_pass_msg: db "PASS",10,0

   section __TEXT,__text
_main:
   push  ebp
   mov   ebp, esp
   sub   esp, 16
   mov   esi, 0xDEADBEEF
   mov   edi, 0xCAFEBABE
   mov   dword [esp], _msg
   call  _printf             ; native printf clobbers rsi/rdi
   cmp   esi, 0xDEADBEEF
   jne   .fail_esi
   cmp   edi, 0xCAFEBABE
   jne   .fail_edi
   mov   dword [esp], _pass_msg
   call  _printf
   mov   dword [esp], 0
   call  _exit
   ud2
.fail_esi:
   mov   dword [esp], 1
   call  _exit
   ud2
.fail_edi:
   mov   dword [esp], 2
   call  _exit
   ud2
