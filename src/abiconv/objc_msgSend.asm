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
;; function, loads the registers from the plan, tail-calls the real
;; objc_msgSend variant in libobjc, and wraps an object return on the way
;; back into a 32-bit handle.
;;
;; struct objc_call_plan layout (must match objc_shim.c):
;;   [plan +  0]  reg[0..5]   ; 48 bytes (rdi,rsi,rdx,rcx,r8,r9)
;;   [plan + 48]  nreg        ; int32  (count of valid regs, unused by asm)
;;   [plan + 52]  ret_is_obj  ; int32  (wrap return into 32-bit handle if 1)
;;   [plan + 56]  super       ; struct objc_super storage (16 bytes)
;;   [plan + 72]  legacy_imp  ; uint64 (translated i386 IMP, or 0)
;;   [plan + 80]  nstack      ; uint32 (count of overflow stack args)
;;   [plan + 88]  stack[..]   ; uint64[] varargs spill (7th SysV int arg onward)
;;
;; Trampoline stack frame after prologue:
;;   [rbp +  0]  saved rbp
;;   [rbp +  8]  4-byte return address (i386 caller pushed via push_r32)
;;   [rbp + 12]  args32[0]
;;   [rbp + 16]  args32[1]
;;   [rbp + 20]  args32[2]
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
   extern _x64_objc_wrap
   extern _x64_objc_bounce_cstr
   extern __dyld_stub_binder_flag

;; ----------------------------------------------------------------------------
;; Trampoline macro
;;
;;   %1 = exported symbol name        (e.g. ___objc_msgSend)
;;   %2 = bridge_prep variant         (e.g. _objc_bridge_prep)
;;   %3 = real msgSend variant        (e.g. _objc_msgSend)
;;
;; The plan struct is allocated on rsp: 72 bytes rounded up to 96 to keep
;; rsp 16-aligned after the and-mask.
;; ----------------------------------------------------------------------------
%macro OBJC_MSGSEND_TRAMP 3
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
   push rbx                        ; callee-saved; carries ret_is_obj
   ;; i386 ABI says ESI/EDI are callee-saved across the call; in x86_64
   ;; SysV they're caller-saved. Save the caller's values so the translated
   ;; i386 caller's ESI/EDI survive. (See the callee-saved register mismatch notes.)
   push rdi
   push rsi

   ;; reserve struct objc_call_plan (344 bytes -> 384), keep rsp 16-aligned
   sub rsp, 384
   and rsp, ~0xf
   mov qword [rsp + 72], 0         ; plan.legacy_imp = 0 (uninit stack mem)
   mov dword [rsp + 80], 0         ; plan.nstack    = 0 (uninit stack mem)
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
   mov r11, rsp
   mov ebx, [r11 + 52]             ; ret_is_obj
   ;; Varargs overflow (arrayWithObjects:, stringWithFormat:, ...): the 7th+
   ;; SysV integer args, and a nil terminator that lands past r9, live in
   ;; plan.stack[0..nstack). Copy them so they sit at [rsp], [rsp+8], ... at
   ;; the call. The %%ret epilogue resets rsp via rbp, so no manual cleanup.
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
   call %3                         ; rsp is 16-aligned

   ;; ebx = return kind: 0 scalar (pass through), 1 object (wrap), 2 cstring
   cmp ebx, 1
   jl %%ret                        ; 0 -> nothing to do
   je %%wrap
   mov rdi, rax                    ; 2 -> char* return -> low-4GB bounce buffer
   call _x64_objc_bounce_cstr      ; result in eax
   jmp %%ret
%%wrap:
   mov rdi, rax                    ; object return -> 32-bit handle
   call _x64_objc_wrap             ; result in eax
%%ret:
   lea rsp, [rbp - 24]             ; rsi slot (top of pushes: rbx, rdi, rsi)
   pop rsi
   pop rdi
   pop rbx
   leave
   mov r11d, dword [rsp]           ; 4-byte i386 return address
   add rsp, 4
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

OBJC_MSGSEND_TRAMP ___objc_msgSend,            _objc_bridge_prep,             _objc_msgSend
OBJC_MSGSEND_TRAMP ___objc_msgSendSuper,       _objc_bridge_prep_super,       _objc_msgSendSuper
OBJC_MSGSEND_TRAMP ___objc_msgSend_stret,      _objc_bridge_prep_stret,       _objc_msgSend_stret
OBJC_MSGSEND_TRAMP ___objc_msgSendSuper_stret, _objc_bridge_prep_super_stret, _objc_msgSendSuper_stret
;; fpret returns long double on x87 — bridge_prep is identical to msgSend,
;; only the real target differs. The wrap-on-return path is bypassed
;; because ret_is_obj is false for fp returns.
OBJC_MSGSEND_TRAMP ___objc_msgSend_fpret,      _objc_bridge_prep,             _objc_msgSend_fpret
