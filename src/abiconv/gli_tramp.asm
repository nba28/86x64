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
;; eax is scratch/return in i386 cdecl, so using it to shuffle the return address
;; is free; the bridge sets it on the way out.
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
   mov  eax, dword [rsp]             ; the 4-byte i386 return address
   mov  dword [rsp + 4], eax         ; overwrite the rend slot with it
   lea  rsp, [rsp + 4]               ; drop one 4-byte slot: rend is gone
   jmp  r10                          ; tail-jump; the bridge sees a normal frame

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
