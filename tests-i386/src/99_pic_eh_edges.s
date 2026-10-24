## 99_pic_eh_edges — a landing pad's PIC anchor state comes from the calls its
## exception table routes to it, not from the code laid out before it.
##
## clang -O1 (C++, Portal 2 libsteam): the catch block follows the function's
## `ret`, and the fall-through path clobbers the anchor before returning:
##         calll __Z7throweri        ; call site -> landing pad LBB1_1
##   LBB1_2: xorl %edi,%edi          ; edi reused on the normal path
##         ...  retl
##   LBB1_1: calll ___cxa_begin_catch
##         movl  _val-L1$pb(%edi),%esi   ; edi = anchor (callee-saved)
## No branch reaches the pad, so without the LSDA edge its state was the one at
## the `ret`, which had lost %edi: the read stayed RAW.
##
## Exit 42 = the catch reads _val. Kill switch M64_NO_PIC_EH_EDGES=1
## (translate time): a raw disp read, a fault or a wrong exit.

	.macosx_version_min 10, 6
	.section	__TEXT,__text,regular,pure_instructions
	.globl	__Z7throweri                    ## -- Begin function _Z7throweri
	.p2align	4
__Z7throweri:                           ## @_Z7throweri
	.cfi_startproc
	pushl	%ebp
	.cfi_def_cfa_offset 8
	.cfi_offset %ebp, -8
	movl	%esp, %ebp
	.cfi_def_cfa_register %ebp
	pushl	%edi
	pushl	%esi
	.cfi_offset %esi, -16
	.cfi_offset %edi, -12
	calll	L0$pb
L0$pb:
	popl	%esi
	movl	8(%ebp), %edi
	testl	%edi, %edi
	jne	LBB0_2
	popl	%esi
	popl	%edi
	popl	%ebp
	retl
LBB0_2:
	subl	$12, %esp
	pushl	$4
	calll	___cxa_allocate_exception
	addl	$16, %esp
	movl	%edi, (%eax)
	subl	$4, %esp
	movl	L__ZTIi$non_lazy_ptr-L0$pb(%esi), %ecx
	pushl	$0
	pushl	%ecx
	pushl	%eax
	calll	___cxa_throw
	.cfi_endproc
	.globl	__Z1fi                          ## -- Begin function _Z1fi
	.p2align	4
__Z1fi:                                 ## @_Z1fi
Lfunc_begin0:
	.cfi_startproc
	.cfi_personality 155, L___gxx_personality_v0$non_lazy_ptr
	.cfi_lsda 16, Lexception0
	pushl	%ebp
	.cfi_def_cfa_offset 8
	.cfi_offset %ebp, -8
	movl	%esp, %ebp
	.cfi_def_cfa_register %ebp
	pushl	%edi
	pushl	%esi
	subl	$16, %esp
	.cfi_offset %esi, -16
	.cfi_offset %edi, -12
	calll	L1$pb
L1$pb:
	popl	%edi
	xorl	%esi, %esi
	movl	8(%ebp), %eax
Ltmp0:
	movl	%eax, (%esp)
	calll	__Z7throweri
Ltmp1:
LBB1_2:
	xorl	%edi, %edi
	movl	%esi, %eax
	addl	$16, %esp
	popl	%esi
	popl	%edi
	popl	%ebp
	retl
LBB1_1:
Ltmp2:
	movl	%eax, (%esp)
	calll	___cxa_begin_catch
	movl	_val-L1$pb(%edi), %esi
	calll	___cxa_end_catch
	jmp	LBB1_2
Lfunc_end0:
	.cfi_endproc
	.section	__TEXT,__gcc_except_tab
	.p2align	2, 0x0
GCC_except_table1:
Lexception0:
	.byte	255                             ## @LPStart Encoding = omit
	.byte	155                             ## @TType Encoding = indirect pcrel sdata4
	.uleb128 Lttbase0-Lttbaseref0
Lttbaseref0:
	.byte	1                               ## Call site Encoding = uleb128
	.uleb128 Lcst_end0-Lcst_begin0
Lcst_begin0:
	.uleb128 Ltmp0-Lfunc_begin0             ## >> Call Site 1 <<
	.uleb128 Ltmp1-Ltmp0                    ##   Call between Ltmp0 and Ltmp1
	.uleb128 Ltmp2-Lfunc_begin0             ##     jumps to Ltmp2
	.byte	1                               ##   On action: 1
	.uleb128 Ltmp1-Lfunc_begin0             ## >> Call Site 2 <<
	.uleb128 Lfunc_end0-Ltmp1               ##   Call between Ltmp1 and Lfunc_end0
	.byte	0                               ##     has no landing pad
	.byte	0                               ##   On action: cleanup
Lcst_end0:
	.byte	1                               ## >> Action Record 1 <<
	.byte	0                               ##   No further actions
	.p2align	2, 0x0
Ltmp3:                                  ## TypeInfo 1
	.long	L__ZTIi$non_lazy_ptr-Ltmp3
Lttbase0:
	.p2align	2, 0x0
	.section	__TEXT,__text,regular,pure_instructions
	.globl	_main                           ## -- Begin function main
	.p2align	4
_main:                                  ## @main
	.cfi_startproc
	pushl	%ebp
	.cfi_def_cfa_offset 8
	.cfi_offset %ebp, -8
	movl	%esp, %ebp
	.cfi_def_cfa_register %ebp
	subl	$8, %esp
	movl	$1, (%esp)
	calll	__Z1fi
	movl	%eax, (%esp)
	calll	_exit
	xorl	%eax, %eax
	addl	$8, %esp
	popl	%ebp
	retl
	.cfi_endproc
	.section	__DATA,__data
	.globl	_val                            ## @val
	.p2align	2, 0x0
_val:
	.long	42                              ## 0x2a

	.section	__IMPORT,__pointers,non_lazy_symbol_pointers
L__ZTIi$non_lazy_ptr:
	.indirect_symbol	__ZTIi
	.long	0
L___gxx_personality_v0$non_lazy_ptr:
	.indirect_symbol	___gxx_personality_v0
	.long	0

.subsections_via_symbols
