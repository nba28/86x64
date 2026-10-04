## 99_jt_slot_snap_epilogue — a PIC switch whose anchor lives in an %ebp frame
## slot, dispatched in a block entered by a forward branch from BEFORE an early
## epilogue.
##
## Portal 2 client vgui::Panel::OnMessage (i386 0x89771f..0x8977da):
##
##         movl  %eax,-0x10(%ebp)          ; spill the anchor
##         je    0x897799                  ; over the early epilogue
##         ... popl %ebp ; retl            ; early epilogue
##   0x897799: ...
##         movl  -0x10(%ebp),%edx          ; reload it
##         movl  0x6a2(%edx,%ecx,4),%ecx ; addl %edx,%ecx ; jmp *%ecx
##
## DetectJumpTables tracked the spilled anchor, but the epilogue's `pop %ebp`
## dropped every %ebp slot on the linear walk and branch targets restored only
## REGISTERS, so the table was never claimed: translated_anchor + raw i386 offset
## landed mid-instruction (SIGILL). Kill switch M64_NO_JT_SLOT_SNAP=1 (translate
## time). Exit 42 = case 2 ran.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	subl	$40, %esp

	calll	Lpic0
Lpic0:
	popl	%eax
	movl	%eax, -12(%ebp)          ## SPILL the anchor to an %ebp slot
	movl	$2, %ecx                 ## the switch value -> case 2
	cmpl	$0, %ecx
	jne	Lbody                    ## forward, over the early epilogue

	addl	$40, %esp                ## early epilogue (never runs)
	popl	%esi
	popl	%ebx
	popl	%ebp
	retl

Lbody:
	cmpl	$3, %ecx
	ja	Ldefault
	movl	-12(%ebp), %edx          ## RELOAD the anchor
	movl	(Ltable - Lpic0)(%edx,%ecx,4), %ecx
	addl	%edx, %ecx
	jmp	*%ecx

Lcase0:
	movl	$10, %esi
	jmp	Lend
Lcase1:
	movl	$20, %esi
	jmp	Lend
Lcase2:
	movl	$42, %esi
	jmp	Lend
Lcase3:
	movl	$30, %esi
	jmp	Lend
Ldefault:
	movl	$1, %esi

Lend:
	movl	%esi, (%esp)
	calll	_exit
	ud2

	.p2align 2
Ltable:
	.long	Lcase0 - Lpic0
	.long	Lcase1 - Lpic0
	.long	Lcase2 - Lpic0
	.long	Lcase3 - Lpic0

	.p2align 4, 0x90
	.space 16, 0x90
