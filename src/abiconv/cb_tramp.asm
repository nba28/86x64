;; cb_tramp.asm — native callback trampolines for the generic fn-ptr bridge.
;;
;; abigen-generated shims call x64_cb_wrap(fn32, sig) (cb_bridge.c) for every
;; function-pointer parameter; it binds the i386 callback to one of these
;; slots and hands the slot's stub address to the native API. When the native
;; code later invokes the "callback", the stub conveys its slot index in r11
;; (scratch, never an argument register) and jumps to the common dispatch
;; entry, which snapshots the full x86_64 argument state — 6 GP regs, 8 XMM
;; regs, and a pointer to the stack args — and calls the C dispatcher. The
;; dispatcher marshals per the signature descriptor and re-enters the
;; translated callback via _86x64_call_i386 (objc_reverse.asm). Same
;; save-everything-then-let-C-decide design as __86x64_reverse_imp.

;; Pool size. Each slot is a ~10-byte stub + an 8-byte table entry, so a large
;; pool is cheap (8192 -> ~150 KB of .text/.data). A big Carbon/GUI target binds
;; one slot per DISTINCT (i386 fn, signature) callback — controls, dialog item
;; procs, timers, event handlers — and Civ IV's nib/UI construction alone needs
;; well over the old 512 (past which x64_cb_wrap returned 0 and Carbon called a
;; NULL callback -> rip=0). Keep CB_SLOTS and cb_bridge.c's g_bind[] in lockstep.
%define CB_SLOTS 8192

   segment .text
   extern _x64_cb_dispatch

_x64_cb_dispatch_entry:
   push rbp
   mov rbp, rsp
   sub rsp, 112                    ; gp[6] (48) + xmm[8] (64); keeps 16-align
   mov [rsp +  0], rdi
   mov [rsp +  8], rsi
   mov [rsp + 16], rdx
   mov [rsp + 24], rcx
   mov [rsp + 32], r8
   mov [rsp + 40], r9
   movsd [rsp + 48], xmm0
   movsd [rsp + 56], xmm1
   movsd [rsp + 64], xmm2
   movsd [rsp + 72], xmm3
   movsd [rsp + 80], xmm4
   movsd [rsp + 88], xmm5
   movsd [rsp + 96], xmm6
   movsd [rsp + 104], xmm7
   ;; uint64_t x64_cb_dispatch(u64 slot, const u64 *gp, const u64 *fp,
   ;;                          const u64 *stack_args)
   mov edi, r11d                   ; slot index (set by the stub)
   mov rsi, rsp                    ; gp[6]
   lea rdx, [rsp + 48]             ; fp[8]
   lea rcx, [rbp + 16]             ; native stack args (above ret addr + rbp)
   call _x64_cb_dispatch
   leave
   ret                             ; rax = converted return value

;; --- the slot stubs: mov r11d, k ; jmp dispatch ---
%assign k 0
%rep CB_SLOTS
_x64_cb_tramp_%+k:
   mov r11d, k
   jmp _x64_cb_dispatch_entry
%assign k k+1
%endrep

   segment .data
   align 8
   global _x64_cb_tramp_table
_x64_cb_tramp_table:
%assign k 0
%rep CB_SLOTS
   dq _x64_cb_tramp_%+k
%assign k k+1
%endrep

   global _x64_cb_nslots
_x64_cb_nslots: dq CB_SLOTS
