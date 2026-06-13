;; objc_msgSend.asm — i386→x86_64 trampolines for the objc_msgSend family.
;;
;; The translated i386 program calls the various objc_msgSend variants with
;; the i386 cdecl ABI: a 4-byte return address at [rsp], then the explicit
;; args as 4-byte stack slots. static-interpose rewrites the lazy-binds for
;; the underscore-prefixed Apple names (_objc_msgSend, _objc_msgSendSuper,
;; _objc_msgSend_stret, _objc_msgSendSuper_stret, _objc_msgSend_fpret) to
;; point at the `__`-prefixed symbols exported by this file.
;;
;; The C side (objc_bridge_prep* in objc_shim.c) does all argument
;; resolution and writes the x86_64 register values into a plan struct;
;; the asm just marshals the i386 stack frame, calls the right prep
;; function, loads the GP + XMM registers from the plan, calls the real
;; objc_msgSend variant in libobjc (or plan.target when the prep overrode
;; it — struct returns that are stret on i386 but register-class on
;; x86_64), and converts the return per plan.ret_kind on the way back.
;;
;; struct objc_call_plan layout (must match objc_shim.c):
;;   [plan +  0]  reg[0..5]   ; 48 bytes (rdi,rsi,rdx,rcx,r8,r9)
;;   [plan + 48]  nreg        ; int32  (count of valid regs, unused by asm)
;;   [plan + 52]  ret_kind    ; int32  (see below)
;;   [plan + 56]  super       ; struct objc_super storage (16 bytes)
;;   [plan + 72]  legacy_imp  ; uint64 (translated i386 IMP, or 0)
;;   [plan + 80]  nstack      ; uint32 (count of overflow stack qwords)
;;   [plan + 88]  stack[32]   ; uint64[] overflow / MEMORY-struct args
;;   [plan +344]  xmm[8]      ; uint64[] xmm0..7 payloads
;;   [plan +408]  nxmm        ; uint32 (-> al, SysV variadic SSE count)
;;   [plan +412]  sret_conv   ; uint32 (C side only)
;;   [plan +416]  sret_dst32  ; uint32 (C side only)
;;   [plan +424]  sret_enc    ; char*  (C side only)
;;   [plan +432]  target      ; uint64 (override for the real msgSend variant)
;;   [plan +440]  fp_out[2]   ; uint64[2] (asm fld scratch)
;;   [plan +456]  stret_buf   ; 184 bytes (native struct bounce buffer)
;;   total 640
;;
;; ret_kind: 0 scalar (rax/rdx pass through)   1 object -> wrap handle
;;           2 char* -> low-4GB bounce          3 stret narrow (C finisher)
;;           4 fp double: xmm0 -> st0           5 small struct -> eax:edx (C)
;;           6 reg-return struct -> i386 buf (C) 7 fp float: xmm0 -> st0
;;
;; Trampoline stack frame after prologue:
;;   [rbp +  0]  saved rbp
;;   [rbp +  8]  4-byte return address (i386 caller pushed via push_r32)
;;   [rbp + 12]  args32[0]
;;   [rbp + 16]  args32[1]
;;   ...

   segment .text
   extern _objc_msgSend
   extern _objc_msgSendSuper
   extern _objc_msgSend_stret
   extern _objc_msgSendSuper_stret
   extern _objc_msgSend_fpret
   extern _objc_bridge_prep
   extern _objc_bridge_prep_super
   extern _objc_bridge_prep_stret
   extern _objc_bridge_prep_super_stret
   extern _objc_bridge_ret_finish
   extern _x64_objc_wrap
   extern _x64_objc_wrap_ret
   extern _x64_objc_bounce_cstr
   extern __dyld_stub_binder_flag

;; ----------------------------------------------------------------------------
;; Trampoline macro
;;
;;   %1 = exported symbol name        (e.g. ___objc_msgSend)
;;   %2 = bridge_prep variant         (e.g. _objc_bridge_prep)
;;   %3 = real msgSend variant        (e.g. _objc_msgSend)
;;   %4 = i386 hidden-ptr pop (1 for the _stret entries: Darwin i386 struct-
;;        returning callees pop the hidden pointer — `ret $4` — verified in
;;        the SL Foundation i386 slice, NSUnionRect ends `retl $0x4`)
;; ----------------------------------------------------------------------------
%macro OBJC_MSGSEND_TRAMP 4
   global %1
%1:
   ;; freshly-bound check (see getopt.asm / abigen output)
   cmp qword [rel __dyld_stub_binder_flag], 0
   je %%l1
   mov rsp, qword [rel __dyld_stub_binder_flag]
   add rsp, 16
   mov qword [rel __dyld_stub_binder_flag], 0
