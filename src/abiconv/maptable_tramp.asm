;
; i386-cdecl -> C trampolines for hand-written shims (legacy NSMapTable/
; NSHashTable + a few libc functions abigen can't marshal, e.g. sysctl).
;
; Each ___NS<fn> is reached by the translated i386 code through its lazy bind
; (static-interpose redirects _NS<fn> -> ___NS<fn>). The i386 caller pushed a
; 4-byte return address then its args as 4-byte stack slots (structs expanded
; inline). We hand the C impl a pointer to that arg block and return its
; uint32_t result in eax, then unwind the i386 frame.
;
; Frame, identical to the abigen-generated shims: after `push rbp; mov rbp,rsp`
; the first i386 arg slot is [rbp+12] (rbp = entry_rsp-8, [rbp+8] = i386
; return addr at +8..+11). i386 esi/edi are callee-saved (= x86_64 rsi/rdi), so
; we preserve them around the SysV call. The __dyld_stub_binder_flag dance
; mirrors abigen's prologue for the lazy-stub-binder entry path.

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
	lea	rdi,	[rbp + 12]      ; &args[0]
	call	%2                      ; eax = i386 return value
	lea	rsp,	[rbp - 0x10]
	pop	rsi
	pop	rdi
	leave
	mov	r11d,	dword [rsp]     ; i386 return address (4 bytes)
	add	rsp,	4
	jmp	r11
%endmacro

	MTSHIM	___NSCreateMapTable,         _shim_NSCreateMapTable
	MTSHIM	___NSMapGet,                 _shim_NSMapGet
	MTSHIM	___NSMapInsert,              _shim_NSMapInsert
	MTSHIM	___NSMapInsertIfAbsent,      _shim_NSMapInsertIfAbsent
	MTSHIM	___NSMapRemove,              _shim_NSMapRemove
	MTSHIM	___NSResetMapTable,          _shim_NSResetMapTable
	MTSHIM	___NSCountMapTable,          _shim_NSCountMapTable
	MTSHIM	___NSFreeMapTable,           _shim_NSFreeMapTable
	MTSHIM	___NSAllMapTableKeys,        _shim_NSAllMapTableKeys
	MTSHIM	___NSEnumerateMapTable,      _shim_NSEnumerateMapTable
	MTSHIM	___NSNextMapEnumeratorPair,  _shim_NSNextMapEnumeratorPair
	MTSHIM	___NSEndMapTableEnumeration, _shim_NSEndMapTableEnumeration

	MTSHIM	___NSCreateHashTable,         _shim_NSCreateHashTable
	MTSHIM	___NSHashGet,                 _shim_NSHashGet
	MTSHIM	___NSHashInsert,              _shim_NSHashInsert
	MTSHIM	___NSHashRemove,              _shim_NSHashRemove
	MTSHIM	___NSCountHashTable,          _shim_NSCountHashTable
	MTSHIM	___NSFreeHashTable,           _shim_NSFreeHashTable
	MTSHIM	___NSEnumerateHashTable,      _shim_NSEnumerateHashTable
	MTSHIM	___NSNextHashEnumeratorItem,  _shim_NSNextHashEnumeratorItem
	MTSHIM	___NSEndHashTableEnumeration, _shim_NSEndHashTableEnumeration

	;; libc functions abigen marshals incorrectly (array / size_t* pointers)
	MTSHIM	___sysctl,                    _shim_sysctl
	MTSHIM	___sysctlbyname,              _shim_sysctlbyname

	;; Carbon Process Manager: ProcessInfoRec embeds pointers (i386/x86_64
	;; layouts differ) and is in/out — abigen can't marshal it. proc_shim.c.
	MTSHIM	___GetProcessInformation,     _shim_GetProcessInformation

	;; CFRunLoop observers: function-pointer callout abigen can't marshal
	;; (it emitted a dead stack-temp address as the callback). Token-based
	;; bridge back into the translated callout. cf_callback_shim.c.
	MTSHIM	___CFRunLoopObserverCreate,     _shim_CFRunLoopObserverCreate
	MTSHIM	___CFRunLoopAddObserver,        _shim_CFRunLoopAddObserver
	MTSHIM	___CFRunLoopRemoveObserver,     _shim_CFRunLoopRemoveObserver
	MTSHIM	___CFRunLoopObserverInvalidate, _shim_CFRunLoopObserverInvalidate
	MTSHIM	___CFRunLoopTimerCreate,        _shim_CFRunLoopTimerCreate
	MTSHIM	___CFRunLoopAddTimer,           _shim_CFRunLoopAddTimer
	MTSHIM	___CFRunLoopRemoveTimer,        _shim_CFRunLoopRemoveTimer
	MTSHIM	___CFRunLoopTimerInvalidate,    _shim_CFRunLoopTimerInvalidate

	;; IOKit system-power notifications (not in abigen's consider-set) +
	;; the two CFRunLoop funcs that consume our opaque tokens (CFRunLoopGet-
	;; Current stays abigen-generated). See iokit_shim.c.
	MTSHIM	___IORegisterForSystemPower,         _shim_IORegisterForSystemPower
	MTSHIM	___IODeregisterForSystemPower,       _shim_IODeregisterForSystemPower
	MTSHIM	___IONotificationPortGetRunLoopSource, _shim_IONotificationPortGetRunLoopSource
	MTSHIM	___IONotificationPortDestroy,        _shim_IONotificationPortDestroy
	MTSHIM	___IOServiceClose,                   _shim_IOServiceClose
	MTSHIM	___IOAllowPowerChange,               _shim_IOAllowPowerChange
	MTSHIM	___IOCancelPowerChange,              _shim_IOCancelPowerChange
	MTSHIM	___CFRunLoopAddSource,               _shim_CFRunLoopAddSource
	MTSHIM	___CFRunLoopRemoveSource,            _shim_CFRunLoopRemoveSource

	;; Variadic AppKit alert panels (abigen skips ALL variadic functions, so
	;; the raw binds passed arena handles straight into native AppKit, which
	;; then ran objc_msgSend on a handle). objc_shim.c alert_panel_common.
	MTSHIM	___NSRunAlertPanel,              _shim_NSRunAlertPanel
	MTSHIM	___NSRunCriticalAlertPanel,      _shim_NSRunCriticalAlertPanel
	MTSHIM	___NSRunInformationalAlertPanel, _shim_NSRunInformationalAlertPanel
	MTSHIM	___NSGetAlertPanel,              _shim_NSGetAlertPanel
	;; CFRetain/CFRelease must no-op on our IOKit tokens (else the token
	;; leaks into real CF and faults). Token-aware override; passthrough else.
	MTSHIM	___CFRetain,                         _shim_CFRetain
	MTSHIM	___CFRelease,                        _shim_CFRelease

;; ===========================================================================
;; Struct-by-value C-function shims (28th blocker): NS/CG geometry family.
;; The C side (objc_shim.c g_geo[]) marshals via the ObjC bridge's classifier;
;; the INDEX passed here is the row in g_geo — KEEP BOTH LISTS IN SYNC.
;;   GEOSHIM_I: integer/BOOL/object/8-byte-struct returns (eax / eax:edx)
;;   GEOSHIM_F: CGFloat/double returns (x64_geo_f returns long double = st0)
;;   GEOSHIM_S: i386 hidden-pointer struct returns — Darwin i386 callees POP
;;              the hidden ptr (`ret $4`, verified: SL Foundation NSUnionRect
;;              ends `retl $0x4`) and return the buffer in eax.
;; ===========================================================================
   extern _x64_geo_i
   extern _x64_geo_f

%macro GEOSHIM_BODY 3              ; %1 = idx, %2 = C dispatcher, %3 = pop bytes
   cmp  qword [rel __dyld_stub_binder_flag], 0
   je   %%g1
   mov  rsp, qword [rel __dyld_stub_binder_flag]
   add  rsp, 16
   mov  qword [rel __dyld_stub_binder_flag], 0
%%g1:
   push rbp
   mov  rbp, rsp
   push rdi
   push rsi
   and  rsp, ~0xf
   mov  edi, %1
   lea  rsi, [rbp + 12]            ; &args32[0]
   call %2
   lea  rsp, [rbp - 0x10]
   pop  rsi
   pop  rdi
   leave
   mov  r11d, dword [rsp]          ; 4-byte i386 return address
   add  rsp, %3
   jmp  r11
%endmacro

%macro GEOSHIM_I 2                 ; %1 = export symbol, %2 = g_geo index
   global %1
%1:
   GEOSHIM_BODY %2, _x64_geo_i, 4
%endmacro

%macro GEOSHIM_F 2
   global %1
%1:
   GEOSHIM_BODY %2, _x64_geo_f, 4
%endmacro

%macro GEOSHIM_S 2                 ; struct return: pop ret addr + hidden ptr
   global %1
%1:
   GEOSHIM_BODY %2, _x64_geo_i, 8
%endmacro

   GEOSHIM_S ___NSUnionRect,         0
   GEOSHIM_S ___NSIntersectionRect,  1
   GEOSHIM_S ___NSInsetRect,         2
   GEOSHIM_S ___NSOffsetRect,        3
   GEOSHIM_S ___NSIntegralRect,      4
   GEOSHIM_S ___NSRectFromString,    5
   GEOSHIM_I ___NSPointFromString,   6
   GEOSHIM_I ___NSSizeFromString,    7
   GEOSHIM_I ___NSContainsRect,      8
   GEOSHIM_I ___NSEqualPoints,       9
   GEOSHIM_I ___NSEqualRects,       10
   GEOSHIM_I ___NSEqualSizes,       11
   GEOSHIM_I ___NSIntersectsRect,   12
   GEOSHIM_I ___NSIsEmptyRect,      13
   GEOSHIM_I ___NSMouseInRect,      14
   GEOSHIM_I ___NSPointInRect,      15
   GEOSHIM_I ___NSStringFromPoint,  16
   GEOSHIM_I ___NSStringFromRange,  17
   GEOSHIM_I ___NSStringFromRect,   18
   GEOSHIM_I ___NSStringFromSize,   19
   GEOSHIM_I ___NSEraseRect,        20
   GEOSHIM_I ___NSFrameRect,        21
   GEOSHIM_I ___NSFrameRectWithWidth, 22
   GEOSHIM_I ___NSRectClip,         23
   GEOSHIM_I ___NSRectFill,         24
   GEOSHIM_I ___NSRectFillUsingOperation, 25
   GEOSHIM_S ___CGRectUnion,        26
   GEOSHIM_S ___CGRectIntersection, 27
   GEOSHIM_S ___CGRectInset,        28
   GEOSHIM_S ___CGRectOffset,       29
   GEOSHIM_S ___CGRectIntegral,     30
   GEOSHIM_S ___CGRectApplyAffineTransform, 31
   GEOSHIM_I ___CGRectContainsPoint, 32
   GEOSHIM_I ___CGRectContainsRect, 33
   GEOSHIM_I ___CGRectEqualToRect,  34
   GEOSHIM_I ___CGRectIntersectsRect, 35
   GEOSHIM_I ___CGRectIsEmpty,      36
   GEOSHIM_I ___CGRectIsInfinite,   37
   GEOSHIM_I ___CGRectIsNull,       38
   GEOSHIM_F ___CGRectGetHeight,    39
   GEOSHIM_F ___CGRectGetWidth,     40
   GEOSHIM_F ___CGRectGetMaxX,      41
   GEOSHIM_F ___CGRectGetMaxY,      42
   GEOSHIM_F ___CGRectGetMidX,      43
   GEOSHIM_F ___CGRectGetMidY,      44
   GEOSHIM_F ___CGRectGetMinX,      45
   GEOSHIM_F ___CGRectGetMinY,      46

;; NSRect out-pointers: hand-marshalled in objc_shim.c (mt_NSDivideRect).
   MTSHIM ___NSDivideRect, _mt_NSDivideRect
