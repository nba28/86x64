## flag_column_entry.s — fixture for flag_column_entry_test.sh
## (core: section.cc DataParser + ParseEnv::flag_column_power_of_two;
## Call of Duty 4 "separate alpha blend, glow will be disabled".)
##
## Call of Duty 4 checks its emulated D3D caps against a __DATA,__const table
## of 20-byte records {capsOffset, maskAny, maskAll, severity, msg}. One maskAll
## is 0x20000 (D3DPMISCCAPS_SEPARATEALPHABLEND), and in that stripped, non-PIE,
## reloc-less executable a real function starts at 0x20000 with `55 89 e5`. The
## word passed every code-pointer test and was relocated, so the check failed.
##
## Here _probe_fn sits at 0x8000 (16 KB alignment; __text itself starts at
## 0x4000) with the same prologue, and the table's mask column holds 0x8000
## among single-bit flags. The harness
## asserts the word survives with the rule and is relocated without it
## (M64_NO_FLAG_COLUMN=1).

	.section __TEXT,__text,regular,pure_instructions

	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp
	call	_probe_fn
	movl	$0, (%esp)
	call	_exit

	.p2align 14, 0x90
_probe_fn:                         ## local + stripped; entry evidence = prologue
	pushl	%ebp
	movl	%esp, %ebp
	leave
	ret

	.section __DATA,__const
	.p2align 2
	.long	0xA5A53333             ## sentinel
_g_caps:                           ## {off, any, all, severity, msg}
	.long	0x0c, 0, 0x20000000, 0, 0
	.long	0x0c, 0, 0x00008000, 1, 0  ## <- maskAll == &_probe_fn
	.long	0x10, 0, 0x00000020, 0, 0
	.long	0x10, 0, 0x00000100, 1, 0
	.long	0x14, 0, 0x80000000, 1, 0
	.long	0x14, 0, 0x00000001, 1, 0
	.long	0xA5A54444             ## sentinel
