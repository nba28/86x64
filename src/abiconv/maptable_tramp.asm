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

	;; variadic POSIX primitives abigen skips (the `...` arg). Reached via the
	;; conformance-variant import (_open$UNIX2003 -> ___open) after static-
	;; interpose strips the $UNIX2003/$INODE64 suffix.
	MTSHIM	___open,                      _shim_open
	MTSHIM	___fcntl,                     _shim_fcntl

	;; dynamic loader: abigen gives these no shim, so a translated i386
	;; `call dlopen` hits NATIVE dlopen with the i386 cdecl ABI -> garbage
	;; args -> fault. Bridge the handle/symbol across the 32/64-bit boundary.
	MTSHIM	___dlopen,                    _shim_dlopen
	MTSHIM	___dlsym,                     _shim_dlsym
	MTSHIM	___dlclose,                   _shim_dlclose
	MTSHIM	___dlerror,                   _shim_dlerror

	;; pthread TSD: abigen forwards straight to native, but macOS keys start
	;; at 258 so key 0 means an UNCONSTRUCTED thread-local. getspecific(0)
	;; must return NULL (not TSD slot 0 = pthread_self) and setspecific(0,..)
	;; must be a no-op (not clobber slot 0). tls_shim.c. Universal: key 0 is
	;; never a valid pthread key for any binary.
	MTSHIM	___pthread_getspecific,       _shim_pthread_getspecific
	MTSHIM	___pthread_setspecific,       _shim_pthread_setspecific

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

	;; OSAtomicAdd32 family: override the abigen shim to guard a near-null
	;; (page-zero) atomic target — a nil-based garbage object ivar passed to a
	;; refcount-Release helper during reverse-bridged teardown. osatomic_shim.c.
	MTSHIM	___OSAtomicAdd32,        _shim_OSAtomicAdd32
	MTSHIM	___OSAtomicAdd32Barrier, _shim_OSAtomicAdd32Barrier

	;; CFAllocator allocate/reallocate/deallocate -> the low-4GB heap. abigen's
	;; native shims hand back >4GB pointers the i386 program truncates, then
	;; aborts when freeing the truncated value via the native default zone
	;; (the dominant iPhoto startup malloc abort at __CFAllocatorDeallocate).
	;; Routed through malloc_shim.c, exactly as malloc/free already are.
	MTSHIM	___CFAllocatorAllocate,   _shim_CFAllocatorAllocate
	MTSHIM	___CFAllocatorReallocate, _shim_CFAllocatorReallocate
	MTSHIM	___CFAllocatorDeallocate, _shim_CFAllocatorDeallocate

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
;; CG context 2D drawing family (struct/CGFloat by value; opaque CF-ptr args).
   GEOSHIM_I ___CGContextFillRect,           50
   GEOSHIM_I ___CGContextStrokeRect,         51
   GEOSHIM_I ___CGContextStrokeRectWithWidth, 52
   GEOSHIM_I ___CGContextClearRect,          53
   GEOSHIM_I ___CGContextClipToRect,         54
   GEOSHIM_I ___CGContextAddRect,            55
   GEOSHIM_I ___CGContextFillEllipseInRect,  56
   GEOSHIM_I ___CGContextStrokeEllipseInRect, 57
   GEOSHIM_I ___CGContextAddEllipseInRect,   58
   GEOSHIM_I ___CGContextMoveToPoint,        59
   GEOSHIM_I ___CGContextAddLineToPoint,     60
   GEOSHIM_I ___CGContextTranslateCTM,       61
   GEOSHIM_I ___CGContextScaleCTM,           62
   GEOSHIM_I ___CGContextRotateCTM,          63
   GEOSHIM_I ___CGContextConcatCTM,          64
   GEOSHIM_I ___CGContextSetLineWidth,       65
   GEOSHIM_I ___CGContextSetAlpha,           66
   GEOSHIM_I ___CGContextSetRGBFillColor,    67
   GEOSHIM_I ___CGContextSetRGBStrokeColor,  68
   GEOSHIM_I ___CGContextSetGrayFillColor,   69
   GEOSHIM_I ___CGContextSetGrayStrokeColor, 70
   GEOSHIM_I ___CGContextDrawImage,          71
   GEOSHIM_I ___CGColorCreateGenericRGB,     72
   GEOSHIM_S ___CGContextGetClipBoundingBox, 73
   GEOSHIM_S ___CGContextGetCTM,             74
   GEOSHIM_I ___CGContextSetPatternPhase,    75

;; NSRect out-pointers: hand-marshalled in objc_shim.c (mt_NSDivideRect).
   MTSHIM ___NSDivideRect, _mt_NSDivideRect

