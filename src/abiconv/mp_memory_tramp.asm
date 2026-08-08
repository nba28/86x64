;
; mp_memory_tramp.asm — i386-cdecl -> C trampolines for the Carbon
; Multiprocessing Services MEMORY allocators implemented in mp_memory_shim.c.
;
; WHY THIS FILE EXISTS. abigen has MPAllocateAligned/MPAllocate/MPFree/
; MPGetAllocatedBlockSize in its consider-set and generated marshalling bridges
; for them. Their `LogicalAddress` return is `typedef void *`, so each bridge
; ended with abigen's generic void*-return rule and minted a PROXY ARENA HANDLE
; for the >4GB native pointer — handing the translated program an 8-byte arena
; slot in place of the N-byte buffer it asked for. See mp_memory_shim.c's header
; for the measured Halo failure and the universal rule.
;
; Like carbon_memory_tramp.asm / cg_display_tramp.asm this module keeps its OWN
; copy of the MTSHIM macro (the same frame the abigen/maptable shims use), so it
; is a self-contained unit with no edit to maptable_tramp.asm. The four symbols
; are listed in custom.syms (abigen's `-i` ignore list) so neither abigen pass
; emits a competing definition — the measured, supported override path; without
; that entry the link fails with a duplicate `global ___sym`.
;
; ⚠RETRANSLATE-CLASS: static-interpose rewrites binds at TRANSLATE time, so an
; already-translated importer keeps its native MP binds. Exporting these is not
; enough on its own — the importing binary must be re-translated.
;
; i386 esi/edi are callee-saved (= x86_64 rsi/rdi) so we preserve them across the
; SysV call; the __dyld_stub_binder_flag dance matches abigen's prologue.

	segment .text
	extern	__dyld_stub_binder_flag

%macro MTSHIM 2                 ; %1 = export symbol, %2 = C impl symbol
	global	%1
	extern	%2
%1:
	cmp	qword [rel __dyld_stub_binder_flag],	0
	je	%%l1
	mov	rsp,	qword [rel __dyld_stub_binder_flag]
	add	rsp,	16
	mov	qword [rel __dyld_stub_binder_flag],	0
%%l1:
	push	rbp
	mov	rbp,	rsp
	push	rdi
	push	rsi
	and	rsp,	~0xf
	lea	rdi,	[rbp + 12]      ; &args[0]  (first i386 cdecl slot)
	call	%2                      ; eax = i386 return value
	lea	rsp,	[rbp - 0x10]
	pop	rsi
	pop	rdi
	leave
	mov	r11d,	dword [rsp]     ; i386 return address (4 bytes)
	add	rsp,	4
	jmp	r11
%endmacro

	MTSHIM	___MPAllocateAligned,        _shim_MPAllocateAligned
	MTSHIM	___MPAllocate,               _shim_MPAllocate
	MTSHIM	___MPFree,                   _shim_MPFree
	MTSHIM	___MPGetAllocatedBlockSize,  _shim_MPGetAllocatedBlockSize
