## 99_prologue_sched_decode — a code pointer handed over as a stack argument
## (`movl $f,(%esp)`, e.g. to __cxa_atexit) whose target schedules more than
## the 12-byte window into its frame setup:
##     push %ebp ; mov ABS,%ecx ; mov ABS,%edx ; mov %esp,%ebp   (89 e5 at +13)
## Call of Duty 4's static destructor at 0x38570: the immediate stayed raw and
## exit jumped to the i386 address. Locals-stripped, reloc-less (-no_pie).
## exit 42 = the pointer was relocated and the call went through; SIGSEGV = raw.
## OFF arm: M64_NO_PROLOGUE_DECODE=1 at translate time.
	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp
	movl	$Lsched, (%esp)          ## code pointer as a stack argument
	calll	Lcall_arg0
	movl	%eax, (%esp)
	calll	_exit
	ud2
	.p2align 4, 0x90
Lcall_arg0:                              ## calls its first argument
	pushl	%ebp
	movl	%esp, %ebp
	movl	8(%ebp), %eax
	calll	*%eax
	popl	%ebp
	retl
	.p2align 4, 0x90
Lsched:
	pushl	%ebp
	movl	_a, %ecx                 ## 6 bytes scheduled into the frame setup
	movl	_b, %edx                 ## 6 more: mov %esp,%ebp lands at +13
	movl	%esp, %ebp
	movl	%ecx, %eax
	addl	%edx, %eax
	popl	%ebp
	retl
	.section __DATA,__data
	.p2align 2
_a:	.long	40
_b:	.long	2
