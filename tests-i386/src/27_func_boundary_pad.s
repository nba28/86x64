## 26_func_boundary_pad — regression for function-symbol-boundary-aware sweep.
##
## Root cause (Civ IV, 2026-06-25): the linear code sweep has no knowledge of
## function boundaries.  When an alignment/padding byte (e.g. 0x00) sits between
## two functions, XED decodes `00 <first-byte-of-next-func>` as a 2- or 3-byte
## instruction that CROSSES the next function's entry symbol.  The misalignment
## cascades: the next function's real PUSH EBP / MOV EBP,ESP prologue is swallowed
## into a garbage decode, and the bytes that follow are decoded out of phase,
## eventually hitting an untranslatable form (e.g. FF /7) that aborts translation.
##
## The fix (Section::Parse1): when func_syms is non-empty (binary has a symtab),
## check that a decoded instruction's byte span does not CROSS a symbol vmaddr.
## If it would, emit a 1-byte DataBlob instead and retry — the sweep re-syncs at
## the true function entry.
##
## Test layout:
##   _pad_victim   (func A): returns 10
##   .byte 0x00    (inter-function padding — would be absorbed by vanilla sweep)
##   _pad_target   (func B): returns 20  <-- MUST start at its symbol's vmaddr
##   _main: calls both, sums result, exits(30) on success.
##
## If the sweep misaligns at the 0x00 pad byte, _pad_target is misdecoded and the
## translated binary either crashes or returns garbage (not 30).
##
## Written in GAS/AT&T so we can emit a literal .byte 0x00 between functions and
## mark both with .globl (so they appear in the Mach-O symtab as N_SECT entries,
## which is what feeds env.func_syms).

	.section __TEXT,__text,regular,pure_instructions

	## --- func A: returns 10 via eax ---
	.globl _pad_victim
	.p2align 2, 0x90        ## align to 4 bytes with NOP fill
_pad_victim:
	pushl	%ebp
	movl	%esp, %ebp
	movl	$10, %eax
	popl	%ebp
	retl

	## A single 0x00 byte inserted between the two functions.
	## Vanilla linear sweep: after the `ret` above, this byte is decoded
	## together with the next byte(s) as a multi-byte instruction that
	## crosses _pad_target's symbol -- misaligning the sweep.
	## With the fix, Parse1 detects the crossing and emits a 1-byte DataBlob.
	.byte 0x00

	## --- func B: returns 20 via eax ---
	.globl _pad_target
_pad_target:
	pushl	%ebp
	movl	%esp, %ebp
	movl	$20, %eax
	popl	%ebp
	retl

	## --- main: call both, sum, exit(30) on success ---
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$8, %esp
	calll	_pad_victim        ## eax = 10
	movl	%eax, -4(%ebp)     ## save A result
	calll	_pad_target        ## eax = 20
	addl	-4(%ebp), %eax     ## eax = 10 + 20 = 30
	movl	%eax, (%esp)
	calll	_exit              ## exit(30)
	ud2
