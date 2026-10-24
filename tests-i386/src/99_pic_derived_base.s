## 99_pic_derived_base — clang selects between two addresses off one PIC anchor
## and folds the common displacement into the use:
##     lea 8(%edi),%eax ; cmove %edi,%eax ; mov tab-L0(%eax),%edx
## The use's raw disp reached the wrong data (the translated anchor sits at
## another distance from it). Both producers are exercised: 30 + 12 = 42.
## OFF arm: M64_NO_PIC_DERIVED_BASE=1 at translate time.
	.text
	.globl _main
_main:
	calll	L0
L0:	popl	%edi
	movl	$1, %ecx
	leal	8(%edi), %eax
	cmpl	$1, %ecx
	cmovel	%edi, %eax              ## taken: the anchor itself
	movl	Ltab-L0(%eax), %edx     ## tab[0] = 30
	leal	8(%edi), %eax
	cmpl	$2, %ecx
	cmovel	%edi, %eax              ## not taken: anchor + 8
	addl	Ltab-L0(%eax), %edx     ## tab[2] = 12
	pushl	%edx
	calll	_exit
	.data
Ltab:	.long	30, 5, 12
