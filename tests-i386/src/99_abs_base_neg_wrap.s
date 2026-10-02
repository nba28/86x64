## 99_abs_base_neg_wrap — an ABSOLUTE disp32 off a base register holding a
## NEGATIVE offset: `disp32(%base)` with disp = an image address.
##
## Portal 2 libbinkmachox86 (Intel compiler) walks a function-pointer table
## with a negative counter: `movl $-0x1c,%ebx ; movl 0x2957c(%ebx),%edx`. The
## lowering `lea table(%rip),%r11 ; mov (%rbx,%r11)` added the zero-extended
## 0xffffffe4 in 64 bits (table + 4GB - 0x1c) instead of wrapping to the table.
##
## Exit code = loaded value = 42 on success. OFF arm (M64_NO_ABS_BASE_ADDR32=1
## at translate time) reads 4GB high: a fault or garbage, never 42.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	movl	$-8, %ebx                 ## ebx = 0xfffffff8
	movl	_table+8(%ebx), %ecx      ## EA = _table + 8 - 8 = _table (i386 wrap)
	pushl	%ecx
	calll	_exit
	ud2

	.section __DATA,__data
	.p2align 4
_table:
	.long	42, 0, 0, 0
