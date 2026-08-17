## 99_pic_anchor_entry_save — a PIC anchor must SURVIVE the epilogue restore of
## the callee-saved PIC register, because the anchor walk is LINEAR and the
## epilogue lies lexically BEFORE basic blocks the function only reaches through
## an INDIRECT (jump-table) dispatch.
##
## MEASURED on Civ IV's Python 2.6 (task #45). i386 function @0x96fd1:
##
##     pushl %ebp ; movl %esp,%ebp ; subl $0x38,%esp
##     movl  %ebx,-0xc(%ebp)          ## ENTRY SAVE — %ebx is not an anchor yet
##     movl  %esi,-0x8(%ebp) ; movl %edi,-0x4(%ebp)
##     calll .+0 ; popl %ebx          ## anchor = 0x96fe5
##     ...
##     movl  0x47(%ebx,%edi,4),%eax ; addl %ebx,%eax ; jmp *%eax   ## 10-case switch
##     <inline table @0x9702c>
##     ...
##     movl  -0xc(%ebp),%ebx          ## EPILOGUE RESTORE  <-- erased the anchor
##     ...
##     movl  0x51033(%ebx),%eax       ## case body reached ONLY via `jmp *%eax`
##     movl  (%eax),%eax
##
## DetectPicAnchoredDisps step (2b) read the epilogue restore as "the register
## was re-purposed" and did anchors.erase(EBX). Every later block then kept its
## RAW i386 displacement while %ebx held the TRANSLATED anchor, so at runtime
## 0x51033 past the translated anchor landed 0x1d87 inside __TEXT,__cstring and
## the following dereference read the ASCII "e AS" -> SIGSEGV at 0x53412065.
##
## branch_anchor_snap repairs a block reached by a DIRECT forward branch, but it
## is keyed on branch displacements and therefore cannot see a jump-table case
## body. That is why every existing pic_anchor_* fixture passes over this.
##
## THE FIXTURE reproduces all three required ingredients:
##   (i)   %ebx saved to a frame slot BEFORE the `calll .+0; popl %ebx` anchor,
##   (ii)  an epilogue restore of that slot early in the byte stream,
##   (iii) a case body reachable ONLY through the indirect `jmp *%eax`.
## Lcase1 loads _magic anchor-relative and checks it.
##   ON  (gate armed):     the load is rewritten rip-relative -> exit 0.
##   OFF (M64_NO_PIC_ANCHOR_ENTRY_SAVE=1): the load keeps its i386 displacement,
##       reads the __text nop padding instead of _magic -> exit 7 (or a fault).

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp

	## (i) ENTRY SAVES — emitted by the prologue, BEFORE the anchor exists.
	movl	%ebx, -12(%ebp)
	movl	%esi, -8(%ebp)
	movl	%edi, -4(%ebp)

	## Establish the PIC anchor in the callee-saved %ebx.
	calll	Lpic0
Lpic0:
	popl	%ebx

	## PIC switch dispatch, table folded into the load's own addressing
	## (`movl tbl(%anchor,%idx,4)` — the GCC/clang combined form).
	movl	$1, %edi
	movl	(Ltab - Lpic0)(%ebx,%edi,4), %eax
	addl	%ebx, %eax
	jmp	*%eax

	.p2align 2
Ltab:
	.long	Lcase0 - Lpic0
	.long	Lcase1 - Lpic0

	## (ii) Case 0 doubles as the function's early exit path: it restores the
	## callee-saved registers from their entry slots and tail-jumps out. The
	## LINEAR walk reaches this restore before Lcase1 below.
Lcase0:
	movl	-12(%ebp), %ebx
	movl	-8(%ebp), %esi
	movl	-4(%ebp), %edi
	movl	$99, %eax
	jmp	Lfinish

	## (iii) Case 1 is reachable ONLY through the indirect `jmp *%eax` above —
	## no direct branch targets it, so branch_anchor_snap cannot restore the
	## anchor here. This is the load the bug corrupts.
Lcase1:
	movl	(_magic - Lpic0)(%ebx), %eax
	cmpl	$0x5A17C0DE, %eax
	jne	Lbad
	xorl	%eax, %eax
	jmp	Lfinish
Lbad:
	movl	$7, %eax

Lfinish:
	pushl	%eax
	calll	_exit
	ud2

	## Padding so the stale-displacement read of the OFF arm lands in MAPPED
	## __text (0x90 nops) and reports a deterministic exit 7 rather than a
	## fault. i386 __DATA sits one 4 KB page above __TEXT, so the stale
	## displacement is ~0x1000 past the translated anchor.
	.p2align 4, 0x90
	.space	0x2000, 0x90

	.section __DATA,__data
	.p2align 2
_magic:
	.long	0x5A17C0DE
