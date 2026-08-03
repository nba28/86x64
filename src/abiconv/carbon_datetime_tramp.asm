;
; carbon_datetime_tramp.asm — i386-cdecl -> C trampolines for the classic Date &
; Time Utilities reimplemented in carbon_datetime_shim.c.
;
; These entry points were REMOVED from modern macOS (measured: a dlsym for
; SecondsToDate / DateToSeconds / GetDateTime all fail). abigen still emits a
; bridge, the pipeline weakens the dangling bind to NULL, and the bridge's call
; jumps to 0 -- `rip = 0`, which is how Halo died once the CFBundle-fnptr fix
; carried it past the wild-FILE* crash.
;
; Like cg_display_tramp.asm / cfstring_range_tramp.asm this module keeps its OWN
; copy of the MTSHIM macro so it is a self-contained single-purpose unit. Its
; symbols are listed in custom.syms so neither abigen pass emits a competing
; (and NULL-calling) definition -- that exclusion is what actually removes the
; jump-to-zero, so it is load-bearing, not bookkeeping.
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


	MTSHIM	___SecondsToDate,	_shim_SecondsToDate
	MTSHIM	___DateToSeconds,	_shim_DateToSeconds
	MTSHIM	___LongSecondsToDate,	_shim_LongSecondsToDate
	MTSHIM	___LongDateToSeconds,	_shim_LongDateToSeconds
