## 99_jt_base_after_dispatch — a PIC switch whose table base is computed AFTER
## the dispatch in address order and reaches it only through a backward branch.
##
## GCC hoists a loop-invariant table base out of a switch-in-a-loop and places
## the code that computes it at the bottom, so the linear walk meets
##
##     Lloop:  movl (%edi,%eax,4),%eax ; addl %ebx,%eax ; jmpl *%eax   <- dispatch
##             <inline table>
##             ...
##             leal Ltable-L0(%ebx),%edi ; jmp Lloop                    <- base
##
## (expat's tokenizer in iMovie's MobileMe.framework: 63 dispatches; also iMovie,
## HeliumRender). Back-edge joins carry anchors, not table bases, so the dispatch
## stayed unclaimed and its entries were translated as instructions. The second
## dispatch keeps the base in a frame slot instead of a register.
##
## Exit code = 15 + 30 = 45 on success; anything else without the fix.
## OFF arm: M64_NO_JT_BASE_EVIDENCE=1 at translate time.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp
	calll	L0
L0:
	popl	%ebx                        ## PIC anchor
	xorl	%esi, %esi                  ## accumulator
	xorl	%ecx, %ecx                  ## i
	jmp	Lsetup

## --- dispatch 1: table base in %edi, set below ---
Lloop:
	movl	%ecx, %eax
	andl	$3, %eax
	movl	(%edi,%eax,4), %eax
	addl	%ebx, %eax
	jmpl	*%eax
	.p2align 2
Ltable:
	.long	Lc0-L0
	.long	Lc1-L0
	.long	Lc2-L0
	.long	Lc3-L0
Lc0:	addl	$1, %esi
	jmp	Lnext
Lc1:	addl	$2, %esi
	jmp	Lnext
Lc2:	addl	$4, %esi
	jmp	Lnext
Lc3:	addl	$8, %esi
Lnext:
	incl	%ecx
	cmpl	$4, %ecx
	je	Lpart2
Lsetup:
	leal	Ltable-L0(%ebx), %edi
	jmp	Lloop

## --- dispatch 2: table base in a frame slot, set below ---
Lpart2:
	xorl	%ecx, %ecx
	jmp	Lsetup2
Lloop2:
	movl	%ecx, %eax
	andl	$3, %eax
	movl	-12(%ebp), %edx
	movl	(%edx,%eax,4), %eax
	addl	%ebx, %eax
	jmpl	*%eax
	.p2align 2
Ltable2:
	.long	Ld0-L0
	.long	Ld1-L0
	.long	Ld2-L0
	.long	Ld3-L0
Ld0:	addl	$3, %esi
	jmp	Lnext2
Ld1:	addl	$5, %esi
	jmp	Lnext2
Ld2:	addl	$7, %esi
	jmp	Lnext2
Ld3:	addl	$15, %esi
Lnext2:
	incl	%ecx
	cmpl	$4, %ecx
	je	Ldone
Lsetup2:
	leal	Ltable2-L0(%ebx), %edx
	movl	%edx, -12(%ebp)
	jmp	Lloop2

Ldone:
	movl	%esi, (%esp)
	calll	_exit
	ud2
