## 99_pic_anchor_slot_reuse — a frame slot that held the PIC anchor is reused
## for an integer, and the integer reloaded from it must not count as the anchor.
##
## Portal server (GCC -O2): the anchor is spilled to -0x18(%ebp) for one loop,
## then `movl $0,-0x18(%ebp)` makes the slot the next loop's counter; the
## counter reloaded into %ecx was taken for the anchor, and
## `leal 0x874(%eax,%ecx),%edx` became `this + (anchor+0x874)` rip-relative:
## a wild pointer. Only a register spill used to end a slot's anchor.
##
## Exit 42 = the byte came from gArr. OFF arm: M64_NO_PIC_SLOT_WRITE_KILL=1 at
## translate time.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	subl	$36, %esp
	calll	L0
L0:
	popl	%ebx                        ## PIC register
	movl	%ebx, -24(%ebp)             ## anchor slot
	movl	-24(%ebp), %ecx
	leal	gArr-L0(%ecx), %eax         ## %eax = gArr (anchored through the slot)
	movl	$0, -24(%ebp)               ## the slot becomes a counter
	movl	-24(%ebp), %ecx             ## %ecx = 0, not the anchor
	leal	0x100(%eax,%ecx), %edx      ## &gArr[0x100]
	movzbl	(%edx), %eax
	movl	%eax, (%esp)
	calll	_exit

	.section __DATA,__data
gArr:
	.space	0x100
	.byte	42, 0, 0, 0
