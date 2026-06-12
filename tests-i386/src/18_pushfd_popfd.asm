;;; 18_pushfd_popfd — exercise PUSHFD/POPFD widening (added 2026-06-11 with
;;; the width guard).
;;;
;;; i386 pushfd/popfd move 4 bytes of EFLAGS; the byte-identical x86_64
;;; forms (pushfq/popfq) move 8, corrupting the i386 4-byte stack model.
;;; Rewrites: pushfd -> pushfq; pop r11; push_r32(r11d) and
;;; popfd -> mov r11d,[rsp]; lea rsp,[rsp+4]; push r11; popfq.
;;;
;;; Sets CF, saves EFLAGS via pushfd, clears CF, restores via popfd, then
;;; branches on CF. Also checks the stack pointer moved by exactly 4 by
;;; storing a sentinel below the flags slot and reading it back.
   bits 32
   global _main
   extern _exit

   section __TEXT,__text
_main:
   push ebp
   mov  ebp, esp

   push 0x5a5a5a5a        ; sentinel — must still be at [esp] after pushfd+popfd
   stc                    ; CF=1
   pushfd                 ; save EFLAGS (4-byte slot)
   clc                    ; CF=0
   popfd                  ; restore EFLAGS -> CF=1 again
   jnc  _bad              ; CF must be set
   pop  eax               ; sentinel — wrong if pushfd/popfd moved esp by 8
   cmp  eax, 0x5a5a5a5a
   jne  _bad
   push 7
   call _exit
   ud2
_bad:
   push 99
   call _exit
   ud2
