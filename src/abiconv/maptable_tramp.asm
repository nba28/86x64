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
;; Private CG drop-shadow "style" chain (opaque CF-ptr + struct/CGFloat by value).
   GEOSHIM_I ___CGStyleCreateShadow, 47
   GEOSHIM_I ___CGContextSetStyle,   48
   GEOSHIM_I ___CGStyleRelease,      49

;; NSRect out-pointers: hand-marshalled in objc_shim.c (mt_NSDivideRect).
   MTSHIM ___NSDivideRect, _mt_NSDivideRect

;; ---------------------------------------------------------------------
;; Legacy ObjC1 runtime compat (objc_shim.c "exc1" section): symbols modern
;; libobjc dropped. static-interpose redirects each image's libobjc binds
;; here (NS_DURING/NS_HANDLER machinery + _dealloc vector et al).
	MTSHIM	___objc_exception_try_enter, _shim_objc_exception_try_enter
	MTSHIM	___objc_exception_try_exit,  _shim_objc_exception_try_exit
	MTSHIM	___objc_exception_extract,   _shim_objc_exception_extract
	MTSHIM	___objc_exception_match,     _shim_objc_exception_match
	MTSHIM	___objc_exception_throw,     _shim_objc_exception_throw
	MTSHIM	___class_nextMethodList,     _shim_class_nextMethodList
	MTSHIM	____objc_setNilReceiver,     _shim_objc_setNilReceiver
;; Not an interpose target: ____dealloc (the C variable) holds this tramp's
;; address; translated code calls it through the 4-byte function pointer.
	MTSHIM	_x64_dealloc_vec_tramp,      _shim_dealloc_vec

;; _setjmp for translated i386 callers (interposes the C `_setjmp`, asm
;; `__setjmp`, that NS_DURING uses on the 72-byte buf inside
;; _objc_exception_data). The native x86_64 _setjmp would write 148 bytes
;; there. Capture exactly the state translated code depends on:
;;   [buf+0]  rbx   [buf+8]  rsi   [buf+16] rdi   [buf+24] rbp
;;   [buf+32] rsp (after the 4-byte i386 ret pop)   [buf+40] resume eip
;;   [buf+48] EXC32_MAGIC
;; rsp is kept full-width (FC threads run translated code on >4GB stacks);
;; code addresses are always low-4GB. Restored ONLY by x64_exc_longjmp below
;; (no iWeb payload imports longjmp; the pair stays consistent by interposing
;; both ends).
	global	____setjmp
____setjmp:
	cmp	qword [rel __dyld_stub_binder_flag],	0
	je	.l1
	mov	rsp,	qword [rel __dyld_stub_binder_flag]
	add	rsp,	16
	mov	qword [rel __dyld_stub_binder_flag],	0
.l1:
	mov	eax,	dword [rsp + 4] ; arg0: buf32 (zero-extends)
	mov	r11d,	dword [rsp]     ; 4-byte i386 return address
	mov	qword [rax],	rbx
	mov	qword [rax + 8],	rsi
	mov	qword [rax + 16],	rdi
	mov	qword [rax + 24],	rbp
	lea	rcx,	[rsp + 4]       ; caller's rsp once we pop the ret addr
	mov	qword [rax + 32],	rcx
	mov	qword [rax + 40],	r11
	mov	dword [rax + 48],	0x36346a62      ; EXC32_MAGIC
	xor	eax,	eax             ; first return: 0
	add	rsp,	4
	jmp	r11

;; void x64_exc_longjmp(uint64_t *regs, int val) — SysV entry, called by
;; shim_objc_exception_throw. Restores the ____setjmp capture; the resumed
;; _setjmp call site sees eax=val (nonzero -> NS_HANDLER branch).
	global	_x64_exc_longjmp
_x64_exc_longjmp:
	mov	eax,	esi
	mov	rbx,	qword [rdi]
	mov	rbp,	qword [rdi + 24]
	mov	rsp,	qword [rdi + 32]
	mov	r11,	qword [rdi + 40]
	mov	rsi,	qword [rdi + 8]
	mov	rdi,	qword [rdi + 16]
	jmp	r11

