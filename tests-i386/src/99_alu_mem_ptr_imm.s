## 99_alu_mem_ptr_imm — `addl/subl $&writable_data, mem` pointer-IMMEDIATE
## in-place offset<->pointer conversions in a non-PIE i386 executable.
##
## Matrix finding (bits 0/1 of the family probe): the mem-dest ADD (81 /0)
## and SUB (81 /5) siblings of the 9510f29/07e7ef0 pointer-imm stores were
## uncaptured for register-base destinations, while
##   - their REG-dest twin (ADD_GPRv_IMMz) relocates via the permissive
##     any-segment probe, and
##   - their ABS32-dest twin relocates via the absolute-[disp32] arm's
##     permissive probe.
## So `field += &base` / `field -= &base` mixed a RELOCATED pointer (every
## other flow of &base) with the RAW i386 base address -> the result is off
## by the whole translation delta (a garbage pointer or garbage offset).
##
## Fix: ADD/SUB mem-dest join the CMP capture arm under the EXACT 9510f29
## discriminator (fixed-load non-PIE MH_EXECUTE + 4-aligned +
## vmaddr_in_writable_data). AND/OR/XOR/TEST stay deliberately uncaptured
## (a mask against a pointer VALUE is meaningless). The ADD_MEMv_IMMz /
## SUB_MEMv_IMMz transforms (lea r11 + 01/29 /r MR rewrite) already
## existed.
##
## Exit 0 iff all cases hold; distinct exit code per first failure.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$0x40, %esp
	movl	$_obj, %ebx

	## ---- A: mem-dest ADD pointer imm (offset -> pointer fixup) ----
	movl	$0x8, 0x14(%ebx)
	addl	$_blk, 0x14(%ebx)           ## 81 /0: field += &blk
	movl	0x14(%ebx), %ecx
	movl	$_blk+8, %eax               ## relocated ground truth
	cmpl	%ecx, %eax
	jne	Lbad_a

	## ---- B: mem-dest SUB pointer imm (pointer -> offset) ----
	movl	$_blk+8, 0x18(%ebx)         ## relocated store (9510f29)
	subl	$_blk, 0x18(%ebx)           ## 81 /5: field -= &blk
	cmpl	$0x8, 0x18(%ebx)
	jne	Lbad_b

	## ---- A': zerofill-target ADD (07e7ef0 parity) ----
	movl	$0x4, 0x1c(%ebx)
	addl	$_bss_blk, 0x1c(%ebx)       ## field += &__bss (zerofill vmaddr)
	movl	0x1c(%ebx), %ecx
	movl	$_bss_blk+4, %eax
	cmpl	%ecx, %eax
	jne	Lbad_z

	## ---- ebp-frame ADD variant ----
	movl	$0x8, -0x8(%ebp)
	addl	$_blk, -0x8(%ebp)           ## 81 45 f8 imm32
	movl	-0x8(%ebp), %ecx
	movl	$_blk+8, %eax
	cmpl	%ecx, %eax
	jne	Lbad_e

	## ---- integer negative control: non-aliasing 81-encoded imm ----
	movl	$0x1, 0x20(%ebx)
	addl	$0x800, 0x20(%ebx)          ## 81 /0 imm32=0x800 < 0x1000: literal
	cmpl	$0x801, 0x20(%ebx)
	jne	Lbad_i

	pushl	$0
	calll	_exit
	ud2

Lbad_a:
	pushl	$2
	calll	_exit
	ud2
Lbad_b:
	pushl	$3
	calll	_exit
	ud2
Lbad_z:
	pushl	$4
	calll	_exit
	ud2
Lbad_e:
	pushl	$5
	calll	_exit
	ud2
Lbad_i:
	pushl	$6
	calll	_exit
	ud2

	.section __DATA,__data
	.globl _obj
	.p2align 2
_obj:
	.space	48

	.p2align 4
_blk:
	.long	0, 0, 0, 0

	.zerofill __DATA,__bss,_bss_blk,16,4