%%l1:
   push rbp
   mov rbp, rsp
   push rbx                        ; callee-saved; carries &plan (low addr)
   ;; i386 ABI says ESI/EDI are callee-saved across the call; in x86_64
   ;; SysV they're caller-saved. Save the caller's values so the translated
   ;; i386 caller's ESI/EDI survive. (See the callee-saved register mismatch notes.)
   push rdi
   push rsi

   ;; reserve struct objc_call_plan (640 bytes), keep rsp 16-aligned
   sub rsp, 640
   and rsp, ~0xf
   mov qword [rsp + 72], 0         ; plan.legacy_imp = 0 (uninit stack mem)
   mov dword [rsp + 80], 0         ; plan.nstack    = 0
   mov dword [rsp + 408], 0        ; plan.nxmm      = 0
   mov qword [rsp + 432], 0        ; plan.target    = 0
   mov rdi, rsp                    ; &plan
   lea rsi, [rbp + 12]             ; args32 -> first i386 stack arg
   call %2

   ;; rsp still points at plan
   ;; Legacy class-method? Resume the i386 cdecl call straight into the IMP.
   ;; legacy_imp is a translated i386 IMP, always a 32-bit (<4GB) address, so
   ;; load it as 32-bit (zero-extending) — exactly like the i386 return address
   ;; at %%ret. A full 64-bit load would carry whatever sits in the plan slot's
   ;; high 4 bytes into the jmp target (observed: an objc proxy-arena handle
   ;; polluting bits 32..63 -> jmp to 0x<handle>_<imp> -> EXC_BAD_ACCESS).
   mov r10d, dword [rsp + 72]      ; plan.legacy_imp (zero-extended)
   test r10, r10
   jnz %%legacy

   ;; r11 = &plan, a stable base while we move rsp to lay down stack args.
   ;; rbx = &plan for AFTER the call: the callee can re-enter translated code
   ;; (reverse bridge), which preserves only the LOW 32 bits of callee-saved
   ;; regs — fine, the plan lives on the translated program's low-4GB stack.
   mov r11, rsp
   mov ebx, r11d
   ;; Stack args: 7th+ SysV integer args, varargs spill, and MEMORY-class
   ;; struct args live in plan.stack[0..nstack). Copy them so they sit at
   ;; [rsp], [rsp+8], ... at the call. The %%ret epilogue resets rsp via rbp,
   ;; so no manual cleanup.
   mov eax, dword [r11 + 80]       ; plan.nstack
   test eax, eax
   jz %%loadregs
   lea rcx, [rax*8 + 15]
   and rcx, ~0xf                   ; 16-aligned byte count (rsp stays aligned)
   sub rsp, rcx
   xor rcx, rcx
%%copy_stack:
   mov r10, [r11 + 88 + rcx*8]
   mov [rsp + rcx*8], r10
   inc rcx
   cmp ecx, eax
   jb %%copy_stack
%%loadregs:
   mov rdi, [r11 +  0]
   mov rsi, [r11 +  8]
   mov rdx, [r11 + 16]
   mov rcx, [r11 + 24]
   mov r8,  [r11 + 32]
   mov r9,  [r11 + 40]
   ;; XMM args (FP scalars / SSE-class struct eightbytes). Slots past nxmm
   ;; hold stack garbage — harmless in registers the callee never reads.
   movsd xmm0, [r11 + 344]
   movsd xmm1, [r11 + 352]
   movsd xmm2, [r11 + 360]
   movsd xmm3, [r11 + 368]
   movsd xmm4, [r11 + 376]
   movsd xmm5, [r11 + 384]
   movsd xmm6, [r11 + 392]
   movsd xmm7, [r11 + 400]
   mov eax, dword [r11 + 408]      ; al = SSE count (SysV variadic rule)
   ;; plan.target overrides the default variant (reg-return struct form)
   mov r10, [r11 + 432]
   test r10, r10
   jnz %%calltgt
   call %3                         ; rsp is 16-aligned
   jmp %%retsw
%%calltgt:
   call r10
%%retsw:
   ;; rbx = &plan (low 32 bits survived any translated re-entry)
   mov ecx, dword [rbx + 52]       ; ret_kind
   test ecx, ecx
   jz %%ret                        ; 0 -> scalar passthrough (rax/rdx)
   cmp ecx, 1
   je %%wrap
   cmp ecx, 2
   je %%bounce
   cmp ecx, 4
   je %%fpd
   cmp ecx, 7
   je %%fpf
   ;; kinds 3/5/6: struct-return finisher. xmm0/xmm1 still hold the callee's
   ;; FP return payload and bind to the finisher's double params.
   mov rdi, rbx                    ; &plan
   mov rsi, rax
   ;; rdx already holds the callee's rdx
   call _objc_bridge_ret_finish    ; -> rax:rdx (i386 eax:edx / buffer ptr)
   jmp %%ret
%%fpd:
   movsd [rbx + 440], xmm0         ; native double -> x87 st0 (i386 fp return)
   fld qword [rbx + 440]
   jmp %%ret
