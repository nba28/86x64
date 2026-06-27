;; block_tramp.asm — native invoke trampoline for the i386-block -> native-block
;; marshalling bridge (the ObjC block bridge in objc_shim.c).
;;
;; When an i386 block crosses into the native runtime as a typed @?/@ argument
;; (enumerateObjectsUsingBlock:, sortUsingComparator:, GCD, completion
;; handlers), objc_shim.c synthesizes a NATIVE x86_64 Block_layout whose
;; `invoke` field points HERE. The native runtime later calls
;; block->invoke(block, args...) with the x86_64 SysV ABI; this stub snapshots
;; the full argument state — 6 GP regs, 8 XMM regs, and a pointer to the stack
;; args — and hands it to the C dispatcher x64_blk_invoke, which recovers the
;; bound i386 block + invoke from the native block's private trailer, marshals
;; SysV -> i386 cdecl per the block's signature, and re-enters the translated
;; invoke via _86x64_call_i386.
;;
;; Same save-everything-then-let-C-decide design as cb_tramp.asm, but it keys
;; off the block pointer (rdi, the block's own first argument) instead of a slot
;; index, so it needs NO per-block stub table.
;;
;; On entry (x86_64 SysV, as the runtime calls a block invoke):
;;   rdi = the native block (our synthesized Block_layout)
;;   rsi,rdx,rcx,r8,r9 = first integer/pointer args
;;   xmm0..7           = FP args
;;   [rsp+8 + 8*k]     = stack args (above the return address)

   segment .text
   extern _x64_blk_invoke

   global _x64_blk_invoke_tramp
_x64_blk_invoke_tramp:
   push rbp
   mov rbp, rsp
   sub rsp, 112                    ; gp[6] (48) + xmm[8] (64); keeps 16-align
   mov [rsp +  0], rdi             ; gp[0] = the native block pointer
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
   ;; uint64_t x64_blk_invoke(u64 block, const u64 *gp, const u64 *fp,
   ;;                         const u64 *stack_args)
   mov rdi, [rsp + 0]              ; the native block (= gp[0])
   mov rsi, rsp                    ; gp[6]
   lea rdx, [rsp + 48]             ; fp[8]
   lea rcx, [rbp + 16]             ; native stack args (above ret addr + rbp)
   call _x64_blk_invoke
   leave
   ret                             ; rax = converted return value
