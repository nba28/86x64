## 99_pic_cfg_dead_pred — dead code after a noreturn call must not feed its
## PIC anchor state into a block live code reaches.
##
## clang -O1+ (C++): a throw path loads the anchor from its spill slot into a
## scratch register, calls __cxa_throw, then a dead `jmp` rejoins a landing
## pad's cleanup:
##         movl  -0x20c(%ebp),%ecx   ; throw path: anchor only in the slot
##         calll ___cxa_throw
##         jmp   Lpad                ; dead
##   Lpad: ...                       ; reached from live code: %edi = anchor
##         cmpl  disp(%edi),%eax     ; stayed RAW
## The dead block has no live pred, so it starts from the code before it, where
## %edi is not the anchor; meeting that state at Lpad lost %edi.
##
## Exit 42 = the PIC read hits _val. Kill switch M64_NO_PIC_DEAD_PREDS=1
## (translate time): a raw disp read, a fault or a wrong exit.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	subl	$44, %esp

	calll	Lpic0
Lpic0:
	popl	%edi                     ## edi = the PIC anchor
	movl	%edi, -0x10(%ebp)        ## spill
	testl	%esp, %esp               ## always taken
	jne	Llive
	xorl	%edi, %edi               ## throw path: edi is not the anchor
	movl	-0x10(%ebp), %ecx
	movl	%ecx, (%esp)
	calll	_exit
	jmp	Ljoin                    ## dead

Llive:
	jmp	Ljoin

Ljoin:                                   ## live: edi = anchor
	movl	(_val - Lpic0)(%edi), %eax
	movl	%eax, (%esp)
	calll	_exit
	ud2

	.data
	.p2align 2
_val:
	.long	42
