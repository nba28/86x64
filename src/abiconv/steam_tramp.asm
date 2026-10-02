;
; steam_tramp.asm — i386 entry points of the Steamworks bridge (steam_bridge.c).
;
; 1. VTABLE SLOT STUBS. A proxy interface's i386 vtable (steam_bridge.c sb_vtbl)
;    holds sb_slot_<k> for slot k. Translated game code calls them like any
;    virtual method (cdecl, `this` = first stack slot, or the second after a
;    hidden struct-return pointer). Each stub loads its slot number and joins
;    sb_entry, which calls sb_dispatch(&args[0], slot, &ret) and returns the
;    result the way the i386 caller expects it:
;       SB_INT   edx:eax   (bool/int in eax; uint64 / CSteamID in edx:eax)
;       SB_FLT   st0       (float/double)
;       SB_SRET  eax = the hidden pointer, and the callee pops it (`ret $4`)
; 2. MTSHIM entries for the C surface (SteamAPI_*, SteamInternal_*,
;    SteamGameServer_*): eax only. Listed in custom.syms so abigen emits none.
;
; ⚠RETRANSLATE-CLASS like every MTSHIM export: static-interpose binds the game's
; imports at translate time, so importers need a re-translate or `m64 reinterpose`.
;
; i386 esi/edi are callee-saved (= x86_64 rsi/rdi): preserved across the SysV call.
; The __dyld_stub_binder_flag dance matches abigen's prologue.

	segment .text
	extern	__dyld_stub_binder_flag
	extern	_sb_dispatch

%define SB_SLOTS 128            ; >= SB_MAX_METHODS in steam_gen.inc (86)
%define SB_INT  1
%define SB_FLT  2
%define SB_SRET 3

sb_entry:                       ; eax = slot, [rsp] = i386 return address
	cmp	qword [rel __dyld_stub_binder_flag],	0
	je	.l1
	mov	rsp,	qword [rel __dyld_stub_binder_flag]
	add	rsp,	16
	mov	qword [rel __dyld_stub_binder_flag],	0
.l1:
	push	rbp
	mov	rbp,	rsp
	push	rdi
	push	rsi
	sub	rsp,	32              ; struct sb_ret { v @0, f @8, kind @16, argbase @20 }
	and	rsp,	~0xf
	lea	rdi,	[rbp + 12]      ; &args[0]
	mov	esi,	eax             ; slot
	mov	rdx,	rsp             ; &ret
	call	_sb_dispatch
	mov	ecx,	dword [rsp + 16] ; kind
	cmp	ecx,	SB_FLT
	jne	.nofloat
	fld	qword [rsp + 8]         ; i386 float/double return: st0
.nofloat:
	mov	rax,	qword [rsp]
	mov	rdx,	rax
	shr	rdx,	32              ; edx = high half (uint64 / CSteamID)
	lea	rsp,	[rbp - 0x10]
	pop	rsi
	pop	rdi
	leave
	mov	r11d,	dword [rsp]     ; i386 return address (4 bytes)
	add	rsp,	4
	cmp	ecx,	SB_SRET
	jne	.ret
	add	rsp,	4               ; struct return: the callee pops the hidden pointer
.ret:
	jmp	r11

%assign k 0
%rep SB_SLOTS
sb_slot_ %+ k:
	mov	eax,	k
	jmp	sb_entry
%assign k k+1
%endrep

	segment .data
	global	_sb_slot_addrs
	global	_sb_slot_count
_sb_slot_count:
	dd	SB_SLOTS
	align	8
_sb_slot_addrs:
%assign k 0
%rep SB_SLOTS
	dq	sb_slot_ %+ k
%assign k k+1
%endrep

	segment .text
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

	MTSHIM	___SteamAPI_Init,				_shim_SteamAPI_Init
	MTSHIM	___SteamAPI_InitSafe,				_shim_SteamAPI_InitSafe
	MTSHIM	___SteamAPI_Shutdown,				_shim_SteamAPI_Shutdown
	MTSHIM	___SteamAPI_RestartAppIfNecessary,		_shim_SteamAPI_RestartAppIfNecessary
	MTSHIM	___SteamAPI_RunCallbacks,			_shim_SteamAPI_RunCallbacks
	MTSHIM	___SteamAPI_GetHSteamPipe,			_shim_SteamAPI_GetHSteamPipe
	MTSHIM	___SteamAPI_GetHSteamUser,			_shim_SteamAPI_GetHSteamUser
	MTSHIM	___SteamAPI_SetMiniDumpComment,			_shim_SteamAPI_SetMiniDumpComment
	MTSHIM	___SteamAPI_SetTryCatchCallbacks,		_shim_SteamAPI_SetTryCatchCallbacks
	MTSHIM	___SteamAPI_UseBreakpadCrashHandler,		_shim_SteamAPI_UseBreakpadCrashHandler
	MTSHIM	___SteamAPI_RegisterCallback,			_shim_SteamAPI_RegisterCallback
	MTSHIM	___SteamAPI_UnregisterCallback,			_shim_SteamAPI_UnregisterCallback
	MTSHIM	___SteamAPI_RegisterCallResult,			_shim_SteamAPI_RegisterCallResult
	MTSHIM	___SteamAPI_UnregisterCallResult,		_shim_SteamAPI_UnregisterCallResult
	MTSHIM	___SteamClient,					_shim_SteamClient
	MTSHIM	___SteamInternal_CreateInterface,		_shim_SteamInternal_CreateInterface
	MTSHIM	___SteamInternal_ContextInit,			_shim_SteamInternal_ContextInit
	MTSHIM	___SteamInternal_FindOrCreateUserInterface,	_shim_SteamInternal_FindOrCreateUserInterface
	MTSHIM	___SteamInternal_FindOrCreateGameServerInterface, _shim_SteamInternal_FindOrCreateGameServerInterface
	MTSHIM	___SteamInternal_GameServer_Init,		_shim_SteamInternal_GameServer_Init
	MTSHIM	___SteamGameServer_GetHSteamPipe,		_shim_SteamGameServer_GetHSteamPipe
	MTSHIM	___SteamGameServer_GetHSteamUser,		_shim_SteamGameServer_GetHSteamUser
	MTSHIM	___SteamGameServer_GetIPCCallCount,		_shim_SteamGameServer_GetIPCCallCount
	MTSHIM	___SteamGameServer_RunCallbacks,		_shim_SteamGameServer_RunCallbacks
	MTSHIM	___SteamGameServer_Shutdown,			_shim_SteamGameServer_Shutdown
