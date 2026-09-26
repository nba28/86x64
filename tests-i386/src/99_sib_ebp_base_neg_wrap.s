## 99_sib_ebp_base_neg_wrap — a SIB operand whose BASE is %ebp used as a plain
## data register (-fomit-frame-pointer code), with a NEGATIVE index.
##
## Plants vs. Zombies' libbass.dylib (BASS audio, built without frame pointers):
##   movss %xmm0, 0xc(%ebp,%esi,4)     with ebp = buffer, esi = 0xfffffffd (-3)
## The i386 EA wraps mod 2^32 to buffer+0. The addr32 (0x67) policy excluded
## RBP as a base (assumed frame pointer), so the translated operand computed
## buffer + 0x3fffffff4 and faulted at startup.
##
## Exit code = loaded value = 42 on success; SIGSEGV (139) without the fix.
## OFF arm: M64_SIB_RBP_BASE_WIDE=1 at translate time.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	movl	$_table, %ebp             ## ebp = data pointer, not a frame
	movl	$-3, %esi                 ## esi = 0xfffffffd
	movl	0xc(%ebp,%esi,4), %eax    ## EA = _table + 0xc - 0xc = _table (i386 wrap)
	pushl	%eax
	calll	_exit
	ud2

	.section __DATA,__data
	.p2align 4
_table:
	.long	42, 0, 0, 0
