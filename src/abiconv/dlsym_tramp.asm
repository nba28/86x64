;; dlsym_tramp.asm — i386->native callable thunks for dlsym'd native functions.
;;
;; shim_dlsym (posix_shim.c) resolves a native symbol to a >4GB address that an
;; i386 caller can neither hold in a 32-bit slot nor call. For a FUNCTION symbol
;; it binds the native target to one of these low-4GB thunk slots and returns the
;; slot's stub address. libabiconv loads in the low-4GB window (like every
;; translated image), so the stub IS a valid 32-bit pointer the i386 code can
;; call. When the translated i386 code calls through it, the stub conveys its
;; slot index in r11 (scratch, never an argument register) and jumps to the
;; common dispatch, which marshals the i386 cdecl stack frame into the SysV
;; argument registers and calls the captured native function.
;;
;; This is the SYMMETRIC counterpart to cb_tramp.asm: cb_tramp goes native->i386
;; (a native API calling an i386 callback); here we go i386->native (i386 code
;; calling a dlsym'd native function), exactly like an abigen-generated shim,
;; but with the target chosen at run time and a signature-free fixed marshalling
;; of up to 6 dword integer/pointer args. Functions with FP/struct args or more
;; than 6 args are not covered (rare for a dlsym'd C entry; they would still need
;; a real signature-aware shim) — the int/pointer case (callback-registration
;; APIs, getters, etc.) is what this serves.
;;
;; Entry ABI (identical to an abigen shim): the translated i386 `call` pushed a
;; 4-byte return address, with args following as 4-byte stack slots. After
;; `push rbp; mov rbp,rsp` the i386 return address is at [rbp+8] and arg0 at
;; [rbp+12]. i386 esi/edi (== x86_64 rsi/rdi) are callee-saved, so we preserve
;; them around the native call. The native return value in eax flows straight
;; back to the i386 caller. We pop the 4-byte return address by hand
;; (`mov r11d,[rsp]; add rsp,4; jmp r11`) because a native 8-byte `ret` would
;; over-pop the i386 frame.

%define DLSYM_SLOTS 512

   segment .text

_x64_dlsym_thunk_dispatch:
   push rbp
   mov  rbp, rsp                     ; [rbp+8]=i386 ret addr, [rbp+12]=arg0...
   ;; r11 = slot index (set by the stub). Load the native target now, before the
   ;; arg loads clobber rdi/rsi/...; r10 survives the arg loads and the `and`.
   lea  r10, [rel _g_dlsym_target]
   mov  r10, [r10 + r11*8]           ; r10 = native function pointer
   push rdi                          ; save incoming i386 edi (callee-saved)
   push rsi                          ; save incoming i386 esi (callee-saved)
   ;; Signature-free marshalling: pass up to 6 dword args, zero-extended, in the
   ;; SysV integer registers. A callee taking fewer simply ignores the extras;
   ;; the reads stay within the (mapped) i386 caller frame.
   mov  edi, dword [rbp + 12]
   mov  esi, dword [rbp + 16]
   mov  edx, dword [rbp + 20]
   mov  ecx, dword [rbp + 24]
   mov  r8d, dword [rbp + 28]
   mov  r9d, dword [rbp + 32]
   and  rsp, ~0xf                    ; 16-align rsp for the SysV call
   call r10                          ; eax/rax = native return value
   lea  rsp, [rbp - 0x10]            ; reposition to the saved rsi slot
   pop  rsi
   pop  rdi
   leave                             ; mov rsp,rbp ; pop rbp  -> rsp = &ret addr
   mov  r11d, dword [rsp]            ; i386 return address (4 bytes)
   add  rsp, 4
   jmp  r11

;; --- the slot stubs: mov r11d, k ; jmp dispatch ---
%assign k 0
%rep DLSYM_SLOTS
_x64_dlsym_thunk_%+k:
   mov r11d, k
   jmp _x64_dlsym_thunk_dispatch
%assign k k+1
%endrep

   segment .data
   align 8
   global _x64_dlsym_thunk_table     ; uint64_t[DLSYM_SLOTS] stub addresses (C reads)
_x64_dlsym_thunk_table:
%assign k 0
%rep DLSYM_SLOTS
   dq _x64_dlsym_thunk_%+k
%assign k k+1
%endrep

   global _x64_dlsym_nslots
_x64_dlsym_nslots: dq DLSYM_SLOTS

   segment .bss
   align 8
   global _g_dlsym_target            ; uint64_t[DLSYM_SLOTS] native targets (C writes,
_g_dlsym_target: resq DLSYM_SLOTS     ; dispatch reads). 0 = free slot.
