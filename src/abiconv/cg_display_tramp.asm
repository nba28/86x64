;
; cg_display_tramp.asm — i386-cdecl -> C trampolines for the classic FULLSCREEN
; DISPLAY-MODE family reimplemented in cg_display_fullscreen_shim.c.
;
; Like cow_string_tramp.asm / crypto_tramp.asm this module keeps its OWN copy of
; the MTSHIM macros (the same frame the abigen/maptable shims use), so it is a
; fully self-contained single-purpose unit (per the one-shim-one-job rule)
; with no edit to maptable_tramp.asm. Its symbols are listed in custom.syms so
; neither abigen pass emits a second `global ___CG*` definition.
;
; Each translated i386 caller reaches an export through its lazy bind
; (static-interpose redirects _CGDisplayFoo -> ___CGDisplayFoo). The caller
; pushed a 4-byte return address then its args as 4-byte cdecl stack slots (an
; i386 `double` occupies TWO slots — CGDisplayBestModeForParametersAndRefreshRate
; is the one such case here). We hand the C impl rdi = &args[0] and return its
; uint32_t result in eax, then unwind the i386 frame.
;
;   MTSHIM       — pops the 4-byte return address (int / CFDictionaryRef returns).
;   MTSHIM_SRET  — CGDisplayBounds returns a CGRect BY VALUE. On i386 a 16-byte
;                  struct comes back through a hidden pointer passed as the FIRST
;                  cdecl slot which the CALLEE pops (`retl $4`) — verified against
;                  abigen's own ___CGDisplayBounds, which ends `add rsp,8` and
;                  returns the buffer in eax. So the C impl returns args[0] in eax
;                  and we unwind 8 bytes.
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

%macro MTSHIM_SRET 2            ; by-value struct return: callee-pops hidden ptr
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
	lea	rdi,	[rbp + 12]      ; &args[0]  (args[0] = hidden sret ptr)
	call	%2                      ; eax = args[0] (the result buffer)
	lea	rsp,	[rbp - 0x10]
	pop	rsi
	pop	rdi
	leave
	mov	r11d,	dword [rsp]     ; i386 return address (4 bytes)
	add	rsp,	8               ; pop ret addr + the 4-byte hidden sret slot
	jmp	r11
%endmacro

	;; --- the mode half: never reconfigures a display -------------------
	MTSHIM      ___CGDisplaySwitchToMode,                     _shim_CGDisplaySwitchToMode
	MTSHIM      ___CGDisplayCurrentMode,                      _shim_CGDisplayCurrentMode
	MTSHIM      ___CGDisplayBestModeForParameters,            _shim_CGDisplayBestModeForParameters
	MTSHIM      ___CGDisplayBestModeForParametersAndRefreshRate, _shim_CGDisplayBestModeForParametersAndRefreshRate
	MTSHIM      ___CGDisplayPixelsWide,                       _shim_CGDisplayPixelsWide
	MTSHIM      ___CGDisplayPixelsHigh,                       _shim_CGDisplayPixelsHigh
	MTSHIM      ___CGDisplayBitsPerPixel,                     _shim_CGDisplayBitsPerPixel
	MTSHIM_SRET ___CGDisplayBounds,                           _shim_CGDisplayBounds

	;; --- the capture half: tracked, never performed --------------------
	MTSHIM      ___CGCaptureAllDisplays,                      _shim_CGCaptureAllDisplays
	MTSHIM      ___CGCaptureAllDisplaysWithOptions,           _shim_CGCaptureAllDisplaysWithOptions
	MTSHIM      ___CGDisplayCapture,                          _shim_CGDisplayCapture
	MTSHIM      ___CGDisplayCaptureWithOptions,               _shim_CGDisplayCaptureWithOptions
	MTSHIM      ___CGReleaseAllDisplays,                      _shim_CGReleaseAllDisplays
	MTSHIM      ___CGDisplayRelease,                          _shim_CGDisplayRelease
	MTSHIM      ___CGDisplayIsCaptured,                       _shim_CGDisplayIsCaptured
