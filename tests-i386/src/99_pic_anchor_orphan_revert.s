## 99_pic_anchor_orphan_revert — an ORPHAN block (after a dead fall-through,
## reached only by a backward branch) must not keep rewrites made from the stale
## state of the code before its jmp: pass 1 reverts its stale rewrites.
##
## Portal 2 client CHudCloseCaption::Process (i386 0x3ec14d..0x3ec1ac):
##         jne   Lsib                      ; %ebx = PIC anchor on this edge
##         xorl  %ebx,%ebx                 ; %ebx becomes a loop index
##         jmp   Lcheck
##   Lsib: ...   jmp Ltop                  ; backward
##   Lbody: movl %eax,-0xab74(%ebp,%ebx)   ; reached only by `ja/jae Lbody`
##   Lcheck: ...  ja Lbody
## The body inherited Lsib's state (%ebx anchored) and the frame-array store
## was rewritten as a store into the image: SIGSEGV on a tagged caption.
##
## Exit 42 = the frame array holds what the loop stored. Kill switch
## M64_NO_PIC_ORPHAN_REVERT=1 (translate time) reproduces the bad rewrite.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_pad:                                    ## anchor-0x200 must land inside __text
	.space	0x240, 0x90
	retl

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	subl	$0x220, %esp

	calll	Lpic0
Lpic0:
	popl	%ebx                     ## ebx = the PIC anchor
	leal	(_val - Lpic0)(%ebx), %edi
	movl	$0, -0x200(%ebp)
	movl	$0, -0x1fc(%ebp)
	movl	$0, -0x1f8(%ebp)
	xorl	%ecx, %ecx
Ltop:
	cmpl	$0, %ecx
	jne	Lsib                     ## not taken; carries the ebx anchor
	xorl	%ebx, %ebx               ## ebx is now a loop index
	jmp	Lcheck
Lsib:
	movl	(%edi), %esi
	incl	%ecx
	jmp	Ltop                     ## backward
Lbody:                                   ## dead fall-through; only `jb` reaches it
	movl	%eax, -0x200(%ebp,%ebx)  ## frame-array store (disp32), ebx = index
	addl	$4, %ebx
Lcheck:
	movl	$7, %eax
	cmpl	$12, %ebx
	jb	Lbody                    ## backward

	movl	-0x200(%ebp), %eax
	addl	-0x1fc(%ebp), %eax
	addl	-0x1f8(%ebp), %eax          ## 21 if all three stores landed
	addl	%eax, %eax
	movl	%eax, (%esp)
	calll	_exit
	ud2

	.data
	.p2align 2
_val:
	.long	0
