;
; atsu_tramp.asm — i386-cdecl -> C trampolines for libabiconv's own ATSUI / ATS
; font subset on CoreText (atsu_shim.c; native ATSUI is blocked on macOS 14+).
; Own copy of MTSHIM, like the other single-purpose tramp modules; the symbols
; are in custom.syms so neither abigen pass emits a bridge to the dead native.
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

	MTSHIM	___ATSUSetAttributes,					_shim_ATSUSetAttributes
	MTSHIM	___ATSUSetLayoutControls,				_shim_ATSUSetLayoutControls
	MTSHIM	___ATSUCreateTextLayoutWithTextPtr,			_shim_ATSUCreateTextLayoutWithTextPtr
	MTSHIM	___ATSUDirectGetLayoutDataArrayPtrFromTextLayout,	_shim_ATSUDirectGetLayoutDataArrayPtrFromTextLayout
	MTSHIM	___ATSUDirectReleaseLayoutDataArrayPtr,			_shim_ATSUDirectReleaseLayoutDataArrayPtr
	MTSHIM	___ATSUGlyphGetIdealMetrics,				_shim_ATSUGlyphGetIdealMetrics
	MTSHIM	___ATSUGlyphGetScreenMetrics,				_shim_ATSUGlyphGetScreenMetrics
	MTSHIM	___ATSUCreateStyle,					_shim_ATSUCreateStyle
	MTSHIM	___ATSUDrawText,					_shim_ATSUDrawText
	MTSHIM	___ATSUDisposeTextLayout,				_shim_ATSUDisposeTextLayout
	MTSHIM	___ATSUSetTransientFontMatching,			_shim_ATSUSetTransientFontMatching
	MTSHIM	___ATSFontFindFromName,					_shim_ATSFontFindFromName
	MTSHIM	___ATSFontFindFromPostScriptName,			_shim_ATSFontFindFromPostScriptName
	MTSHIM	___ATSFontGetName,					_shim_ATSFontGetName
	MTSHIM	___ATSFontGetHorizontalMetrics,				_shim_ATSFontGetHorizontalMetrics
	MTSHIM	___ATSFontActivateFromMemory,				_shim_ATSFontActivateFromMemory