;; CGPatternCreate: struct/CGFloat by value + a CGPatternCallbacks* whose
;; drawPattern/releaseInfo native CG calls back -> hand-marshalled in objc_shim.c.
   MTSHIM ___CGPatternCreate, _shim_CGPatternCreate

;; objc_setProperty/getProperty: @synthesize accessor helpers (libobjc, never
;; shimmed). Operate on the i386 SHADOW ivar; bridge retain/copy/release.
   MTSHIM ___objc_setProperty, _shim_objc_setProperty
   MTSHIM ___objc_getProperty, _shim_objc_getProperty

;; Deprecated CarbonCore volume-notification SPIs (no-ops on modern macOS);
;; native ret over-pops the i386 4-byte return addr -> fused PC. No-op shims.
   MTSHIM ___RequestVolumeNotification, _shim_RequestVolumeNotification
   MTSHIM ___DeclineVolumeNotification, _shim_DeclineVolumeNotification

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
;; Carbon Memory Manager (carbon_memory.c): classic Handle/Ptr on the low-4GB
;; heap. Abigen's native shims would return >4GB pointers that truncate into
;; the i386 4-byte Handle/Ptr slots. Excluded from abigen via custom.syms.
	MTSHIM	___NewHandle,        _shim_NewHandle
	MTSHIM	___NewHandleClear,   _shim_NewHandleClear
	MTSHIM	___DisposeHandle,    _shim_DisposeHandle
	MTSHIM	___GetHandleSize,    _shim_GetHandleSize
	MTSHIM	___SetHandleSize,    _shim_SetHandleSize
	MTSHIM	___NewPtr,           _shim_NewPtr
	MTSHIM	___NewPtrClear,      _shim_NewPtrClear
	MTSHIM	___DisposePtr,       _shim_DisposePtr

;; ---------------------------------------------------------------------
;; Carbon Component Manager (carbon_component.c): the dead-on-modern-macOS
;; component registry/dispatch, reimplemented generically. Backends (e.g. the
;; ImageIO GraphicsImporter) register with it. Excluded from abigen (custom.syms).
	MTSHIM	___OpenDefaultComponent,   _shim_OpenDefaultComponent
	MTSHIM	___OpenADefaultComponent,  _shim_OpenADefaultComponent
	MTSHIM	___OpenComponent,          _shim_OpenComponent
	MTSHIM	___OpenAComponent,         _shim_OpenAComponent
	MTSHIM	___CloseComponent,         _shim_CloseComponent
	MTSHIM	___FindNextComponent,      _shim_FindNextComponent
	MTSHIM	___CountComponents,        _shim_CountComponents
	MTSHIM	___GetComponentInfo,       _shim_GetComponentInfo
	MTSHIM	___ResolveComponentAlias,  _shim_ResolveComponentAlias
	MTSHIM	___UnregisterComponent,    _shim_UnregisterComponent

;; ---------------------------------------------------------------------
;; QuickTime GraphicsImporter (quicktime_image.c): still-image decode on
;; ImageIO/CoreGraphics. QuickTime.framework is GONE on modern macOS, so these
;; were unshimmed (binding to a non-existent @rpath/QuickTime). Re-translate to
;; bind them to libabiconv.
	MTSHIM	___GraphicsImportSetDataHandle,        _shim_GraphicsImportSetDataHandle
	MTSHIM	___GraphicsImportSetDataReference,     _shim_GraphicsImportSetDataReference
	MTSHIM	___GraphicsImportSetDataFile,          _shim_GraphicsImportSetDataFile
	MTSHIM	___GraphicsImportGetNaturalBounds,     _shim_GraphicsImportGetNaturalBounds
	MTSHIM	___GraphicsImportGetImageDescription,  _shim_GraphicsImportGetImageDescription
	MTSHIM	___GraphicsImportSetGWorld,            _shim_GraphicsImportSetGWorld
	MTSHIM	___GraphicsImportSetBoundsRect,        _shim_GraphicsImportSetBoundsRect
	MTSHIM	___GraphicsImportGetBoundsRect,        _shim_GraphicsImportGetBoundsRect
	MTSHIM	___GraphicsImportDraw,                 _shim_GraphicsImportDraw
	MTSHIM	___GraphicsImportSetFlags,             _shim_GraphicsImportSetFlags
	MTSHIM	___GraphicsImportSetQuality,           _shim_GraphicsImportSetQuality
	MTSHIM	___GraphicsImportSetMatrix,            _shim_GraphicsImportSetMatrix
	MTSHIM	___GraphicsImportGetColorSyncProfile,  _shim_GraphicsImportGetColorSyncProfile
	MTSHIM	___GraphicsImportGetDataOffsetAndSize, _shim_GraphicsImportGetDataOffsetAndSize
	MTSHIM	___GraphicsImportGetMetaData,          _shim_GraphicsImportGetMetaData
	MTSHIM	___GraphicsImportReadData,             _shim_GraphicsImportReadData
	MTSHIM	___GetGraphicsImporterForDataRef,      _shim_GetGraphicsImporterForDataRef
	MTSHIM	___GetGraphicsImporterForFile,         _shim_GetGraphicsImporterForFile

