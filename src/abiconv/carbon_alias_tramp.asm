;
; carbon_alias_tramp.asm — i386-cdecl -> C trampolines for the classic ALIAS MANAGER
; implemented in carbon_alias_shim.c.
;
; WHY THIS FILE EXISTS. Every entry point below was a NULL-JUMP ORPHAN: the legacy shim
; pass reads the 10.6 SDK's *i386* headers and emits a pass-through bridge `___NewAlias`
; that calls a native `_NewAlias`, and Apple deleted the FSSpec generation of the Alias
; Manager from 64-bit Carbon in 2009. The bridge could only ever `call 0`, and nothing else
; in the process supplies these — so unlike the QuickTime family this cannot be fixed by
; staying out of the path.
;
; MEASURED with src/86x64/nulljump_probe.c built -arch x86_64 and run under Rosetta: the
; FSRef generation (FSNewAlias / FSResolveAlias / FSResolveAliasFile / FSIsAliasFile /
; FSCopyAliasInfo / GetAliasSize / GetAliasUserType) is ALIVE on x86_64, and only the FSSpec
; spellings are dead. carbon_alias_shim.c therefore BRIDGES onto Apple's surviving engine
; rather than reimplementing the record format — the exports listed here are exactly the
; dead names, and deliberately NOT the live ones, because interposing a working native
; implementation is the defect f71a3c1 exists to prevent.
;
; Like carbon_memory_tramp.asm / cg_display_tramp.asm this module keeps its OWN copy of the
; MTSHIM macro (the same frame the abigen/maptable shims use), so it is a self-contained
; unit with no edit to maptable_tramp.asm. Symbols are listed in custom.syms so neither
; abigen pass emits a competing definition.
;
; ⚠RETRANSLATE-CLASS: static-interpose rewrites binds at TRANSLATE time, so an
; already-translated importer keeps its old (weak-NULL) binds. Exporting these is not enough
; on its own — the importing binary must be re-translated.
;
; i386 esi/edi are callee-saved (= x86_64 rsi/rdi) so we preserve them across the SysV call;
; the __dyld_stub_binder_flag dance matches abigen's prologue.

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

	;; --- create ---------------------------------------------------------
	MTSHIM	___NewAlias,			_shim_NewAlias
	MTSHIM	___NewAliasMinimal,		_shim_NewAliasMinimal
	MTSHIM	___NewAliasMinimalFromFullPath,	_shim_NewAliasMinimalFromFullPath
	;; QuickTime's spelling of the same job (const FSSpec*, AliasHandle*, Boolean minimal).
	MTSHIM	___QTNewAlias,			_shim_QTNewAlias

	;; --- resolve --------------------------------------------------------
	MTSHIM	___ResolveAlias,		_shim_ResolveAlias
	MTSHIM	___ResolveAliasFile,		_shim_ResolveAliasFile
	;; The WithMountFlags / NoUI spellings are the SAME function with one extra argument
	;; selecting whether the user may be prompted to mount a volume. We never prompt and
	;; never mount, so they share an implementation. They are not on the NULL-jump list only
	;; because no target we have translated so far imports them; a Carbon app picks between
	;; these spellings arbitrarily, and without the export the bind stays weak-NULL.
	MTSHIM	___ResolveAliasWithMountFlags,		_shim_ResolveAliasWithMountFlags
	MTSHIM	___ResolveAliasFileWithMountFlags,	_shim_ResolveAliasFileWithMountFlags
	MTSHIM	___ResolveAliasFileWithMountFlagsNoUI,	_shim_ResolveAliasFileWithMountFlagsNoUI

	;; --- match ----------------------------------------------------------
	MTSHIM	___MatchAlias,			_shim_MatchAlias
	MTSHIM	___MatchAliasNoUI,		_shim_MatchAliasNoUI
	;; The FSRef-array spelling, DEAD on x86_64 even though the rest of the FSRef
	;; generation survives (Apple kept only FSMatchAliasBulk).
	MTSHIM	___FSMatchAliasNoUI,		_shim_FSMatchAliasNoUI
