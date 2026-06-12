;;; 01_hello — sanity: printf one line and exit 0.
;;; Exercises: variadic call, __cstring access (via stub addressing).
   bits 32
   global _main
   extern _printf
   extern _exit

   section __TEXT,__cstring
_hello: db "hello",10,0

   section __TEXT,__text
_main:
   push   ebp
   mov    ebp, esp
   sub    esp, 16
   mov    dword [esp], _hello
   call   _printf
   mov    dword [esp], 0
   call   _exit
   ;; not reached — _exit doesn't return
   ud2
