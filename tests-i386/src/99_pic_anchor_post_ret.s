## 99_pic_anchor_post_ret — a PIC store in a block placed AFTER the function's
## `ret`, reached only through branches (Portal 2009 filesystem_stdio
## CSearchPathsIterator::GetNext). The loop body is entered only by a back-edge
## (an "orphan" on the first walk), so its forward branch past the ret records
## no snapshot; the ret then clears the anchor set, the block after it starts
## empty, its back-edges carry that loss into the second walk, and the anchored
## store kept its raw i386 displacement. Kill switch M64_NO_PIC_POST_RET_BODY=1.
##
## Exit 42 = the store landed in gVal.
##
## Second case: the anchors kept after a ret must not reach an UNNAMED function
## that follows it (a `push %ebp` start with no symbol). Treated as the same
## body, its absolute pointer immediates were left raw (pvz, civ4, Halo).
##
## Third case: those anchors must die at the next function symbol. _pa's
## post-ret code ends in a call (like a pad ending in _Unwind_Resume), so its
## %edi anchor reached _pb, whose prologue save of %edi then read as an anchor
## slot; the epilogue restore no longer kept _pb's own anchor for the block
## after its tail call, which stored through a raw displacement (Portal 2
## libsteam CryptoPP::FileSink::~FileSink).

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp
	calll	_work
	cmpl	$42, %eax
	jne	Ldone
	calll	_anch
	calll	Lunnamed                 ## 42 only if its $gVal2 was relocated
	cmpl	$42, %eax
	jne	Ldone
	calll	_pa
	calll	_pb
	movl	gVal3, %eax              ## 42 only if Lpbpad stored through its anchor
Ldone:
	movl	%eax, (%esp)
	calll	_exit

	.globl _work
	.p2align 4, 0x90
_work:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	subl	$20, %esp
	calll	Lpic
Lpic:
	popl	%ebx                     ## ebx = the PIC anchor
	movl	$0, %ecx
	jmp	Lhead                    ## Lbody is entered only by the back-edge
Lbody:
	cmpl	$2, %ecx
	je	Lpost                    ## forward, past the ret
Lnext:
	addl	$1, %ecx
Lhead:
	cmpl	$4, %ecx
	jl	Lbody
	movl	gVal-Lpic(%ebx), %eax
	addl	$20, %esp
	popl	%ebx
	popl	%ebp
	retl
Lpost:
	movl	$42, gVal-Lpic(%ebx)     ## the anchored store after the ret
	addl	$1, %ecx
	jmp	Lbody                    ## back ABOVE the branch: its empty first-walk
	                         ## state is what the second walk intersects with

	.globl _anch
	.p2align 4, 0x90
_anch:
	pushl	%ebp
	movl	%esp, %ebp
	calll	Lpic2
Lpic2:
	popl	%esi                     ## %esi anchor still live at the ret
	movl	gVal-Lpic2(%esi), %eax
	popl	%ebp
	retl
Lunnamed:                        ## no symbol: only its frame says "new function"
	pushl	%ebp
	movl	%esp, %ebp
	movl	$gVal2, %eax
	movl	(%eax), %eax
	popl	%ebp
	retl

	.globl _pa
	.p2align 4, 0x90
_pa:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$8, %esp
	calll	Lpa
Lpa:
	popl	%edi                     ## %edi anchor still live at the ret
	movl	gVal-Lpa(%edi), %eax
	addl	$8, %esp
	popl	%ebp
	retl
	movl	%eax, (%esp)             ## pad-shaped: ends in a call, keeps %edi
	calll	_abort

	.globl _pb
	.p2align 4, 0x90
_pb:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp
	movl	%edi, -4(%ebp)           ## entry save, before the anchor exists
	calll	Lpb
Lpb:
	popl	%edi
	leal	Lpbpad-Lpb(%edi), %eax
	calll	*%eax                    ## runs the block below, which returns from _pb
	movl	-4(%ebp), %edi           ## epilogue restore of the entry %edi
	leave
	jmp	_abort                   ## tail-call shape (never runs)
Lpbpad:
	movl	$42, gVal3-Lpb(%edi)     ## anchored store after the tail call
	movl	-4(%ebp), %edi
	leave
	retl

	.section __DATA,__data
	.p2align 2
gVal:
	.long	3
gVal2:
	.long	42
gVal3:
	.long	5
