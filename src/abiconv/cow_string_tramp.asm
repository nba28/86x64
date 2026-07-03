;
; cow_string_tramp.asm — i386-cdecl -> C trampolines for the libstdc++ COW
; std::string / std::wstring surface reimplemented in cow_string_shim.c.
;
; This module keeps its OWN copy of the MTSHIM macro (the same frame the
; abigen/maptable shims use) plus an MTSHIM_SRET variant, so it is a fully
; self-contained single-purpose unit (per the one-shim-one-job rule) with no
; edit to maptable_tramp.asm.
;
; Each translated i386 caller reaches a mangled export through its (weak) lazy
; bind: static-interpose (PREFIX=__) redirects the mach-o import __ZNSs... to
; the export ____ZNSs... defined here. The i386 caller pushed a 4-byte return
; address then its args as 4-byte cdecl stack slots (`this` is the first arg for
; member functions). We hand the C impl rdi=&args[0] and return its uint32_t
; result in eax, then unwind the i386 frame.
;
;   MTSHIM       — pops the 4-byte return address (eax/void/iterator returns).
;   MTSHIM_SRET  — for functions returning std::string/std::wstring BY VALUE
;                  (substr, operator+): the hidden result pointer is the FIRST
;                  cdecl slot (args[0]) and the i386 callee POPS it (`retl $4`,
;                  verified against the SL i386 libstdc++ substr/operator+ which
;                  end `retl $0x4` and return the buffer in eax). So the C impl
;                  returns args[0] in eax and we unwind 8 bytes (ret addr +
;                  hidden ptr) — mirrors maptable_tramp.asm's GEOSHIM_S.
;
; i386 esi/edi are callee-saved (= x86_64 rsi/rdi) so we preserve them across
; the SysV call; the __dyld_stub_binder_flag dance matches abigen's prologue.

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

%macro MTSHIM_SRET 2            ; by-value string return: callee-pops hidden ptr
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

