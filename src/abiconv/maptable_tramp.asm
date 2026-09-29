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

	; removed 64-bit Window Manager calls (carbon_window_removed_shim.c)
	MTSHIM	___SetWindowAlpha,                              _shim_SetWindowAlpha
	MTSHIM	___ChangeWindowGroupAttributes,                 _shim_ChangeWindowGroupAttributes
	MTSHIM	___SetWindowGroupParent,                        _shim_SetWindowGroupParent
	MTSHIM	___CreateQDContextForCollapsedWindowDockTile,   _shim_CreateQDContextForCollapsedWindowDockTile
	MTSHIM	___ReleaseQDContextForCollapsedWindowDockTile,  _shim_ReleaseQDContextForCollapsedWindowDockTile

	; variadic libSystem calls (posix_shim.c) + UCKeyTranslate (uckey_shim.c)
	MTSHIM	___shm_open,                 _shim_shm_open
	MTSHIM	___ioctl,                    _shim_ioctl
	MTSHIM	___UCKeyTranslate,           _shim_UCKeyTranslate

	; legacy vecLib FFT names (vdsp_legacy_shim.c)
	MTSHIM	___create_fftsetup,          _shim_create_fftsetup
	MTSHIM	___destroy_fftsetup,         _shim_destroy_fftsetup
	MTSHIM	___fft_zip,                  _shim_fft_zip

	; OpenSSL 0.9.x libcrypto on CommonCrypto (openssl_shim.c)
	MTSHIM	___SHA1,                     _shim_SHA1
	MTSHIM	___EVP_sha1,                 _shim_EVP_sha1
	MTSHIM	___EVP_md5,                  _shim_EVP_md5
	MTSHIM	___HMAC,                     _shim_HMAC
	MTSHIM	___RAND_pseudo_bytes,        _shim_RAND_pseudo_bytes
	MTSHIM	___RAND_bytes,               _shim_RAND_bytes
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

	;; CGFunctionCreate: the gradient/shading evaluate callback needs
	;; native-double<->i386-float in/out marshalling + low-4GB buffers
	;; (cg_function_shim.c); the generic wrap truncated the >4GB out ptr.
	MTSHIM	___CGFunctionCreate,          _shim_CGFunctionCreate

	;; CGContextSet{Fill,Stroke}Color(ctx, const CGFloat components[]): the
	;; variable-length CGFloat ARRAY form. i386 CGFloat=float(4B), native=
	;; double(8B); abigen passed the array pointer raw so native read doubles
	;; from a float buffer -> ~black fill (Quinn white well). cg_color_shim.c
	;; converts float[n]->double[n], n = colour-space components+1 tracked via
	;; the ColorSpace setters. (Scalar SetRGB/SetGray forms are already handled
	;; by the GEOSHIM CGFloat classifier below.)
	MTSHIM	___CGContextSetFillColorSpace,   _shim_CGContextSetFillColorSpace
	MTSHIM	___CGContextSetStrokeColorSpace, _shim_CGContextSetStrokeColorSpace
	MTSHIM	___CGContextSetFillColor,        _shim_CGContextSetFillColor
	MTSHIM	___CGContextSetStrokeColor,      _shim_CGContextSetStrokeColor

	;; CGFloat-GEOMETRY-ARRAY family (cg_geoarray_shim.c): functions taking a
	;; pointer to an ARRAY of CGRect/CGPoint elements plus a count. i386
	;; elements are 4-byte-float structs; abigen's generated shims read them
	;; as doubles AND converted only element [0] while passing the full count
	;; (garbage clip/path/stroke lists — Quinn's LCD-digit partial-redraw
	;; ClipToRects). Convert the FULL count element-wise instead.
	;; Guard: tests-i386 99_cg_rect_arrays.
	MTSHIM	___CGContextClipToRects,         _shim_CGContextClipToRects
	MTSHIM	___CGContextAddRects,            _shim_CGContextAddRects
	MTSHIM	___CGContextStrokeLineSegments,  _shim_CGContextStrokeLineSegments
	MTSHIM	___NSRectFillList,               _shim_NSRectFillList
	MTSHIM	___NSRectFillListUsingOperation, _shim_NSRectFillListUsingOperation
	MTSHIM	___NSRectFillListWithGrays,      _shim_NSRectFillListWithGrays

	;; NSCopyBits(srcGState, srcRect, destPoint): the legacy view pixel-blit
	;; (nscopybits_shim.c). Modern -[NSView gState] returns 0 so the abigen
	;; shim's NSCopyBits(0,...) copies nothing (Quinn board reflection loses
	;; the settled pieces). We recognise the gState TOKEN the -[NSView gState]
	;; swizzle hands legacy callers and render the source view's rect into the
	;; current focus context; a real gState falls through to native NSCopyBits.
	MTSHIM	___NSCopyBits,                   _shim_NSCopyBits

	;; CFXMLNodeCreate(alloc, xmlType, dataString, const void *additionalInfoPtr,
	;; version): additionalInfoPtr is a DISCRIMINATED UNION selected by xmlType
	;; (CFXMLElementInfo*/CFXMLDocumentInfo*/... — CFXMLNode.h). Those info structs
	;; carry pointer + CFIndex fields, so their LAYOUT DIFFERS i386(4B) vs x86_64
	;; (8B): abigen forwards the i386 record VERBATIM (opaque void* pointee), so
	;; native CF reads a 12-byte i386 CFXMLElementInfo as a 24-byte x86_64 one ->
	;; fused handle 0x08000100_00000008 -> EXC_BAD_ACCESS in CFXMLNodeCreate (Civ
	;; IV Steam startup XML/mod-config parse). cfxml_shim.c relayouts the info per
	;; xmlType (and resolves each pointer field handle->real). GetInfoPtr does the
	;; mirror (native record -> i386 record in a low-4GB buffer).
	;; Guard: tests-i386 cfxml_infoptr_layout.
	MTSHIM	___CFXMLNodeCreate,              _shim_CFXMLNodeCreate
	MTSHIM	___CFXMLNodeGetInfoPtr,          _shim_CFXMLNodeGetInfoPtr

	;; Carbon GetKeys override (keystate_shim.c): emulate a held key at launch
	;; so a vendor "hold KEY to bypass" self-check (Halo's 'p') can be triggered
	;; without a physical keypress (Finder consumes it). Default-off (env-gated);
	;; transparent passthrough to native GetKeys otherwise. Overriding here also
	;; removes GetKeys from the abigen consider set.
	MTSHIM	___GetKeys,                   _shim_GetKeys

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
	;; dladdr completes the family. Dl_info is FOUR POINTERS: 16 bytes on
	;; i386, 32 on x86_64, so this one also has to convert the RECORD, not
	;; just the call. Civ IV: translated Python calls dladdr during Python
	;; start-up; with no shim it reached native dladdr, which read rdi/rsi
	;; garbage (rsi=8 as the Dl_info*) and faulted at 0x56.
	MTSHIM	___dladdr,                    _shim_dladdr

	;; classic NeXT dyld NSSymbol lookup API — the pre-dlopen dynamic loader
	;; old Mac ports use as their dlsym ("_"+name probe/bind/address idiom).
	;; abigen can't shim these (modern <mach-o/dyld.h> marks them unavailable)
	;; so translated code reached NATIVE libdyld: its 64-bit `ret` over-popped
	;; the i386 4-byte return slot, fusing arg0 (a stack buffer pointer) into
	;; the popped PC's high half (Civ IV Steam 0x877ffaf0`09babb13; same latent
	;; defect in iPhoto/iWeb NSLookupSymbolInImage). posix_shim.c routes the
	;; result through the dlsym shim-first/callable-thunk machinery.
	MTSHIM	___NSIsSymbolNameDefined,     _shim_NSIsSymbolNameDefined
	MTSHIM	___NSLookupAndBindSymbol,     _shim_NSLookupAndBindSymbol
	MTSHIM	___NSLookupSymbolInImage,     _shim_NSLookupSymbolInImage
	MTSHIM	___NSAddressOfSymbol,         _shim_NSAddressOfSymbol
	MTSHIM	___NSNameOfSymbol,            _shim_NSNameOfSymbol

	;; fortify _chk family (mem/str): clang emits these for _FORTIFY_SOURCE
	;; builds (10.6 default) but modern headers expose them only as compiler
	;; builtins, so abigen has no prototype -> they bound NATIVE and the
	;; 64-bit ret over-popped the i386 frame (same fused-PC class as above;
	;; Civ IV + vendored Python 2.6 both import several). posix_shim.c.
	MTSHIM	_____memcpy_chk,              _shim_memcpy_chk
	MTSHIM	_____memmove_chk,             _shim_memmove_chk
	MTSHIM	_____memset_chk,              _shim_memset_chk
	MTSHIM	_____strcpy_chk,              _shim_strcpy_chk
	MTSHIM	_____stpcpy_chk,              _shim_stpcpy_chk
	MTSHIM	_____strncpy_chk,             _shim_strncpy_chk
	MTSHIM	_____strcat_chk,              _shim_strcat_chk
	MTSHIM	_____strncat_chk,             _shim_strncat_chk

	;; libgcc __popcountsi2 (libSystem re-exports compiler-rt; Civ IV binds
	;; it). Trivial value computation, but the native ret still over-pops.
	MTSHIM	_____popcountsi2,             _shim_popcountsi2

	;; CFBundleGetFunctionPointerForName: the CF analogue of dlsym. abigen's
	;; bridge returned the >4GB native fn pointer untouched -> the i386 caller
	;; truncated it to 32 bits and jumped to garbage (Civ IV GetBSDProcAddress
	;; -> internal_chdir "chdir"). Shim wraps the result in a low-4GB callable
	;; thunk. abigen excluded via custom.syms (_CFBundleGetFunctionPointerForName).
	MTSHIM	___CFBundleGetFunctionPointerForName, _shim_CFBundleGetFunctionPointerForName

	;; pthread TSD: abigen forwards straight to native, but macOS keys start
	;; at 258 so key 0 means an UNCONSTRUCTED thread-local. getspecific(0)
	;; must return NULL (not TSD slot 0 = pthread_self) and setspecific(0,..)
	;; must be a no-op (not clobber slot 0). tls_shim.c. Universal: key 0 is
	;; never a valid pthread key for any binary.
	MTSHIM	___pthread_getspecific,       _shim_pthread_getspecific
	MTSHIM	___pthread_setspecific,       _shim_pthread_setspecific

	;; pthread SYNCHRONIZATION primitives (mutex/cond/rwlock). abigen marshals
	;; these BY VALUE (copy the i386 struct into a native stack buffer, call
	;; native pthread on the COPY, copy back) — fatal for kernel-backed sync
	;; handles, which require ONE stable shared address across threads (cross-
	;; thread lock handoff + cond signal/wait rendezvous are lost -> deadlock;
	;; Portal 2 CThreadPool startup). pthread_sync_shim.c keeps a stable native
	;; object per i386 object. tests-i386/35. Universal (any multithreaded i386).
	MTSHIM	___pthread_mutex_init,        _shim_pthread_mutex_init
	MTSHIM	___pthread_mutex_lock,        _shim_pthread_mutex_lock
	MTSHIM	___pthread_mutex_trylock,     _shim_pthread_mutex_trylock
	MTSHIM	___pthread_mutex_unlock,      _shim_pthread_mutex_unlock
	MTSHIM	___pthread_mutex_destroy,     _shim_pthread_mutex_destroy
	MTSHIM	___pthread_cond_init,         _shim_pthread_cond_init
	MTSHIM	___pthread_cond_wait,         _shim_pthread_cond_wait
	MTSHIM	___pthread_cond_timedwait,    _shim_pthread_cond_timedwait
	MTSHIM	___pthread_cond_timedwait_relative_np, _shim_pthread_cond_timedwait_relative_np
	MTSHIM	___pthread_cond_signal,       _shim_pthread_cond_signal
	MTSHIM	___pthread_cond_broadcast,    _shim_pthread_cond_broadcast
	MTSHIM	___pthread_cond_destroy,      _shim_pthread_cond_destroy
	MTSHIM	___pthread_rwlock_init,       _shim_pthread_rwlock_init
	MTSHIM	___pthread_rwlock_rdlock,     _shim_pthread_rwlock_rdlock
	MTSHIM	___pthread_rwlock_wrlock,     _shim_pthread_rwlock_wrlock
	MTSHIM	___pthread_rwlock_tryrdlock,  _shim_pthread_rwlock_tryrdlock
	MTSHIM	___pthread_rwlock_trywrlock,  _shim_pthread_rwlock_trywrlock
	MTSHIM	___pthread_rwlock_unlock,     _shim_pthread_rwlock_unlock
	MTSHIM	___pthread_rwlock_destroy,    _shim_pthread_rwlock_destroy

	;; pthread_t IDENTITY (pthread_sync_shim.c). abigen deep-copies pthread_t
	;; BY VALUE (a pointer to _opaque_pthread_t), losing thread identity, and
	;; never wrote pthread_create's OUT-PARAM back -- so the caller's handle
	;; stayed 0 and every CallWorker()/join/kill on it silently no-opped while
	;; the thread itself ran fine. The shims hand out the thread's MACH PORT as
	;; a stable 32-bit token. Guard 99_pthread_create_handle (ON=42, OFF=9).
	MTSHIM	___pthread_create,            _shim_pthread_create
	MTSHIM	___pthread_self,              _shim_pthread_self
	MTSHIM	___pthread_join,              _shim_pthread_join
	MTSHIM	___pthread_detach,            _shim_pthread_detach
	MTSHIM	___pthread_equal,             _shim_pthread_equal
	MTSHIM	___pthread_kill,              _shim_pthread_kill
	MTSHIM	___pthread_cancel,            _shim_pthread_cancel
	MTSHIM	___pthread_setschedparam,     _shim_pthread_setschedparam
	MTSHIM	___pthread_getschedparam,     _shim_pthread_getschedparam
	MTSHIM	___pthread_get_stackaddr_np,  _shim_pthread_get_stackaddr_np
	MTSHIM	___pthread_get_stacksize_np,  _shim_pthread_get_stacksize_np
	MTSHIM	___pthread_mach_thread_np,    _shim_pthread_mach_thread_np
	MTSHIM	___pthread_threadid_np,       _shim_pthread_threadid_np
	MTSHIM	___AudioHardwareGetProperty,  _shim_AudioHardwareGetProperty
	MTSHIM	___AudioDeviceGetProperty,    _shim_AudioDeviceGetProperty
	MTSHIM	___AudioDeviceGetPropertyInfo, _shim_AudioDeviceGetPropertyInfo
	MTSHIM	___AudioDeviceAddIOProc,      _shim_AudioDeviceAddIOProc
	MTSHIM	___AudioDeviceRemoveIOProc,   _shim_AudioDeviceRemoveIOProc
	MTSHIM	___AudioDeviceStart,          _shim_AudioDeviceStart
	MTSHIM	___AudioDeviceStop,           _shim_AudioDeviceStop
	MTSHIM	___AudioUnitSetProperty, _shim_AudioUnitSetProperty
	MTSHIM	___AudioUnitGetProperty, _shim_AudioUnitGetProperty
	MTSHIM	___AudioUnitInitialize, _shim_AudioUnitInitialize
	MTSHIM	___AudioUnitUninitialize, _shim_AudioUnitUninitialize
	MTSHIM	___AudioOutputUnitStart, _shim_AudioOutputUnitStart
	MTSHIM	___AudioOutputUnitStop, _shim_AudioOutputUnitStop
	MTSHIM	___AudioUnitGetParameter, _shim_AudioUnitGetParameter
	MTSHIM	___AudioUnitSetParameter, _shim_AudioUnitSetParameter
	MTSHIM	___AudioUnitAddRenderNotify, _shim_AudioUnitAddRenderNotify
	MTSHIM	___AudioUnitRemoveRenderNotify, _shim_AudioUnitRemoveRenderNotify
	MTSHIM	___AudioConverterFillComplexBuffer, _shim_AudioConverterFillComplexBuffer
	MTSHIM	___qsort, _shim_qsort
	;; pthread_once: the i386 once-control block ({long __sig; opaque} = 8B,
	;; 4-byte sig) is NOT a valid native os_once_t, and its init routine is an
	;; i386 void(void) fn ptr. abigen's forward to native pthread_once trips
	;; _os_once_gate_corruption_abort (Civ IV s30). pthread_sync_shim.c runs
	;; the once semantics itself, keyed on the stable i386 control address, and
	;; calls the init routine via _86x64_call_i386. Universal.
	MTSHIM	___pthread_once,              _shim_pthread_once

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
	MTSHIM	___IONotificationPortCreate,         _shim_IONotificationPortCreate
	MTSHIM	___IONotificationPortGetRunLoopSource, _shim_IONotificationPortGetRunLoopSource
	MTSHIM	___IONotificationPortDestroy,        _shim_IONotificationPortDestroy
	MTSHIM	___IOServiceClose,                   _shim_IOServiceClose
	MTSHIM	___IOAllowPowerChange,               _shim_IOAllowPowerChange
	MTSHIM	___IOCancelPowerChange,              _shim_IOCancelPowerChange
	;; IOKit device-notification (hot-plug) registration: consumes the port
	;; TOKEN from IONotificationPortCreate (abigen zero-extended it into a
	;; native IONotificationPortRef -> deref fault at 0xF1A00000, Halo), plus
	;; the CF-ref return/out-param and callback fn-ptr cases abigen can't
	;; marshal for IOKit. See iokit_shim.c device-notification section.
	MTSHIM	___IOServiceMatching,                 _shim_IOServiceMatching
	MTSHIM	___IOServiceAddMatchingNotification,  _shim_IOServiceAddMatchingNotification
	MTSHIM	___IOServiceAddInterestNotification,  _shim_IOServiceAddInterestNotification
	MTSHIM	___IORegistryEntryCreateCFProperties, _shim_IORegistryEntryCreateCFProperties
	MTSHIM	___CFRunLoopAddSource,               _shim_CFRunLoopAddSource
	MTSHIM	___CFRunLoopRemoveSource,            _shim_CFRunLoopRemoveSource

	;; IOKit CFPlugIn (COM) interfaces: the entry points plus one i386-callable
	;; trampoline per bridged interface method, referenced from the i386
	;; vtables of the low-4GB proxies. See iokit_com_shim.c.
	MTSHIM	___IOCreatePlugInInterfaceForService, _shim_IOCreatePlugInInterfaceForService
	MTSHIM	___IODestroyPlugInInterface,          _shim_IODestroyPlugInInterface
	MTSHIM	___iokcom_QueryInterface, _shim_iokcom_QueryInterface
	MTSHIM	___iokcom_AddRef, _shim_iokcom_AddRef
	MTSHIM	___iokcom_Release, _shim_iokcom_Release
	MTSHIM	___iokcom_Probe, _shim_iokcom_Probe
	MTSHIM	___iokcom_Start, _shim_iokcom_Start
	MTSHIM	___iokcom_Stop, _shim_iokcom_Stop
	MTSHIM	___iokcom_createAsyncEventSource, _shim_iokcom_createAsyncEventSource
	MTSHIM	___iokcom_getAsyncEventSource, _shim_iokcom_getAsyncEventSource
	MTSHIM	___iokcom_createAsyncPort, _shim_iokcom_createAsyncPort
	MTSHIM	___iokcom_getAsyncPort, _shim_iokcom_getAsyncPort
	MTSHIM	___iokcom_open, _shim_iokcom_open
	MTSHIM	___iokcom_close, _shim_iokcom_close
	MTSHIM	___iokcom_setRemovalCallback, _shim_iokcom_setRemovalCallback
	MTSHIM	___iokcom_getElementValue, _shim_iokcom_getElementValue
	MTSHIM	___iokcom_setElementValue, _shim_iokcom_setElementValue
	MTSHIM	___iokcom_queryElementValue, _shim_iokcom_queryElementValue
	MTSHIM	___iokcom_startAllQueues, _shim_iokcom_startAllQueues
	MTSHIM	___iokcom_stopAllQueues, _shim_iokcom_stopAllQueues
	MTSHIM	___iokcom_allocQueue, _shim_iokcom_allocQueue
	MTSHIM	___iokcom_allocOutputTransaction, _shim_iokcom_allocOutputTransaction
	MTSHIM	___iokcom_setReport, _shim_iokcom_setReport
	MTSHIM	___iokcom_getReport, _shim_iokcom_getReport
	MTSHIM	___iokcom_copyMatchingElements, _shim_iokcom_copyMatchingElements
	MTSHIM	___iokcom_setInterruptReportHandlerCallback, _shim_iokcom_setInterruptReportHandlerCallback
	MTSHIM	___iokcom_q_create, _shim_iokcom_q_create
	MTSHIM	___iokcom_q_dispose, _shim_iokcom_q_dispose
	MTSHIM	___iokcom_q_addElement, _shim_iokcom_q_addElement
	MTSHIM	___iokcom_q_removeElement, _shim_iokcom_q_removeElement
	MTSHIM	___iokcom_q_hasElement, _shim_iokcom_q_hasElement
	MTSHIM	___iokcom_q_start, _shim_iokcom_q_start
	MTSHIM	___iokcom_q_stop, _shim_iokcom_q_stop
	MTSHIM	___iokcom_q_getNextEvent, _shim_iokcom_q_getNextEvent
	MTSHIM	___iokcom_q_setEventCallout, _shim_iokcom_q_setEventCallout
	MTSHIM	___iokcom_q_getEventCallout, _shim_iokcom_q_getEventCallout

	;; Variadic AppKit alert panels (abigen skips ALL variadic functions, so
	;; the raw binds passed arena handles straight into native AppKit, which
	;; then ran objc_msgSend on a handle). objc_shim.c alert_panel_common.
	MTSHIM	___NSRunAlertPanel,              _shim_NSRunAlertPanel
	MTSHIM	___NSRunCriticalAlertPanel,      _shim_NSRunCriticalAlertPanel
	MTSHIM	___NSRunInformationalAlertPanel, _shim_NSRunInformationalAlertPanel
	MTSHIM	___NSGetAlertPanel,              _shim_NSGetAlertPanel
	;; Variadic CF formatting (same class as the panels: abigen skips
	;; variadics, so the raw native bind read registers for i386 stack varargs).
	MTSHIM	___CFStringCreateWithFormat,     _shim_CFStringCreateWithFormat
	;; va_list CF formatting (the AndArguments siblings): abigen CAN'T marshal a
	;; va_list arg (i386 char* vs x86_64 __va_list_tag), so its auto-shim
	;; deep-copied garbage. Same cure, reading varargs through the va_list pointer.
	MTSHIM	___CFStringCreateWithFormatAndArguments, _shim_CFStringCreateWithFormatAndArguments
	MTSHIM	___CFStringAppendFormatAndArguments,     _shim_CFStringAppendFormatAndArguments
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
   ;; CG text/path/gradient/shadow draw family (KEEP IN SYNC with g_geo[]).
   GEOSHIM_I ___CGContextShowTextAtPoint,    76
   GEOSHIM_I ___CGContextSetTextPosition,    77
   GEOSHIM_I ___CGContextSelectFont,         78
   GEOSHIM_I ___CGContextAddArc,             79
   GEOSHIM_I ___CGContextAddArcToPoint,      80
   GEOSHIM_I ___CGContextAddQuadCurveToPoint, 81
   GEOSHIM_I ___CGContextAddCurveToPoint,    82
   GEOSHIM_I ___CGContextDrawLinearGradient, 83
   GEOSHIM_I ___CGContextDrawRadialGradient, 84
   GEOSHIM_I ___CGContextSetShadow,          85
   GEOSHIM_I ___CGContextSetShadowWithColor, 86
   GEOSHIM_I ___CGContextSetTextMatrix,      87
   GEOSHIM_I ___CGContextDrawLayerInRect,    88
   ;; CATransform3D (CoreAnimation) C builder family (KEEP IN SYNC w/ g_geo[]).
   ;; Struct returns use the i386 hidden-ptr stret convention -> GEOSHIM_S;
   ;; the bool-returning IsIdentity -> GEOSHIM_I.
   GEOSHIM_S ___CATransform3DMakeRotation,       89
   GEOSHIM_S ___CATransform3DMakeScale,          90
   GEOSHIM_S ___CATransform3DMakeTranslation,    91
   GEOSHIM_S ___CATransform3DRotate,             92
   GEOSHIM_S ___CATransform3DScale,              93
   GEOSHIM_S ___CATransform3DTranslate,          94
   GEOSHIM_S ___CATransform3DConcat,             95
   GEOSHIM_S ___CATransform3DInvert,             96
   GEOSHIM_S ___CATransform3DMakeAffineTransform, 97
   GEOSHIM_S ___CATransform3DGetAffineTransform,  98
   GEOSHIM_I ___CATransform3DIsIdentity,         99

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

