;; halo_tramp.asm — i386-cdecl -> C trampolines for the Halo hand shims (halo_shim.c).
;;
;; Self-contained (own MTSHIM copy + `extern __dyld_stub_binder_flag`) so this lane stays
;; isolated from the shared maptable_tramp.asm — no collision with the Civ IV removed-Carbon
;; shim lane. Each ___<fn> export is reached by the translated i386 code through its lazy bind
;; (static-interpose redirects _<fn> -> ___<fn>, only because libabiconv now exports ___<fn>).
;;
;; Frame, identical to maptable_tramp.asm / the abigen-generated shims: after `push rbp; mov
;; rbp,rsp` the first i386 arg slot is [rbp+12] ([rbp+8] = the 4-byte i386 return addr). i386
;; esi/edi are callee-saved (= x86_64 rsi/rdi) so we preserve them around the SysV call; the
;; __dyld_stub_binder_flag dance mirrors abigen's lazy-stub-binder entry path. The C impl
;; returns its i386 result in eax, then we unwind the i386 cdecl frame (caller cleans args).

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

	MTSHIM	___mach_timebase_info,	_shim_mach_timebase_info