; ---- narrow std::string members (eax/void returns) ----
	MTSHIM      ____ZNSsC1Ev,                                                                _shim_Ss_ctor_default
	MTSHIM      ____ZNSsC2Ev,                                                                _shim_Ss_ctor_default
	MTSHIM      ____ZNSsC1ERKSs,                                                             _shim_Ss_ctor_copy
	MTSHIM      ____ZNSsC2ERKSs,                                                             _shim_Ss_ctor_copy
	MTSHIM      ____ZNSsC1EPKcRKSaIcE,                                                       _shim_Ss_ctor_cstr
	MTSHIM      ____ZNSsC2EPKcRKSaIcE,                                                       _shim_Ss_ctor_cstr
	MTSHIM      ____ZNSsC1EPKcmRKSaIcE,                                                      _shim_Ss_ctor_buf
	MTSHIM      ____ZNSsC2EPKcmRKSaIcE,                                                      _shim_Ss_ctor_buf
	MTSHIM      ____ZNSsC1EmcRKSaIcE,                                                        _shim_Ss_ctor_fill
	MTSHIM      ____ZNSsC2EmcRKSaIcE,                                                        _shim_Ss_ctor_fill
	MTSHIM      ____ZNSsD1Ev,                                                                _shim_Ss_dtor
	MTSHIM      ____ZNSsD2Ev,                                                                _shim_Ss_dtor
	MTSHIM      ____ZNSsaSERKSs,                                                             _shim_Ss_assign_op
	MTSHIM      ____ZNSsixEm,                                                                _shim_Ss_index
	MTSHIM      ____ZNSs5beginEv,                                                            _shim_Ss_begin
	MTSHIM      ____ZNSs3endEv,                                                              _shim_Ss_end
	MTSHIM      ____ZNKSs5beginEv,                                                           _shim_Ss_cbegin
	MTSHIM      ____ZNKSs3endEv,                                                             _shim_Ss_cend
	MTSHIM      ____ZNKSs9_M_ibeginEv,                                                       _shim_Ss_cbegin
	MTSHIM      ____ZNKSs7_M_iendEv,                                                         _shim_Ss_cend
	MTSHIM      ____ZNKSs5c_strEv,                                                           _shim_Ss_cstr
	MTSHIM      ____ZNKSs4dataEv,                                                            _shim_Ss_cstr
	MTSHIM      ____ZNKSs4sizeEv,                                                            _shim_Ss_size
	MTSHIM      ____ZNSs5clearEv,                                                            _shim_Ss_clear
	MTSHIM      ____ZNSs5eraseEmm,                                                           _shim_Ss_erase_range
	MTSHIM      ____ZNSs5eraseEN9__gnu_cxx17__normal_iteratorIPcSsEE,                        _shim_Ss_erase_iter
	MTSHIM      ____ZNSs6appendEPKc,                                                         _shim_Ss_append_cstr
	MTSHIM      ____ZNSs6appendERKSs,                                                        _shim_Ss_append_str
	MTSHIM      ____ZNSs6assignEPKc,                                                         _shim_Ss_assign_cstr
	MTSHIM      ____ZNSs6assignEPKcm,                                                        _shim_Ss_assign_buf
	MTSHIM      ____ZNSs6assignERKSs,                                                        _shim_Ss_assign_str
	MTSHIM      ____ZNSs6resizeEmc,                                                          _shim_Ss_resize
	MTSHIM      ____ZNSs7replaceEmmPKc,                                                      _shim_Ss_replace_pos_cstr
	MTSHIM      ____ZNSs7replaceEmmRKSs,                                                     _shim_Ss_replace_pos_str
	MTSHIM      ____ZNSs7replaceEN9__gnu_cxx17__normal_iteratorIPcSsEES2_S2_S2_,             _shim_Ss_replace_iter
	MTSHIM      ____ZNSs7replaceEN9__gnu_cxx17__normal_iteratorIPcSsEES2_NS0_IPKcSsEES5_,    _shim_Ss_replace_iter
	MTSHIM      ____ZNSs7reserveEm,                                                          _shim_Ss_reserve
	MTSHIM      ____ZNSs9push_backEc,                                                        _shim_Ss_push_back
	MTSHIM      ____ZNKSs4findEPKcm,                                                         _shim_Ss_find_cstr
	MTSHIM      ____ZNKSs4findERKSsm,                                                        _shim_Ss_find_str
	MTSHIM      ____ZNKSs4findEcm,                                                           _shim_Ss_find_ch
	MTSHIM      ____ZNKSs5rfindEPKcm,                                                        _shim_Ss_rfind_cstr
	MTSHIM      ____ZNKSs5rfindEcm,                                                          _shim_Ss_rfind_ch
	MTSHIM      ____ZNKSs13find_first_ofEPKcm,                                               _shim_Ss_ffo_cstr
	MTSHIM      ____ZNKSs13find_first_ofERKSsm,                                              _shim_Ss_ffo_str
	MTSHIM      ____ZNKSs17find_first_not_ofEPKcm,                                           _shim_Ss_ffno_cstr
	MTSHIM      ____ZNKSs17find_first_not_ofERKSsm,                                          _shim_Ss_ffno_str
	MTSHIM      ____ZNKSs7compareEPKc,                                                       _shim_Ss_compare_cstr
	MTSHIM      ____ZNKSs7compareERKSs,                                                      _shim_Ss_compare_str
	MTSHIM      ____ZNKSs7compareEmmPKcm,                                                    _shim_Ss_compare_sub_cstr
