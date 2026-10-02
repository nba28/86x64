## 99_pic_anchor_call_adopt — after a CALL the fall-through carries nothing in
## eax/ecx/edx, so at a branch target right after a call the branch's
## caller-saved PIC anchor is ADOPTED (the call cannot return into it).
##
## MEASURED, Portal 2 client.dylib static init: an EH landing pad ending in
## `call _Unwind_Resume` (noreturn, and preceded by ordinary code so the
## ret/jmp deadness rule does not apply) falls through into a `je` target that
## reads/writes a global via the %eax anchor. The join intersected with the
## post-call state (eax erased) -> raw i386 disp -> a store into __text, SIGBUS.
##   ON : the read is rewritten rip-relative -> _magic -> exit 42.
##   OFF (M64_NO_PIC_ANCHOR_CALL_ADOPT=1 at TRANSLATE time): the raw disp reads
##       the __text 0x90 padding -> exit 9.
	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	calll	Lpic0
Lpic0:
	popl	%eax                    ## anchor in caller-saved EAX
	xorl	%ecx, %ecx
	testl	%ecx, %ecx
	je	Ltarget                 ## TAKEN; snapshot {eax}
	movl	%ecx, %edx              ## an ordinary instruction (live fall-through)
	calll	Lnoreturn               ## never executed; clobbers eax for the walk
Ltarget:
	movl	(_magic - Lpic0)(%eax), %eax
	cmpl	$0x5A17C0DE, %eax
	jne	Lbad
	movl	$42, %eax
	jmp	Lfinish
Lbad:
	movl	$9, %eax
Lfinish:
	pushl	%eax
	calll	_exit
	ud2
Lnoreturn:
	ret
	.p2align 4, 0x90
	.space	0x2000, 0x90

	.section __DATA,__data
	.p2align 2
_magic:
	.long	0x5A17C0DE
