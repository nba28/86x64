## 96_abs32_opcode_breadth — breadth battery for the absolute `[disp32]`
## memory operand (i386 mod=00 r/m=101) across the OPCODE space.
##
## The parser's three [abs32] arms (simple / +imm32 / +imm8/16) and the
## Emit-side xed_patch_disp are opcode-agnostic BY DESIGN; this fixture
## pins that property so a future opcode-special-cased change can't
## silently narrow it. Exercises, all against __data globals in a non-PIE
## exec (every disp32 must relocate to the M64 layout):
##
##   fld/fstp dword   D9 05 / D9 1D    x87 load/store
##   movzbl           0F B6 05          byte load zero-extend (2-byte opcode)
##   movswl           0F BF 05          word load sign-extend
##   incl             FF 05             rmw inc
##   negl             F7 1D             rmw neg
##   notl             F7 15             rmw not
##   shll $3          C1 25 imm8        shift group w/ trailing imm8
##   btsl $2          0F BA 2D imm8     bit-test group (2-byte opcode + imm8)
##   addl $imm8       83 05 imm8        ALU group-1 sign-extended imm8
##   movb $imm8       C6 05 imm8        byte store (guard 12's class)
##   movw $imm16      66 C7 05 imm16    16-bit store (odd-struct report class)
##
## Exit 0 iff every operation observed/produced the expected value through
## the relocated globals.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	## x87 roundtrip: _fsrc (2.0) -> st0 -> _fdst
	flds	_fsrc                       ## D9 05 disp32
	fstps	_fdst                       ## D9 1D disp32
	movl	_fdst, %eax
	cmpl	$0x40000000, %eax           ## 2.0f bits
	jne	Lbad

	## byte / word loads with extension
	movzbl	_b1, %eax                   ## 0F B6 05
	cmpl	$0xab, %eax
	jne	Lbad
	movswl	_w1, %eax                   ## 0F BF 05
	cmpl	$-2, %eax                   ## 0xfffe sign-extends
	jne	Lbad

	## rmw group: incl, negl, notl
	incl	_ctr                        ## FF 05: 7 -> 8
	negl	_ctr                        ## F7 1D: -> -8
	notl	_ctr                        ## F7 15: -> 7
	movl	_ctr, %eax
	cmpl	$7, %eax
	jne	Lbad

	## shift w/ imm8, bts w/ imm8, add w/ sign-extended imm8
	shll	$3, _sh                     ## C1 25 imm8: 3 -> 24
	btsl	$2, _sh                     ## 0F BA 2D imm8: |= 4 -> 28
	addl	$0x10, _sh                  ## 83 05 imm8: -> 44
	movl	_sh, %eax
	cmpl	$44, %eax
	jne	Lbad

	## small-imm stores: byte and 16-bit word
	movb	$0x5a, _b2                  ## C6 05 imm8
	movzbl	_b2, %eax
	cmpl	$0x5a, %eax
	jne	Lbad
	movw	$0xbeef, _w2                ## 66 C7 05 imm16
	movzwl	_w2, %eax
	cmpl	$0xbeef, %eax
	jne	Lbad

	pushl	$0
	calll	_exit
	ud2

Lbad:
	pushl	$1
	calll	_exit
	ud2

	.section __DATA,__data
	.globl _fsrc
	.p2align 2
_fsrc:	.long	0x40000000              ## 2.0f
_fdst:	.long	0
_ctr:	.long	7
_sh:	.long	3
_b1:	.byte	0xab
_b2:	.byte	0
	.p2align 1
_w1:	.short	0xfffe
_w2:	.short	0
