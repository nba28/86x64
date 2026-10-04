## 99_pic_anchor_local_noreturn — a call to a DEFINED noreturn helper
## (clang's private ___clang_call_terminate, one copy per image) must end the
## fall-through like an import stub does.
##
## Portal 2 engine S_StartSound_Immediate (i386 0x5af40..0x5afab):
##   pad:  movl  %eax,(%esp)
##         call  ___clang_call_terminate   ; local `t` symbol, never returns
##   join: ...                             ; also the target of an earlier jbe
##         mulss 0x48816b(%ebx),%xmm0      ; PIC load: stayed RAW -> SIGSEGV
## noreturn_stubs held import stubs only, so the pad's dead fall-through was
## intersected into the join and wiped the anchor.
##
## Exit 42 = the PIC store/load hit _val. Kill switch M64_NO_PIC_NORETURN=1
## (translate time) reproduces the raw-disp store.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
___clang_call_terminate:                 ## same shape as clang's helper
	pushl	%ebp
	movl	%esp, %ebp
	subl	$8, %esp
	calll	_abort

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	pushl	%edi
	subl	$32, %esp

	calll	Lpic0
Lpic0:
	popl	%esi                     ## esi = the PIC anchor
	movl	$3, %edi                 ## loop counter
	cmpl	$99, %edi
	je	Lpad                     ## never taken: a real edge INTO the pad, so
	                                 ## the pad is not an orphan block and only
	                                 ## the noreturn rule can protect the join
	jmp	Lentry                   ## the loop's only real entry

Lpad:                                    ## EH-landing-pad shape, never run
	movl	%eax, %esi               ## clobbers the anchor register
	movl	%esi, (%esp)
	calll	___clang_call_terminate  ## noreturn, DEFINED here

Lhead:                                   ## reached only by the backward jg
	decl	%edi

Lentry:
	cmpl	$0, %edi
	jg	Lhead                    ## BACKWARD

	movl	$42, (_val - Lpic0)(%esi)        ## PIC store after the loop
	movl	(_val - Lpic0)(%esi), %eax
	movl	%eax, (%esp)
	calll	_exit
	ud2

	.data
	.p2align 2
_val:
	.long	0