;; ---------------------------------------------------------------------
;; ObjC runtime C functions (objc_shim.c "[rt]" section): translated code
;; calls these directly; unshimmed they reached native libobjc with i386
;; stack args (iPhoto 29th blocker, object_getClass).
	MTSHIM	___object_getClass,             _shim_object_getClass
	MTSHIM	___objc_getClass,               _shim_objc_getClass
	MTSHIM	___class_getSuperclass,         _shim_class_getSuperclass
	MTSHIM	___class_getName,               _shim_class_getName
	MTSHIM	___object_getClassName,         _shim_object_getClassName
	MTSHIM	___class_isMetaClass,           _shim_class_isMetaClass
	MTSHIM	___object_isClass,              _shim_object_isClass
	MTSHIM	___object_setClass,             _shim_object_setClass
	MTSHIM	___objc_retain,                 _shim_objc_retain
	MTSHIM	___objc_sync_enter,             _shim_objc_sync_enter
	MTSHIM	___objc_sync_exit,              _shim_objc_sync_exit
	MTSHIM	___objc_setAssociatedObject,    _shim_objc_setAssociatedObject
	MTSHIM	___objc_getAssociatedObject,    _shim_objc_getAssociatedObject
	MTSHIM	___sel_registerName,            _shim_sel_registerName
	MTSHIM	___sel_getName,                 _shim_sel_getName
	MTSHIM	___sel_isEqual,                 _shim_sel_isEqual
	MTSHIM	____objc_flush_caches,          _shim_objc_flush_caches
	MTSHIM	___class_createInstance,        _shim_class_createInstance
	MTSHIM	___objc_allocateClassPair,      _shim_objc_allocateClassPair
	MTSHIM	___objc_registerClassPair,      _shim_objc_registerClassPair
	MTSHIM	___class_addMethod,             _shim_class_addMethod
	MTSHIM	___class_getInstanceMethod,     _shim_class_getInstanceMethod
	MTSHIM	___class_getClassMethod,        _shim_class_getClassMethod
	MTSHIM	___class_copyMethodList,        _shim_class_copyMethodList
	MTSHIM	___method_getName,              _shim_method_getName
	MTSHIM	___method_getTypeEncoding,      _shim_method_getTypeEncoding
	MTSHIM	___method_getNumberOfArguments, _shim_method_getNumberOfArguments
	MTSHIM	___method_getImplementation,    _shim_method_getImplementation
	MTSHIM	___method_setImplementation,    _shim_method_setImplementation
	MTSHIM	___method_copyReturnType,       _shim_method_copyReturnType
	MTSHIM	___method_copyArgumentType,     _shim_method_copyArgumentType
	MTSHIM	___objc_getClassList,           _shim_objc_getClassList
	MTSHIM	___objc_copyClassList,          _shim_objc_copyClassList
	MTSHIM	___object_getInstanceVariable,  _shim_object_getInstanceVariable

;; ---------------------------------------------------------------------
;; C++ runtime (cxx_shim.c): libstdc++.6 imports of legacy i386 binaries.
;; Guards are implemented locally (native libc++abi sizes its lock words
;; for x86_64); new/delete use the low-4GB shim heap; _Rb_tree_* are
;; reimplemented for the 4-byte i386 node layout.
	MTSHIM	_____cxa_guard_acquire, _shim_cxa_guard_acquire
	MTSHIM	_____cxa_guard_release, _shim_cxa_guard_release
	MTSHIM	_____cxa_guard_abort,   _shim_cxa_guard_abort
	MTSHIM	____Znwm,               _shim_Znwm
	MTSHIM	____Znam,               _shim_Znam
	MTSHIM	____ZnwmRKSt9nothrow_t, _shim_Znwm_nothrow
	MTSHIM	____ZnamRKSt9nothrow_t, _shim_Znam_nothrow
	MTSHIM	____ZdlPv,              _shim_ZdlPv
	MTSHIM	____ZdaPv,              _shim_ZdaPv
	MTSHIM	____ZdlPvRKSt9nothrow_t, _shim_ZdlPv_nothrow
	MTSHIM	____ZSt17__throw_bad_allocv,        _shim_throw_bad_alloc
	MTSHIM	____ZSt20__throw_length_errorPKc,   _shim_throw_length_error
	MTSHIM	____ZSt20__throw_out_of_rangePKc,   _shim_throw_out_of_range
	MTSHIM	____ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base,  _shim_rb_increment
	MTSHIM	____ZSt18_Rb_tree_incrementPKSt18_Rb_tree_node_base, _shim_rb_increment
	MTSHIM	____ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base,  _shim_rb_decrement
	MTSHIM	____ZSt18_Rb_tree_decrementPKSt18_Rb_tree_node_base, _shim_rb_decrement
	MTSHIM	____ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_, _shim_rb_insert_rebalance
	MTSHIM	____ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_,      _shim_rb_rebalance_for_erase
