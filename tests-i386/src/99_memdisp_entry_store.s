## 99_memdisp_entry_store — `movl %edx, disp(%eax)` whose disp equals a real
## function ENTRY is still a struct offset, not a code address.
## PvZ: `movl %edx,0x558c(%eax)`; 0x558c is a function entry (after `retl`),
## the entry-evidence rule rebased it and the member store hit the GPU driver.
## ON: the store lands inside the 64 KB buffer -> exit 42. OFF
## (M64_MEMDISP_ACCESS_ENTRY_RULE=1 at TRANSLATE time): it lands ~256 MB away
## -> crash or exit 1.
	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	pushl	%edi
	subl	$0x10, %esp
	movl	$0x10000, 4(%esp)
	movl	$1, (%esp)
	calll	_calloc                   ## 64 KB of zeroes
	movl	%eax, %esi
	movl	$0x5a, %edx
	movl	%edx, Lentry(%eax)        ## *** member store, disp == Lentry ***
	xorl	%ecx, %ecx
	xorl	%edi, %edi
Lsum:
	movzbl	(%esi,%ecx), %eax
	addl	%eax, %edi
	incl	%ecx
	cmpl	$0x10000, %ecx
	jb	Lsum
	movl	$42, %eax
	cmpl	$0x5a, %edi
	je	Ldone
	movl	$1, %eax
Ldone:
	movl	%eax, (%esp)
	calll	_exit
	ud2
	retl
Lentry:                               ## a real function entry after `retl`
	pushl	%ebp
	movl	%esp, %ebp
	popl	%ebp
	retl
