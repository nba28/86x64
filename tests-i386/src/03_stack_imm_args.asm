;;; 03_stack_imm_args — exercises the heuristic that mis-classified imm32 on
;;; `mov [esp+N], imm32` (c7 04/44/84 24 imm32) as a pointer if the value
;;; happened to fall in a segment range.
;;;
;;; We push two specific integer constants that DO fall into the binary's
;;; __TEXT vmaddr range on i386. If the translator rewrites them to
;;; lea+store thinking they're pointers, printf will see something other
;;; than the constants.
;;;
;;; Note as of 2026-05-27: the c7-mov-to-stack heuristic IS required for
;;; printf("hello", ...)-style calls — 01_hello regressed without it. So
;;; the expected output below assumes the heuristic stays ENABLED, and the
;;; test then characterizes its false-positive behavior: today, an integer
;;; constant inside __TEXT WILL be silently rewritten. When the heuristic
;;; tightens (e.g., only treat __cstring/__const destinations as
;;; pointers), this expected output stops needing to encode the FP
;;; behavior.
   bits 32
   global _main
   extern _printf
   extern _exit

   section __TEXT,__cstring
_fmt: db "a=%d b=%d",10,0

   section __TEXT,__text
_main:
   push   ebp
   mov    ebp, esp
   sub    esp, 16
   ;; Use very small ints — well below any plausible __TEXT/__DATA vmaddr
   ;; in the i386 binary (base 0x1000 for non-PIE i386, so __TEXT is
   ;; >= 0x1000). Values 7 and 42 are guaranteed not to alias any segment.
   mov    dword [esp+8], 42
   mov    dword [esp+4], 7
   mov    dword [esp], _fmt
   call   _printf
   mov    dword [esp], 0
   call   _exit
   ud2
