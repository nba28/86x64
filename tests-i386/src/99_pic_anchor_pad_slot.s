## 99_pic_anchor_pad_slot — a block after the function's final `ret`, reached
## by no branch (an EH landing pad; here an indirect jmp stands in for the
## unwinder), that reloads the PIC anchor from its frame slot.
##
## The anchor tracker ended the function at the `ret` and cleared everything,
## frame slots included, so the pad's `mov -8(%ebp),%ecx; mov value(%ecx),%eax`
## kept its raw i386 displacement: a wild read from translated anchor + i386
## offset (Portal 2 launcher, libtogl, materialsystem cleanup paths). The pad
## runs in the function's own frame, so its slots stay valid until the next
## function symbol.
##
## Exit code 42 on success; anything else without the fix.
## OFF arm: M64_NO_PIC_SLOTS_PAST_RET=1 at translate time.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	calll	_f
	movl	%eax, (%esp)
	calll	_exit
	ud2

	.globl _f
	.p2align 4, 0x90
_f:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	subl	$20, %esp
	calll	L0
L0:
	popl	%ecx                        ## PIC anchor
	movl	%ecx, -8(%ebp)              ## spilled to a frame slot
	leal	Lpad-L0(%ecx), %eax
	jmpl	*%eax                       ## no recorded edge to the pad
Lret:
	addl	$20, %esp
	popl	%ebx
	popl	%ebp
	retl                                ## the function's final ret
Lpad:
	movl	-8(%ebp), %ecx              ## reload the anchor
	movl	_value-L0(%ecx), %eax
	jmp	Lret

	.section __DATA,__data
	.p2align 2
	.space	32
_value:
	.long	42