;; libxml2 deprecated global memory-allocator override (xml_shim.c). The
;; override API was removed on modern macOS (>=15.4): xmlMemSetup/xmlMemGet
;; return -1, but legacy iWork/iLife (SFAXMLMemoryManager) ASSERTS ==0. The
;; xmlMemSetup shim reports success (no-op); the xmlMemGet shim reports success
;; AND hands back four CALLABLE low-4GB allocator trampolines (the manager
;; STORES then INVOKES them i386-cdecl, so they cannot be native 8-byte ptrs).
   MTSHIM ___xmlMemSetup, _shim_xmlMemSetup
   MTSHIM ___xmlMemGet,   _shim_xmlMemGet
;; The four allocator trampolines ___xmlMemGet writes into its out-params.
;; Reached only via the stored i386 function pointers, so they are NOT
;; ___-prefixed interpose targets; they forward to libabiconv's low-4GB heap
;; (native malloc would return a >4GB ptr that truncates into the i386 slot).
   MTSHIM _xml_malloc_tramp,  _shim_xml_malloc
   MTSHIM _xml_free_tramp,    _shim_xml_free
   MTSHIM _xml_realloc_tramp, _shim_xml_realloc
   MTSHIM _xml_strdup_tramp,  _shim_xml_strdup

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
;; Classic Foundation NS_DURING (pre-@try SDKs — Quinn, Civ IV): the OLDER
;; NSHandler2 spelling of the SAME setjmp-exception model. C names carry a
;; leading underscore (_NSAddHandler2), so the mach-o import is __NSAddHandler2
;; and static-interpose (PREFIX=__) redirects it to ____NSAddHandler2 here.
;; These forward to the objc_exception_* chain logic (see objc_shim.c): both
;; NSHandler2 and @try frames share ONE per-thread exc_chain + ____setjmp +
;; shim_objc_exception_throw. Unshimmed, native _NSAddHandler2 over-pops the
;; i386 4-byte frame -> fused-PC SIGSEGV (Quinn play crash, no throw needed).
	MTSHIM	____NSAddHandler2,                 _shim_NSAddHandler2
	MTSHIM	____NSRemoveHandler2,              _shim_NSRemoveHandler2
	MTSHIM	____NSExceptionObjectFromHandler2, _shim_NSExceptionObjectFromHandler2
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
;; code addresses are always low-4GB. Restored ONLY by x64_exc_longjmp below;
;; the pair stays consistent by interposing both ends. C setjmp/sigsetjmp take
;; the same path: abigen's forward to native _setjmp wrote 148 bytes over the
;; caller's frame (PvZ jmp_buf at ebp-0x60 clobbered the saved esi).
;; ponytail: the signal mask is not saved/restored; add it if a target
;; longjmps out of a signal handler.
	global	____setjmp
	global	___setjmp
	global	___sigsetjmp
