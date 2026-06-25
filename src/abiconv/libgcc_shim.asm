;; i386-ABI shims for the libgcc / compiler-rt 64-bit integer helpers.
;;
;; GCC-built i386 code emits calls to ___udivdi3 / ___umoddi3 / ___divdi3 /
;; ___moddi3 for 64-bit integer division & modulo (the i386 ISA has no 64-bit
;; DIV). These are IMPORTED externals (undefined symbols, resolved at runtime).
;; In the translated x86_64 image they would otherwise bind to the NATIVE
;; x86_64 compiler-rt implementation — which uses the x86_64 ABI (args in
;; rdi/rsi, 8-byte `ret`). The translated caller pushes a 4-byte i386 return
;; address, so the native callee's 8-byte `ret` pops that 4-byte return PLUS 4
;; bytes of adjacent (uninitialised) stack -> returns to a garbage 64-bit PC
;; (observed in Portal 2's libtier0 CalculateCPUFreq: pc=0xffffe7xx_00321d8e).
;;
;; These shims provide an i386-cdecl-callable implementation: two 64-bit args
;; on the stack just above the 4-byte return address, result returned in
;; edx:eax, and an i386-style return that consumes the 4-byte return slot
;; (matching the translator's own `ret` idiom `mov r11d,[rsp]; add rsp,4;
;; jmp r11`). Only x86_64 scratch regs (rax/rcx/rdx/r8/r9/r11) are touched, so
;; the i386 callee-saved registers (ebx/esi/edi/ebp) are preserved.
;;
;; i386 cdecl stack layout at entry (rsp points at the 4-byte return address):
;;   [rsp]      = return address (4 bytes)
;;   [rsp+4]    = arg a  (64-bit, little-endian over [rsp+4],[rsp+8])
;;   [rsp+12]   = arg b  (64-bit, little-endian over [rsp+12],[rsp+16])
;; Universal: every GCC-built i386 binary doing 64-bit arithmetic needs these.

   segment .text
   global _____udivdi3
   global _____umoddi3
   global _____divdi3
   global _____moddi3
   global _____fixdfdi
   global _____fixsfdi
   global _____fixunsdfdi
   global _____fixunssfdi
   extern __dyld_stub_binder_flag
   extern _abiconv_libgcc_log     ; gated runtime breadcrumb (osatomic_shim.c)

;; If we were reached through a lazy stub, dyld_stub_binder may have left rsp
;; needing the same fixup the other custom shims apply (see getopt.asm). The
;; flag is 0 on the normal on-thread path, so this is a no-op then.
%macro STUB_FIXUP 0
   cmp qword [rel __dyld_stub_binder_flag], 0
   je %%done
   mov rsp, qword [rel __dyld_stub_binder_flag]
   add rsp, 16
   mov qword [rel __dyld_stub_binder_flag], 0
%%done:
%endmacro

;; Pack quotient/remainder (held in %1, a 64-bit reg) into edx:eax and return
;; i386-style (consume the 4-byte return slot).
%macro RET_I386_64 1
   mov rcx, %1
   mov eax, ecx                 ; eax = low 32 bits of result
   shr rcx, 32
   mov edx, ecx                 ; edx = high 32 bits of result
   mov r11d, [rsp]              ; 4-byte i386 return address
   add rsp, 4                   ; consume the return slot
   jmp r11
%endmacro

;; Gated runtime breadcrumb. Calls the C logger _abiconv_libgcc_log(a,b,result,
;; which) (inert unless ABICONV_LIBGCC_TRACE is set) while preserving the i386
;; callee-saved rsi/rdi AND the result register across the SysV call, and keeping
;; the stack 16-aligned. %4 (result) is restored after the call; %1/%2/%3 may be
;; regs or immediates. Used only to diagnose Portal 2 CalculateCPUFreq=0.
%macro LG_TRACE 4               ; %1=which, %2=a, %3=b, %4=result(reg, preserved)
   push rbp
   mov rbp, rsp
   and rsp, ~0xf
   push rsi
   push rdi
   push %4                      ; save result (call-clobbered)
   sub rsp, 8                   ; pad: rbp+3 pushes+8 = 16-aligned before call
   mov rdi, %2                  ; arg0 = a
   mov rsi, %3                  ; arg1 = b
   mov rdx, %4                  ; arg2 = result
   mov rcx, %1                  ; arg3 = which
   call _abiconv_libgcc_log
   add rsp, 8
   pop %4                       ; restore result
   pop rdi
   pop rsi
   mov rsp, rbp
   pop rbp
%endmacro

_____udivdi3:
   STUB_FIXUP
   mov r8, [rsp + 4]            ; a
   mov r9, [rsp + 12]           ; b
   mov rax, r8
   xor edx, edx
   div r9                       ; rax = quotient, rdx = remainder
   LG_TRACE 0, r8, r9, rax      ; breadcrumb: dividend, divisor, quotient
   RET_I386_64 rax

_____umoddi3:
   STUB_FIXUP
   mov r8, [rsp + 4]
   mov r9, [rsp + 12]
   mov rax, r8
   xor edx, edx
   div r9
   RET_I386_64 rdx              ; remainder

_____divdi3:
   STUB_FIXUP
   mov r8, [rsp + 4]
   mov r9, [rsp + 12]
   mov rax, r8
   cqo                          ; sign-extend rax into rdx:rax
   idiv r9                      ; rax = quotient, rdx = remainder
   RET_I386_64 rax

_____moddi3:
   STUB_FIXUP
   mov r8, [rsp + 4]
   mov r9, [rsp + 12]
   mov rax, r8
   cqo
   idiv r9
   RET_I386_64 rdx              ; remainder

;; Floating-point -> 64-bit integer conversions. i386 cdecl: the FP arg sits on
;; the stack just above the 4-byte return address; the 64-bit integer result is
;; returned in edx:eax. (double is 8 bytes at [rsp+4]; float is 4 bytes at
;; [rsp+4].) Signed forms truncate toward zero via cvttsd2si/cvttss2si. Unsigned
;; forms apply the standard 2^63 bias for inputs that don't fit a signed 64-bit
;; (cvtt* would otherwise saturate to 0x8000000000000000).

_____fixdfdi:                   ; (long long) double, signed
   STUB_FIXUP
   movsd xmm0, [rsp + 4]
   cvttsd2si rax, xmm0
   RET_I386_64 rax

_____fixsfdi:                   ; (long long) float, signed
   STUB_FIXUP
   movss xmm0, [rsp + 4]
   cvttss2si rax, xmm0
   RET_I386_64 rax

_____fixunsdfdi:                ; (unsigned long long) double
   STUB_FIXUP
   movsd xmm0, [rsp + 4]
   mov r10, [rsp + 4]           ; breadcrumb: capture input double bits (r10 survives)
   mov rax, 0x43e0000000000000  ; 2^63 as a double bit pattern
   movq xmm1, rax
   comisd xmm0, xmm1
   jae .big
   cvttsd2si rax, xmm0          ; value < 2^63: direct
   LG_TRACE 1, r10, 0, rax      ; breadcrumb: input double bits, int64 out
   RET_I386_64 rax
.big:
   subsd xmm0, xmm1             ; value - 2^63
   cvttsd2si rax, xmm0
   mov rcx, 0x8000000000000000
   add rax, rcx                 ; + 2^63
   LG_TRACE 1, r10, 0, rax      ; breadcrumb: input double bits, int64 out
   RET_I386_64 rax

_____fixunssfdi:                ; (unsigned long long) float
   STUB_FIXUP
   movss xmm0, [rsp + 4]
   mov eax, 0x5f000000          ; 2^63 as a float bit pattern
   movd xmm1, eax
   comiss xmm0, xmm1
   jae .big
   cvttss2si rax, xmm0
   RET_I386_64 rax
.big:
   subss xmm0, xmm1
   cvttss2si rax, xmm0
   mov rcx, 0x8000000000000000
   add rax, rcx
   RET_I386_64 rax
