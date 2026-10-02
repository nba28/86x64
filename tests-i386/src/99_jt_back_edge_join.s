## 99_jt_back_edge_join — a PIC switch whose dispatch block sits BEFORE the
## code that reloads its anchor, reached only by a BACKWARD branch.
##
## Portal 2 server `CUtlBuffer::VaScanf` (i386 dispatch 0x82de45): the anchor
## is spilled to -0x64(%ebp); %ebx holds the format character; a block much
## later reloads the anchor into %ebx and `jbe`s back to
##
##     movl 0x6ce(%ebx,%ecx,4),%ecx ; addl %ebx,%ecx ; jmp *%ecx
##
## Both linear walks (DetectJumpTables, DetectPicAnchoredDisps) only joined
## FORWARD-branch snapshots, so at the dispatch %ebx had no anchor: the table
## was never claimed and the load kept its raw disp. Translated, %ebx is the
## translated anchor, so the "table" read was code bytes and the jump landed
## past the image (rip in the Rosetta AOT mapping, SIGSEGV at 0x200).
##
## Exit 42 = case 2 ran. Kill switch M64_NO_PIC_BACK_EDGE=1 (translate time)
## reproduces the wild jump: a wrong exit code or a signal death.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	subl	$40, %esp

	calll	Lpic0
Lpic0:
	popl	%eax                     ## eax = the PIC anchor
	movl	%eax, -20(%ebp)          ## SPILL it
	movl	$7, %ebx                 ## ebx is NOT the anchor on the way in
	jmp	Lscan

Ldispatch:                               ## reached only from the jbe below
	movl	(Ltable - Lpic0)(%ebx,%ecx,4), %ecx
	addl	%ebx, %ecx
	jmp	*%ecx

Lcase0:
	movl	$10, %esi
	jmp	Lend
Lcase1:
	movl	$20, %esi
	jmp	Lend
Lcase2:
	movl	$42, %esi
	jmp	Lend
Lcase3:
	movl	$30, %esi
	jmp	Lend

Lscan:
	movl	$2, %ecx                 ## the switch value -> case 2
	cmpl	$3, %ecx
	movl	-20(%ebp), %ebx          ## RELOAD the anchor
	jbe	Ldispatch                ## BACKWARD branch to the dispatch
	movl	$1, %esi

Lend:
	movl	%esi, (%esp)
	calll	_exit
	ud2

	.p2align 2
Ltable:
	.long	Lcase0 - Lpic0
	.long	Lcase1 - Lpic0
	.long	Lcase2 - Lpic0
	.long	Lcase3 - Lpic0

	.p2align 4, 0x90
	.space 16, 0x90
