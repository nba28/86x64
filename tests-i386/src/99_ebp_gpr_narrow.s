## 99_ebp_gpr_narrow — %ebp as a DATA register (no frame pointer) in a
## register-only op must keep i386's 32-bit result and flags.
##
## Portal 2 libbinkmachox86 (Intel compiler) uses %ebp as a GPR ~1,600 times.
## The stack-pointer widening rule prefixed REX.W to every reg-only op on
## %esp OR %ebp, so `add $-1,%ebp` from 0 left rbp = 2^64-1 and the Huffman
## reader's `cmp %ecx,%ebp ; jb` compared 64 bits: the Bink video decoded to
## garbage from its first real frame.
##
## Here: ebp = 0x7fffffff + 1 = 0x80000000, negative as an i386 int, so `jl`
## is taken. Widened, rbp = +0x80000000 and `jl` falls through.
## Exit code 42 on success, 7 without the fix.
## OFF arm: M64_RBP_REG_WIDE=1 at translate time.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	$0x7fffffff, %ebp
	addl	$1, %ebp                  ## i386: 0x80000000 (negative)
	cmpl	$0, %ebp
	jl	1f
	pushl	$7
	calll	_exit
1:	pushl	$42
	calll	_exit
	ud2
