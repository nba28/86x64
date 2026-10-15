## 99_jt_lea_sum_dispatch — an unoptimised (-O0) PIC switch: the anchor and the
## switch value are both parked in frame slots, reloaded, and the target is
## summed with LEA instead of ADD:
##
##     call Lpic; Lpic: pop %eax           ; anchor
##     mov %eax,-0x14(%ebp)                ; park the anchor
##     mov %eax,-0x18(%ebp)                ; park the (range-checked) index
##     mov -0x14(%ebp),%eax                ; reload both
##     mov -0x18(%ebp),%ecx
##     mov (Ltable-Lpic)(%eax,%ecx,4),%edx ; edx = table[idx]
##     lea (%edx,%eax),%edx                ; edx = anchor + table[idx]
##     jmp *%edx
##
## MEASURED, Portal's libSDL2 SDL_GL_SetAttribute (i386 dispatch 0xfd5de, anchor
## 0xfd58b, 24-entry table at 0xfd900 right after the function). The table was
## never claimed, so its bytes were translated as instructions and the dispatch
## added an opcode word to the anchor: rip 0x2e63a386, unmapped.
##
## Exit 42 = the table was relocated and case 2 ran.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$56, %esp

	calll	Lpic0
Lpic0:
	popl	%eax                     ## eax = the PIC anchor
	movl	$2, -8(%ebp)             ## the switch value -> case 2
	movl	%eax, -20(%ebp)          ## park the anchor
	movl	-8(%ebp), %eax
	cmpl	$3, %eax
	movl	%eax, -24(%ebp)          ## park the index
	ja	Ldefault
	movl	-20(%ebp), %eax          ## reload the anchor
	movl	-24(%ebp), %ecx          ## reload the index
	movl	(Ltable - Lpic0)(%eax,%ecx,4), %edx
	leal	(%edx,%eax), %edx
	jmpl	*%edx

Lcase0:
	movl	$10, -4(%ebp)
	jmp	Lend
Lcase1:
	movl	$20, -4(%ebp)
	jmp	Lend
Lcase2:
	movl	$42, -4(%ebp)
	jmp	Lend
Lcase3:
	movl	$30, -4(%ebp)
	jmp	Lend
Ldefault:
	movl	$1, -4(%ebp)

Lend:
	movl	-4(%ebp), %eax
	movl	%eax, (%esp)
	calll	_exit
	ud2

	.p2align 2
Ltable:
	.long	Lcase0 - Lpic0
	.long	Lcase1 - Lpic0
	.long	Lcase2 - Lpic0
	.long	Lcase3 - Lpic0

	.p2align 4, 0x90
	.space 16
