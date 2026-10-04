## 99_pic_anchor_orphan_block — a block after a dead fall-through that only a
## BACKWARD branch reaches has unknown state on the linear walk; its forward
## branches must not constrain the join they reach.
##
## Portal 2 engine Mod_LoadNodes (i386 0x268997..0x268a1f):
##         jmp   head                      ; ecx = anchor
##   slow: call  ...                       ; reached only by the backward jae
##         jmp   join
##   head: movl  %ecx,%ebx                 ; save the anchor across calls
##         ...   jae slow                  ; BACKWARD
##         call  ...
##   join: movl  %ebx,%ecx                 ; restore
##         jl    head                      ; back edge
##         movl  0x30aaa3(%ecx),%ecx       ; PIC load after the loop: stayed RAW
## The slow block ran on the stale pre-jmp state (no %ebx anchor) and its
## `jmp join` dropped %ebx at the join; the loop head's back-edge snapshot then
## carried the loss into the second walk too.
##
## Exit 42 = the PIC store/load hit _val. Kill switch M64_NO_PIC_ORPHAN_TOP=1
## (translate time) reproduces the raw-disp store: a fault or a wrong exit.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_noop:
	retl

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
	popl	%ecx                     ## ecx = the PIC anchor
	movl	$3, %edi                 ## loop counter
	jmp	Lhead

Lslow:                                   ## reached only by the backward jae
	calll	_noop                    ## clobbers ecx (caller-saved)
	jmp	Ljoin

Lhead:
	movl	%ecx, %ebx               ## save the anchor
	cmpl	$2, %edi
	jae	Lslow                    ## BACKWARD
	calll	_noop
Ljoin:
	movl	%ebx, %ecx               ## restore the anchor
	decl	%edi
	jg	Lhead                    ## back edge

## Part 2 (Portal 2 engine CClientState::SetSignonState): a reached block that
## OPENS with a call is live, not orphan. ft_dead stayed sticky across that
## call, so its `je` recorded nothing and the target ran on stale state.
	movl	%ecx, %ebx
	jmp	Lcase
	nop
Lcase:                                   ## forward target after a dead fall-through
	calll	_noop
	movl	%ebx, %esi               ## anchor -> esi
	testl	%edi, %edi               ## edi == 0 after the loop: taken
	je	Lfar
	xorl	%esi, %esi               ## linear state loses esi ...
	movl	%esi, (%esp)
	calll	_exit                    ## ... on a path that never returns

Lfar:
	movl	$42, (_val - Lpic0)(%esi)        ## PIC store via the je edge's anchor
	movl	(_val - Lpic0)(%esi), %eax
	movl	%eax, (%esp)
	calll	_exit
	ud2

	.data
	.p2align 2
_val:
	.long	0
