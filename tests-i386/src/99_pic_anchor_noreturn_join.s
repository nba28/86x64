## 99_pic_anchor_noreturn_join — the code after a call that never returns is
## reached only by branches; it must not dilute the next join.
##
## Portal 2 server CServerGameDLL::DLLInit (i386 0x3b0cc3..0x3b0d47):
##
##         jmp   0x3b0ce1                  ; the loop's real entry (%esi = anchor)
##   pad:  movl  %eax,%esi                 ; EH landing pad clobbers %esi ...
##         call  _Unwind_Resume            ; ... and never returns
##   head: call  _ThreadSleep              ; reached only by backward branches
##   0x3b0ce1: ...                         ; join: forward jmp + fall-through
##         jbe   head                      ; backward
##         movl  %eax,0x8d821a(%esi)       ; PIC store after the loop
##
## DetectPicAnchoredDisps treated the noreturn call as returning: the dead
## fall-through (no %esi anchor) was intersected into the join and dropped the
## anchor, so the store kept its raw i386 disp and wrote into __TEXT (SIGBUS).
## The back-edge join cannot recover it: the back-edge state is computed from
## the already-diluted join.
##
## Exit 42 = the PIC store/load hit _val. Kill switch M64_NO_PIC_NORETURN=1
## (translate time) reproduces the raw-disp store: a fault or a wrong exit.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	pushl	%edi
	subl	$32, %esp

	calll	Lpic0
Lpic0:
	popl	%esi                     ## esi = the PIC anchor
	movl	$3, %edi                 ## loop counter
	cmpl	$99, %edi
	je	Lpad                     ## never taken: a real edge INTO the pad, so
	                                 ## the pad is not an orphan block and only
	                                 ## the noreturn rule can protect the join
	jmp	Lentry                   ## the loop's only real entry

Lpad:                                    ## EH-landing-pad shape, never run
	movl	%eax, %esi               ## clobbers the anchor register
	movl	%esi, (%esp)
	calll	_abort                   ## noreturn

Lhead:                                   ## reached only by the backward jg
	decl	%edi

Lentry:
	cmpl	$0, %edi
	jg	Lhead                    ## BACKWARD

	movl	$42, (_val - Lpic0)(%esi)        ## PIC store after the loop
	movl	(_val - Lpic0)(%esi), %eax
	movl	%eax, (%esp)
	calll	_exit
	ud2

	.data
	.p2align 2
_val:
	.long	0