;; ---------------------------------------------------------------------
;; QuickDraw offscreen GWorld + PixMap (quicktime_image.c). Excluded from abigen
;; (custom.syms): the GWorld/PixMap handles are ours and opaque to the caller.
	MTSHIM	___NewGWorld,          _shim_NewGWorld
	MTSHIM	___NewGWorldFromPtr,   _shim_NewGWorldFromPtr
	MTSHIM	___QTNewGWorldFromPtr, _shim_QTNewGWorldFromPtr
	MTSHIM	___DisposeGWorld,      _shim_DisposeGWorld
	MTSHIM	___GetGWorldPixMap,    _shim_GetGWorldPixMap
	MTSHIM	___GetPortPixMap,      _shim_GetPortPixMap
	MTSHIM	___GetGWorldDevice,    _shim_GetGWorldDevice
	MTSHIM	___GetGWorld,          _shim_GetGWorld
	MTSHIM	___SetGWorld,          _shim_SetGWorld
	MTSHIM	___GetPixBaseAddr,     _shim_GetPixBaseAddr
	MTSHIM	___GetPixRowBytes,     _shim_GetPixRowBytes
	MTSHIM	___GetPixBounds,       _shim_GetPixBounds
	MTSHIM	___LockPixels,         _shim_LockPixels
	MTSHIM	___UnlockPixels,       _shim_UnlockPixels

;; ---------------------------------------------------------------------
;; C++ runtime (cxx_shim.c): libstdc++.6 imports of legacy i386 binaries.
;; Guards are implemented locally (native libc++abi sizes its lock words
;; for x86_64); new/delete use the low-4GB shim heap; _Rb_tree_* are
;; reimplemented for the 4-byte i386 node layout.
	MTSHIM	_____cxa_guard_acquire, _shim_cxa_guard_acquire
	MTSHIM	_____cxa_guard_release, _shim_cxa_guard_release
	MTSHIM	_____cxa_guard_abort,   _shim_cxa_guard_abort
	MTSHIM	_____cxa_atexit,        _shim_cxa_atexit
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

	; Legacy ImageCapture (ICA) Carbon host API (imagecapture_shim.c). Dead C API on
	; modern macOS (header removed); reached via bare stubs => native 8-byte ret over-pops
	; the 4-byte i386 frame. Graceful "no device" shims so device enumeration fails cleanly.
	MTSHIM	___ICAGetDeviceList,                _shim_ICAGetDeviceList
	MTSHIM	___ICAGetChildCount,                _shim_ICAGetChildCount
	MTSHIM	___ICAGetNthChild,                  _shim_ICAGetNthChild
	MTSHIM	___ICAGetPropertyByType,            _shim_ICAGetPropertyByType
	MTSHIM	___ICAGetPropertyData,              _shim_ICAGetPropertyData
	MTSHIM	___ICACopyObjectPropertyDictionary, _shim_ICACopyObjectPropertyDictionary
	MTSHIM	___ICACopyObjectThumbnail,          _shim_ICACopyObjectThumbnail
	MTSHIM	___ICADownloadFile,                 _shim_ICADownloadFile
	MTSHIM	___ICAObjectSendMessage,            _shim_ICAObjectSendMessage
	MTSHIM	___ICARegisterEventNotification,    _shim_ICARegisterEventNotification

	; Dead FSSpec-based Carbon File Manager (carbon_fsspec_shim.c). The FSSpec generation was
	; removed on modern macOS (symbols + headers gone); the FSRef generation is abigen-shimmed.
	; A missing symbol's bare stub jumps through a null lazy pointer (rip=0 SIGSEGV); these
	; graceful-error shims keep the caller on its "no such file/volume" path.
	MTSHIM	___FSpMakeFSRef,    _shim_FSpMakeFSRef
	MTSHIM	___FSMakeFSSpec,    _shim_FSMakeFSSpec
	MTSHIM	___FSpOpenResFile,  _shim_FSpOpenResFile
