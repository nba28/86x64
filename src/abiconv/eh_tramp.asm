;
; i386-cdecl -> C trampolines for the C++ EXCEPTION runtime entry points, plus
; the landing-pad RESUME primitive. Single concern: C++ exception unwinding
; across translated i386 frames (see eh_shim.c). Kept separate from
; maptable_tramp.asm / cxx_shim.c so exceptions are their own unit
; (the one-shim-one-job rule).
;
; The legacy i386 binaries import these from /usr/lib/libstdc++.6.dylib
; (___cxa_*) and /usr/lib/libSystem (__Unwind_*). cxx_shim.c deliberately does
; NOT shim them ("only reachable from unwinding"); without a shim dyld binds
; them to the NATIVE x86_64 libstdc++/libunwind, which the translated i386
; cdecl call frame then crashes (over-pop / fused PC). static-interpose
; redirects each import _X -> __<X> (PREFIX "__") when libabiconv exports it, so
; the export names below are the import nlist name with a leading "__".
;
; The frame contract is identical to maptable_tramp.asm's MTSHIM: after
; `push rbp; mov rbp,rsp` the first i386 arg slot is [rbp+12], the i386 return
; address is the 4 bytes at [rbp+8], esi/edi (= rsi/rdi) are callee-saved. The C
; impl is handed &args[0] and returns its uint32 result in eax.
;
; __cxa_throw / _Unwind_Resume are NORETURN: their C impls run the unwinder and
; either tail-jump into a translated landing pad via eh_resume (below) or
; terminate; they never fall back through the trampoline epilogue. They still
; use the same prologue so the i386 arg block is reachable.
;

	segment .text
	extern	__dyld_stub_binder_flag

%macro EHSHIM 2                 ; %1 = export symbol, %2 = C impl symbol
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
	lea	rdi,	[rbp + 12]      ; &args[0]
	call	%2                      ; eax = i386 return value
	lea	rsp,	[rbp - 0x10]
	pop	rsi
	pop	rdi
	leave
	mov	r11d,	dword [rsp]     ; i386 return address (4 bytes)
	add	rsp,	4
	jmp	r11
%endmacro

	EHSHIM	_____cxa_allocate_exception, _shim_cxa_allocate_exception
	EHSHIM	_____cxa_free_exception,     _shim_cxa_free_exception
	EHSHIM	_____cxa_throw,              _shim_cxa_throw
	EHSHIM	_____cxa_begin_catch,        _shim_cxa_begin_catch
	EHSHIM	_____cxa_end_catch,          _shim_cxa_end_catch
	EHSHIM	_____cxa_rethrow,            _shim_cxa_rethrow
	EHSHIM	_____cxa_get_exception_ptr,  _shim_cxa_get_exception_ptr
	EHSHIM	_____cxa_call_unexpected,    _shim_cxa_call_unexpected
	EHSHIM	_____gxx_personality_v0,     _shim_gxx_personality_v0
	EHSHIM	____Unwind_Resume,           _shim_Unwind_Resume
	EHSHIM	____Unwind_Resume_or_Rethrow,_shim_Unwind_Resume_or_Rethrow

;
; void eh_resume(uint32_t rip, uint32_t esp, uint32_t ebp,
;                uint32_t eax_exc, uint32_t edx_sel)   -- NORETURN
;
; Install a translated landing-pad context: set the i386 GPR state the Itanium
; landing pad expects (rax = _Unwind_Exception handle, rdx = selector switch
; value, rbp = the handler frame pointer, rsp = the handler frame's CFA) and
; jump to the translated landing-pad address. ebx/esi/edi are left as-is:
; frame-pointer unwinding cannot recover them and -O0 landing pads reload
; locals through rbp; the precise restore is a later increment driven by the
; original i386 CFI register rules (valid because the translator preserves the
; i386 stack layout). All inputs are low-4GB (translated/i386 address space).
;
; SysV in: rdi=rip, rsi=esp, rdx=ebp, rcx=eax_exc, r8=edx_sel.
;
	global	_eh_resume
_eh_resume:
	mov	r11d,	edi             ; r11 = target rip (survives esp/rbp loads)
	mov	eax,	ecx             ; rax = exception handle
	mov	r9d,	r8d             ; stash selector (rdx about to be reloaded)
	mov	ebp,	edx             ; rbp = handler frame pointer
	mov	esp,	esi             ; rsp = handler frame CFA
	mov	edx,	r9d             ; rdx = selector switch value
	jmp	r11
