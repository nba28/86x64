## 99_pic_anchor_pad_respill — a landing pad (after the final `ret`, reached by
## no branch; an indirect jmp stands in for the unwinder) that first re-spills
## the PIC register into the anchor's frame slot, then reloads it.
##
## -O0 GCC pads do `mov %ebx,-0x18(%ebp)` because the unwinder restored the
## callee-saved registers of the throwing call, where %ebx is the PIC base. The
## anchor tracker knows no register anchor past the `ret`, so that store
## killed the slot and the reload's GOT read kept its raw displacement (Portal
## 2 launcher). A callee-saved register stored into a live anchor slot past
## the final ret is now taken as the anchor.
##
## Exit code 42 on success; anything else without the fix.
## OFF arm: M64_NO_PIC_PAD_RESPILL=1 at translate time.

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
	popl	%ebx                        ## PIC register
	movl	%ebx, -12(%ebp)             ## spilled to a frame slot
	leal	Lpad-L0(%ebx), %eax
	jmpl	*%eax                       ## no recorded edge to the pad
Lret:
	addl	$20, %esp
	popl	%ebx
	popl	%ebp
	retl                                ## the function's final ret
Lpad:
	movl	%ebx, -12(%ebp)             ## re-spill the PIC register
	movl	-12(%ebp), %ecx             ## reload the anchor
	movl	_value-L0(%ecx), %eax
	jmp	Lret

	.section __DATA,__data
	.p2align 2
	.space	32
_value:
	.long	42
