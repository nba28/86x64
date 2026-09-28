## 99_call_mem_eax_arg — an indirect `call *mem` must not clobber an i386
## register: the callee may take an argument in %eax (regparm / hand-written
## asm). Covers the translated forms: absolute slot, base register, scaled
## index. Exit 0 iff the callee saw eax == 42 every time.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	movl	$42, %eax
	calll	*_fp
	cmpl	$42, %eax
	jne	Lbad

	movl	$_fp, %edx
	movl	$42, %eax
	calll	*(%edx)
	cmpl	$42, %eax
	jne	Lbad

	movl	$1, %ecx
	movl	$42, %eax
	calll	*_fp(,%ecx,4)
	cmpl	$42, %eax
	jne	Lbad

	pushl	$0
	calll	_exit
	ud2

Lbad:
	pushl	$1
	calll	_exit
	ud2

	## returns its eax argument unchanged
	.globl _echo_eax
	.p2align 4, 0x90
_echo_eax:
	pushl	%ebp
	movl	%esp, %ebp
	popl	%ebp
	retl

	.section __DATA,__data
	.globl _fp
	.p2align 2
_fp:
	.long	_echo_eax
	.long	_echo_eax
