## 99_jt_orphan_pending — a block reached only by `jmp *%esi` jumps over a
## `ret` to a PIC switch. The orphan's forward jmp recorded no pending target,
## so the ret ended the walk's state and the table was decoded as code. Exit 42.
## OFF arm: M64_NO_JT_ORPHAN_PENDING=1 at translate time.
	.text
	.globl _main
_main:
	calll	L0
L0:	popl	%ebx
	movl	$2, %eax
	leal	Lx-L0(%ebx), %esi
	jmp	*%esi
Lx:	jmp	Ld
Lc:	ret
Ld:	andl	$3, %eax
	movl	Ltab-L0(%ebx,%eax,4), %ecx
	addl	%ebx, %ecx
	jmp	*%ecx
	.align 2
Ltab:	.long	Lbad-L0
	.long	Lbad-L0
	.long	Lgood-L0
	.long	Lbad-L0
Lbad:	pushl	$7
	calll	_exit
Lgood:	pushl	$42
	calll	_exit
