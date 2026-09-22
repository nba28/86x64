## 99_jt_fused_x64_reparse — PIC jump tables must be claimed by EVERY parse pass.
##
## 86x64.sh re-parses the whole translated __text in every M64 step (modify
## --insert, strip-bind, interpose, convert) and each re-runs DetectJumpTables.
## A table one pass misses is decoded as CODE by that pass and re-emitted: a
## translated entry whose low byte is 0x70..0x7f decodes as a short jcc, gets
## widened to a 6-byte near jcc, and every later entry shifts by 4.
##
## MEASURED, Portal 2 shaderapidx9 ImageLoader::D3DFormatToImageFormat (i386
## 0x49b10): two FUSED dispatches whose tables sit back to back BELOW the case
## bodies. The M32 pass claimed 26 tables, the M64 re-parses 19; 1 of 20 entries
## landed on the right case body. Five sub-fixes in DetectJumpTables, one arm
## each here (select one arm with JT_ARM=a|b|c|d|e, default all):
##
##  a  M64 fused dispatch + next-table clamp  (_jt_two)
##     The translated fused dispatch is `lea r11,[rip+d]; add edx,[r11d+rcx*4];
##     jmp rdx` — table base in r11, anchor in the add's destination. OFF:
##     M64_NO_JT_FUSED_ADD_X64=1. Its two tables use DIFFERENT anchors (P1/P2)
##     and the lower one's dispatch comes LAST: without the clamp the lower
##     table's auto-size runs through the upper one and re-stamps its slots with
##     the wrong anchor. OFF: M64_NO_JT_TABLE_BOUND=1.
##  b  anchor copied to another register before the dispatch (_jt_copy).
##     OFF: M64_NO_JT_ANCHOR_COPY=1.
##  c  a NOT-TAKEN arm's call clobbers the anchor register in the linear walk;
##     the branch-target snapshot restores it (_jt_snap).
##     OFF: M64_NO_JT_BRANCH_SNAP=1.
##  d  a previous function's anchor survives its mid-function RET (a forward
##     branch to a later function is still pending) and turns `lea %ecx,[%eax-4]`
##     (switch(x-4)) into a bogus table base (_jt_leak / _jt_reset).
##     OFF: M64_NO_JT_FUNC_RESET=1.
##  e  anchor from a get_pc_thunk CALL. Translated, the call is `lea r11,[rip+ret];
##     …; jmp thunk` and the thunk `mov ebx,[rsp]; mov r11d,[rsp]; …` (_jt_thunk).
##     OFF: M64_NO_JT_X64_THUNK=1. The translated thunk sits after a 0x00 pad
##     byte, so the thunk pre-sweep also needs the symbol re-sync:
##     M64_NO_JT_SYM_RESYNC=1 fails c AND e. LPAD_E=80 makes a translated entry
##     decode as a short jcc (other pads are benign; 81 made the OFF arm SPIN).
##
## (M64_NO_JT_X64_ANCHOR_AT — an epilogue `mov reg,[rsp]` taken as an M64 anchor
## — is covered only indirectly, by the per-pass table-count invariant.)
##
## Exit 42 = every selected arm returned its case value. Otherwise 64 + a bit per
## failing arm (a=1 b=2 c=4 d=8 e=16), or a signal death.

	.set LPAD_A, 0
	.set LPAD_E, 80
	.section __TEXT,__cstring,cstring_literals
Lenv:	.asciz	"JT_ARM"

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi                     ## failure bits
	pushl	%edi                     ## selected arm char (0 = all)
	subl	$8, %esp
	xorl	%esi, %esi
	xorl	%edi, %edi
	pushl	$Lenv
	calll	_getenv
	addl	$4, %esp
	testl	%eax, %eax
	je	Lrun_a
	movzbl	(%eax), %edi

Lrun_a:
	testl	%edi, %edi
	je	1f
	cmpl	$'a', %edi
	jne	Lrun_b
1:	pushl	$2
	pushl	$0
	calll	_jt_two                  ## LT2 (upper table, anchor LP2), case 2
	addl	$8, %esp
	cmpl	$0x22, %eax
	jne	Lfail_a
	pushl	$3
	pushl	$1
	calll	_jt_two                  ## LT1 (lower table, anchor LP1), case 3
	addl	$8, %esp
	cmpl	$0x13, %eax
	je	Lrun_b
