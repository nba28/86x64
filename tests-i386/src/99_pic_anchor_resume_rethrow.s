## 99_pic_anchor_resume_rethrow — an EH pad ending in `call
## __Unwind_Resume_or_Rethrow` sits between a forward branch and its target.
##
## Some GCC toolchains end every landing pad with _Unwind_Resume_or_Rethrow
## instead of _Unwind_Resume (Portal 2 libsteam: 3253 pads). It never returns
## to its caller, but it was not on the noreturn list, so the pad's
## `mov %eax,%esi` (the exception object) fell into the join below and wiped
## the %esi anchor the branch carried: the folded table read at the dispatch
## kept its raw i386 displacement (and the later stages lost the table).
##
## Exit code 42 on success; anything else without the fix.
## OFF arm: M64_NO_NORETURN_RESUME_RETHROW=1 at translate time.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	subl	$20, %esp
	calll	L0
L0:
	popl	%esi                        ## PIC anchor
	xorl	%eax, %eax
	testl	%eax, %eax
	je	Ljoin                       ## always taken; carries the %esi anchor
	movl	%eax, %esi                  ## a landing pad: the exception object
	movl	%esi, (%esp)
	calll	__Unwind_Resume_or_Rethrow  ## never returns
Ljoin:
	movl	_value-L0(%esi), %eax
	movl	%eax, (%esp)
	calll	_exit
	ud2

	.section __DATA,__data
	.p2align 2
	.space	32
_value:
	.long	42