___setjmp:
___sigsetjmp:
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
;; i386 longjmp(buf, val) family -> x64_exc_longjmp; val 0 resumes as 1.
	global	___longjmp
	global	____longjmp
	global	___siglongjmp
___longjmp:
____longjmp:
___siglongjmp:
	cmp	qword [rel __dyld_stub_binder_flag],	0
	je	.l1
	mov	rsp,	qword [rel __dyld_stub_binder_flag]
	add	rsp,	16
	mov	qword [rel __dyld_stub_binder_flag],	0
.l1:
	mov	edi,	dword [rsp + 4]
	mov	esi,	dword [rsp + 8]
	mov	eax,	1
	test	esi,	esi
	cmovz	esi,	eax
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
	MTSHIM	___method_exchangeImplementations, _shim_method_exchangeImplementations
	MTSHIM	___method_copyReturnType,       _shim_method_copyReturnType
	MTSHIM	___method_copyArgumentType,     _shim_method_copyArgumentType
	MTSHIM	___objc_getClassList,           _shim_objc_getClassList
	MTSHIM	___objc_copyClassList,          _shim_objc_copyClassList
	MTSHIM	___object_getInstanceVariable,  _shim_object_getInstanceVariable
	MTSHIM	___object_setInstanceVariable,  _shim_object_setInstanceVariable

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
;; QuickDraw offscreen GWorld + PixMap (qd_gworld.c — CG-backed ports).
;; Excluded from abigen (custom.syms): the GWorld/PixMap handles are ours.
	MTSHIM	___NewGWorld,          _shim_NewGWorld
	MTSHIM	___NewGWorldFromPtr,   _shim_NewGWorldFromPtr
	MTSHIM	___QTNewGWorldFromPtr, _shim_QTNewGWorldFromPtr
	MTSHIM	___DisposeGWorld,      _shim_DisposeGWorld
	MTSHIM	___UpdateGWorld,       _shim_UpdateGWorld
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

	;; GetCTable/DisposeCTable — classic 'clut' color-table loader. Native
	;; QuickDraw is gone so the abigen ___GetCTable returned NULL, and Civ IV's
	;; HBITMAP_Mac paletted-bitmap path deref'd the NULL handle (NULL-deref at
	;; 0x0 building an indexed bitmap's color table). qd_gworld.c returns a real
	;; correctly-sized ColorTable Handle (the same QDColorTable the indexed-
	;; GWorld path consumes). ___GetCTable is abigen-generated (forwards to the
	;; dead native) so it's excluded via the handshim ignore set (this MTSHIM).
	;; (___DisposeCTable already MTSHIM'd below near the qd_shim.c block)
	MTSHIM	___GetCTable,          _shim_GetCTable

;; ---------------------------------------------------------------------
;; QuickDraw drawing substrate — CG-backed (qd_gworld.c). CopyBits/CopyMask
;; image blits, rect/rgn fills, pen/line, PICT-less draw, offscreen buffer
;; flush, GDevice. Region args arrive as objc-arena handles wrapping the
;; surviving-native RgnHandle (abigen ___NewRgn wrap) and are unwrapped here.
;; EraseRect + QDFlushPortBuffer are also abigen-generated (they call a GONE
;; native) -> excluded via custom.syms so these real impls win.
	MTSHIM	___CopyMask,           _shim_CopyMask
	MTSHIM	___CopyDeepMask,       _shim_CopyDeepMask
	MTSHIM	___EraseRect,          _shim_EraseRect
	MTSHIM	___EraseRgn,           _shim_EraseRgn
	MTSHIM	___PaintRgn,           _shim_PaintRgn
	MTSHIM	___FillRgn,            _shim_FillRgn
	MTSHIM	___FrameRgn,           _shim_FrameRgn
	MTSHIM	___InvertRect,         _shim_InvertRect
	MTSHIM	___InvertRgn,          _shim_InvertRgn
	MTSHIM	___Line,               _shim_Line
	MTSHIM	___Move,               _shim_Move
	MTSHIM	___PenMode,            _shim_PenMode
	MTSHIM	___PenPat,             _shim_PenPat
	MTSHIM	___BackPat,            _shim_BackPat
	MTSHIM	___HidePen,            _shim_HidePen
	MTSHIM	___ShowPen,            _shim_ShowPen
	MTSHIM	___TextMode,           _shim_TextMode
	MTSHIM	___GetPortTextSize,    _shim_GetPortTextSize
	MTSHIM	___GetPortTextMode,    _shim_GetPortTextMode
	MTSHIM	___GetGDevice,         _shim_GetGDevice
	MTSHIM	___QDError,            _shim_QDError
	MTSHIM	___QDFlushPortBuffer,  _shim_QDFlushPortBuffer
	MTSHIM	___QDIsPortBuffered,   _shim_QDIsPortBuffered
	MTSHIM	___QDIsPortBufferDirty,_shim_QDIsPortBufferDirty
	MTSHIM	___QDBeginCGContext,   _shim_QDBeginCGContext
	MTSHIM	___QDEndCGContext,     _shim_QDEndCGContext

;; ---------------------------------------------------------------------
;; QuickDraw PICT playback (qd_gworld.c). classic PICT decode via ImageIO's
;; surviving com.apple.pict reader; DrawPicture renders into the current port,
;; GetPicture loads the 'PICT' resource through rm_shim, and the modern
;; QDPictToCGContext provider->CGContext API is reimplemented on ImageIO.
;; DrawPicture/GetPicture/GetCursor/SetCursor are also abigen-faulting shims
;; (they call a REMOVED native) -> excluded via custom.syms so these win.
	MTSHIM	___DrawPicture,             _shim_DrawPicture
	MTSHIM	___GetPicture,              _shim_GetPicture
	MTSHIM	___QDPictCreateWithProvider,_shim_QDPictCreateWithProvider
	MTSHIM	___QDPictCreateWithURL,     _shim_QDPictCreateWithURL
	MTSHIM	___QDPictGetBounds,         _shim_QDPictGetBounds
	MTSHIM	___QDPictDrawToCGContext,   _shim_QDPictDrawToCGContext
	MTSHIM	___QDPictRelease,           _shim_QDPictRelease
	MTSHIM	___GetCursor,               _shim_GetCursor
	MTSHIM	___SetCursor,               _shim_SetCursor
	MTSHIM	___SetCCursor,              _shim_SetCCursor

;; ---------------------------------------------------------------------
;; Display Manager (DM*) screen-device + mode enumeration (qd_gworld.c).
;; Real main-screen GDevice; empty (safe) display-mode list. abigen-faulting
;; -> excluded via custom.syms. (DMGetDisplayIDByGDevice etc. in carbon_ui_shim.)
	MTSHIM	___DMGetFirstScreenDevice,             _shim_DMGetFirstScreenDevice
	MTSHIM	___DMGetNextScreenDevice,              _shim_DMGetNextScreenDevice
	MTSHIM	___DMNewDisplayModeList,               _shim_DMNewDisplayModeList
	MTSHIM	___DMGetIndexedDisplayModeFromList,    _shim_DMGetIndexedDisplayModeFromList
	MTSHIM	___DMDisposeList,                      _shim_DMDisposeList
	MTSHIM	___NewDMDisplayModeListIteratorUPP,    _shim_NewDMDisplayModeListIteratorUPP
	MTSHIM	___DisposeDMDisplayModeListIteratorUPP,_shim_DisposeDMDisplayModeListIteratorUPP

;; ---------------------------------------------------------------------
;; C++ runtime (cxx_shim.c): libstdc++.6 imports of legacy i386 binaries.
;; Guards are implemented locally (native libc++abi sizes its lock words
;; for x86_64); new/delete use the low-4GB shim heap; _Rb_tree_* are
;; reimplemented for the 4-byte i386 node layout.
	MTSHIM	_____cxa_guard_acquire, _shim_cxa_guard_acquire
	MTSHIM	_____cxa_guard_release, _shim_cxa_guard_release
	MTSHIM	_____cxa_guard_abort,   _shim_cxa_guard_abort
	MTSHIM	_____cxa_atexit,        _shim_cxa_atexit
	MTSHIM	____ZSt15set_new_handlerPFvvE, _shim_ZSt15set_new_handler
	MTSHIM	____Znwm,               _shim_Znwm
	MTSHIM	____Znam,               _shim_Znam
	MTSHIM	____ZnwmRKSt9nothrow_t, _shim_Znwm_nothrow
	MTSHIM	____ZnamRKSt9nothrow_t, _shim_Znam_nothrow
	MTSHIM	____ZdlPv,              _shim_ZdlPv
	MTSHIM	____ZdaPv,              _shim_ZdaPv
	MTSHIM	____ZdlPvRKSt9nothrow_t, _shim_ZdlPv_nothrow
	; std::ios_base::Init::Init / ~Init — <iostream> per-TU static ctor. Binds to
	; NATIVE libstdc++ whose 8-byte ret OVER-POPS the i386 4-byte frame (universal
	; C++ ctors-ON crash). Route through the i386 4-byte-ret discipline; the shim
	; still calls the real native ctor/dtor. C1/C2 + D1/D2 funnel to one impl.
	MTSHIM	____ZNSt8ios_base4InitC1Ev, _shim_ios_base_Init_ctor
	MTSHIM	____ZNSt8ios_base4InitC2Ev, _shim_ios_base_Init_ctor
	MTSHIM	____ZNSt8ios_base4InitD1Ev, _shim_ios_base_Init_dtor
	MTSHIM	____ZNSt8ios_base4InitD2Ev, _shim_ios_base_Init_dtor
	MTSHIM	____ZSt17__throw_bad_allocv,        _shim_throw_bad_alloc
	MTSHIM	____ZSt20__throw_length_errorPKc,   _shim_throw_length_error
	MTSHIM	____ZSt20__throw_out_of_rangePKc,   _shim_throw_out_of_range
	MTSHIM	____ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base,  _shim_rb_increment
	MTSHIM	____ZSt18_Rb_tree_incrementPKSt18_Rb_tree_node_base, _shim_rb_increment
	MTSHIM	____ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base,  _shim_rb_decrement
	MTSHIM	____ZSt18_Rb_tree_decrementPKSt18_Rb_tree_node_base, _shim_rb_decrement
	MTSHIM	____ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_, _shim_rb_insert_rebalance
	MTSHIM	____ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_,      _shim_rb_rebalance_for_erase

	; RTTI (cxx_shim.c). __dynamic_cast + type_info compares reimplemented on the
	; i386 (4-byte-field) typeinfo layout; native libstdc++ walks 8-byte typeinfo.
	; bad_typeid/bad_cast/pure_virtual = loud aborts (can't throw across translated
	; frames). The three __cxxabiv1 type_info VTABLE symbols are exported as data
	; sentinels below so static-interpose redirects every typeinfo's vtable-ptr to a
	; known low-4GB address -> the typeinfo becomes self-describing (kind discrimination).
	MTSHIM	_____dynamic_cast,                _shim_dynamic_cast
	MTSHIM	_____cxa_bad_typeid,              _shim_cxa_bad_typeid
	MTSHIM	_____cxa_bad_cast,                _shim_cxa_bad_cast
	MTSHIM	_____cxa_pure_virtual,            _shim_cxa_pure_virtual
	MTSHIM	____ZNKSt9type_infoeqERKS_,       _shim_type_info_eq
	MTSHIM	____ZNKSt9type_infoneERKS_,       _shim_type_info_ne
	MTSHIM	____ZNKSt9type_infoltERKS_,       _shim_type_info_before
	MTSHIM	____ZNSt9exceptionD2Ev,           _shim_exception_dtor
	MTSHIM	____ZNSt9exceptionD1Ev,           _shim_exception_dtor
	MTSHIM	____ZNKSt9exception4whatEv,       _shim_exception_what

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
	; FSRefMakePath itself is native and works; hand-shimmed ONLY to enforce the
	; output-buffer invariant — Apple documents `path` as UNDEFINED on failure, so a
	; caller that skips the OSStatus check reads its own stack garbage as a C string
	; (Halo 0x2da50c -> CFStringCreateWithCString -> NULL -> CFStringGetCString SIGSEGV).
	MTSHIM	___FSRefMakePath,   _shim_FSRefMakePath
	; FindFolder is the PRODUCER of the (vRefNum, dirID) pair FSpMakeFSRef consumes.
	; Modern CarbonCore returns a small internal token (6, 7, 8 ...) as the dirID, not a
	; catalog node id, and every API that once consumed a dirID is deleted — so the pair
	; is a dead end. We delegate to the native FSRef-based FSFindFolder and mint a dirID
	; our own consumer half can resolve. See carbon_fsspec_shim.c.
	MTSHIM	___FindFolder,      _shim_FindFolder

	; The classic File Manager WRITE path (carbon_fsspec_shim.c). These were NULL-JUMP
	; ORPHANS: the legacy shim pass reads the 10.6 SDK's i386 headers and emits a
	; pass-through bridge that calls a native which has never existed on x86_64 -- checked
	; against the REAL Mojave 10.14 x86_64 CarbonCore (the last macOS that ran 32-bit code),
	; only 2 of our 114 orphans exist there, and none of these. So the bridge could only
	; `call 0`, and nothing else in the process supplies them. This is the family a classic
	; app uses to SAVE.
	MTSHIM	___FSpCreate,       _shim_FSpCreate
	MTSHIM	___FSpDirCreate,    _shim_FSpDirCreate
	MTSHIM	___DirCreate,       _shim_DirCreate
	MTSHIM	___HDelete,         _shim_HDelete
	MTSHIM	___FSpRename,       _shim_FSpRename
	MTSHIM	___HRename,         _shim_HRename
	MTSHIM	___CatMove,         _shim_CatMove
	MTSHIM	___FSpCatMove,      _shim_FSpCatMove
	MTSHIM	___FSpExchangeFiles, _shim_FSpExchangeFiles
	MTSHIM	___FSpSetFInfo,     _shim_FSpSetFInfo
	MTSHIM	___FlushVol,        _shim_FlushVol

	; Classic resource-fork open with the FLATTENED (data-fork) fallback
	; (rsrc_datafork_shim.c). codesign refuses any bundle carrying an HFS
	; resource fork, so `m64 forks --flatten` migrates *.rsrc payloads into the
	; data fork / a .86x64rsrc sidecar; this makes FSOpenResFile find them there.
	MTSHIM	___FSOpenResFile,   _shim_FSOpenResFile

	; ===== Civ IV: removed Carbon/QuickDraw/QuickTime-callback symbols =====
	; 139 classic APIs deleted from 64-bit macOS that Civ IV's translated __jt_ptrs
	; slots bind to; without these dyld fails LOAD (Symbol not found _RGBBackColor ...).
	; Grouped by single-purpose shim file; see each file's header for the rationale.
	; --- QuickDraw 2D (qd_shim.c) ---
	MTSHIM	___BackColor,                         _shim_BackColor
	MTSHIM	___ClipRect,                          _shim_ClipRect
	MTSHIM	___ClosePicture,                      _shim_ClosePicture
	MTSHIM	___CopyBits,                          _shim_CopyBits
	MTSHIM	___CreateCGContextForPort,            _shim_CreateCGContextForPort
	MTSHIM	___CreateNewPort,                     _shim_CreateNewPort
	MTSHIM	___CreateNewPortForCGDisplayID,       _shim_CreateNewPortForCGDisplayID
	MTSHIM	___DisposeCTable,                     _shim_DisposeCTable
	MTSHIM	___DisposePort,                       _shim_DisposePort
	MTSHIM	___DrawText,                          _shim_DrawText
	MTSHIM	___FillRect,                          _shim_FillRect
	MTSHIM	___ForeColor,                         _shim_ForeColor
	MTSHIM	___FrameRect,                         _shim_FrameRect
	MTSHIM	___GetBackColor,                      _shim_GetBackColor
	MTSHIM	___GetClip,                           _shim_GetClip
	MTSHIM	___GetDeviceList,                     _shim_GetDeviceList
	MTSHIM	___GetFontInfo,                       _shim_GetFontInfo
	MTSHIM	___GetForeColor,                      _shim_GetForeColor
	MTSHIM	___GetMainDevice,                     _shim_GetMainDevice
	MTSHIM	___GetNextDevice,                     _shim_GetNextDevice
	MTSHIM	___GetPenState,                       _shim_GetPenState
	MTSHIM	___GetPort,                           _shim_GetPort
	MTSHIM	___GetPortBackColor,                  _shim_GetPortBackColor
	MTSHIM	___GetPortBitMapForCopyBits,          _shim_GetPortBitMapForCopyBits
	MTSHIM	___GetPortBounds,                     _shim_GetPortBounds
	MTSHIM	___GetPortClipRegion,                 _shim_GetPortClipRegion
	MTSHIM	___GetPortForeColor,                  _shim_GetPortForeColor
	MTSHIM	___GetPortTextFace,                   _shim_GetPortTextFace
	MTSHIM	___GetPortTextFont,                   _shim_GetPortTextFont
	MTSHIM	___GetPortVisibleRegion,              _shim_GetPortVisibleRegion
	MTSHIM	___GetQDGlobalsBlack,                 _shim_GetQDGlobalsBlack
	MTSHIM	___GetQDGlobalsDarkGray,              _shim_GetQDGlobalsDarkGray
	MTSHIM	___GetQDGlobalsGray,                  _shim_GetQDGlobalsGray
	MTSHIM	___GetQDGlobalsLightGray,             _shim_GetQDGlobalsLightGray
	MTSHIM	___GetQDGlobalsWhite,                 _shim_GetQDGlobalsWhite
	MTSHIM	___GlobalToLocal,                     _shim_GlobalToLocal
	MTSHIM	___KillPicture,                       _shim_KillPicture
	MTSHIM	___LineTo,                            _shim_LineTo
	MTSHIM	___LocalToGlobal,                     _shim_LocalToGlobal
	MTSHIM	___MoveTo,                            _shim_MoveTo
	MTSHIM	___OpenCPicture,                      _shim_OpenCPicture
	MTSHIM	___PaintRect,                         _shim_PaintRect
	MTSHIM	___PenNormal,                         _shim_PenNormal
	MTSHIM	___PenSize,                           _shim_PenSize
	MTSHIM	___QDIsNamedPixMapCursorRegistered,   _shim_QDIsNamedPixMapCursorRegistered
	MTSHIM	___QDRegisterNamedPixMapCursor,       _shim_QDRegisterNamedPixMapCursor
	MTSHIM	___QDSetNamedPixMapCursor,            _shim_QDSetNamedPixMapCursor
	MTSHIM	___QDUnregisterNamedPixMapCursur,     _shim_QDUnregisterNamedPixMapCursur
	MTSHIM	___RGBBackColor,                      _shim_RGBBackColor
	MTSHIM	___RGBForeColor,                      _shim_RGBForeColor
	MTSHIM	___AddPt,                             _shim_AddPt
	MTSHIM	___SubPt,                             _shim_SubPt
	MTSHIM	___EqualPt,                           _shim_EqualPt
	MTSHIM	___PtInRect,                          _shim_PtInRect
	MTSHIM	___PinRect,                           _shim_PinRect
	MTSHIM	___SetClip,                           _shim_SetClip
	MTSHIM	___SetGDevice,                        _shim_SetGDevice
	MTSHIM	___SetOrigin,                         _shim_SetOrigin
	MTSHIM	___SetPenState,                       _shim_SetPenState
	MTSHIM	___SetPort,                           _shim_SetPort
	MTSHIM	___SetPortBounds,                     _shim_SetPortBounds
	MTSHIM	___SetQDGlobalsRandomSeed,            _shim_SetQDGlobalsRandomSeed
	MTSHIM	___TestDeviceAttribute,               _shim_TestDeviceAttribute
	MTSHIM	___TextFace,                          _shim_TextFace
	MTSHIM	___TextFont,                          _shim_TextFont
	MTSHIM	___TextSize,                          _shim_TextSize
	MTSHIM	___TextWidth,                         _shim_TextWidth
	; --- AGL drawable binding of a Carbon window (agl_drawable_shim.c) ---
	;     GetWindowPort now returns a REAL window-backed qd_port, so the
	;     abigen legacy bridge must NOT keep forwarding it to native
	;     aglSetDrawable as a CGrafPtr (it would deref a non-GrafPort).
	;     These two MTSHIMs both intercept the pair AND remove it from the
	;     abigen legacy pass. aglGetDrawable is here because native AGL is
	;     BLIND to an aglSetWindowRef attach (measured), and Carbon+AGL apps
	;     save/restore the drawable around QuickDraw UI.
	MTSHIM	___aglSetDrawable,                    _shim_aglSetDrawable
	MTSHIM	___aglGetDrawable,                    _shim_aglGetDrawable
	; --- AGL renderer-info enumeration (agl_renderer_shim.c) ---
	;     aglQueryRendererInfo takes a QuickDraw GDHandle and is a DEAD entry
	;     point on 64-bit (measured: NULL for every input), so classic GPU
	;     capability checks count zero renderers. Routed to the surviving
	;     aglQueryRendererInfoForCGDirectDisplayIDs via the qd_gworld GDevice
	;     registry. The whole family is here because a real AGLRendererInfo is
	;     a >4GB native pointer that must be arena-wrapped end to end.
	MTSHIM	___aglQueryRendererInfo,              _shim_aglQueryRendererInfo
	MTSHIM	___aglQueryRendererInfoForCGDirectDisplayIDs, _shim_aglQueryRendererInfoForCGDirectDisplayIDs
	MTSHIM	___aglNextRendererInfo,               _shim_aglNextRendererInfo
	MTSHIM	___aglDescribeRenderer,               _shim_aglDescribeRenderer
	MTSHIM	___aglDestroyRendererInfo,            _shim_aglDestroyRendererInfo
	; --- CGL/AGL CONTEXT OBJECT (cgl_macro_shim.c) ---
	;     <OpenGL/CGLMacro.h> #defines every glXxx() to jump through the
	;     context object's embedded GLIFunctionDispatch table, so a 32-bit
	;     game DEREFERENCES its AGLContext and never calls a GL symbol. A
	;     proxy arena handle read as a struct _CGLContextObject yields a
	;     truncated pointer for ->rend and 0 for every ->disp entry (measured:
	;     Halo jmp *0 at disp offset 0x1D8 = get_string). Every entry point
	;     that produces or consumes a context is owned here so the i386-layout
	;     SHADOW is the only thing the app ever holds — token ownership has to
	;     be end to end.
	; --- Carbon event PARAMETERS (carbon_event_param_shim.c): refs are 4-byte
	;     handles on i386 and 8-byte pointers natively, CGFloat geometry is float
	;     vs double, HICommand embeds a MenuRef. The generic bridge truncated a
	;     WindowRef to its low half (PvZ windowed ignored every click).
	MTSHIM	___GetEventParameter,                 _shim_GetEventParameter
	MTSHIM	___SetEventParameter,                 _shim_SetEventParameter
	MTSHIM	___aglCreateContext,                  _shim_aglCreateContext
	MTSHIM	___aglDestroyContext,                 _shim_aglDestroyContext
	MTSHIM	___aglGetCurrentContext,              _shim_aglGetCurrentContext
	MTSHIM	___aglGetCGLContext,                 _shim_aglGetCGLContext
	MTSHIM	___aglSetCurrentContext,              _shim_aglSetCurrentContext
	MTSHIM	___aglUpdateContext,                  _shim_aglUpdateContext
	MTSHIM	___aglSwapBuffers,                    _shim_aglSwapBuffers
	MTSHIM	___aglSetInteger,                     _shim_aglSetInteger
	MTSHIM	___aglGetInteger,                     _shim_aglGetInteger
	MTSHIM	___aglSetFullScreen,                  _shim_aglSetFullScreen
	MTSHIM	___aglGetVirtualScreen,               _shim_aglGetVirtualScreen
	MTSHIM	___aglSetVirtualScreen,               _shim_aglSetVirtualScreen
	MTSHIM	___aglTexImagePBuffer,                _shim_aglTexImagePBuffer
	MTSHIM	___aglSetPBuffer,                     _shim_aglSetPBuffer
	MTSHIM	___CGLCreateContext,                  _shim_CGLCreateContext
	MTSHIM	___CGLDestroyContext,                 _shim_CGLDestroyContext
	MTSHIM	___CGLGetCurrentContext,              _shim_CGLGetCurrentContext
	MTSHIM	___CGLSetCurrentContext,              _shim_CGLSetCurrentContext
	MTSHIM	___CGLSetFullScreen,                  _shim_CGLSetFullScreen
	MTSHIM	___CGLSetFullScreenOnDisplay,         _shim_CGLSetFullScreenOnDisplay
	MTSHIM	___CGLClearDrawable,                  _shim_CGLClearDrawable
	MTSHIM	___CGWarpMouseCursorPosition,         _shim_CGWarpMouseCursorPosition
	; --- Carbon HIToolbox UI + Display Mgr (carbon_ui_shim.c) ---
	MTSHIM	___BeginUpdate,                       _shim_BeginUpdate
	MTSHIM	___FindWindow,                        _shim_FindWindow
	MTSHIM	___MenuSelect,                        _shim_MenuSelect
	MTSHIM	___DMGetDeskRegion,                   _shim_DMGetDeskRegion
	MTSHIM	___DMGetDisplayIDByGDevice,           _shim_DMGetDisplayIDByGDevice
	MTSHIM	___DMGetGDeviceByDisplayID,           _shim_DMGetGDeviceByDisplayID
	MTSHIM	___Draw1Control,                      _shim_Draw1Control
	MTSHIM	___DrawThemeTextBox,                  _shim_DrawThemeTextBox
	MTSHIM	___EndUpdate,                         _shim_EndUpdate
	MTSHIM	___GetControl32BitValue,              _shim_GetControl32BitValue
	MTSHIM	___GetControlPopupMenuHandle,         _shim_GetControlPopupMenuHandle
	MTSHIM	___GetThemeTextDimensions,            _shim_GetThemeTextDimensions
	MTSHIM	___GetWindowFromPort,                 _shim_GetWindowFromPort
	MTSHIM	___GetWindowPort,                     _shim_GetWindowPort
	MTSHIM	___GetWindowRegion,                   _shim_GetWindowRegion
	MTSHIM	___InvalWindowRect,                   _shim_InvalWindowRect
	MTSHIM	___SetControl32BitMaximum,            _shim_SetControl32BitMaximum
	MTSHIM	___SetControl32BitValue,              _shim_SetControl32BitValue
	MTSHIM	___SetControlMaximum,                 _shim_SetControlMaximum
	MTSHIM	___SetPortWindowPort,                 _shim_SetPortWindowPort
	MTSHIM	___SetWRefCon,                        _shim_SetWRefCon
	MTSHIM	___GetWRefCon,                        _shim_GetWRefCon
	MTSHIM	___SetWindowContentColor,             _shim_SetWindowContentColor
	MTSHIM	___SetWindowProxyCreatorAndType,      _shim_SetWindowProxyCreatorAndType
	MTSHIM	___ValidWindowRect,                   _shim_ValidWindowRect
	; --- Carbon IBCarbon NIB window/control materialization (carbon_nib_shim.c) ---
	;     Real functionality: intercept the nib loader, forward to native first, and
	;     custom-build (compositing window + modern controls) when native's gutted
	;     IBCarbonRuntime fails (-5601 non-compositing windows / dropped IBCarbonEditText).
	;     These 5 nib symbols are removed from the abigen legacy pass by their MTSHIM
	;     presence (legacy consider-set scans this file), so no duplicate-symbol clash.
	MTSHIM	___CreateNibReference,                _shim_CreateNibReference
	MTSHIM	___CreateNibReferenceWithCFBundle,    _shim_CreateNibReferenceWithCFBundle
	MTSHIM	___CreateWindowFromNib,               _shim_CreateWindowFromNib
	MTSHIM	___DisposeNibReference,               _shim_DisposeNibReference
	MTSHIM	___SetMenuBarFromNib,                 _shim_SetMenuBarFromNib
	; --- 64-bit Window Manager compositing contract (carbon_window_shim.c) ---
	;     PROGRAMMATIC counterpart of the nib fix above: 64-bit HIToolbox's
	;     NewWindowCommon rejects any window without kWindowCompositingAttribute
	;     (bit 19) with -5601, so every 32-bit-era CreateNewWindow call fails and
	;     the app sees a NULL WindowRef. The shim ORs the bit in and forwards to
	;     the real native window. CreateNewWindow is the ONLY programmatic
	;     creation entry that still exists on 64-bit, so this one line covers the
	;     whole family. MTSHIM presence removes it from the abigen legacy pass.
	MTSHIM	___CreateNewWindow,                   _shim_CreateNewWindow
	; ==== Reclassified abigen-DANGLING classic Carbon (removed on 64-bit) ====
	; abigen forwarded these to now-removed natives (call _X -> dangling abort). Real
	; bridges where a modern equivalent survives, else signature-correct graceful.
	; MTSHIM presence removes each from the abigen legacy pass (no dup-symbol clash).
	; --- Reclassified dangling Control/Window/Menu Mgr -> REAL bridges (carbon_control_shim.c) ---
	MTSHIM	___AdvanceKeyboardFocus,                    _shim_AdvanceKeyboardFocus
	MTSHIM	___AppendResMenu,                           _shim_AppendResMenu
	MTSHIM	___CreatePopupButtonControl,                _shim_CreatePopupButtonControl
	MTSHIM	___CreateScrollBarControl,                  _shim_CreateScrollBarControl
	MTSHIM	___CreateScrollingTextBoxControl,           _shim_CreateScrollingTextBoxControl
	MTSHIM	___DisposeControl,                          _shim_DisposeControl
	MTSHIM	___DisposeMenu,                             _shim_DisposeMenu
	MTSHIM	___DrawGrowIcon,                            _shim_DrawGrowIcon
	MTSHIM	___DumpControlHierarchy,                    _shim_DumpControlHierarchy
	MTSHIM	___EmbedControl,                            _shim_EmbedControl
	MTSHIM	___EnableMenuCommand,                       _shim_EnableMenuCommand
	MTSHIM	___GetControlCommandID,                     _shim_GetControlCommandID
	MTSHIM	___GetControlData,                          _shim_GetControlData
	MTSHIM	___SetControlData,                          _shim_SetControlData
	MTSHIM	___GetControlValue,                         _shim_GetControlValue
	MTSHIM	___SetControlValue,                         _shim_SetControlValue
	MTSHIM	___ActivateControl,                         _shim_ActivateControl
	MTSHIM	___DeactivateControl,                       _shim_DeactivateControl
	MTSHIM	___GetControlID,                            _shim_GetControlID
	MTSHIM	___GetControlMaximum,                       _shim_GetControlMaximum
	MTSHIM	___GetControlMinimum,                       _shim_GetControlMinimum
	MTSHIM	___GetControlRegion,                        _shim_GetControlRegion
	MTSHIM	___GetControlVariant,                       _shim_GetControlVariant
	MTSHIM	___GetMenuItemKeyGlyph,                     _shim_GetMenuItemKeyGlyph
	MTSHIM	___HIComboBoxCreate,                        _shim_HIComboBoxCreate
	MTSHIM	___HIImageViewSetOpaque,                    _shim_HIImageViewSetOpaque
	MTSHIM	___HIImageViewSetScaleToFit,                _shim_HIImageViewSetScaleToFit
	MTSHIM	___IsWindowContainedInGroup,                _shim_IsWindowContainedInGroup
	MTSHIM	___IsWindowUpdatePending,                   _shim_IsWindowUpdatePending
	MTSHIM	___NewCWindow,                              _shim_NewCWindow
	MTSHIM	___NewWindow,                               _shim_NewWindow
	MTSHIM	___GetNewCWindow,                           _shim_GetNewCWindow
	MTSHIM	___GetNewWindow,                            _shim_GetNewWindow
	MTSHIM	___ReleaseWindowGroup,                      _shim_ReleaseWindowGroup
	MTSHIM	___ReverseKeyboardFocus,                    _shim_ReverseKeyboardFocus
	MTSHIM	___SetControl32BitMinimum,                  _shim_SetControl32BitMinimum
	MTSHIM	___SetControlColorProc,                     _shim_SetControlColorProc
	MTSHIM	___SetControlMinimum,                       _shim_SetControlMinimum
	MTSHIM	___SetControlViewSize,                      _shim_SetControlViewSize
	MTSHIM	___SetItemCmd,                              _shim_SetItemCmd
	MTSHIM	___SetMenuFont,                             _shim_SetMenuFont
	MTSHIM	___SetMenuItemData,                         _shim_SetMenuItemData
	MTSHIM	___SetWindowKind,                           _shim_SetWindowKind
	MTSHIM	___ValidWindowRgn,                          _shim_ValidWindowRgn
	; --- Classic Dialog Manager: graceful no-classic-dialog (carbon_dialog_shim.c) ---
	MTSHIM	___AppendDialogItemList,                    _shim_AppendDialogItemList
	MTSHIM	___AutoSizeDialog,                          _shim_AutoSizeDialog
	MTSHIM	___DisposeDialog,                           _shim_DisposeDialog
	MTSHIM	___GetDialogCancelItem,                     _shim_GetDialogCancelItem
	MTSHIM	___GetDialogDefaultItem,                    _shim_GetDialogDefaultItem
	MTSHIM	___GetDialogItem,                           _shim_GetDialogItem
	MTSHIM	___GetDialogItemText,                       _shim_GetDialogItemText
	MTSHIM	___GetDialogWindow,                         _shim_GetDialogWindow
	MTSHIM	___GetDialogTextEditHandle,                 _shim_GetDialogTextEditHandle
	MTSHIM	___ClearKeyboardFocus,                      _shim_ClearKeyboardFocus
	MTSHIM	___GetModalDialogEventMask,                 _shim_GetModalDialogEventMask
	MTSHIM	___GetNewDialog,                            _shim_GetNewDialog
	MTSHIM	___ModalDialog,                             _shim_ModalDialog
	MTSHIM	___MoveDialogItem,                          _shim_MoveDialogItem
	MTSHIM	___SetDialogDefaultItem,                    _shim_SetDialogDefaultItem
	MTSHIM	___SetDialogItemText,                       _shim_SetDialogItemText
	MTSHIM	___SetPortDialogPort,                       _shim_SetPortDialogPort
	MTSHIM	___SizeDialogItem,                          _shim_SizeDialogItem
	MTSHIM	___StopAlert,                               _shim_StopAlert
	; --- StandardAlert/Alert family -> real clickable+movable NSAlert (carbon_standardalert_shim.m) ---
	; Unshimmed they bound to LIVE 64-bit HIToolbox, which drew a DEAD alert
	; (unclickable/immovable) in a pure-Carbon translated process. Route through
	; a real -runModal NSAlert instead (Civ IV "XML Load Error" + universal).
	MTSHIM	___CreateStandardAlert,                     _shim_CreateStandardAlert
	MTSHIM	___RunStandardAlert,                        _shim_RunStandardAlert
	MTSHIM	___StandardAlert,                           _shim_StandardAlert
	MTSHIM	___GetStandardAlertDefaultParams,           _shim_GetStandardAlertDefaultParams
	; --- Classic TextEdit/List/Drag/Theme: graceful dead-surface (carbon_classic_ui_shim.c) ---
	MTSHIM	___AddDragItemFlavor,                       _shim_AddDragItemFlavor
	MTSHIM	___CountDragItemFlavors,                    _shim_CountDragItemFlavors
	MTSHIM	___CountDragItems,                          _shim_CountDragItems
	MTSHIM	___DisposeThemeDrawingState,                _shim_DisposeThemeDrawingState
	MTSHIM	___GetDragAttributes,                       _shim_GetDragAttributes
	MTSHIM	___GetDragHiliteColor,                      _shim_GetDragHiliteColor
	MTSHIM	___GetDragItemReferenceNumber,              _shim_GetDragItemReferenceNumber
	MTSHIM	___GetDragModifiers,                        _shim_GetDragModifiers
	MTSHIM	___GetDragMouse,                            _shim_GetDragMouse
	MTSHIM	___GetDropLocation,                         _shim_GetDropLocation
	MTSHIM	___GetFlavorData,                           _shim_GetFlavorData
	MTSHIM	___GetFlavorDataSize,                       _shim_GetFlavorDataSize
	MTSHIM	___GetFlavorFlags,                          _shim_GetFlavorFlags
	MTSHIM	___GetFlavorType,                           _shim_GetFlavorType
	MTSHIM	___GetListActive,                           _shim_GetListActive
	MTSHIM	___GetScrapFlavorFlags,                     _shim_GetScrapFlavorFlags
	MTSHIM	___GetThemeScrollBarArrowStyle,             _shim_GetThemeScrollBarArrowStyle
	MTSHIM	___LActivate,                               _shim_LActivate
	MTSHIM	___LNextCell,                               _shim_LNextCell
	MTSHIM	___LScroll,                                 _shim_LScroll
	MTSHIM	___SetDragItemFlavorData,                   _shim_SetDragItemFlavorData
	MTSHIM	___SetDropLocation,                         _shim_SetDropLocation
	MTSHIM	___SetListSelectionFlags,                   _shim_SetListSelectionFlags
	MTSHIM	___SetThemePen,                             _shim_SetThemePen
	MTSHIM	___TEActivate,                              _shim_TEActivate
	MTSHIM	___TEAutoView,                              _shim_TEAutoView
	MTSHIM	___TECopy,                                  _shim_TECopy
	MTSHIM	___TECut,                                   _shim_TECut
	MTSHIM	___TEDeactivate,                            _shim_TEDeactivate
	MTSHIM	___TEDelete,                                _shim_TEDelete
	MTSHIM	___TEGetText,                               _shim_TEGetText
	MTSHIM	___TEIdle,                                  _shim_TEIdle
	MTSHIM	___TEInsert,                                _shim_TEInsert
	MTSHIM	___TEKey,                                   _shim_TEKey
	MTSHIM	___TEPaste,                                 _shim_TEPaste
	MTSHIM	___TESetSelect,                             _shim_TESetSelect
	MTSHIM	___TEStyleNew,                              _shim_TEStyleNew
	MTSHIM	___TrackMouseLocation,                      _shim_TrackMouseLocation
	MTSHIM	___TrackMouseLocationWithOptions,           _shim_TrackMouseLocationWithOptions
	; --- Control/EventLoopTimer UPP New/Dispose identity (upp_shim.c) ---
	MTSHIM	___NewControlActionUPP,               _shim_NewControlActionUPP
	MTSHIM	___DisposeControlActionUPP,           _shim_DisposeControlActionUPP
	MTSHIM	___DisposeEventLoopTimerUPP,          _shim_DisposeEventLoopTimerUPP
	; --- Removed HIToolbox UI surface linked by translated QuickTime.framework
	;     (qt_hitoolbox_shim.c; Civ IV / Halo s32; 115 no-op ___X shims) ---
	MTSHIM	____InitHLTB,                        _shim_InitHLTB
	MTSHIM	___DrawThemeButton,                  _shim_DrawThemeButton
	MTSHIM	___DrawThemeEditTextFrame,           _shim_DrawThemeEditTextFrame
	MTSHIM	___DrawThemeFocusRect,               _shim_DrawThemeFocusRect
	MTSHIM	___DrawThemeListBoxFrame,            _shim_DrawThemeListBoxFrame
	MTSHIM	___DrawThemePrimaryGroup,            _shim_DrawThemePrimaryGroup
	MTSHIM	___DrawThemeSeparator,               _shim_DrawThemeSeparator
	MTSHIM	___DrawThemeText,                    _shim_DrawThemeText
	MTSHIM	___DrawThemeTickMark,                _shim_DrawThemeTickMark
	MTSHIM	___DrawThemeTrack,                   _shim_DrawThemeTrack
	MTSHIM	___GetThemeDrawingState,             _shim_GetThemeDrawingState
	MTSHIM	___GetThemeFont,                     _shim_GetThemeFont
	MTSHIM	___GetThemeTrackThumbRgn,            _shim_GetThemeTrackThumbRgn
	MTSHIM	___NormalizeThemeDrawingState,       _shim_NormalizeThemeDrawingState
	MTSHIM	___SetThemeBackground,               _shim_SetThemeBackground
	MTSHIM	___SetThemeDrawingState,             _shim_SetThemeDrawingState
	MTSHIM	___AutoEmbedControl,                 _shim_AutoEmbedControl
	MTSHIM	___CreateCustomControl,              _shim_CreateCustomControl
	MTSHIM	___CreateUserPaneControl,            _shim_CreateUserPaneControl
	MTSHIM	___FindControl,                      _shim_FindControl
	MTSHIM	___GetControlAction,                 _shim_GetControlAction
	MTSHIM	___GetControlDataHandle,             _shim_GetControlDataHandle
	MTSHIM	___GetControlReference,              _shim_GetControlReference
	MTSHIM	___GetControlTitle,                  _shim_GetControlTitle
	MTSHIM	___IdleControls,                     _shim_IdleControls
	MTSHIM	___NewControl,                       _shim_NewControl
	MTSHIM	___RegisterSystemControlDefinition,  _shim_RegisterSystemControlDefinition
	MTSHIM	___SetControlBounds,                 _shim_SetControlBounds
	MTSHIM	___SetControlDataHandle,             _shim_SetControlDataHandle
	MTSHIM	___SetControlPopupMenuHandle,        _shim_SetControlPopupMenuHandle
	MTSHIM	___SetControlReference,              _shim_SetControlReference
	MTSHIM	___SetControlSupervisor,             _shim_SetControlSupervisor
	MTSHIM	___SetControlTitle,                  _shim_SetControlTitle
	MTSHIM	___SetUpControlBackground,           _shim_SetUpControlBackground
	MTSHIM	___TestControl,                      _shim_TestControl
	MTSHIM	___TrackControl,                     _shim_TrackControl
	; ___Alert: was a qt_hitoolbox_shim.c no-op (returned 0 = NO modal, breaking
	; any app that branches on the button hit). Route to a REAL 'ALRT'-resource
	; NSAlert modal (carbon_standardalert_shim.m -> the same run_alert as StopAlert).
	MTSHIM	___Alert,                            _shim_AlertReal
	MTSHIM	___DialogCopy,                       _shim_DialogCopy
	MTSHIM	___DialogCut,                        _shim_DialogCut
	MTSHIM	___DialogDelete,                     _shim_DialogDelete
	MTSHIM	___DialogPaste,                      _shim_DialogPaste
	MTSHIM	___DrawDialog,                       _shim_DrawDialog
	MTSHIM	___FindDialogItem,                   _shim_FindDialogItem
	MTSHIM	___GetDialogKeyboardFocusItem,       _shim_GetDialogKeyboardFocusItem
	MTSHIM	___GetDialogPort,                    _shim_GetDialogPort
	MTSHIM	___HideDialogItem,                   _shim_HideDialogItem
	MTSHIM	___InsertDialogItem,                 _shim_InsertDialogItem
	MTSHIM	___IsDialogEvent,                    _shim_IsDialogEvent
	MTSHIM	___NewColorDialog,                   _shim_NewColorDialog
	MTSHIM	___NewFeaturesDialog,                _shim_NewFeaturesDialog
	MTSHIM	___RemoveDialogItems,                _shim_RemoveDialogItems
	MTSHIM	___SelectDialogItemText,             _shim_SelectDialogItemText
	MTSHIM	___SetDialogCancelItem,              _shim_SetDialogCancelItem
	MTSHIM	___SetDialogItem,                    _shim_SetDialogItem
	MTSHIM	___SetDialogTracksCursor,            _shim_SetDialogTracksCursor
	MTSHIM	___ShowDialogItem,                   _shim_ShowDialogItem
	MTSHIM	___UpdateDialog,                     _shim_UpdateDialog
	MTSHIM	___DragGrayRgn,                      _shim_DragGrayRgn
	MTSHIM	___GetNextEvent,                     _shim_GetNextEvent
	MTSHIM	___StillDown,                        _shim_StillDown
	;; Button / GetCurrentEventButtonState: the OTHER half of the classic
;; click idiom `if (Button()) { while (StillDown()) ... }`. StillDown was
	;; fixed long ago; these two were left as raw abigen bridges to
	;; HIToolbox, and a permanently-false Button() defeats the idiom just as
	;; completely. Overriding here also removes them from the abigen
	;; consider set. See classic_input_coords.c.
	MTSHIM	___Button,                           _shim_Button
	MTSHIM	___GetCurrentEventButtonState,       _shim_GetCurrentEventButtonState
	;; SendEventToEventTarget: on modern macOS the DISPATCHER consumes a
	;; mouse-DOWN that every window handler declined, so it never propagates
	;; up to the APPLICATION target where a classic app installs its mouse
	;; handler. Measured on Halo CE: 11 declines, 0 claims, and the app
	;; target saw zero DOWNs while UP/MOVED/DRAGGED arrived normally.
	;; Overriding here also removes it from the abigen consider set.
	;; See carbon_event_appdown.c.
	MTSHIM	___SendEventToEventTarget,           _shim_SendEventToEventTarget
	;; SetWindowBounds: a RESIZE of a Carbon window is FATAL on modern macOS
	;; (NSCGSPanic in -[NSCGSWindow _createContext]); a MOVE is safe. Bisected
	;; in cg_display_fullscreen_shim.c. Halo resizes its window to the chosen
	;; resolution after its settings dialog and dies. See
	;; carbon_window_resize_guard.c.
	MTSHIM	___SetWindowBounds,                  _shim_SetWindowBounds
	;; ShowWindow/IsWindowVisible/HideWindow: unwrap the WindowRef handle and
	;; forward. See carbon_window_show_shim.c.
	MTSHIM	___ShowWindow,                       _shim_ShowWindow
	MTSHIM	___IsWindowVisible,                  _shim_IsWindowVisible
	MTSHIM	___HideWindow,                       _shim_HideWindow
	MTSHIM	___WaitMouseUp,                      _shim_WaitMouseUp
	MTSHIM	___CreateCustomList,                 _shim_CreateCustomList
	MTSHIM	___GetListCellSize,                  _shim_GetListCellSize
	MTSHIM	___GetListDataBounds,                _shim_GetListDataBounds
	MTSHIM	___GetListDataHandle,                _shim_GetListDataHandle
	MTSHIM	___GetListRefCon,                    _shim_GetListRefCon
	MTSHIM	___GetListVerticalScrollBar,         _shim_GetListVerticalScrollBar
	MTSHIM	___GetScrapFlavorInfoList,           _shim_GetScrapFlavorInfoList
	MTSHIM	___LAddRow,                          _shim_LAddRow
	MTSHIM	___LAutoScroll,                      _shim_LAutoScroll
	MTSHIM	___LCellSize,                        _shim_LCellSize
	MTSHIM	___LClick,                           _shim_LClick
	MTSHIM	___LDelRow,                          _shim_LDelRow
	MTSHIM	___LDispose,                         _shim_LDispose
	MTSHIM	___LGetCell,                         _shim_LGetCell
	MTSHIM	___LGetSelect,                       _shim_LGetSelect
	MTSHIM	___LNew,                             _shim_LNew
	MTSHIM	___LSetCell,                         _shim_LSetCell
	MTSHIM	___LSetDrawingMode,                  _shim_LSetDrawingMode
	MTSHIM	___LSetSelect,                       _shim_LSetSelect
	MTSHIM	___LSize,                            _shim_LSize
	MTSHIM	___LUpdate,                          _shim_LUpdate
	MTSHIM	___SetListRefCon,                    _shim_SetListRefCon
	MTSHIM	___AppendMenu,                       _shim_AppendMenu
	MTSHIM	___GetMenu,                          _shim_GetMenu
	MTSHIM	___GetMenuItemText,                  _shim_GetMenuItemText
	MTSHIM	___MenuKey,                          _shim_MenuKey
	MTSHIM	___SetMenuItemText,                  _shim_SetMenuItemText
	MTSHIM	___ClearCurrentScrap,                _shim_ClearCurrentScrap
	MTSHIM	___GetCurrentScrap,                  _shim_GetCurrentScrap
	MTSHIM	___GetScrapFlavorCount,              _shim_GetScrapFlavorCount
	MTSHIM	___GetScrapFlavorData,               _shim_GetScrapFlavorData
	MTSHIM	___GetScrapFlavorSize,               _shim_GetScrapFlavorSize
	MTSHIM	___PutScrapFlavor,                   _shim_PutScrapFlavor
	MTSHIM	___TECalText,                        _shim_TECalText
	MTSHIM	___TEDispose,                        _shim_TEDispose
	MTSHIM	___TEGetHeight,                      _shim_TEGetHeight
	MTSHIM	___TENew,                            _shim_TENew
	MTSHIM	___TEScroll,                         _shim_TEScroll
	MTSHIM	___TESetText,                        _shim_TESetText
	MTSHIM	___TETextBox,                        _shim_TETextBox
	MTSHIM	___TEToScrap,                        _shim_TEToScrap
	MTSHIM	___TEUpdate,                         _shim_TEUpdate
	MTSHIM	___GetStdFilterProc,                 _shim_GetStdFilterProc
	MTSHIM	___RegisterToolboxObjectClass,       _shim_RegisterToolboxObjectClass
	MTSHIM	___TypeSelectClear,                  _shim_TypeSelectClear
	MTSHIM	___TypeSelectFindItem,               _shim_TypeSelectFindItem
	MTSHIM	___TypeSelectNewKey,                 _shim_TypeSelectNewKey
	MTSHIM	___GetWVariant,                      _shim_GetWVariant
	MTSHIM	___GrowWindow,                       _shim_GrowWindow
	MTSHIM	___HIViewReshapeStructure,           _shim_HIViewReshapeStructure
	MTSHIM	___HIViewSetNeedsDisplayInRegion,    _shim_HIViewSetNeedsDisplayInRegion
	MTSHIM	___InstallWindowContentPaintProc,    _shim_InstallWindowContentPaintProc
	MTSHIM	___InvalWindowRgn,                   _shim_InvalWindowRgn
	MTSHIM	___SetWTitle,                        _shim_SetWTitle
	; --- Present but __LP64__-header-gated Carbon entries translated QuickTime
	;     calls: FORWARD to native w/ ABI conversion (carbon_fwd_shim.c; s34) ---
	MTSHIM	___RMOpenResourceFileRef,            _shim_RMOpenResourceFileRef
	MTSHIM	___SetStdCProcs,                     _shim_SetStdCProcs
	; --- Resource Manager Handle marshalling (rm_shim.c; s35): wrap the native
	;     >4GB Handle abigen would truncate into a low-4GB arena Handle ---
	MTSHIM	___Get1Resource,                     _shim_Get1Resource
	MTSHIM	___GetResource,                      _shim_GetResource
	MTSHIM	___Get1IndResource,                  _shim_Get1IndResource
	MTSHIM	___GetIndResource,                   _shim_GetIndResource
	MTSHIM	___LoadResource,                     _shim_LoadResource
	MTSHIM	___SizeResource,                     _shim_SizeResource
	MTSHIM	___GetResAttrs,                      _shim_GetResAttrs
	MTSHIM	___HomeResFile,                      _shim_HomeResFile
	MTSHIM	___GetResInfo,                       _shim_GetResInfo
	MTSHIM	___DetachResource,                   _shim_DetachResource
	MTSHIM	___ReleaseResource,                  _shim_ReleaseResource
	; --- Universal Procedure Pointers (upp_shim.c) ---
	MTSHIM	___DisposeControlUserPaneDrawUPP,     _shim_DisposeControlUserPaneDrawUPP
	MTSHIM	___DisposeControlUserPaneHitTestUPP,  _shim_DisposeControlUserPaneHitTestUPP
	MTSHIM	___DisposeControlUserPaneTrackingUPP, _shim_DisposeControlUserPaneTrackingUPP
	MTSHIM	___DisposeEventHandlerUPP,            _shim_DisposeEventHandlerUPP
	MTSHIM	___DisposeSndCallBackUPP,             _shim_DisposeSndCallBackUPP
	MTSHIM	___NewControlUserPaneDrawUPP,         _shim_NewControlUserPaneDrawUPP
	MTSHIM	___NewControlUserPaneHitTestUPP,      _shim_NewControlUserPaneHitTestUPP
	MTSHIM	___NewControlUserPaneTrackingUPP,     _shim_NewControlUserPaneTrackingUPP
	MTSHIM	___NewEventHandlerUPP,                _shim_NewEventHandlerUPP
	MTSHIM	___NewEventLoopTimerUPP,              _shim_NewEventLoopTimerUPP
	MTSHIM	___NewSndCallBackUPP,                 _shim_NewSndCallBackUPP
	; --- Sound Manager (sndmgr_shim.c) ---
	MTSHIM	___SndChannelStatus,                  _shim_SndChannelStatus
	MTSHIM	___SndDisposeChannel,                 _shim_SndDisposeChannel
	MTSHIM	___SndNewChannel,                     _shim_SndNewChannel
	MTSHIM	___SndDoCommand,                      _shim_SndDoCommand
	MTSHIM	___SndDoImmediate,                    _shim_SndDoImmediate
	MTSHIM	___SndPlay,                           _shim_SndPlay
	MTSHIM	___SysBeep,                           _shim_SysBeep
	; --- Font Manager (fontmgr_shim.c) ---
	MTSHIM	___FMActivateFonts,                   _shim_FMActivateFonts
	MTSHIM	___FMDeactivateFonts,                 _shim_FMDeactivateFonts
	MTSHIM	___FMGetFontFamilyFromName,           _shim_FMGetFontFamilyFromName
	MTSHIM	___FMGetFontFamilyName,               _shim_FMGetFontFamilyName
	MTSHIM	___GetAppFont,                        _shim_GetAppFont
	; --- MLTE/TXN + HIView (mlte_shim.c) ---
	MTSHIM	___HIImageViewSetImage,               _shim_HIImageViewSetImage
	MTSHIM	___HITextViewCreate,                  _shim_HITextViewCreate
	MTSHIM	___HITextViewGetTXNObject,            _shim_HITextViewGetTXNObject
	MTSHIM	___TXNDeleteObject,                   _shim_TXNDeleteObject
	MTSHIM	___TXNGetDataEncoded,                 _shim_TXNGetDataEncoded
	MTSHIM	___TXNGetHIRect,                      _shim_TXNGetHIRect
	MTSHIM	___TXNInitTextension,                 _shim_TXNInitTextension
	MTSHIM	___TXNNewObject,                      _shim_TXNNewObject
	MTSHIM	___TXNSetDataFromCFURLRef,            _shim_TXNSetDataFromCFURLRef
	MTSHIM	___TXNSetDataFromFile,                _shim_TXNSetDataFromFile
	MTSHIM	___TXNSetHIRectBounds,                _shim_TXNSetHIRectBounds
	; --- Classic File Manager (fmgr_shim.c) ---
	MTSHIM	___FSClose,                           _shim_FSClose
	MTSHIM	___FSRead,                            _shim_FSRead
	MTSHIM	___FSWrite,                           _shim_FSWrite
	MTSHIM	___FSpDelete,                         _shim_FSpDelete
	MTSHIM	___FSpGetFInfo,                       _shim_FSpGetFInfo
	MTSHIM	___FSpOpenDF,                         _shim_FSpOpenDF
	MTSHIM	___FSpRstFLock,                       _shim_FSpRstFLock
	MTSHIM	___FSpSetFLock,                       _shim_FSpSetFLock
	MTSHIM	___GetEOF,                            _shim_GetEOF
	MTSHIM	___HGetVol,                           _shim_HGetVol
	MTSHIM	___HSetVol,                           _shim_HSetVol
	MTSHIM	___PBHGetVInfoSync,                   _shim_PBHGetVInfoSync
	MTSHIM	___PBXGetVolInfoSync,                 _shim_PBXGetVolInfoSync
	MTSHIM	___PBXGetVolInfoAsync,                _shim_PBXGetVolInfoAsync
	MTSHIM	___PBMakeFSRefSync,                   _shim_PBMakeFSRefSync
	MTSHIM	___PBHGetVolParmsSync,                _shim_PBHGetVolParmsSync
	MTSHIM	___PBGetCatInfoSync,                  _shim_PBGetCatInfoSync
	MTSHIM	___PBHCopyFileSync,                   _shim_PBHCopyFileSync
	MTSHIM	___PBHGetDirAccessSync,               _shim_PBHGetDirAccessSync
	MTSHIM	___SetEOF,                            _shim_SetEOF
	MTSHIM	___SetFPos,                           _shim_SetFPos
	; --- OS utilities, real impls (osutil_shim.c) ---
	MTSHIM	___GetDateTime,                       _shim_GetDateTime
	MTSHIM	___ReadLocation,                      _shim_ReadLocation
	MTSHIM	___GetGlobalMouse,                    _shim_GetGlobalMouse
	MTSHIM	___GetMouse,                          _shim_GetMouse
	MTSHIM	___OTAtomicClearBit,                  _shim_OTAtomicClearBit
	; --- Text Encoding Converter (tec_shim.c) ---
	MTSHIM	___ConvertFromUnicodeToText,          _shim_ConvertFromUnicodeToText
	MTSHIM	___c2pstrcpy,                         _shim_c2pstrcpy
	MTSHIM	___p2cstrcpy,                         _shim_p2cstrcpy
	; --- Python C-API name shim (py_shim.c) ---
	MTSHIM	___Py_InitModule4,                    _shim_Py_InitModule4
	; --- QuickTime Movie Toolbox init/teardown no-ops (quicktime_movie_shim.c) ---
	MTSHIM	___EnterMovies,                      _shim_EnterMovies
	MTSHIM	___EnterMoviesOnThread,              _shim_EnterMoviesOnThread
	MTSHIM	___ExitMovies,                       _shim_ExitMovies
	MTSHIM	___ExitMoviesOnThread,               _shim_ExitMoviesOnThread
	; --- QuickTime Movie Toolbox REAL playback -> AVFoundation (quicktime_movie_bridge.m):
	;     open (NewMovieFromDataRef), bind GWorld, transport (Start/Stop/rate/seek),
	;     frame pull (MoviesTask/UpdateMovie -> AVPlayerItemVideoOutput -> GWorld ARGB),
	;     timing (Box/Duration/TimeScale/Time/IsMovieDone), teardown (DisposeMovie). ---
	MTSHIM	___NewMovieFromDataRef,               _shim_NewMovieFromDataRef
	MTSHIM	___DisposeMovie,                      _shim_DisposeMovie
	MTSHIM	___SetMovieGWorld,                    _shim_SetMovieGWorld
	MTSHIM	___GetMovieGWorld,                    _shim_GetMovieGWorld
	MTSHIM	___GetMovieBox,                       _shim_GetMovieBox
	MTSHIM	___GetMovieNaturalBoundsRect,         _shim_GetMovieNaturalBoundsRect
	MTSHIM	___SetMovieBox,                       _shim_SetMovieBox
	MTSHIM	___StartMovie,                        _shim_StartMovie
	MTSHIM	___StopMovie,                         _shim_StopMovie
	MTSHIM	___GoToBeginningOfMovie,              _shim_GoToBeginningOfMovie
	MTSHIM	___SetMovieRate,                      _shim_SetMovieRate
	MTSHIM	___GetMovieRate,                      _shim_GetMovieRate
	MTSHIM	___GetMovieTimeScale,                 _shim_GetMovieTimeScale
	MTSHIM	___GetMovieDuration,                  _shim_GetMovieDuration
	MTSHIM	___GetMovieTime,                      _shim_GetMovieTime
	MTSHIM	___SetMovieTimeValue,                 _shim_SetMovieTimeValue
	MTSHIM	___IsMovieDone,                       _shim_IsMovieDone
	MTSHIM	___MoviesTask,                        _shim_MoviesTask
	MTSHIM	___UpdateMovie,                       _shim_UpdateMovie
	MTSHIM	___GetMoviePict,                      _shim_GetMoviePict
	MTSHIM	___PrerollMovie,                      _shim_PrerollMovie
	MTSHIM	___SetMovieActive,                    _shim_SetMovieActive
	MTSHIM	___SetMovieVolume,                    _shim_SetMovieVolume

	; --- keymgr process-wide pointer store + DWARF2 EH-section registration
	;     (keymgr_shim.c). NOT removed — present natively but an i386->x86_64 ABI
	;     trap: unbridged, the i386 crt's EH-frame registration passes a garbage
	;     `void**result` out-pointer to native _keymgr_get_and_lock_processwide_ptr_2,
	;     which stores through it (movq %rax,(%rbx)) and SIGSEGVs. The trampoline
	;     marshals the cdecl args correctly; keymgr's keyed store is reimplemented
	;     in keymgr_shim.c (vestigial under 86x64's own unwinder). Trampoline names
	;     are `__` + the exact (multi-underscore) import symbol. ---
	MTSHIM	_____keymgr_dwarf2_register_sections,      _shim_keymgr_dwarf2_register_sections
	MTSHIM	____keymgr_get_and_lock_processwide_ptr,   _shim_keymgr_get_and_lock_processwide_ptr
	MTSHIM	____keymgr_get_and_lock_processwide_ptr_2, _shim_keymgr_get_and_lock_processwide_ptr_2
	MTSHIM	____keymgr_set_and_unlock_processwide_ptr, _shim_keymgr_set_and_unlock_processwide_ptr

	; --- Navigation Services -> AppKit NSOpenPanel/NSSavePanel (nav_shim.m) ---
	; The classic Carbon file dialogs (removed in 64-bit macOS). Reimplemented on
	; NSOpenPanel/NSSavePanel; the reply.selection AEDescList round-trips through
	; abigen's AE-desc x64_objc_wrap table. Create/run/reply API + classic blocking
	; API + defaults/availability. custom.syms lists these so abigen does not also
	; emit a (broken, forward-to-removed) version.
	MTSHIM	___NavCreatePutFileDialog,            _shim_NavCreatePutFileDialog
	MTSHIM	___NavCreateGetFileDialog,            _shim_NavCreateGetFileDialog
	MTSHIM	___NavCreateChooseFolderDialog,       _shim_NavCreateChooseFolderDialog
	MTSHIM	___NavCreateChooseFileDialog,         _shim_NavCreateChooseFileDialog
	MTSHIM	___NavCreateChooseObjectDialog,       _shim_NavCreateChooseObjectDialog
	MTSHIM	___NavDialogRun,                      _shim_NavDialogRun
	MTSHIM	___NavDialogGetUserAction,            _shim_NavDialogGetUserAction
	MTSHIM	___NavDialogGetReply,                 _shim_NavDialogGetReply
	MTSHIM	___NavDialogGetSaveFileName,          _shim_NavDialogGetSaveFileName
	MTSHIM	___NavDialogSetSaveFileName,          _shim_NavDialogSetSaveFileName
	MTSHIM	___NavDialogGetWindow,                _shim_NavDialogGetWindow
	MTSHIM	___NavDialogDispose,                  _shim_NavDialogDispose
	MTSHIM	___NavCustomControl,                  _shim_NavCustomControl
	MTSHIM	___NavGetFile,                        _shim_NavGetFile
	MTSHIM	___NavChooseFile,                     _shim_NavChooseFile
	MTSHIM	___NavChooseFolder,                   _shim_NavChooseFolder
	MTSHIM	___NavChooseObject,                   _shim_NavChooseObject
	MTSHIM	___NavPutFile,                        _shim_NavPutFile
	MTSHIM	___NavDisposeReply,                   _shim_NavDisposeReply
	MTSHIM	___NavCompleteSave,                   _shim_NavCompleteSave
	MTSHIM	___NavGetDefaultDialogCreationOptions,_shim_NavGetDefaultDialogCreationOptions
	MTSHIM	___NavGetDefaultDialogOptions,        _shim_NavGetDefaultDialogOptions
	MTSHIM	___NavServicesAvailable,              _shim_NavServicesAvailable
	MTSHIM	___NavServicesCanRun,                 _shim_NavServicesCanRun
	MTSHIM	___NavLoad,                           _shim_NavLoad
	MTSHIM	___NavUnload,                         _shim_NavUnload

	; --- RTTI type_info vtable sentinels (used by cxx_shim.c's __dynamic_cast) ---
	; Exported under the exact libstdc++ __cxxabiv1 vtable names so static-interpose
	; redirects every translated typeinfo's vtable-ptr field (typeinfo+0) to one of
	; these low-4GB addresses. The compiler stores &vtable+8 (the i386 ABI address
	; point) into the typeinfo, so the bound value lands inside the 16-byte sentinel;
	; cxx_shim's ti_kind_of() range-checks +0 against these to recover the kind.
	; Binding to the NATIVE 8-byte vtables would truncate to an unusable 32-bit value.
	; 16 bytes each so adjacent sentinels never alias under the +8 addend.
	; classic Time Manager (carbon_timemgr_shim.c): the TMTask stays the i386
	; record and tmAddr runs through the callback bridge
	MTSHIM	___InsTime,          _shim_InsTime
	MTSHIM	___InsXTime,         _shim_InsTime
	MTSHIM	___InstallTimeTask,  _shim_InstallTimeTask
	MTSHIM	___InstallXTimeTask, _shim_InstallTimeTask
	MTSHIM	___PrimeTime,        _shim_PrimeTime
	MTSHIM	___PrimeTimeTask,    _shim_PrimeTimeTask
	MTSHIM	___RmvTime,          _shim_RmvTime
	MTSHIM	___RemoveTimeTask,   _shim_RemoveTimeTask

	segment .data
	global	____ZTVN10__cxxabiv117__class_type_infoE
	global	____ZTVN10__cxxabiv120__si_class_type_infoE
	global	____ZTVN10__cxxabiv121__vmi_class_type_infoE
	align	16
____ZTVN10__cxxabiv117__class_type_infoE:	times 16 db 0
____ZTVN10__cxxabiv120__si_class_type_infoE:	times 16 db 0
____ZTVN10__cxxabiv121__vmi_class_type_infoE:	times 16 db 0

