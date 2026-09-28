## 99_cmp_nlptr_imm — `cmp [abs32], $imm32` whose memory operand is a dyld
## non-lazy pointer slot (Halo `cmpl $func, nl_ptr`). Parse only captured an
## absolute [disp32] into ordinary data sections, so the slot operand stayed
## unbound: its raw i386 address was emitted as [rip+disp32] and the pointer
## immediate was parsed as if it were the address. Exit 0 iff the slot reads
## back equal to the function it points to.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	cmpl	$_target, L_target$non_lazy_ptr
	jne	Lbad
	pushl	$0
	calll	_exit
	ud2
Lbad:
	pushl	$1
	calll	_exit
	ud2

	.globl _target
	.p2align 4, 0x90
_target:
	pushl	%ebp
	movl	%esp, %ebp
	popl	%ebp
	retl

	.section __IMPORT,__pointers,non_lazy_symbol_pointers
L_target$non_lazy_ptr:
	.indirect_symbol _target
	.long	0
