;
; cfdata_bytebuf_tramp.asm — i386-cdecl -> C trampolines for the byte-buffer
; CF-return family reimplemented in cfdata_bytebuf_shim.c (CFDataGetBytePtr /
; CFDataGetMutableBytePtr / CFStringGetPascalStringPtr).
;
; Like cfnumber_width_tramp.asm / cfstring_range_tramp.asm this module keeps
; its OWN copy of the MTSHIM macro (the same frame the abigen/maptable shims
; use), so it is a fully self-contained single-purpose unit (per
; the one-shim-one-job rule) with no edit to maptable_tramp.asm. Its
; symbols are listed in custom.syms so neither abigen pass emits a second
; `global ___CFDataGetBytePtr` etc.

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

	MTSHIM	___CFDataGetBytePtr,           _shim_CFDataGetBytePtr
	MTSHIM	___CFDataGetMutableBytePtr,    _shim_CFDataGetMutableBytePtr
	MTSHIM	___CFStringGetPascalStringPtr, _shim_CFStringGetPascalStringPtr
