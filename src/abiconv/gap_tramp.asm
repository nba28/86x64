;; gap_tramp.asm — first-hit report for an EAGERLY bound raw native slot (gap.c).
;;
;; A translated image's __la_symbol_ptr / __jt_ptrs slot that dyld bound at load
;; time to a NATIVE function (no libabiconv bridge) is a raw cross-ABI call. gap.c
;; points such a slot at a per-slot stub:
;;     movabs r11, rec ; jmp [rip+0] ; dq _x64_gap_raw_tramp      (rec follows)
;; On the first call this reports once (x64_gap_raw_hit writes the real target
;; back into the slot, so later calls never come here) and continues to the
;; native exactly as before. Every register a native callee could read is
;; preserved: behaviour is unchanged, it only speaks.
;; rec layout (gap.c struct gap_raw): +0 name, +8 slot, +16 target.

   segment .text
   global _x64_gap_raw_tramp
   extern _x64_gap_raw_hit

_x64_gap_raw_tramp:             ; r11 = rec, [rsp] = i386 4-byte return address
   push rbp
   mov rbp, rsp
   push rax
   push rcx
   push rdx
   push rsi
   push rdi
   push r8
   push r9
   push r10
   push r11                     ; rbp - 72
   and rsp, ~0xf
   sub rsp, 128
   movdqu [rsp + 0x00], xmm0
   movdqu [rsp + 0x10], xmm1
   movdqu [rsp + 0x20], xmm2
   movdqu [rsp + 0x30], xmm3
   movdqu [rsp + 0x40], xmm4
   movdqu [rsp + 0x50], xmm5
   movdqu [rsp + 0x60], xmm6
   movdqu [rsp + 0x70], xmm7
   mov rdi, r11
   mov esi, dword [rbp + 8]     ; i386 caller's return address
   call _x64_gap_raw_hit
   movdqu xmm0, [rsp + 0x00]
   movdqu xmm1, [rsp + 0x10]
   movdqu xmm2, [rsp + 0x20]
   movdqu xmm3, [rsp + 0x30]
   movdqu xmm4, [rsp + 0x40]
   movdqu xmm5, [rsp + 0x50]
   movdqu xmm6, [rsp + 0x60]
   movdqu xmm7, [rsp + 0x70]
   lea rsp, [rbp - 72]
   pop r11
   pop r10
   pop r9
   pop r8
   pop rdi
   pop rsi
   pop rdx
   pop rcx
   pop rax
   pop rbp
   jmp [r11 + 16]
