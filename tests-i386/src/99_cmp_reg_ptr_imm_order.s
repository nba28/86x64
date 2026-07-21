## 99_cmp_reg_ptr_imm_order — reg-dest pointer-IDENTITY compare
## (`cmpl $&writable_data, %reg`) parsed BEFORE any relocated pointer-imm
## base in its segment (parse-ORDER independence).
##
## Matrix finding (bit 2 of the family probe): CMP_GPRv_IMMz was gated
## SOLELY by imm_bounds_relocated_table — "a relocated base at/below the
## imm in the same segment" — which is a function of PARSE ORDER: a
## compare in a function that parses before any relocated MOV/store of a
## data pointer keeps its literal imm, while the byte-identical compare
## later in the sweep relocates. That is precisely the accidental-
## consistency trap 695d60f closed on the code-alias side (the Civ IV
## first-in-sweep static-init dispatcher). The reg-dest identity idiom
## `movl field(%reg),%eax; cmpl $&sentinel,%eax` is the compiler's other
## spelling of the 99_cmp_mem_ptr_imm mem-dest compare and must classify
## identically.
##
## Fix: CMP_GPRv_IMMz gains the 9510f29 store discriminator (4-aligned +
## vmaddr_in_writable_data; the arm already requires fixed-load + M32 +
## imm32 + !imm32_code_alias_is_constant) as a second admit alongside
## imm_bounds_relocated_table. In real images the bounds table admits
## nearly every data-aliasing value anyway once one store relocated below
## it — this makes classification deterministic, not broader. The Halo
## loop-sentinel path (test 69) is unaffected.
##
## _cmp_first sits FIRST in the file so it parses with an EMPTY
## relocated_ptr_imms set. Exit 0 iff both compare forms match; distinct
## exit code per first failure.

	.section __TEXT,__text,regular,pure_instructions

	.p2align 4, 0x90
_cmp_first:
	## eax-form: CMP_OrAX_IMMz (3d imm32)
	cmpl	$_blk+8, %eax
	jne	Lno
	## general-reg form: CMP_GPRv_IMMz (81 f9 imm32)
	cmpl	$_blk+8, %ecx
	jne	Lno
	movl	$1, %eax
	ret
Lno:
	movl	$0, %eax
	ret

	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$0x10, %esp

	## the identity value arrives via a relocated store + load (field flow)
	movl	$_obj, %ebx
	movl	$_blk+8, 0x8(%ebx)          ## relocated store (9510f29)
	movl	0x8(%ebx), %eax
	movl	%eax, %ecx
	calll	_cmp_first
	cmpl	$1, %eax
	jne	Lbad

	pushl	$0
	calll	_exit
	ud2

Lbad:
	pushl	$2
	calll	_exit
	ud2

	.section __DATA,__data
	.globl _obj
	.p2align 2
_obj:
	.space	16

	.p2align 4
_blk:
	.long	0, 0, 0, 0