; ---- wide std::wstring members ----
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEEC1Ev,                                      _shim_Sw_ctor_default
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEEC2Ev,                                      _shim_Sw_ctor_default
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEEC1ERKS2_,                                  _shim_Sw_ctor_copy
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEEC2ERKS2_,                                  _shim_Sw_ctor_copy
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEEC1EPKwRKS1_,                               _shim_Sw_ctor_cstr
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEEC2EPKwRKS1_,                               _shim_Sw_ctor_cstr
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEEC1EmwRKS1_,                                _shim_Sw_ctor_fill
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEEC2EmwRKS1_,                                _shim_Sw_ctor_fill
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEEC1ERKS2_mm,                                _shim_Sw_ctor_substr
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEEC2ERKS2_mm,                                _shim_Sw_ctor_substr
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEED1Ev,                                      _shim_Sw_dtor
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEED2Ev,                                      _shim_Sw_dtor
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEEixEm,                                      _shim_Sw_index
	MTSHIM      ____ZNKSbIwSt11char_traitsIwESaIwEE5c_strEv,                                 _shim_Sw_cstr
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEE5clearEv,                                  _shim_Sw_clear
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEE6appendEPKw,                               _shim_Sw_append_cstr
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEE6appendERKS2_,                             _shim_Sw_append_str
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEE6assignEPKw,                               _shim_Sw_assign_cstr
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEE6assignEPKwm,                              _shim_Sw_assign_buf
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEE6assignERKS2_,                             _shim_Sw_assign_str
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEE6assignERKS2_mm,                           _shim_Sw_assign_substr
	MTSHIM      ____ZNSbIwSt11char_traitsIwESaIwEE7replaceEmmPKw,                            _shim_Sw_replace_pos_cstr
	MTSHIM      ____ZNKSbIwSt11char_traitsIwESaIwEE4findEPKwm,                               _shim_Sw_find_cstr
	MTSHIM      ____ZNKSbIwSt11char_traitsIwESaIwEE4findERKS2_m,                             _shim_Sw_find_str
	MTSHIM      ____ZNKSbIwSt11char_traitsIwESaIwEE4findEwm,                                 _shim_Sw_find_ch
	MTSHIM      ____ZNKSbIwSt11char_traitsIwESaIwEE7compareEPKw,                             _shim_Sw_compare_cstr
	MTSHIM      ____ZNKSbIwSt11char_traitsIwESaIwEE7compareERKS2_,                           _shim_Sw_compare_str
; ---- iterator==, allocator no-ops, list node ops, terminate ----
	MTSHIM      ____ZN9__gnu_cxxeqIPKcSsEEbRKNS_17__normal_iteratorIT_T0_EES8_,              _shim_iter_eq
	MTSHIM      ____ZNSaIcEC1Ev,                                                             _shim_alloc_noop
	MTSHIM      ____ZNSaIcED1Ev,                                                             _shim_alloc_noop
	MTSHIM      ____ZNSaIwEC1Ev,                                                             _shim_alloc_noop
	MTSHIM      ____ZNSaIwED1Ev,                                                             _shim_alloc_noop
	MTSHIM      ____ZNSt15_List_node_base4hookEPS_,                                          _shim_list_hook
	MTSHIM      ____ZNSt15_List_node_base6unhookEv,                                          _shim_list_unhook
	MTSHIM      ____ZNSt15_List_node_base4swapERS_S0_,                                       _shim_list_swap
	MTSHIM      ____ZNSt15_List_node_base8transferEPS_S0_,                                   _shim_list_transfer
	MTSHIM      ____ZSt9terminatev,                                                          _shim_terminate
; ---- by-value returns (i386 callee-pops hidden sret ptr): substr, operator+ ----
	MTSHIM_SRET ____ZNKSs6substrEmm,                                                         _shim_Ss_substr
	MTSHIM_SRET ____ZNKSbIwSt11char_traitsIwESaIwEE6substrEmm,                               _shim_Sw_substr
	MTSHIM_SRET ____ZStplIcSt11char_traitsIcESaIcEESbIT_T0_T1_EPKS3_RKS6_,                   _shim_Ss_opplus_cstr_str
	MTSHIM_SRET ____ZStplIcSt11char_traitsIcESaIcEESbIT_T0_T1_ERKS6_S8_,                     _shim_Ss_opplus_str_str
	MTSHIM_SRET ____ZStplIwSt11char_traitsIwESaIwEESbIT_T0_T1_ERKS6_S8_,                     _shim_Sw_opplus_str_str