%%fpf:
   movss [rbx + 440], xmm0         ; native float -> x87 st0
   fld dword [rbx + 440]
   jmp %%ret
%%bounce:
   mov rdi, rax                    ; char* return -> low-4GB bounce buffer
   call _x64_objc_bounce_cstr      ; result in eax
   jmp %%ret
%%wrap:
   mov rdi, rax                    ; object return -> 32-bit handle
   call _x64_objc_wrap_ret         ; identity-preserving: legacy instance -> its
                                   ; i386 shadow (ivar space); else proxy handle
%%ret:
   lea rsp, [rbp - 24]             ; rsi slot (top of pushes: rbx, rdi, rsi)
   pop rsi
   pop rdi
   pop rbx
   leave
   mov r11d, dword [rsp]           ; 4-byte i386 return address
%if %4
   add rsp, 8                      ; ret addr + hidden struct ptr (ret $4)
%else
   add rsp, 4
%endif
   jmp r11

%%legacy:
   ;; The i386 caller's frame is still intact at [rbp+8] (4-byte ret addr),
   ;; [rbp+12] (self32), [rbp+16] (_cmd32), [rbp+20...] (args). Restore the
   ;; caller's callee-saved esi/edi (prep clobbered rsi/rdi), restore rbp, set
   ;; rsp back to the entry frame, and jmp into the translated IMP — exactly
   ;; as if the i386 caller had called it directly. The IMP's terminal i386
   ;; `ret` pops the 4-byte ret addr and returns to the original caller with
   ;; eax/edx:eax/st0 holding the result, per i386 cdecl. r10 = IMP, survives.
   mov rsi, [rbp - 24]             ; caller's esi
   mov rdi, [rbp - 16]             ; caller's edi
   mov rbx, [rbp - 8]              ; caller's ebx
   mov rsp, rbp
   pop rbp                         ; restore caller rbp; rsp -> entry (ret-addr slot)
   jmp r10
%endmacro

OBJC_MSGSEND_TRAMP ___objc_msgSend,            _objc_bridge_prep,             _objc_msgSend,            0
OBJC_MSGSEND_TRAMP ___objc_msgSendSuper,       _objc_bridge_prep_super,       _objc_msgSendSuper,       0
OBJC_MSGSEND_TRAMP ___objc_msgSend_stret,      _objc_bridge_prep_stret,       _objc_msgSend_stret,      1
OBJC_MSGSEND_TRAMP ___objc_msgSendSuper_stret, _objc_bridge_prep_super_stret, _objc_msgSendSuper_stret, 1
;; fpret: same entry layout as msgSend; ret_kind 4/7 converts the xmm0
;; result to st0 for the i386 caller (long double 'D' stays kind 0 — the
;; x86_64 long-double return already lives in st0).
OBJC_MSGSEND_TRAMP ___objc_msgSend_fpret,      _objc_bridge_prep,             _objc_msgSend_fpret,      0

;; ----------------------------------------------------------------------------
;; __int128 _86x64_plan_call(struct objc_call_plan *plan, void *fn);
;;
;; Native helper for the table-driven C-function shims (NS/CG geometry):
;; loads the plan's stack/GP/XMM args, calls fn, stores xmm0/xmm1 into
;; plan.fp_out for the C caller, and returns the callee's rax:rdx. Called
;; from C (native SysV context, NOT an i386 entry).
;; ----------------------------------------------------------------------------
   global __86x64_plan_call
__86x64_plan_call:
   push rbp
   mov rbp, rsp
   push rbx
   push r12
   mov rbx, rdi                    ; &plan
   mov r12, rsi                    ; fn
   mov eax, dword [rbx + 80]       ; nstack
   test eax, eax
   jz .regs
   lea rcx, [rax*8 + 15]
   and rcx, ~0xf
   sub rsp, rcx
   xor rcx, rcx
.cpy:
   mov r10, [rbx + 88 + rcx*8]
   mov [rsp + rcx*8], r10
   inc rcx
   cmp ecx, eax
   jb .cpy
.regs:
   mov rdi, [rbx +  0]
   mov rsi, [rbx +  8]
   mov rdx, [rbx + 16]
   mov rcx, [rbx + 24]
   mov r8,  [rbx + 32]
   mov r9,  [rbx + 40]
   movsd xmm0, [rbx + 344]
   movsd xmm1, [rbx + 352]
   movsd xmm2, [rbx + 360]
   movsd xmm3, [rbx + 368]
   movsd xmm4, [rbx + 376]
   movsd xmm5, [rbx + 384]
   movsd xmm6, [rbx + 392]
   movsd xmm7, [rbx + 400]
   mov eax, dword [rbx + 408]      ; al = SSE count
   call r12
   movsd [rbx + 440], xmm0         ; fp_out[0]
   movsd [rbx + 448], xmm1         ; fp_out[1]
   lea rsp, [rbp - 16]
   pop r12
   pop rbx
   leave
   ret
