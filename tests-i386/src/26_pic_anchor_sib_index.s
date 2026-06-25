## 26_pic_anchor_sib_index — PIC-anchored SIB-indexed table access.
##
## Mirrors CRC32_ProcessBuffer's `xorl 0xda32(%edi,%ebx,4),%ecx` pattern:
##   edi = PIC anchor (call $+0; pop %edi)
##   ebx = live integer index
##   disp  = table-base displacement from anchor
##   scale = 4 (32-bit table entries)
##
## DetectPicAnchoredDisps must recognise the SIB form [anchor+idx*scale+disp]
## and set pic_anchored=true.  The instruction.cc transform must emit:
##   lea  r11, [rip + <table_disp>]   ; r11 = table base
##   op   [r11 + idx*scale]           ; index into table
## instead of leaving edi with the translated code address → wrong table.
##
## Validation: XOR all 4 table entries into accumulator.
## Table: { 1, 2, 3, 4 }.  0 ^ 1 ^ 2 ^ 3 ^ 4 = 4.
## Exit code = accumulator = 4 on success; SIGBUS or wrong value otherwise.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$16, %esp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	## Establish PIC anchor: edi = vmaddr of Lpic0
	calll	Lpic0
Lpic0:
	popl	%edi

	## acc (esi) = 0; loop idx (ebx) = 0
	xorl	%esi, %esi
	xorl	%ebx, %ebx

Lloop:
	## SIB-indexed PIC table access: acc ^= table[idx]
	## edi = PIC anchor; (_table - Lpic0) = displacement from anchor to table
	xorl	(_table - Lpic0)(%edi,%ebx,4), %esi

	incl	%ebx
	cmpl	$4, %ebx
	jl	Lloop

	## pass accumulator as exit code: push it explicitly before calll _exit
	pushl	%esi
	calll	_exit
	ud2

	.section __DATA,__data
	.p2align 2
_table:
	.long	1
	.long	2
	.long	3
	.long	4
