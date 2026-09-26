## 99_cstring_tailmerge_ptr — a __data string-pointer TABLE whose middle entry
## targets a __cstring INTERIOR must still be relocated.
##
## ld tail-merges literals: "00000000PACPOP..." is stored as the suffix of
## "XXXXXXXX00000000PACPOP...", so a genuine `char *` can point mid-string.
## Plants vs. Zombies keeps such a build-info pointer in a table of string
## pointers; the cstring-interior gate (99_cstring_interior_alias, Civ IV's
## byte table) left it as the raw i386 address and strncpy faulted on it.
## The evidence: a neighbouring slot points at a string START.
##
## exit 0 = relocated; exit 8 = left raw (the bug). OFF arm:
## M64_NO_CSTR_NEIGHBOUR=1 at translate time. Built -no_pie + strip -x.
	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	movl	_tbl+4, %eax
	leal	_s2, %ecx
	addl	$8, %ecx                     ## translated interior = correct value
	cmpl	%eax, %ecx
	jne	Lbad
	pushl	$0
	calll	_exit
	ud2
Lbad:
	pushl	$8
	calll	_exit
	ud2
	.section __TEXT,__cstring,cstring_literals
_s1:
	.asciz	"first"
_s2:
	.asciz	"XXXXXXXX00000000BUILDINFO"   ## +8 = tail-merged "00000000BUILDINFO"
_s3:
	.asciz	"third"
	.section __DATA,__data
	.p2align 2
_tbl:
	.long	_s1
	.long	_s2 + 8
	.long	_s3
