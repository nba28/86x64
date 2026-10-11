## 99_jt_case_after_ret — case bodies placed after a mid-function `ret` keep
## the PIC anchor, so a second switch dispatched from one of them is claimed.
##
## A forward branch target counts as pending, so a `ret` before it does not end
## the function for the jump-table walk. The case bodies of a claimed table are
## branch targets too, but the walk did not record them: the early epilogue
## cleared the anchor, and a dispatch inside a later case body (Portal 2
## libtier0, a 148-case parser loop with an inner 21-case switch) was never
## claimed, so its entries were translated as instructions.
##
## Exit code = 3 + 40 = 43 on success; anything else without the fix.
## OFF arm: M64_NO_JT_CASE_SNAP=1 at translate time.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	$1
	calll	_f
	movl	%eax, %esi
	pushl	$2
	calll	_f
	addl	%esi, %eax
	movl	%eax, (%esp)
	calll	_exit
	ud2

## int f(int k): switch (k) { case 0: return 0; case 1: return 3;
##                            case 2: switch (k + 1) { ... case 3: return 40; } }
	.globl _f
	.p2align 4, 0x90
_f:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	calll	L0
L0:
	popl	%ebx                                ## PIC anchor
	movl	8(%ebp), %ecx
	andl	$3, %ecx
	movl	Ltab1-L0(%ebx,%ecx,4), %eax
	addl	%ebx, %eax
	jmpl	*%eax
	.p2align 2
Ltab1:
	.long	Lret0-L0
	.long	Lret3-L0
	.long	Linner-L0
	.long	Lret0-L0
Lret0:
	xorl	%eax, %eax
Lout:
	popl	%ebx
	popl	%ebp
	retl                                    ## the early epilogue
Lret3:
	movl	$3, %eax
	jmp	Lout
Linner:                                     ## reached only through Ltab1
	movl	8(%ebp), %ecx
	incl	%ecx
	andl	$3, %ecx
	movl	Ltab2-L0(%ebx,%ecx,4), %eax
	addl	%ebx, %eax
	jmpl	*%eax
	.p2align 2
Ltab2:
	.long	Lret0-L0
	.long	Lret0-L0
	.long	Lret0-L0
	.long	Lret40-L0
Lret40:
	movl	$40, %eax
	jmp	Lout
