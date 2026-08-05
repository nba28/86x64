;
; carbon_fscatinfo_tramp.asm — i386-cdecl -> C trampolines for the classic File
; Manager's FSCatalogInfo family, reimplemented in carbon_fscatinfo_shim.c.
;
; These symbols DO exist natively; the problem is not a dangling bind but the
; SHAPE of the marshalling. FSCatalogInfo is 144 bytes on i386 and 148 on
; x86_64 (FSPermissionInfo ends in an FSFileSecurityRef pointer), so abigen
; deep-copies it through a scratch slot in the shim frame sized for exactly ONE
; struct — which is right for the single-item calls and fatal for the BULK ones,
; where the native callee fills `actualObjects` entries and overruns the frame
; into the i386 caller's locals. Measured on Civ IV: 11 * 148 bytes into an
; 880-byte frame, and the caller's two `operator new[]` pointers read back NULL.
;
; Like carbon_datetime_tramp.asm / cfstring_range_tramp.asm this module keeps its
; OWN copy of the MTSHIM macro so it is a self-contained single-purpose unit. Its
; symbols are listed in custom.syms so neither abigen pass emits a competing
; (single-element) definition -- that exclusion is what actually removes the
; overrun, so it is load-bearing, not bookkeeping.
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


	MTSHIM	___FSGetCatalogInfoBulk,		_shim_FSGetCatalogInfoBulk
	MTSHIM	___FSCatalogSearch,			_shim_FSCatalogSearch
	MTSHIM	___FSGetCatalogInfo,			_shim_FSGetCatalogInfo
	MTSHIM	___FSSetCatalogInfo,			_shim_FSSetCatalogInfo
	MTSHIM	___FSCreateFileUnicode,			_shim_FSCreateFileUnicode
	MTSHIM	___FSCreateDirectoryUnicode,		_shim_FSCreateDirectoryUnicode
	MTSHIM	___FSCreateFileAndOpenForkUnicode,	_shim_FSCreateFileAndOpenForkUnicode
