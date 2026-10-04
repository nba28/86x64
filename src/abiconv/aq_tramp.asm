;
; aq_tramp.asm — i386-cdecl -> C trampolines for aq_shim.c (AudioQueue output
; API with low queue handles, i386-layout buffer mirrors and bridged callbacks).
; Own MTSHIM copy; the symbols are in custom.syms so abigen emits no bridge.
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

	MTSHIM	___AudioQueueNewOutput,			_shim_AudioQueueNewOutput
	MTSHIM	___AudioQueueAllocateBuffer,		_shim_AudioQueueAllocateBuffer
	MTSHIM	___AudioQueueEnqueueBuffer,		_shim_AudioQueueEnqueueBuffer
	MTSHIM	___AudioQueueFreeBuffer,		_shim_AudioQueueFreeBuffer
	MTSHIM	___AudioQueueDispose,			_shim_AudioQueueDispose
	MTSHIM	___AudioQueueStart,			_shim_AudioQueueStart
	MTSHIM	___AudioQueueStop,			_shim_AudioQueueStop
	MTSHIM	___AudioQueuePrime,			_shim_AudioQueuePrime
	MTSHIM	___AudioQueueSetParameter,		_shim_AudioQueueSetParameter
	MTSHIM	___AudioQueueGetProperty,		_shim_AudioQueueGetProperty
	MTSHIM	___AudioQueueAddPropertyListener,	_shim_AudioQueueAddPropertyListener
	MTSHIM	___AudioQueueRemovePropertyListener,	_shim_AudioQueueRemovePropertyListener
