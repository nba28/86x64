   ;; wrapper.asm — trampoline that hands off to a translated i386 _main.
   ;;
   ;; ld links this exec with LC_MAIN, so dyld calls the entry as
   ;; `int main(int argc, char **argv, char **envp, char **apple)` —
   ;; args arrive in rdi/rsi/rdx/rcx per the System V AMD64 ABI.
   ;;
   ;; We forward (argc, argv) to a C helper that builds an i386-style
   ;; kernel argument frame in 32-bit-addressable memory, then switch
   ;; rsp to that frame and `jmp _main` (the translated startup stub).
   segment .text
   global _main_wrapper
   extern _build_i386_main_frame
   extern _main

_main_wrapper:
   push rbp
   mov rbp, rsp
   sub rsp, 16                 ; 16-byte alignment for the call below
   ;; rdi already holds argc (low 32 bits valid), rsi already holds argv.
   movsxd rdi, edi             ; sign-extend in case argc has stale high bits
   call _build_i386_main_frame ; returns 32-bit esp in eax
   ;; Writing to esp implicitly zero-extends, so the upper 32 bits of
   ;; rsp become 0 — that's a low-4GB stack, which is what 32-bit code
   ;; needs to be able to dereference.
   mov esp, eax
   jmp _main
