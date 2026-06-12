;;; 08_printf_4args — regression test for the libabiconv 4-arg printf
;;; vararg garbling bug. printf-conv.cc had `typedef int32_t i64_t;` (typo
;;; for int64_t), so convert_arg_s<i32_t, i64_t> wrote only 4 bytes per
;;; arg into args64 but advanced the cursor by 8 — leaving the upper
;;; 32 bits of each register slot uninitialized. Symptom: 4-arg %x
;;; printf produced garbage in args 2+.
;;;
;;; Test it directly: print four distinct constants via %x and diff
;;; against an exact expected output.

   bits 32
   global _main
   extern _printf
   extern _exit

   section __TEXT,__cstring
_fmt: db "%x %x %x %x",10,0

   section __TEXT,__text
_main:
   push   ebp
   mov    ebp, esp
   sub    esp, 32
   mov    dword [esp+16], 0x44444444
   mov    dword [esp+12], 0x33333333
   mov    dword [esp+8],  0x22222222
   mov    dword [esp+4],  0x11111111
   mov    dword [esp],    _fmt
   call   _printf
   mov    dword [esp], 0
   call   _exit
   ud2
