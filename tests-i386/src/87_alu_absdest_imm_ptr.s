## 87_alu_absdest_imm_ptr — ALU `<op> [abs32], imm32` where the imm32 is a
## POINTER (the dual-address IMMEDIATE-group shape).
##
## A fixed-address i386 exec comparing a global function pointer against a
## specific handler compiles to ONE instruction carrying TWO addresses:
##
##     cmpl $_default_handler, _handler     ; 81 3d disp32 imm32
##
## The parser's [abs32]-dest arm captures the destination in memdisp AND
## probes the trailing imm32 as a pointer (function entry w/ symbol -> kept).
## Pre-fix only MOV_MEMv_IMMz had an explicit Transform rule; every other
## IMMEDIATE-group member fell to the default rule, whose flavor-1 assumption
## ("the imm IS the disp32") patched the DESTINATION displacement with the
## IMMEDIATE's pointee and left the raw i386 imm32 bytes unpatched — the cmp
## read the FUNCTION BODY and compared it against the STALE i386 address:
## both operands wrong, so the equality test always failed (and any
## flag-consumer downstream misbehaved silently).
##
## The fix generalizes the MOV rewrite to the whole family (ISA-verified MR
## siblings 01/09/11/19/21/29/31/39/85):
##     lea r11, [rip+imm_target]     ; relocated pointer, re-parse-stable
##     <op> [rip+dest], r11d         ; original operand order preserved
##
## Exercises: cmp (the realistic dual-address member), add (pointer-arith
## into a global accumulator), and the covered mov as ground truth.
## Exit 0 iff the stored/compared pointers match the relocated layout.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	## ground truth: covered store path installs the RELOCATED &_default_handler
	movl	$_default_handler, _handler      ## c7 05 disp32 imm32 (covered)

	## G1 core: dual-address cmp — [dest] load AND imm must BOTH relocate
	cmpl	$_default_handler, _handler      ## 81 3d disp32 imm32
	jne	Lbad

	## dual-address add: accumulator += &_default_handler; verify by
	## independent recomputation through the covered MOV_GPRv_IMMv path
	addl	$_default_handler, _acc          ## 81 05 disp32 imm32
	movl	$_default_handler, %eax          ## covered: lea eax,[rip+...]
	addl	$5, %eax
	cmpl	_acc, %eax
	jne	Lbad

	pushl	$0
	calll	_exit
	ud2

Lbad:
	pushl	$1
	calll	_exit
	ud2

	## a real function ENTRY (i386 prologue + nlist symbol) so the imm probe
	## keeps the pointer classification (code_alias_is_constant exempts
	## symboled function entries)
	.globl _default_handler
	.p2align 4, 0x90
_default_handler:
	pushl	%ebp
	movl	%esp, %ebp
	popl	%ebp
	ret

	.section __DATA,__data
	.globl _handler
	.p2align 2
_handler:
	.long	0                                ## installed then compared
	.globl _acc
_acc:
	.long	5                                ## += &_default_handler
