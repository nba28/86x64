## 99_jt_lea_out_of_section — a PIC-anchored LEA that is NOT a jump-table base.
##
## i386 PIC reaches EVERYTHING anchor-relative — strings, globals, vtables — so
## most `lea %reg,[%anchor + disp]` instructions are not table bases. The
## jump-table pass recorded EVERY one as a table base with no bounds check, and
## that poisons the dispatch two instructions later: case (c) of the walk finds a
## `tbl_addr` entry for the base register and takes the table-base branch instead
## of the (c-combined) `anchor + disp` branch that computes the REAL table.
##
## MEASURED, Portal 2 libtogl `GLMDecode` (i386 0x27890). The DEFAULT arm of the
## switch loads its "unknown" string:
##     lea eax,[eax + 0x19fbf]          -> tbl_addr[EAX] = 0x41858, a __cstring
##                                         address, far outside __text
##                                         [0x1630,0x2fb10)
##     mov edx,[eax + edx*4 + 0xb3]     -> table resolved as 0x41858, not 0x2794c
## Auto-size then failed bounds immediately (i = 0 < 2) so NO slots were recorded,
## and the 11 table bytes were parsed as CODE: 0x73 decoded as a short `jae` and
## was WIDENED to a 6-byte near jcc, 0x4d became `dec ebp`, 0x5d became the
## `pop ebp` idiom. The emitted table was shifted and interleaved with
## instructions, and GLMDecode(10) jumped to anchor+0x24648d48.
##
## ★THE LINEAR WALK IS WHY A NOT-TAKEN ARM CAN DO THIS: the pass goes in ADDRESS
## order, so it sees the default arm's lea even though at run time control jumps
## straight over it and %eax is still the anchor at the indexed load.
##
## THE FIX: a lea is a table base only if `anchor + disp` lands in THIS section —
## the same bound the auto-size loop and every case target already satisfy,
## applied one step earlier. Kill switch M64_NO_JT_LEA_BOUNDS=1.
##
## This test mirrors that shape exactly: a default arm whose anchor-relative lea
## targets __TEXT,__cstring (a DIFFERENT section, so out of __text), placed
## between the bounds check and the dispatch. Index 1 selects case 1.
##
## Exit 42 = the table was relocated and case 1 ran. With the kill switch the
## table keeps raw i386 offsets, so `anchor + entry` lands mid-instruction:
## a wrong exit code or a signal death, both FAILs.

	.section __TEXT,__cstring
Lstr:
	.asciz "not a jump table"

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$16, %esp
	pushl	%esi

	calll	Lpic0
Lpic0:
	popl	%eax                     ## eax = the PIC anchor

	movl	$1, %edx                 ## the switch value -> case 1
	cmpl	$3, %edx
	jbe	Lswitch

	## DEFAULT ARM — an anchor-relative lea to a string in ANOTHER section.
	## Never executed here, but the linear walk sees it.
	leal	(Lstr - Lpic0)(%eax), %eax
	movl	$1, %esi
	jmp	Lend

Lswitch:
	movl	(Ltable - Lpic0)(%eax,%edx,4), %edx
	addl	%eax, %edx
	jmp	*%edx

Lcase0:
	movl	$10, %esi
	jmp	Lend
Lcase1:
	movl	$42, %esi
	jmp	Lend
Lcase2:
	movl	$20, %esi
	jmp	Lend
Lcase3:
	movl	$30, %esi
	jmp	Lend

Lend:
	pushl	%esi
	calll	_exit
	ud2

	.p2align 2
Ltable:
	.long	Lcase0 - Lpic0
	.long	Lcase1 - Lpic0
	.long	Lcase2 - Lpic0
	.long	Lcase3 - Lpic0
