;; gli_tramp.asm — the dispatch-slot thunks for the i386-layout CGL context shadow.
;;
;; See cgl_macro_shim.c for the why. In one line: a 32-bit app compiled with
;; <OpenGL/CGLMacro.h> never calls a GL entry point — it calls
;;
;;     (*(cgl_ctx)->disp.<fn>)((cgl_ctx)->rend, args...)
;;
;; i.e. it loads a function pointer out of the context object and jumps through
;; it, passing the context's renderer as an EXTRA LEADING ARGUMENT. Our shadow
;; context fills every disp slot with one of these thunks.
;;
;; Each thunk has exactly one job: drop that leading `rend` argument and tail-jump
;; to the abigen-generated i386 bridge for the corresponding GL function
;; (`___glXxx`), which already marshals the i386 cdecl frame into the SysV ABI
;; correctly for that specific prototype.
;;
;; ★Why dropping one slot is enough, for EVERY entry, without knowing prototypes:
;; in i386 cdecl every argument — including GLfloat/GLdouble — is passed in 4-byte
;; stack slots, never in registers. So converting `fn(rend, a, b, c)` into
;; `glFn(a, b, c)` is a pure frame edit that is completely signature-independent.
;; That is the property that makes a 760-entry table feasible at all.
;;
;; Entry ABI (identical to any abigen shim): the translated i386 `jmp *%rax` was
;; preceded by a 4-byte return-address push, so on entry
;;     [rsp]   = i386 return address (4 bytes)
;;     [rsp+4] = rend        <- to be dropped
;;     [rsp+8] = first real argument
;; (measured verbatim in a live fault dump: [rsp+0]=Halo.dylib+0x3a09a6,
;;  [rsp+4]=the rend value, [rsp+8]=0x1F03=GL_EXTENSIONS).
;;
;; ★★HOW the drop is done, and why the obvious way is WRONG.
;; The tempting edit is "move the return address up one slot and `lea rsp,[rsp+4]`".
;; That produces a correct-looking frame but PERMANENTLY RAISES rsp BY 4 PER CALL:
;; the bridge returns having consumed only the 4-byte return address, so the caller
;; gets rsp 4 higher than it left it. Translated i386 callers do not push/pop their
;; arguments — they reserve one outgoing-argument area at function entry and write
;; args into it with `movl %eax,(%rsp)` — so nothing ever re-balances the drift.
;; After a few dozen GL calls the outgoing-argument writes climb into the frame's
;; LOCALS and silently corrupt them. (Measured: Halo survived ~1KB further into the
;; same function and then stored through a clobbered local at 0x103a0d98.)
;;
;; So instead we SHIFT THE ARGUMENT BLOCK DOWN over the rend slot and leave rsp
;; exactly as we found it. That needs the argument block's exact size, which is why
;; gli_dispatch_names.h also generates `gli_slot_argbytes` from the same header:
;; copying MORE than the caller actually pushed would write past its
;; outgoing-argument area into its locals — the very bug being avoided.
;;
;; eax is scratch/return in i386 cdecl, so it is free to use here; the bridge sets
;; it on the way out. rsi/rdi/rcx are i386 callee-saved, so the copy saves them.
;;
;; Slots we could not map to a verified bridge hold target 0. Those must NOT jump
;; through a null pointer — that is precisely the `jmp *0` this whole mechanism
;; exists to prevent — so they report loudly and return 0 instead.

%define GLI_SLOTS 760

   segment .text

   extern _x64_gli_unmapped          ; void (uint32_t slot) — C, logs once per slot

_x64_gli_thunk_dispatch:
   ;; r11d = slot index (set by the stub below).
   lea  r10, [rel _g_gli_target]
   mov  r10, [r10 + r11*8]           ; r10 = ___glXxx bridge, or 0 if unmapped
   test r10, r10
   jz   .unmapped
   lea  rax, [rel _g_gli_argbytes]
   movzx eax, byte [rax + r11]       ; exact i386 size of the argument block
   test eax, eax
   jz   .go                          ; nothing but rend: the bridge ignores it
   push rsi                          ; i386 esi/edi are callee-saved, and rep movs
   push rdi                          ; clobbers them; rcx likewise
   push rcx
   lea  rsi, [rsp + 24 + 8]          ; src = the caller's first real argument
   lea  rdi, [rsp + 24 + 4]          ; dst = the rend slot (strictly below src, so
   mov  ecx, eax                     ;       a forward copy cannot overlap badly)
   shr  ecx, 2
   cld
   rep  movsd
   pop  rcx
   pop  rdi
   pop  rsi
.go:
   jmp  r10                          ; rsp UNCHANGED; [rsp]=ret, [rsp+4]=arg0

.unmapped:
   push rbp
   mov  rbp, rsp
   mov  edi, r11d
   and  rsp, ~0xf                    ; 16-align for the SysV call
   call _x64_gli_unmapped
   leave
   xor  eax, eax                     ; a defined 0, never an undefined register
   mov  r11d, dword [rsp]            ; i386 return address (4 bytes)
   add  rsp, 4
   jmp  r11

;; --- the slot stubs: mov r11d, k ; jmp dispatch ---
%assign k 0
%rep GLI_SLOTS
_x64_gli_thunk_%+k:
   mov r11d, k
   jmp _x64_gli_thunk_dispatch
%assign k k+1
%endrep

   segment .data
   align 8
   global _x64_gli_thunk_table       ; uint64_t[GLI_SLOTS] stub addresses (C reads)
_x64_gli_thunk_table:
%assign k 0
%rep GLI_SLOTS
   dq _x64_gli_thunk_%+k
%assign k k+1
%endrep

   global _x64_gli_nslots
_x64_gli_nslots:
   dq GLI_SLOTS

   segment .bss
   align 8
   global _g_gli_target              ; uint64_t[GLI_SLOTS] resolved bridges (C writes)
_g_gli_target:
   resq GLI_SLOTS

   global _g_gli_argbytes            ; uint8_t[GLI_SLOTS] arg-block sizes (C writes)
_g_gli_argbytes:
   resb GLI_SLOTS
