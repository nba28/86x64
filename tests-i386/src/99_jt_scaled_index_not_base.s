## 99_jt_scaled_index_not_base — a register scaled by 4 is a switch's CASE
## INDEX, never its table base.
##
## Portal 2 engine MXR_LoadAllSoundMixers (i386 0x85faf..0x85fc2):
##
##         leal  -0x1(%esi),%eax                 ; case = counter - 1
##         movl  0x82b(%ebx,%eax,4),%eax         ; folded [anchor + idx*4 + d]
##         addl  %ebx,%eax ; xorl %esi,%esi ; jmpl *%eax
##
## DetectJumpTables keeps an anchor across an overwrite of its register, and
## %esi had been a copy of the anchor before it became the loop counter. So
## `lea -1(%esi)` looked like "anchor - 1 = a table base", and the load matched
## "table base in the index field" before the folded form: wrong base, table
## never claimed, raw i386 entries -> SIGILL mid-instruction. Kill switch
## M64_NO_JT_SCALED_INDEX_GUARD=1 (translate time). Exit 42 = case 2 ran.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	subl	$28, %esp

	calll	Lpic0
Lpic0:
	popl	%ebx                     ## ebx = anchor
	movl	%ebx, %esi               ## a copy of the anchor ...
	movl	$3, %esi                 ## ... then esi becomes the counter
	leal	-1(%esi), %eax           ## the case index (2)
	cmpl	$3, %eax
	ja	Ldefault
	movl	(Ltable - Lpic0)(%ebx,%eax,4), %eax
	addl	%ebx, %eax
	xorl	%esi, %esi
	jmp	*%eax

Lcase0:
	movl	$10, %edi
	jmp	Lend
Lcase1:
	movl	$20, %edi
	jmp	Lend
Lcase2:
	movl	$42, %edi
	jmp	Lend
Lcase3:
	movl	$30, %edi
	jmp	Lend
Ldefault:
	movl	$1, %edi

Lend:
	movl	%edi, (%esp)
	calll	_exit
	ud2

	.p2align 2
Ltable:
	.long	Lcase0 - Lpic0
	.long	Lcase1 - Lpic0
	.long	Lcase2 - Lpic0
	.long	Lcase3 - Lpic0

	.p2align 4, 0x90
	.space 16, 0x90
