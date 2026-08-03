;
; cfstring_range_tramp.asm — i386-cdecl -> C trampoline for the CFString
; RANGE-CLAMP family reimplemented in cfstring_range_shim.c.
;
; Like cg_display_tramp.asm / cow_string_tramp.asm this module keeps its OWN
; copy of the MTSHIM macro (the same frame the abigen/maptable shims use), so it
; is a fully self-contained single-purpose unit (per the one-shim-one-job rule)
; with no edit to maptable_tramp.asm. Its symbol is listed in custom.syms so
; neither abigen pass emits a second `global ___CFStringGetCharacters`.
;
; The translated i386 caller reaches the export through its lazy bind
; (static-interpose redirects _CFStringGetCharacters -> ___CFStringGetCharacters).
; It pushed a 4-byte return address then its args as 4-byte cdecl stack slots —
; and note the by-value CFRange occupies TWO of them, because i386 CFIndex is a
; 4-byte long. We hand the C impl rdi = &args[0] and return its uint32_t in eax
; (unused: CFStringGetCharacters is void), then unwind the i386 frame.
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

	MTSHIM	___CFStringGetCharacters,	_shim_CFStringGetCharacters
