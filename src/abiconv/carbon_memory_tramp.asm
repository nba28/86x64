;
; carbon_memory_tramp.asm — i386-cdecl -> C trampolines for the parts of the
; classic Memory Manager implemented in carbon_memory.c that are NOT already
; reached through maptable_tramp.asm, plus ALL of CarbonCore's `CSMem*` twins.
;
; WHY THIS FILE EXISTS. maptable_tramp.asm already routes the eight classic
; entry points carbon_memory.c originally implemented (NewHandle, NewHandleClear,
; DisposeHandle, GetHandleSize, SetHandleSize, NewPtr, NewPtrClear, DisposePtr).
; CarbonCore ALSO exports every one of them under a `CSMem` prefix, and that is
; what CarbonCore itself and the classic QuickTime.framework call internally:
; the translated QuickTime imports 22 of them. None were shimmed, so they bound
; straight to the NATIVE 64-bit Memory Manager with i386-layout arguments.
;
; A classic Handle is `Ptr*`. Native CarbonCore dereferences 8 bytes where our
; i386 Handle stores 4, so CSMemDisposeHandle read a real 4-byte master pointer
; plus 4 bytes of adjacent heap garbage and handed the result to free().
; MEASURED (probes/badfree.m, Civilization IV 2026-08-03): the pointer was
; 0x0000_0037_9000_ff9c and libmalloc aborted the process. The mirror defect
; applies on the way out: CSMemNewHandle returns a >4GB native Handle that
; truncates into the caller's 4-byte slot.
;
; Like cg_display_tramp.asm / cow_string_tramp.asm this module keeps its OWN copy
; of the MTSHIM macro (the same frame the abigen/maptable shims use), so it is a
; self-contained unit with no edit to maptable_tramp.asm. Symbols are listed in
; custom.syms so neither abigen pass emits a competing definition.
;
; ⚠RETRANSLATE-CLASS: static-interpose rewrites binds at TRANSLATE time, so an
; already-translated importer keeps its native binds. Exporting these is not
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

	;; --- classic names NOT already in maptable_tramp.asm ---------------
	MTSHIM	___NewEmptyHandle,	_shim_NewEmptyHandle
	MTSHIM	___EmptyHandle,		_shim_EmptyHandle
	MTSHIM	___ReallocateHandle,	_shim_ReallocateHandle
	MTSHIM	___RecoverHandle,	_shim_RecoverHandle
	MTSHIM	___HandToHand,		_shim_HandToHand
	MTSHIM	___HandAndHand,		_shim_HandAndHand
	MTSHIM	___PtrToHand,		_shim_PtrToHand
	MTSHIM	___PtrToXHand,		_shim_PtrToXHand
	MTSHIM	___PtrAndHand,		_shim_PtrAndHand
	MTSHIM	___Munger,		_shim_Munger
	MTSHIM	___HGetState,		_shim_HGetState
	MTSHIM	___HSetState,		_shim_HSetState
	MTSHIM	___GetPtrSize,		_shim_GetPtrSize
	MTSHIM	___SetPtrSize,		_shim_SetPtrSize

	;; --- CarbonCore's CSMem* twins: all 22 the translated QuickTime imports
	MTSHIM	___CSMemNewHandle,	_shim_CSMemNewHandle
	MTSHIM	___CSMemNewHandleClear,	_shim_CSMemNewHandleClear
	MTSHIM	___CSMemNewEmptyHandle,	_shim_CSMemNewEmptyHandle
	MTSHIM	___CSMemDisposeHandle,	_shim_CSMemDisposeHandle
	MTSHIM	___CSMemEmptyHandle,	_shim_CSMemEmptyHandle
	MTSHIM	___CSMemGetHandleSize,	_shim_CSMemGetHandleSize
	MTSHIM	___CSMemSetHandleSize,	_shim_CSMemSetHandleSize
	MTSHIM	___CSMemReallocateHandle, _shim_CSMemReallocateHandle
	MTSHIM	___CSMemRecoverHandle,	_shim_CSMemRecoverHandle
	MTSHIM	___CSMemHandToHand,	_shim_CSMemHandToHand
	MTSHIM	___CSMemHandAndHand,	_shim_CSMemHandAndHand
	MTSHIM	___CSMemPtrToHand,	_shim_CSMemPtrToHand
	MTSHIM	___CSMemPtrToXHand,	_shim_CSMemPtrToXHand
	MTSHIM	___CSMemPtrAndHand,	_shim_CSMemPtrAndHand
	MTSHIM	___CSMemMunger,		_shim_CSMemMunger
	MTSHIM	___CSMemHGetState,	_shim_CSMemHGetState
	MTSHIM	___CSMemHSetState,	_shim_CSMemHSetState
	MTSHIM	___CSMemNewPtr,		_shim_CSMemNewPtr
	MTSHIM	___CSMemNewPtrClear,	_shim_CSMemNewPtrClear
	MTSHIM	___CSMemDisposePtr,	_shim_CSMemDisposePtr
	MTSHIM	___CSMemGetPtrSize,	_shim_CSMemGetPtrSize
	MTSHIM	___CSMemSetPtrSize,	_shim_CSMemSetPtrSize
