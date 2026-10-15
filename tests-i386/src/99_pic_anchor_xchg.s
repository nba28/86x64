## 99_pic_anchor_xchg — the cpuid idiom in PIC code:
##
##     movl  %ebx, %esi         ## park the PIC register (cpuid clobbers %ebx)
##     cpuid
##     xchgl %ebx, %esi         ## anchor back in %ebx, vendor word in %esi
##     movl  %esi, gVendor-Lpic(%ebx)
##
## The anchor tracker followed the `mov` copy and killed %ebx at cpuid, but had
## no rule for `xchg`, so the final store kept its raw i386 displacement off the
## translated anchor and wrote into __TEXT (Portal 2009 libtier0
## GetProcessorVendorId -> SIGBUS during static init). Kill switch
## M64_NO_PIC_ANCHOR_XCHG=1.
##
## Exit 42 = the vendor word landed in gVendor.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	subl	$16, %esp
	calll	Lpic
Lpic:
	popl	%ebx                     ## ebx = the PIC anchor
	movl	$0, gVendor-Lpic(%ebx)
	xorl	%eax, %eax
	movl	%ebx, %esi
	cpuid
	xchgl	%ebx, %esi
	movl	%esi, gVendor-Lpic(%ebx)
	movl	$3, %eax                 ## exit 3 = the store missed gVendor
	cmpl	$0, gVendor-Lpic(%ebx)
	je	Lout
	movl	$42, %eax
Lout:
	movl	%eax, (%esp)
	calll	_exit

	.section __DATA,__data
	.p2align 2
gVendor:
	.long	0
