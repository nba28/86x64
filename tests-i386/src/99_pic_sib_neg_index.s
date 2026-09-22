## 99_pic_sib_neg_index — PIC-anchored SIB table access with a NEGATIVE index.
##
## Mirrors Portal 2 shaderapidx9 ImageLoader::ImageFormatToD3DFormat(fmt):
##   call L; L: popl %eax; movl 0x8(%ebp),%ecx; movl 0x13404(%eax,%ecx,4),%eax
## called with fmt = IMAGE_FORMAT_UNKNOWN (-1). On i386 the EA wraps mod 2^32 to
## table-4 (a valid read). The transform rewrites the anchored operand as
##   lea r11,[rip+table]; movl (%r11,%rcx,4),%eax
## whose zero-extended index lands at table+0x3fffffffc -> SIGSEGV. The fix is
## an addr32 (0x67) `(%r11d,%ecx,4)` operand, exactly as 55_sib_index_neg_wrap
## does for absolute (non-PIC) SIB operands.
##
## Exit code = loaded value = 42 on success; 139 without the fix.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	calll	L0
L0:
	popl	%eax                      ## PIC anchor
	movl	$-1, %ecx                 ## sentinel index
	movl	_table-L0(%eax,%ecx,4), %edx  ## i386 EA = _table - 4 = _slot
	pushl	%edx                      ## exit code = loaded value
	calll	_exit
	ud2

	.section __DATA,__data
	.p2align 4
	.space	12
_slot:
	.long	42                        ## _table - 4
_table:
	.long	1, 2, 3, 4
