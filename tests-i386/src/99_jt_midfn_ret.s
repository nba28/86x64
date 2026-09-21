## 99_jt_midfn_ret — a PIC jump table that follows a MID-FUNCTION `ret` must
## still be claimed.
##
## DetectJumpTables walks __text linearly and cleared every PIC anchor at every
## RET. A linear walk reaches an EARLY-RETURN epilogue before the code that
## follows it, so in a function shaped
##
##     call $+0; pop %ebx                 ; the anchor
##     ...
##     jg   Lbody                         ; forward, OVER the early return
##     ...; leave; ret                    ; early return
##   Lbody:
##     mov  %eax,[%ebx + idx*4 + disp]    ; split dispatch: load ...
##     add  %eax,%ebx                     ; ... add the anchor ...
##     jmp  *%eax                         ; ... jump
##
## the anchor was gone by the time the dispatch arrived, the table was never
## claimed, and its entries were parsed as CODE. The anchor REWRITE pass already
## knows this shape — it treats a RET as a function exit only when no forward
## branch target is pending — so it re-anchored the load while the entries kept
## their i386 offsets (`translated_anchor + i386_offset`, mid-instruction).
##
## MEASURED, Portal 2 libcef.dylib (stripped Chromium): anchor 0xc7823e, early
## return at 0xc7825f, dispatches at 0xc78930 and 0xc78aa4, neither claimed. The
## entries (0x274, 0x562, …) decode as BOUND, which trips the translate-time width
## guard; tables whose entries decode cleanly fail silently at run time instead.
##
## Exit 42 = the table was relocated and case 2 ran. OFF arm is
## M64_NO_JT_MIDFN_RET=1 at TRANSLATE time: the entries stay raw i386 offsets and
## the jump lands mid-instruction (a wrong exit code or a signal death).

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$32, %esp
	pushl	%esi
	pushl	%ebx

	calll	Lpic0
Lpic0:
	popl	%ebx                     ## ebx = the PIC anchor

	movl	$2, %edx                 ## the switch value -> case 2
	testl	%edx, %edx
	jg	Lbody                    ## FORWARD branch over the early return

	movl	$1, %eax                 ## early return: dead at run time, but the
	popl	%ebx                     ## linear walk reaches this RET before the
	popl	%esi                     ## dispatch below
	leave
	retl

Lbody:
	cmpl	$3, %edx
	ja	Ldefault
	movl	(Ltable - Lpic0)(%ebx,%edx,4), %eax
	addl	%ebx, %eax
	jmpl	*%eax

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
	pushl	%esi
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
