## 99_abs_table_rewrite_forms — absolute-table memory operands on the iforms
## the transform REWRITES (push/pop [mem], `op [mem], $ptr`) rather than
## copying byte-identical. Each rewrite must carry the operand's relocation;
## before the shared operand lowering they rebuilt the bytes from the i386
## instruction and dropped the captured table address, so the translated
## access hit the unmapped i386 vmaddr.
## Exit 0 iff every access saw the relocated table.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	## push [disp32 + idx*4]
	movl	$2, %ecx
	pushl	_tab(,%ecx,4)
	popl	%edx
	cmpl	$0x33333333, %edx
	jne	Lbad

	## push [base + disp32]
	movl	$4, %eax
	pushl	_tab(%eax)
	popl	%edx
	cmpl	$0x22222222, %edx
	jne	Lbad

	## push [base + idx*4 + disp32]
	movl	$4, %eax
	movl	$2, %ecx
	pushl	_tab(%eax,%ecx,4)
	popl	%edx
	cmpl	$0x44444444, %edx
	jne	Lbad

	## pop [disp32 + idx*4]
	movl	$0, %ecx
	pushl	$0x5a5a5a5a
	popl	_tab(,%ecx,4)
	cmpl	$0x5a5a5a5a, _tab
	jne	Lbad

	## pop [base + disp32]
	movl	$4, %eax
	pushl	$0x6b6b6b6b
	popl	_tab(%eax)
	cmpl	$0x6b6b6b6b, _tab+4
	jne	Lbad

	## mov [base + disp32], $pointer  (field store of a data pointer)
	movl	$8, %eax
	movl	$_tab, _slots(%eax)
	movl	_slots+8, %edx
	cmpl	$_tab, %edx
	jne	Lbad
	movl	8(%edx), %edx
	cmpl	$0x33333333, %edx
	jne	Lbad

	pushl	$0
	calll	_exit
	ud2

Lbad:
	pushl	$1
	calll	_exit
	ud2

	.section __DATA,__data
	.globl _tab
	.p2align 2
_tab:
	.long	0x11111111
	.long	0x22222222
	.long	0x33333333
	.long	0x44444444
	.globl _slots
_slots:
	.long	0, 0, 0, 0
