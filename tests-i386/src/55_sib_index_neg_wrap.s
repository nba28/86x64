## 55_sib_index_neg_wrap — absolute SIB table access with a NEGATIVE index.
##
## Mirrors Quinn's -[QuinnGame setPieceIndex:position:rotation:updateShadow:]:
##   movl 0xb2f40(,%edx,8),%edx      with edx = 0xfffffffc  (-4, "no piece" sentinel)
##
## The i386 effective address WRAPS mod 2^32:
##   (table + 0xfffffffc*8) mod 2^32 = table - 0x20      (VALID, in-bounds)
##
## Copied verbatim to x86_64 the index register widens to %rdx and the EA is
## computed in the FULL 64-bit address space (no wrap):
##   table + 0xfffffffc*8 = table + 0x7ffffffe0          (>4GB, unmapped -> SIGSEGV)
##
## The fix emits a 0x67 address-size override on memory operands that retain a
## non-stack/non-RIP addressing register, so the EA is computed with 32-bit
## registers and truncated mod 2^32 (Intel SDM Vol.2, LEA Table 3-55: in
## 64-bit mode a 0x67 prefix makes the CPU compute a 32-bit effective
## address) = exact i386 semantics.
##
## Data layout: _slot (value 42) sits exactly 0x20 bytes before _table, so the
## wrapped read at table-0x20 lands on _slot.
## Exit code = loaded value = 42 on success; SIGSEGV (exit 139) without the fix.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	movl	$-4, %edx                 ## edx = 0xfffffffc  (pieceIndex = -1, *4 sentinel)
	movl	_table(,%edx,8), %edx     ## EA = _table + (-4)*8 = _table-0x20 (i386 wrap)
	pushl	%edx                      ## exit code = loaded value
	calll	_exit
	ud2

	.section __DATA,__data
	.p2align 4
_slot:
	.long	42                        ## lands at _table - 0x20 after the wrap
	.space	28                        ## 4 + 28 = 0x20 bytes between _slot and _table
_table:
	.long	0, 0, 0, 0
