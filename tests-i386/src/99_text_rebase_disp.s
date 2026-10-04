## 99_text_rebase_disp — an LC_DYLD_INFO rebase of a __text slot (a TEXT
## relocation) attests that the operand field holds an in-image address.
##
## Portal 2 libmilesx86 (non-PIC, relocatable): `movzbl 0x602d4(%eax),%edx`
## reads path[len-1] of a __data buffer. Its disp32 is rebased in __text, but
## the translator only trusted CLASSIC local relocs (and only in images without
## a rebase stream), so the field stayed a raw i386 address -> SIGSEGV at
## 0x602d9 in AIL_set_redist_directory.
##
## Linked -pie -read_only_relocs suppress (not fixed-load, like a dylib), so
## the only evidence is the rebase stream. Exit 42 = the byte at _buf-1 was
## read through the relocated disp. Kill switch M64_NO_TEXT_REBASE_ATTEST=1
## (translate time): a fault or a wrong exit.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp
	xorl	%eax, %eax               ## index 0 -> the byte at _buf-1
	movzbl	_buf-1(%eax), %edx       ## disp32 = _buf-1, rebased in __text
	movl	%edx, (%esp)
	calll	_exit
	ud2

	.data
	.byte	0
_val:	.byte	42
_buf:	.byte	7, 7, 7, 7
