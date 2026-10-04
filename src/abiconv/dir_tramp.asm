;
; dir_tramp.asm — i386-cdecl -> C trampolines for dir_shim.c (opendir/readdir/
; closedir with a low DIR wrapper and the classic i386 dirent). Own MTSHIM copy;
; the symbols are in custom.syms so abigen emits no competing bridge.
;
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

	MTSHIM	___opendir,	_shim_opendir
	MTSHIM	___readdir,	_shim_readdir
	MTSHIM	___closedir,	_shim_closedir
	MTSHIM	___scandir,	_shim_scandir
	MTSHIM	___alphasort,	_shim_alphasort