Lfail_a:
	orl	$1, %esi

Lrun_b:
	testl	%edi, %edi
	je	1f
	cmpl	$'b', %edi
	jne	Lrun_c
1:	pushl	$2
	calll	_jt_copy
	addl	$4, %esp
	cmpl	$0x32, %eax
	je	Lrun_c
	orl	$2, %esi

Lrun_c:
	testl	%edi, %edi
	je	1f
	cmpl	$'c', %edi
	jne	Lrun_d
1:	pushl	$1
	calll	_jt_snap
	addl	$4, %esp
	cmpl	$0x41, %eax
	je	Lrun_d
	orl	$4, %esi

Lrun_d:
	testl	%edi, %edi
	je	1f
	cmpl	$'d', %edi
	jne	Lrun_e
1:	pushl	$0
	calll	_jt_leak                 ## returns without the tail
	addl	$4, %esp
	pushl	$6
	calll	_jt_reset                ## switch (6 - 4) -> case 2
	addl	$4, %esp
	cmpl	$0x52, %eax
	je	Lrun_e
	orl	$8, %esi

Lrun_e:
	testl	%edi, %edi
	je	1f
	cmpl	$'e', %edi
	jne	Ldone
1:	pushl	$3
	calll	_jt_thunk
	addl	$4, %esp
	cmpl	$0x63, %eax
	je	Ldone
	orl	$16, %esi

Ldone:
	movl	$42, %eax
	testl	%esi, %esi
	je	1f
	leal	64(%esi), %eax
1:	pushl	%eax
	calll	_exit
	ud2

## ---- arm b: anchor copied into another register --------------------------
	.globl _jt_copy
	.p2align 4, 0x90
_jt_copy:
	pushl	%esi
	movl	8(%esp), %eax
	calll	LPc
LPc:	popl	%esi
	movl	%esi, %edx               ## the copy the dispatch uses
	movl	(LTc - LPc)(%edx,%eax,4), %eax
	addl	%edx, %eax
	jmpl	*%eax
	.p2align 2
LTc:	.long	Lcc0 - LPc
	.long	Lcc1 - LPc
	.long	Lcc2 - LPc
	.long	Lcc3 - LPc
Lcc0:	movl	$0x30, %eax
	jmp	Lcout
Lcc1:	movl	$0x31, %eax
	jmp	Lcout
Lcc2:	movl	$0x32, %eax
	jmp	Lcout
Lcc3:	movl	$0x33, %eax
Lcout:	popl	%esi
	ret

## ---- arm c: not-taken arm with a call before the dispatch -----------------
	.globl _jt_nop
	.p2align 4, 0x90
_jt_nop:
	ret

	.globl _jt_snap
	.p2align 4, 0x90
_jt_snap:
	movl	4(%esp), %ecx
	calll	LPs
LPs:	popl	%eax
	cmpl	$3, %ecx
	jbe	Lsd
	calll	_jt_nop                  ## the walk erases the %eax anchor here
	movl	$0x99, %eax
	ret
Lsd:	addl	(LTs - LPs)(%eax,%ecx,4), %eax
	jmpl	*%eax
	.p2align 2
LTs:	.long	Lcs0 - LPs
	.long	Lcs1 - LPs
	.long	Lcs2 - LPs
	.long	Lcs3 - LPs
Lcs0:	movl	$0x40, %eax
	ret
Lcs1:	movl	$0x41, %eax
	ret
Lcs2:	movl	$0x42, %eax
	ret
Lcs3:	movl	$0x43, %eax
	ret

## ---- arm d: an anchor leaking across a function start ---------------------
	.globl _jt_leak
	.p2align 4, 0x90
_jt_leak:
	movl	4(%esp), %ecx
	calll	LPl
LPl:	popl	%eax                     ## %eax anchor
	testl	%ecx, %ecx
	jne	_jt_tail                 ## forward, to a LATER function: stays pending
	movl	$0, %eax
	ret                              ## mid-function RET: the anchor is kept

	.globl _jt_reset
	.p2align 4, 0x90
