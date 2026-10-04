## 99_pic_anchor_unreached_pad — two ways the noreturn/orphan rules dropped a
## live PIC anchor (corpus diff 2026-10-04: Angry Birds 4442 hunks, Civ IV 811).
##
## Part 1 (M64_NO_PIC_ORPHAN_BACK_ONLY): a block that NO recorded edge reaches
## (an EH landing pad; the unwinder enters it with the throw site's callee-saved
## registers) is not ORPHAN: only a backward-branch target is. As orphan, the
## pad's `jmp` recorded no snapshot, and the join after a noreturn call ran on
## the linear state, which had lost the anchor (Angry Birds 0x28af..0x28c3).
## An indirect jmp stands in for the unwinder.
##
## Part 2 (M64_NO_PIC_UNREACHED_ONCE): `unreached` (after a noreturn call) is
## the dead edge into the NEXT instruction only. Kept sticky, it leaked into a
## following function that has no symbol (stripped image), so a conditional
## branch's fall-through that is also a backward target went ORPHAN and its
## jmp recorded nothing (Angry Birds 0x1aae4).
##
## Each part stores 21 through its anchor; exit 42 = both stores hit _val.
## Either kill switch (translate time) leaves a raw operand: fault or wrong exit.

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
	popl	%edi                     ## edi = the PIC anchor (callee-saved)
	leal	(Lpad1 - Lpic0)(%edi), %eax
	jmp	*%eax                    ## the "unwinder": no edge the walker sees

Lpad1:                                   ## reached by nothing recorded
	movl	%eax, %esi
	jmp	Ljoin

Lpad2:                                   ## linear predecessor of Ljoin
	xorl	%edi, %edi               ## ... has lost the anchor
	movl	%edi, (%esp)
	calll	_abort                   ## never returns

Ljoin:
	movl	$21, (_val - Lpic0)(%edi)        ## PIC store via the pad's anchor
	calll	Lfn                      ## part 2 adds 21
	movl	(_val - Lpic0)(%edi), %eax
	movl	%eax, (%esp)
	calll	_exit
	calll	_abort                   ## noreturn call right before Lfn

Lfn:                                     ## a function with NO symbol
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	subl	$8, %esp
	calll	Lpic1
Lpic1:
	popl	%ebx                     ## ebx = anchor
	movl	$1, %esi
	testl	%esi, %esi
	je	Lother                   ## not taken
Lfall:                                   ## fall-through; also a BACKWARD target
	jmp	Lfnjoin
Lother:
	xorl	%ebx, %ebx               ## the linear state before Lfnjoin loses ebx
	jmp	Lfall                    ## backward: makes Lfall a back target
Lfnjoin:
	movl	(_val - Lpic1)(%ebx), %eax
	addl	$21, %eax
	movl	%eax, (_val - Lpic1)(%ebx)
	addl	$8, %esp
	popl	%esi
	popl	%ebx
	popl	%ebp
	retl

	.data
	.p2align 2
_val:
	.long	0
