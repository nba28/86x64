## 99_highbyte_rex — a PIC-anchored, indexed byte load into a HIGH-byte register:
##
##     movb  gTable-Lpic(%edi,%eax), %ch
##     movb  gTable-Lpic(%edi,%eax), %bh
##
## The translator lowers the anchored operand to `lea r11,[rip+gTable]; [r11+eax]`.
## r11 needs a REX prefix, and under REX the ModR/M reg encodings of %ch/%bh mean
## %bpl/%dil, so the load silently landed in the frame pointer and the anchor
## (Portal engine ImageLoader::ConvertFromDXT5: rbp corrupted -> a decoded pixel
## written through a stack address). Kill switch M64_NO_HIGHBYTE_REX=1.
##
## The same with an operand that indexes through the high byte's own register
## (Portal engine and materialsystem):
##
##     movb  gTable-Lpic(%edi,%eax), %ah
##     movb  %ah, gOut-Lpic(%edi,%eax)
##
## Exit 42 = every byte arrived and %ebp / %edi survived.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%edi
	subl	$24, %esp
	calll	Lpic
Lpic:
	popl	%edi                     ## edi = the PIC anchor
	movl	%edi, -16(%ebp)          ## keep a copy to check %edi later
	movl	$2, %eax
	xorl	%ecx, %ecx
	xorl	%ebx, %ebx
	movb	gTable-Lpic(%edi,%eax), %ch
	movl	$3, %eax
	movb	gTable-Lpic(%edi,%eax), %bh
	movl	$1, %eax
	movb	gTable-Lpic(%edi,%eax), %ah   ## eax = 0x2201
	movb	%ah, gOut-Lpic(%edi,%eax)     ## gOut[0x2201] = 0x22
	movl	%eax, -20(%ebp)
	movl	$1, %eax                 ## exit 1 = %edi was clobbered
	cmpl	-16(%ebp), %edi
	jne	Lout
	movl	$2, %eax                 ## exit 2 = %ebp was clobbered (stack copy wrong)
	leal	8(%esp), %edx
	cmpl	%edx, %ebp
	je	Lebp_bad
	movl	%ebp, %edx
	subl	%esp, %edx
	cmpl	$32, %edx                ## frame size: 24 locals + ebx + edi
	jne	Lout
	movl	$3, %eax                 ## exit 3 = a byte did not arrive
	cmpl	$0x2a00, %ecx
	jne	Lout
	cmpl	$0x1500, %ebx
	jne	Lout
	movl	$4, %eax                 ## exit 4 = the %eax-indexed load went wrong
	cmpl	$0x2201, -20(%ebp)
	jne	Lout
	movl	$5, %eax                 ## exit 5 = the %eax-indexed store went wrong
	cmpb	$0x22, gOut-Lpic+0x2201(%edi)
	jne	Lout
	movl	$42, %eax
	jmp	Lout
Lebp_bad:
	movl	$2, %eax
Lout:
	movl	%eax, (%esp)
	calll	_exit

	.section __DATA,__data
gTable:
	.byte	0x11, 0x22, 0x2a, 0x15
gOut:
	.space	0x2210
