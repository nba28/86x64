## 99_pic_anchor_imm — LLVM folds an anchor-relative offset into an ALU
## immediate instead of a displacement (clang -O1+, differential fuzzer):
##   lea  (%anchor,%i,4),%t ; add $T-anchor,%t       &table[i]  (_t26)
##   mov  %anchor,%t        ; adc $T-anchor,%t       &bytes[CF] (_t38)
##   mov  %anchor,%t ; (join) ; add %i,%t ; add $T-anchor,%t   &words[i] (_t20)
## No displacement names T, so %t kept the i386 address. %t holds anchor + X,
## so the immediate must add T' - anchor' in the translation.
##
## Exit 42 = all three reads hit their data (30 + 2 + 10). Kill
## switch M64_NO_PIC_ANCHOR_IMM=1 (translate time): raw reads, a fault or a
## wrong exit.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	subl	$28, %esp

	calll	Lpic0
Lpic0:
	popl	%esi                     ## esi = the PIC anchor
	movl	$2, %ecx
	leal	(%esi,%ecx,4), %eax
	addl	$(_tbl - Lpic0), %eax    ## &tbl[2] (the short eax form)
	movl	(%eax), %edi
	xorl	%ebx, %ebx               ## the add's flags are dead

	movl	%esi, %ebx               ## anchor copy
	movl	$4, %eax
	testl	%esp, %esp
	jne	Ljoin
	movl	$4, %eax
Ljoin:                                   ## %ebx = anchor on both edges
	addl	%eax, %ebx               ## anchor + 4
	addl	$(_words - Lpic0), %ebx  ## &words[1]
	addl	(%ebx), %edi             ## the add's flags are dead

	stc
	movl	%esi, %edx
	adcl	$(_bytes - Lpic0), %edx  ## &bytes[CF] = &bytes[1]
	movzbl	(%edx), %eax
	addl	%edi, %eax               ## the adc's flags are dead
	movl	%eax, (%esp)
	calll	_exit
	ud2

	.data
	.p2align 2
_tbl:
	.long	0, 0, 30, 0
_words:
	.long	0, 10
_bytes:
	.byte	0, 2
