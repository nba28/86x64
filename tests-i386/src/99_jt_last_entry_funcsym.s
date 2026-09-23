## 99_jt_last_entry_funcsym — a CLAIMED jump-table slot beats the function-symbol
## straddle resync.
##
## Section::Parse1 has a guard for STRIPPED-ish layouts: if the instruction it is
## about to decode would run past the next function symbol, the bytes are
## inter-function padding, so emit a 1-byte DataBlob and re-sync. That guard runs
## BEFORE `parser(...)`, and `parser` is the only place env.jump_table_slots is
## consulted — so a claimed table slot whose bytes happen to decode into the next
## function never becomes a JumpTableEntry and ships its RAW i386
## anchor-relative delta. `translated_anchor + i386_delta` is then a
## mid-instruction address.
##
## This is the NORMAL PIC-switch layout: the table sits inline at the end of its
## function, so its LAST slot always butts against the next function symbol.
##
## ★MEASURED — Portal 2 wall 7, libtogl D3DToGL::WriteGLSLSamplerDefinitions
## (i386 0x11880, anchor 0x1188e, table 0x11960, DetectJumpTables count=4):
##     slot 3 @0x1196c = a1 00 00 00 ; next func symbol 0x11970
##     `a1 00 00 00 55` decodes as movl 0x55000000,%eax (len 5) -> straddle
## so slots 0..2 were re-anchored (0x3d/0x57/0x69 -> 0x60/0x81/0x96) and slot 3
## kept 0xa1. At run time anchor 0x10019ae7 + 0xa1 = 0x10019b88 = ONE BYTE INTO
## another case's `leal 0x32515(%rip),%ecx`, which decodes as
## `orl $0x32515,%eax; jmp <shared tail>` and falls into `movl %ecx,0x4(%rsp)`
## WITHOUT EVER SETTING %ecx -> PrintToBuf(buf, fmt=0, ...) -> SIGSEGV inside the
## printf-format walk, four frames away from the real defect.
##
## THE FIXTURE reproduces the three ingredients:
##   (i)   a PIC switch whose inline table ENDS the function,
##   (ii)  a LAST entry whose bytes (0x3d 00 00 00) plus the first byte of the
##         next function (0x55 = pushl %ebp) decode as ONE 5-byte instruction
##         (`cmpl $imm32,%eax`), so the straddle test fires,
##   (iii) a GLOBAL function symbol at exactly table_end, with no alignment
##         padding in between.
##   ON  (gate armed):  slot 1 is re-anchored (0x3d -> 0x50) and the dispatch
##       reaches Lcase1 -> exit 42.
##   OFF (M64_NO_JT_SLOT_BEATS_FUNCSYM=1): slot 1 keeps the raw i386 delta 0x3d,
##       so the dispatch lands at translated_anchor+0x3d, INSIDE the gap ->
##       exit 133 (SIGTRAP).
## The gap is filled with 0xcc (int3) rather than nops on purpose: every byte of
## it is a complete instruction, so ANY landing traps no matter how the
## translation expands the code. Filled with nops the OFF arm fell THROUGH the
## padding into Lcase1 and returned 42 — a silently inert guard (measured).

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.globl _after_table

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp

	## PIC anchor.
	calll	Lpic
Lpic:
	popl	%ebx

	## Switch on 1 = the LAST table entry, the one the straddle guard eats.
	movl	$1, %eax
	cmpl	$1, %eax
	ja	Ldefault
	movl	(Ltab-Lpic)(%ebx,%eax,4), %eax
	addl	%ebx, %eax
	jmp	*%eax

Lcase0:
	movl	$7, %eax
	jmp	Lfinish

	## Padding tuned so that Lcase1-Lpic == 0x3d, i.e. the last table entry's
	## first byte is 0x3d = the opcode of `cmpl $imm32,%eax`. THAT is what makes
	## the 4 entry bytes plus the next function's first byte decode as one
	## 5-byte instruction and trip the straddle test. int3 fill, so the OFF arm's
	## stale-delta landing anywhere in here is a deterministic SIGTRAP.
	.space	32, 0xcc

Lcase1:
	movl	$42, %eax
	jmp	Lfinish
Ldefault:
	movl	$8, %eax
Lfinish:
	pushl	%eax
	calll	_exit
	ud2

	## The inline table, at the END of the function — no alignment before it, so
	## the following GLOBAL symbol lands exactly at table_end.
Ltab:
	.long	Lcase0 - Lpic
	.long	Lcase1 - Lpic

## Must start with 0x55 (pushl %ebp) and must be a FUNCTION SYMBOL sitting
## exactly at table_end. No .p2align here: alignment padding would move the
## symbol off table_end and the straddle test would not fire.
_after_table:
	pushl	%ebp
	movl	%esp, %ebp
	xorl	%eax, %eax
	popl	%ebp
	retl
