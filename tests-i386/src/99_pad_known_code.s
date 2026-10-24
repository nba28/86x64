## 99_pad_known_code — after a `ret`, the sweep treats a 0x00 byte as alignment
## padding, but a function symbol there starts with `addb %al,%al` (00 c0).
## Padding it decodes `c0 66 21 d1` next (shlb $0xd1,0x21(%esi): a store
## through %esi = 0) instead of `andw %dx,%cx`. Exit 42.
## A direct call would make _dbl a branch target the sweep already rescues;
## only the symbol names it here.
## OFF arm: M64_NO_PAD_KNOWN_CODE=1 at translate time.
	.text
	.globl _nop_ret
_nop_ret:
	ret
	.globl _dbl
_dbl:
	addb	%al, %al                ## 00 c0
	andw	%dx, %cx                ## 66 21 d1
	ret
	.globl _main
_main:
	xorl	%esi, %esi
	movb	$21, %al
	calll	*_fp                    ## no code reference to _dbl
	movzbl	%al, %eax
	pushl	%eax
	calll	_exit
	.data
_fp:	.long	_dbl
