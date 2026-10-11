## 99_pic_anchor_ebp_index_wrap — a PIC-anchored table read whose SIB INDEX is
## %ebp holding a NEGATIVE value.
##
## `movl tbl(%anchor,%ebp,4)` is lowered to `lea r11,[rip+tbl]; mov [r11+rbp*4]`.
## i386 wraps the effective address mod 2^32, so the lowered operand needs the
## 0x67 prefix whenever the index can be negative. The generic copy path learned
## that %ebp as an index is a data register (99_sib_ebp_index_neg_wrap), but the
## PIC lowering kept its own copy of the old rule and left any %ebp index wide:
## base - 12 became base + 0x3fffffff4.
##
## Exit code = loaded value = 42 on success; SIGSEGV (139) without the fix.
## OFF arm: M64_SIB_RBP_INDEX_WIDE=1 at translate time.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	calll	L0
L0:
	popl	%ebx                                ## PIC anchor
	movl	$-3, %ebp                           ## ebp = 0xfffffffd, a data register
	movl	_table+12-L0(%ebx,%ebp,4), %ecx     ## EA = _table (i386 wrap)
	pushl	%ecx
	calll	_exit
	ud2

	.section __DATA,__data
	.p2align 4
_table:
	.long	42, 0, 0, 0
