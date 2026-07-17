## 99_cmp_abs32_imm_notptr — `cmpl/testl $imm32, [abs32]` where the imm32
## NUMERICALLY ALIASES the i386 __TEXT,__text vmaddr range, in a binary
## STRIPPED of local text symbols (the Civ IV Steam shape).
##
## Mirrors the Civ IV (Steam) OS-version-check mis-relocation: the game's
## Gestalt version gate
##     cmpl $0x100308, _version    ; 81 3d disp32 imm32 (packed 10.3.8)
##     ja   <pass>
## carries an immediate that numerically falls inside the i386 __text range
## [0x23b0, 0xdd2176). The [abs32]+imm32 parser's imm-is-pointer probe
## (instruction.cc) treated it as an intra-image pointer; the
## code_alias_is_constant mid-function gate is DISARMED for locals-stripped
## binaries (have_local_text_syms == false), so nothing rescued it. The
## translated form became `lea r11,[rip+..]; cmpl %r11d, _version(%rip)` —
## comparing the version (0x260500) against a translated ADDRESS
## (~0x101a690d) -> "insufficient system version" alert + exit.
##
## The fix: in `cmp/test [abs32], imm32` the immediate is a COMPARISON
## VALUE / bit mask, essentially never a pointer; only POSITIVE evidence
## (an nlist symbol AT the immediate's value — 87_alu_absdest_imm_ptr's
## `cmpl $_default_handler, _handler`) keeps the pointer classification.
## MOV (stored-pointer install) and the ADD pointer-arithmetic family keep
## the permissive probe; the register-compare loop-sentinel case
## (69_ptr_loop_end_sentinel) keeps its imm_bounds_relocated_table gate.
##
## The compare constant 0x2400 lands inside this binary's __text (~0x1f40 +
## the nop pad below; ASSERTED at build time by the Makefile rule) and the
## binary is `strip -x`ed so have_local_text_syms is false — both trap
## conditions armed. All live values are register-built from sub-0x1000
## pieces so no OTHER immediate can alias a vmaddr. Exit 99 = both the CMP
## direction test and the TEST mask stayed literal.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	subl	$16, %esp

	## _gver = 0x260500 (a macOS-26.5.0-style packed version), built
	## in-register from sub-0x1000 pieces (no imm can alias a vmaddr).
	movl	$0x26, %eax
	shll	$16, %eax
	orl	$0x500, %eax
	movl	%eax, _gver

	## THE CIV SHAPE: 81 3d disp32 imm32 + ja. Pre-fix the imm 0x2400 was
	## rebased to a lea target (>= 0x10000000) -> 0x260500 > R false ->
	## ja falls through to the "insufficient version" path.
	cmpl	$0x2400, _gver
	ja	L1
	jmp	Lbad0
L1:
	## Inverse direction: a version BELOW the constant must NOT pass —
	## catches an imm zeroed/clamped instead of kept literal.
	movl	$0x123, _gver
	cmpl	$0x2400, _gver
	ja	Lbad1

	## TEST twin (F7 /0): mask 0x2400 aliases __text. _gflags = ~0x2400
	## (register-built), so the literal mask gives ZF=1; a relocated mask
	## (>= 0x10000000: bit 28 set, and set in ~0x2400) gives ZF=0.
	movl	$0x24, %eax
	shll	$8, %eax
	notl	%eax
	movl	%eax, _gflags
	testl	$0x2400, _gflags
	jnz	Lbad2

	## Positive control: a sub-0x1000 mask that SHOULD hit stays nonzero.
	testl	$0x1, _gflags
	jz	Lbad3

	movl	$99, %ecx
	jmp	Lout
## Distinct failure exits (10+N) so a FAIL names the broken form.
Lbad0:	movl	$10, %ecx
	jmp	Lout
Lbad1:	movl	$11, %ecx
	jmp	Lout
Lbad2:	movl	$12, %ecx
	jmp	Lout
Lbad3:	movl	$13, %ecx
	jmp	Lout
Lout:
	movl	%ecx, (%esp)
	calll	_exit
	ud2

	## PAD: extend __text well past the aliased constant 0x2400 so the
	## value is guaranteed inside the section's vmaddr range (suite
	## binaries' __text starts ~0x1f40; the Makefile rule asserts
	## containment at build time and errors loudly if layout drifts).
	.space	0x1000, 0x90

	.section __DATA,__data
	.p2align 2
_gver:
	.long	0
_gflags:
	.long	0
