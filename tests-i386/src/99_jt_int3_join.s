## 99_jt_int3_join — a PIC jump table dispatched after an `int3` join must
## still be claimed.
##
## DetectJumpTables cleared every PIC anchor at ANY interrupt, int3 included,
## while the anchor REWRITE pass (DetectPicAnchoredDisps, guard 83) treats int3
## as transparent. In the shape below (a noreturn trap on one path, as compilers
## emit after abort()/__builtin_trap()),
##
##     call $+0; pop %ebx          ; the anchor
##     je   Lslow
##     int3                        ; anchors cleared here ...
##     jmp  Ldispatch              ; ... so this records an EMPTY snapshot
##   Lslow:
##     ...
##   Ldispatch:                    ; intersect(empty, {ebx}) = {} -> not claimed
##     mov  (Ltable-Lpic)(%ebx,%edx,4),%eax; add %ebx,%eax; jmp *%eax
##
## the rewrite pass re-anchored the table load while the entries kept their
## i386 offsets: the dispatch lands mid-instruction.
##
## Exit 42 = the table was relocated and case 2 ran.

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
	jne	Lslow                    ## taken at run time
	int3                             ## dead trap path
	jmp	Ldispatch
Lslow:
	nop
Ldispatch:
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
