;;; 17_push_mem_jmp_mem — exercise PUSH_MEMv/POP_MEMv and 4-byte-slot
;;; JMP_MEMv (all fixed 2026-06-11).
;;;
;;; PUSH_MEMv [base+disp]: rewritten `mov r11d,[mem]; push r11d`. The old
;;; rewrite encoded the mov's target as R12D (REX.R+reg=100), so the loaded
;;; value was discarded and stale R11 got pushed.
;;;
;;; PUSH_MEMv [abs32] (FF 35): previously fell through the imm-pointee
;;; default rule as a byte-identical QWORD push — 8-byte memory read AND
;;; an 8-byte rsp decrement that shifted every later cdecl argument.
;;;
;;; POP_MEMv [abs32]/[base] (8F /0): previously untranslated — the
;;; byte-identical M64 form pops 8 bytes (wrong rsp adjust + 8-byte store
;;; smearing the neighboring word).
;;;
;;; JMP_MEMv [abs32]/[base] through a 4-byte S_REGULAR function-pointer
;;; slot: must narrow the load to 4 bytes; the byte-identical x86_64 form
;;; reads 8 bytes and joins the slot with the adjacent word (same family
;;; as the CALL_NEAR_MEMv photocd bug). The guard words after each slot
;;; make the 8-byte join jump to garbage.
   bits 32
   global _main
   extern _printf
   extern _exit

   section __DATA,__data
   align 4
_val:      dd 7
_vals:     dd 11, 22, 33
_fnptr1:   dd _t1
   dd 0x41414141                ; guard: joined into target if read as 8 bytes
_fnptr2:   dd _t2
   dd 0x42424242                ; guard
_scratch:  dd 0
   dd 0x43434343                ; guard: smeared if POP stores 8 bytes
_scratch2: dd 0
   dd 0x44444444                ; guard

   section __TEXT,__cstring
_fmt: db "a=%d b=%d c=%d",10,0

   section __TEXT,__text
_t1:
   mov  ecx, _fnptr2
   jmp  [ecx]                   ; JMP_MEMv [base] — register-relative 4-byte slot
   ud2
_t2:
   cmp  dword [_scratch + 4], 0x43434343   ; POP guard intact?
   jne  _bad
   cmp  dword [_scratch2 + 4], 0x44444444
   jne  _bad
   mov  eax, [_scratch]         ; 22
   add  eax, [_scratch2]        ; + 7
   push eax
   call _exit
   ud2
_bad:
   push 99
   call _exit
   ud2

_main:
   push ebp
   mov  ebp, esp
   mov  eax, _vals
   push dword [eax+8]           ; PUSH_MEMv [base+disp8] -> 33
   push dword [eax+4]           ; PUSH_MEMv [base+disp8] -> 22
   push dword [_val]            ; PUSH_MEMv [abs32]      -> 7
   push _fmt
   call _printf
   add  esp, 16
   push dword [_vals+4]         ; PUSH_MEMv [abs32], mid-blob pointee -> 22
   pop  dword [_scratch]        ; POP_MEMv  [abs32]
   mov  edx, _scratch2
   push dword [_val]            ; 7
   pop  dword [edx]             ; POP_MEMv  [base] — register-relative
   jmp  [_fnptr1]               ; JMP_MEMv  [abs32] — 4-byte S_REGULAR slot
   ud2
