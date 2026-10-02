## 99_sib_ebp_index_neg_wrap — a SIB operand whose INDEX is %ebp (a data
## register: a frame pointer is never a scaled index), holding a NEGATIVE value.
##
## Portal 2 libbinkmachox86 (Intel compiler, ebp as a GPR) has 223 accesses of
## the form `movl (%eax,%ebp,4)`. The addr32 (0x67) policy excluded any %ebp
## index, so a negative index (a motion vector, a negative stride) computed
## base + 0x3fffffff4 instead of wrapping to base - 12 as on i386.
##
## Exit code = loaded value = 42 on success; SIGSEGV (139) without the fix.
## OFF arm: M64_SIB_RBP_INDEX_WIDE=1 at translate time.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	movl	$_table, %eax
	addl	$12, %eax                 ## eax = _table + 12
	movl	$-3, %ebp                 ## ebp = 0xfffffffd, a data register
	movl	(%eax,%ebp,4), %ecx       ## EA = _table + 12 - 12 = _table (i386 wrap)
	pushl	%ecx
	calll	_exit
	ud2

	.section __DATA,__data
	.p2align 4
_table:
	.long	42, 0, 0, 0
