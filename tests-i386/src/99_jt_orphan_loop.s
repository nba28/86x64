## 99_jt_orphan_loop — a loop entered by a forward jmp to its test, whose
## body sits after an early epilogue, dispatching off an anchor reloaded from a
## frame slot.
##
## The jump-table walk reaches the body (Lskip) right after the `ret` with the
## epilogue's empty state; nothing records an edge to it yet (only a backward
## branch reaches it). Falling from there into the loop test intersected that
## stale state with the forward jmp's snapshot and dropped the spilled anchor,
## so the dispatch below was never claimed and its entries were translated as
## instructions (Portal 2 engine, two dispatches). Such ORPHAN code now runs
## without recording edges and the next join adopts its snapshot, as in
## DetectPicAnchoredDisps.
##
## Exit code 30 on success; anything else without the fix.
## OFF arm: M64_NO_JT_ORPHAN=1 at translate time.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	$6                          ## k = 6 -> decremented to 3 -> case 3
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
	movl	8(%ebp), %ebx
	xorl	%eax, %eax
	jmp	Lloop
Lret:
	addl	$20, %esp
	popl	%ebx
	popl	%ebp
	retl                                ## the early epilogue
Lskip:                                  ## ORPHAN: only the backward `ja` reaches it
	decl	%ebx
Lloop:
	cmpl	$3, %ebx
	ja	Lskip
	movl	-8(%ebp), %edx              ## reload the anchor
	movl	Ltab-L0(%edx,%ebx,4), %ecx
	addl	%edx, %ecx
	jmpl	*%ecx
	.p2align 2
Ltab:
	.long	Lc0-L0
	.long	Lc1-L0
	.long	Lc2-L0
	.long	Lc3-L0
Lc0:	movl	$10, %eax
	jmp	Lret
Lc1:	movl	$11, %eax
	jmp	Lret
Lc2:	movl	$20, %eax
	jmp	Lret
Lc3:	movl	$30, %eax
	jmp	Lret
