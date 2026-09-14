## 99_jt_anchor_spill_slot — the PIC ANCHOR spilled to a frame slot, reloaded
## into a DIFFERENT register, and then used as the dispatch base.
##
## DetectJumpTables already followed a TABLE BASE across a frame spill
## (99_jt_spill_slot). It did not follow the ANCHOR, and under register pressure
## GCC spills that instead:
##
##     mov [ebp-0x14],%edi                 ; park the anchor
##     ...
##     mov %ecx,[ebp-0x14]                 ; reload it somewhere else
##     add %ecx,(%ecx,%eax,4),0x236        ; ecx = anchor + table[idx]
##     jmp *%ecx
##
## The anchor REWRITE follows the spill perfectly well — it turns that add into
## `lea r11,[rip+X]; add ecx,[r11+rax*4]`, retargeting the load at the
## TRANSLATED copy of the table. But the detector is what rewrites the table's
## ENTRIES, and it never claimed this table, so the entries kept their i386
## anchor-relative offsets. The dispatch then computes
## `translated_anchor + i386_offset` — an address that is real, in-section, and
## MID-INSTRUCTION.
##
## MEASURED, Portal 2 soundemittersystem `KeyValues::MakeCopy` (i386 dispatch
## 0xefd5, anchor 0xeeee, table 0xf124, 7 entries). The landing at translated
## +0x1843d sat inside `mov [rsp+8],edi`, and the bytes there happen to decode
## into a stream ending in `jmp rcx` with rcx still holding the landing address:
## a 4-million-iteration self-call that consumed the entire 16 MB i386 main
## stack and died on its guard page. Detected tables 8 -> 9 for that one dylib.
##
## The spilled anchor obeys the same lifetime rules as a spilled table base:
## the slot dies when anything else writes it, when %ebp is redefined, and —
## for %esp-keyed slots — whenever %esp moves. Kill switch
## M64_NO_JT_ANCHOR_SPILL=1.
##
## Exit 42 = the table was relocated and case 2 ran. With the kill switch the
## entries stay raw i386 offsets, so the jump lands mid-instruction: a wrong
## exit code or a signal death, both FAILs.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$32, %esp
	pushl	%esi
	pushl	%edi

	calll	Lpic0
Lpic0:
	popl	%edi                     ## edi = the PIC anchor

	movl	%edi, -20(%ebp)          ## SPILL the anchor to a frame slot
	movl	$7, %edi                 ## and clobber the register it came from

	movl	-20(%ebp), %ecx          ## RELOAD it into a different register
	movl	$2, %eax                 ## the switch value -> case 2
	cmpl	$3, %eax
	ja	Ldefault

	addl	(Ltable - Lpic0)(%ecx,%eax,4), %ecx
	jmp	*%ecx

Lcase0:
	movl	$10, %esi
	jmp	Lend
Lcase1:
	movl	$20, %esi
	jmp	Lend
Lcase2:
	movl	$42, %esi
	jmp	Lend
Lcase3:
	movl	$30, %esi
	jmp	Lend
Ldefault:
	movl	$1, %esi

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

	.p2align 4, 0x90
	.space 16
