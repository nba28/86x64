## 99_prologue_sched_entry — a data-resident code pointer (a C++ vtable slot)
## whose target's frame setup has an instruction scheduled inside it:
##     push %ebp ; mov $3,%edx ; mov %esp,%ebp
## must be relocated in a locals-stripped, reloc-less image. Plants vs.
## Zombies' Sexy:: vtables point at such regparm functions; the code-entry gate
## only knew the contiguous `55 89 e5`, left the slot raw, and the virtual
## call jumped to the i386 address.
## exit 42 = the call went through; SIGSEGV = slot left raw. OFF arm:
## M64_STRICT_PROLOGUE=1 at translate time. Built -no_pie + strip -x.
	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	movl	_vtbl+4, %eax
	calll	*%eax
	pushl	%eax
	calll	_exit
	ud2
	.p2align 4, 0x90
_first:
	pushl	%ebp
	movl	%esp, %ebp
	movl	$1, %eax
	popl	%ebp
	retl
	.p2align 4, 0x90
_sched:
	pushl	%ebp
	movl	$3, %edx                  ## scheduled into the frame setup
	movl	%esp, %ebp
	movl	$42, %eax
	popl	%ebp
	retl
	.section __DATA,__const
	.p2align 2
_vtbl:
	.long	_first
	.long	_sched
