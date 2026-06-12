;; init_trampoline.asm — run translated i386 static initializers on a low-4GB stack.
;;
;; THE BUG (see the init high-stack bug): dyld runs each translated dylib's
;; __DATA,__mod_init_func C++/+load initializers BEFORE the wrapper exec sets
;; up its low-4GB stack. Under Rosetta on Apple Silicon the main-thread stack
;; lives ABOVE 4GB (0x2_xxxxxxxx). i386 code needs a <4GB stack: `push ebp` /
;; `pop ebp` are 4-byte (translator push_r32/pop_r32), so a saved frame pointer
;; that points into a >4GB stack is truncated on `pop ebp` while the widened
;; `mov rsp,rbp`/`leave` keep rsp's high bits — rbp/rsp desync and the next
;; `ret` jumps to a garbage return address (SIGBUS).
;;
;; THE FIX: objc_slide.c rewrites each translated image's __mod_init_func
;; pointers to a small per-slot JIT stub:
;;     movabs r11, <original target i386 init>
;;     movabs rax, _abiconv_init_trampoline
;;     jmp rax
;; dyld calls the stub (x86_64 ABI: rdi=argc rsi=argv rdx=envp rcx=apple), the
;; stub lands here with r11 = the real init func. We save dyld's callee state +
;; the incoming (possibly >4GB) rsp to a shadow save-stack, switch to a
;; dedicated low-4GB stack if we're currently high, build an i386 cdecl frame,
;; and jmp into the translated init. Its `ret` returns to .landing (a libabiconv
;; address — libabiconv maps <4GB, so its low 32 bits work as the i386 4-byte
;; return address). .landing restores dyld's state and returns.
;;
;; Re-entrancy: the dyld init phase is single-threaded; a nested dlopen-driven
;; init runs deeper on whatever stack we already switched to (rsp is then
;; already low, so we DON'T reset to the stack top — we keep descending). The
;; shadow save-stack is LIFO so nested levels restore correctly.

   segment .text
   global _abiconv_init_trampoline
   extern _g_init_shadow_sp        ; uint64_t: grows DOWN; 64 bytes per nesting level
   extern _g_init_low_stack_top    ; uint64_t: 16-aligned top of the dedicated low-4GB init stack

_abiconv_init_trampoline:
   ;; inputs: r11 = target i386 init func (set by the per-slot stub)
   ;;         rdi=argc rsi=argv rdx=envp rcx=apple  (dyld x86_64 ABI)

   ;; --- save dyld's callee-saved regs + incoming rsp to the shadow stack ---
   mov rax, [rel _g_init_shadow_sp]
   sub rax, 64
   mov [rax +  0], rbx
   mov [rax +  8], rbp
   mov [rax + 16], r12
   mov [rax + 24], r13
   mov [rax + 32], r14
   mov [rax + 40], r15
   mov [rax + 48], rsp             ; incoming rsp (may be >4GB)
   mov [rel _g_init_shadow_sp], rax

   ;; --- switch to the low-4GB init stack only if we're currently high ---
   mov r10, rsp
   shr r10, 32
   jz .low_ok                      ; high bits already 0 -> already on a low stack
   mov rsp, [rel _g_init_low_stack_top]
.low_ok:
   ;; --- build an i386 cdecl frame on the (now low) stack ---
   ;; layout at entry of the translated init:
   ;;   [esp+ 0] = 4-byte return address (-> .landing)
   ;;   [esp+ 4] = argc   [esp+ 8] = argv
   ;;   [esp+12] = envp   [esp+16] = apple   (pointers truncated to low 32)
   sub rsp, 32
   and rsp, 0xfffffffffffffff0      ; 16-align (high bits already 0)
   lea rax, [rel .landing]
   mov dword [rsp +  0], eax        ; i386 4-byte return address (libabiconv is <4GB)
   mov dword [rsp +  4], edi        ; argc
   mov dword [rsp +  8], esi        ; argv  (low 32)
   mov dword [rsp + 12], edx        ; envp  (low 32)
   mov dword [rsp + 16], ecx        ; apple (low 32)
   jmp r11                          ; enter the translated init (i386 stack discipline)

.landing:
   ;; reached via the init's translated `ret` (mov r11d,[rsp]; jmp r11).
   ;; rsp is wherever the init left it; recover everything from the shadow stack.
   mov rax, [rel _g_init_shadow_sp]
   mov rbx, [rax +  0]
   mov rbp, [rax +  8]
   mov r12, [rax + 16]
   mov r13, [rax + 24]
   mov r14, [rax + 32]
   mov r15, [rax + 40]
   mov rsp, [rax + 48]             ; restore dyld's incoming rsp
   add rax, 64
   mov [rel _g_init_shadow_sp], rax
   ret                             ; back to dyld (8-byte return addr on restored stack)
