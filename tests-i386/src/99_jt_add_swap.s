## 99_jt_add_swap — clang -O0 adds the table entry INTO the anchor register,
## `add %entry,%anchor; jmp *%anchor`; only the GCC order was claimed, so the
## table was decoded as code. Exit 42.
## OFF arm: M64_NO_JT_ADD_SWAP=1 at translate time.
	.text
	.globl _main
_main:
	calll	L0
L0:	popl	%edx
	movl	$2, %ecx
	movl	Ltab-L0(%edx,%ecx,4), %ecx
	addl	%ecx, %edx
	jmp	*%edx
	.align 2
Ltab:	.long	Lbad-L0
	.long	Lbad-L0
	.long	Lgood-L0
	.long	Lbad-L0
Lbad:	pushl	$7
	calll	_exit
Lgood:	pushl	$42
	calll	_exit
