;; objc_reverse.asm — x86_64→i386 reverse IMP trampoline.
;;
;; The mirror image of objc_msgSend.asm. When the modern x86_64 ObjC runtime
;; sends a message to one of the translated binary's REGISTERED legacy classes
;; (see _86x64_objc_register_classes in objc_shim.c), dispatch lands here: this
;; single generic IMP is registered for EVERY legacy method. It marshals the
;; x86_64 register ABI down to the i386 cdecl ABI the translated IMP expects,
;; runs the IMP on a low-4GB stack, and marshals the return value back up.
;;
;; On entry (x86_64 SysV, as objc_msgSend would call an IMP):
;;   rdi = self (real 64-bit object or Class)
;;   rsi = _cmd (SEL)
;;   rdx, rcx, r8, r9 = first integer/pointer args
;;   [rsp+8 + 8*k]   = stack args 7.. (above the return address)
;;
;; The C prep (_86x64_reverse_prep) inspects (self,sel), looks up the legacy
;; IMP + its type encoding, gets/creates the i386 shadow for self, and fills a
;; plan with the i386 cdecl frame words (self32, cmd32, args...) plus the IMP
;; and a low-4GB stack to run it on. The asm then:
;;   - copies the frame words onto the low stack with a 4-byte return address
;;     below them (so the IMP's terminal i386 `ret` returns to .back),
;;   - jmps into the IMP,
;;   - on return converts eax via _86x64_reverse_ret.
;;
;; struct reverse_plan layout (must match objc_shim.c):
;;   [plan +   0]  legacy_imp     ; u64  (translated IMP, <4GB)
;;   [plan +   8]  lowstack_top   ; u64  (16-aligned top of a low-4GB stack)
;;   [plan +  16]  lowstack_base  ; u64  (malloc base, freed by _86x64_reverse_ret)
;;   [plan +  24]  ret_kind       ; i32  (0 scalar, 1 object->unwrap, 2 void,
;;                                  3 stret widen, 4 fp st0->xmm0,
;;                                  6 small-struct widen -> rax:rdx/xmm0:xmm1)
;;   [plan +  28]  frame_words    ; u32  (count of 4-byte words: self,cmd,args)
;;   [plan +  32]  frame[64]      ; u32 x64 (self32, cmd32, args... 4-byte slots)
;;   [plan + 288]  stret_dst      ; u64  (native caller's struct buffer)
;;   [plan + 296]  stret_src      ; u32  (i386 hidden-ptr scratch on lowstack)
;;   [plan + 304]  stret_types    ; char* (legacy method encoding)
;;   [plan + 312]  fp_out[2]      ; u64x2 (st0 bank / native xmm0:xmm1 return)

   segment .text
   extern __86x64_reverse_prep
   extern __86x64_reverse_ret
   extern __86x64_rsp_stash
   extern __86x64_rsp_unstash

;; %1 = stret flag (0 normal, 1 objc_msgSend_stret entry). The body is
;; identical; the flag is forwarded to prep in edx so it reads the (shifted)
;; register file correctly and marks the plan for struct widening.
%macro REVERSE_IMP 1
   push rbp
   mov rbp, rsp
   push rbx
   push r12
   push r13
   push r14
   push r15

   ;; --- gather the incoming x86_64 args into a 16-slot buffer for prep ---
   ;; regs[0..5] = rdi,rsi,rdx,rcx,r8,r9 ; regs[6] = &stack-args (caller frame)
   ;; regs[7] = caller ra ; regs[8..15] = xmm0..7 (FP / SSE-struct args)
   sub rsp, 128
   mov [rsp +  0], rdi
   mov [rsp +  8], rsi
   mov [rsp + 16], rdx
   mov [rsp + 24], rcx
   mov [rsp + 32], r8
   mov [rsp + 40], r9
   lea rax, [rbp + 16]             ; stack args sit above saved-rbp + ret addr
   mov [rsp + 48], rax
   mov rax, [rbp + 8]              ; regs[7] = native caller's return address
   mov [rsp + 56], rax             ;   (prep logs it when the lookup fails)
   movsd [rsp +  64], xmm0
   movsd [rsp +  72], xmm1
   movsd [rsp +  80], xmm2
   movsd [rsp +  88], xmm3
   movsd [rsp +  96], xmm4
   movsd [rsp + 104], xmm5
   movsd [rsp + 112], xmm6
   movsd [rsp + 120], xmm7
   mov r12, rsp                    ; r12 = &regs (callee-saved, survives call)

   ;; --- reserve the plan and run prep ---
   sub rsp, 560                    ; sizeof(reverse_plan) -> 560 (aligned, slack)
   and rsp, ~0xf
   mov r13, rsp                    ; r13 = &plan (callee-saved)
   mov rdi, r13
   mov rsi, r12
   mov edx, %1                     ; is_stret
   call __86x64_reverse_prep
   ;; prep failure (no legacy method / tramp invoked not-as-an-IMP) leaves
   ;; legacy_imp = 0: skip the i386 call entirely and return 0 via reverse_ret
   ;; (frees the lowstack). Jumping into imp=0 used to kill the process.
   cmp dword [r13 + 0], 0
   jne %%have_imp
   mov rdi, r13
   xor esi, esi
   xor edx, edx
   call __86x64_reverse_ret
   jmp %%out
%%have_imp:

   ;; --- build the i386 cdecl frame on the low-4GB stack ---
   mov r14, [r13 + 8]              ; lowstack_top
   mov ecx, [r13 + 28]            ; frame_words
   lea rax, [rcx*4 + 4]
   mov rdx, r14
   sub rdx, rax                    ; rdx = candidate new low rsp
   and rdx, ~0xf                   ; 16-align the i386 frame base
   lea rax, [rel %%back]
   mov [rdx], eax                  ; 4-byte return address (low-4GB)
   lea rsi, [r13 + 32]             ; &plan.frame
   lea rdi, [rdx + 4]
   test ecx, ecx
   jz %%nocopy
%%cpy:
   mov eax, [rsi]
   mov [rdi], eax
   add rsi, 4
   add rdi, 4
   dec ecx
   jnz %%cpy
%%nocopy:
   ;; Switch to the low stack and enter the IMP. NO register survives
   ;; translated execution with its top 32 bits intact (i386 4-byte frame
   ;; slots zero-extend on pop) — on a >4GB native stack (FileCoordination
   ;; threads) a register-parked rsp/rbp comes back truncated and the
   ;; epilogue pops ASCII from the translated image's string pool. Park
   ;; rsp+rbp in the per-thread stash instead; recover them at %%back.
   mov r14, rdx                    ; i386 frame base across the stash call
   mov rdi, rsp                    ; high rsp (= &plan, 16-aligned)
   mov rsi, rbp
   call __86x64_rsp_stash
   mov r10d, dword [r13 + 0]       ; legacy_imp (zero-extended, <4GB)
   mov rsp, r14                    ; rsp -> i386 frame ([rsp]=ret32, [rsp+4]=self32...)
   xor esi, esi                    ; i386 callee-saved esi/edi/ebx: zero
   xor edi, edi
   xor ebx, ebx
   jmp r10
%%back:
   ;; IMP returned via i386 `ret`: eax(/edx for 8-byte structs & long long) =
   ;; result, st0 = fp result; rsp = i386 frame + 4 (+8 for an i386 hidden-ptr
   ;; method, which pops it via `ret $4`) on the lowstack is the ONLY
   ;; trustworthy state. Stay on the lowstack for the unstash call; eax/edx
   ;; ride in rbx/r12 (the C callee is native, SysV callee-saved holds).
   mov ebx, eax
   mov r12d, edx
   and rsp, ~0xf
   call __86x64_rsp_unstash        ; rax = high rsp, rdx = rbp
   mov rsp, rax                    ; back on the high stack (= &plan)
   mov rbp, rdx
   mov r13, rax                    ; &plan (rsp at the switch was the plan base)
   ;; fp return (kind 4): bank st0 into plan.fp_out[0] BEFORE any further C
   ;; call can touch the x87 stack. (unstash is x87-clean by construction.)
   cmp dword [r13 + 24], 4
   jne %%nofp
   fstp qword [r13 + 312]          ; double (i386 'f' CGFloat returns widen)
%%nofp:
   mov rdi, r13                    ; &plan
   mov esi, ebx                    ; raw 32-bit eax (zero-extended)
   mov edx, r12d                   ; raw 32-bit edx
   call __86x64_reverse_ret        ; -> rax:rdx (__int128)
   ;; FP halves of the native return ride in plan.fp_out (zeroed by prep,
   ;; filled by reverse_ret for kinds 4/6): always load — the native caller
   ;; only reads xmm0/xmm1 when the signature says so.
   movsd xmm0, [r13 + 312]
   movsd xmm1, [r13 + 320]
%%out:
   lea rsp, [rbp - 40]             ; pop r15..rbx (5 regs)
   pop r15
   pop r14
   pop r13
   pop r12
   pop rbx
   leave
   ret
%endmacro

   global __86x64_reverse_imp
__86x64_reverse_imp:
   REVERSE_IMP 0

   global __86x64_reverse_imp_stret
__86x64_reverse_imp_stret:
   REVERSE_IMP 1

;; ---------------------------------------------------------------------------
;; uint32_t _86x64_call_i386(uint64_t fn, uint64_t nwords,
;;                           const uint32_t *words, uint64_t lowstack_top);
;;
;; Generic native→i386 C-function call: the primitive for bridging C
;; CALLBACKS registered by translated code with native APIs (CFRunLoop
;; observers, etc.). Lays `nwords` 4-byte cdecl args + a 4-byte return
;; address on the caller-provided low-4GB stack, runs the translated
;; function, and returns its eax. Same frame/return dance as
;; __86x64_reverse_imp above, without the ObjC plan/prep.
;;   rdi = fn (translated entry, <4GB)
;;   rsi = nwords
;;   rdx = &words (uint32_t array)
;;   rcx = lowstack_top (16-aligned top of a low-4GB stack)
;; ---------------------------------------------------------------------------
   global __86x64_call_i386
__86x64_call_i386:
   push rbp
   mov rbp, rsp
   push rbx
   push r12
   push r13
   push r14
   push r15

   mov r12, rdi                    ; fn (callee-saved: survives the stash call)
   ;; carve the i386 frame on the low stack: [low]=ret32, then args
   lea rax, [rsi*4 + 4]
   mov r11, rcx
   sub r11, rax
   and r11, ~0xf                   ; 16-align the frame base
   lea rax, [rel .cback]
   mov [r11], eax                  ; 4-byte return address (low-4GB code)
   ;; copy nwords dwords words -> [r11+4...]
   mov rcx, rsi
   lea rsi, [rdx]
   lea rdi, [r11 + 4]
   test ecx, ecx
   jz .cnocopy
.ccpy:
   mov eax, [rsi]
   mov [rdi], eax
   add rsi, 4
   add rdi, 4
   dec ecx
   jnz .ccpy
.cnocopy:
   ;; same no-register-survives rule as __86x64_reverse_imp above
   mov r14, r11                    ; i386 frame base across the stash call
   mov rdi, rsp                    ; high rsp
   mov rsi, rbp
   call __86x64_rsp_stash
   mov rsp, r14                    ; switch to the i386 frame
   xor esi, esi                    ; i386 callee-saved esi/edi/ebx: zero
   xor edi, edi
   xor ebx, ebx
   jmp r12
.cback:
   ;; rsp = i386 frame + 4; eax = result. Recover rsp/rbp from the stash.
   mov ebx, eax
   and rsp, ~0xf
   call __86x64_rsp_unstash        ; rax = high rsp, rdx = rbp
   mov rsp, rax
   mov rbp, rdx
   mov eax, ebx                    ; i386 result

   lea rsp, [rbp - 40]
   pop r15
   pop r14
   pop r13
   pop r12
   pop rbx
   leave
   ret