_jt_reset:
	pushl	%ebx
	movl	8(%esp), %eax            ## %eax is NOT an anchor here
	calll	LPr
LPr:	popl	%ebx
	leal	-4(%eax), %ecx           ## switch (x - 4)
	cmpl	$3, %ecx
	ja	Lrdef
	movl	(LTr - LPr)(%ebx,%ecx,4), %eax
	addl	%ebx, %eax
	jmpl	*%eax
	.p2align 2
LTr:	.long	Lcr0 - LPr
	.long	Lcr1 - LPr
	.long	Lcr2 - LPr
	.long	Lcr3 - LPr
Lcr0:	movl	$0x50, %eax
	jmp	Lrout
Lcr1:	movl	$0x51, %eax
	jmp	Lrout
Lcr2:	movl	$0x52, %eax
	jmp	Lrout
Lcr3:	movl	$0x53, %eax
	jmp	Lrout
Lrdef:	movl	$0x5f, %eax
Lrout:	popl	%ebx
	ret

	.globl _jt_tail
	.p2align 4, 0x90
_jt_tail:
	movl	$0x77, %eax
	ret

## ---- arm e: anchor from a get_pc_thunk CALL (translated: lea r11 + jmp thunk)
	.globl _jt_pcthunk
	.p2align 4, 0x90
_jt_pcthunk:
	movl	(%esp), %ebx
	ret

	.globl _jt_thunk
	.p2align 4, 0x90
_jt_thunk:
	pushl	%ebx
	movl	8(%esp), %eax
	calll	_jt_pcthunk
Lt:	movl	(LTt - Lt)(%ebx,%eax,4), %eax
	addl	%ebx, %eax
	jmpl	*%eax
	.p2align 2
LTt:	.long	Lct0 - Lt
	.long	Lct1 - Lt
	.long	Lct2 - Lt
	.long	Lct3 - Lt
	.space	LPAD_E, 0x90
Lct0:	movl	$0x60, %eax
	jmp	Ltout
Lct1:	movl	$0x61, %eax
	jmp	Ltout
Lct2:	movl	$0x62, %eax
	jmp	Ltout
Lct3:	movl	$0x63, %eax
Ltout:	popl	%ebx
	ret

## ---- arm a: two fused tables back to back BELOW the case bodies -----------
## Must stay LAST in __text: the unclamped lower table then runs on through
## the upper one and stops only at the section end.
	.globl _jt_two
	.p2align 4, 0x90
_jt_two:
	pushl	%ebx
	pushl	%edi
	movl	12(%esp), %ecx           ## which table
	movl	16(%esp), %edx           ## index
	calll	LP1
LP1:	popl	%ebx                     ## anchor 1
	cld                              ## LP2 - LP1 = 7 = one case body
	calll	LP2
LP2:	popl	%edi                     ## anchor 2
	testl	%ecx, %ecx
	jne	Ld1
	addl	(LT2 - LP2)(%edi,%edx,4), %edi   ## upper table's dispatch FIRST
	jmpl	*%edi
Ld1:	addl	(LT1 - LP1)(%ebx,%edx,4), %ebx   ## lower table's dispatch LAST
	jmpl	*%ebx
	.space	LPAD_A, 0x90
Lc10:	movl	$0x10, %eax
	jmp	Lout
Lc11:	movl	$0x11, %eax
	jmp	Lout
Lc12:	movl	$0x12, %eax
	jmp	Lout
Lc13:	movl	$0x13, %eax
	jmp	Lout
Lc20:	movl	$0x20, %eax
	jmp	Lout
Lc21:	movl	$0x21, %eax
	jmp	Lout
Lc22:	movl	$0x22, %eax
	jmp	Lout
Lc23:	movl	$0x23, %eax
Lout:	popl	%edi
	popl	%ebx
	ret
	.p2align 2
LT1:	.long	Lc10 - LP1
	.long	Lc11 - LP1
	.long	Lc12 - LP1
	.long	Lc13 - LP1
LT2:	.long	Lc20 - LP2
	.long	Lc21 - LP2
	.long	Lc22 - LP2
	.long	Lc23 - LP2
