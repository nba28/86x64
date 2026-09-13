## 99_pic_anchor_nonmov_def_kill — a PIC anchor register redefined by ANYTHING,
## not just by a plain `mov`, must stop being treated as an anchor.
##
## 99_pic_anchor_memload_kill established the rule for `mov <mem>,%reg`. That fix
## was written as an iform test, so every OTHER way of defining a register still
## left the dead anchor live:
##
##     movzbl 0x6bfa(%esi),%ebx    ## a zero-extending byte load — not MOV_GPRv_MEMv
##     xorl   %ebx,%ebx            ## no memory operand at all
##     movsbl 0x6bfa(%esi),%ebx    ## a sign-extending byte load, a third iform family
##
## The rule is about the DEFINITION, not about which opcode performed it, so the
## translator now asks XED which operands an instruction WRITES and retires the
## anchor of every 32-bit GPR among them.
##
## All three arms here arm the SUPPRESSED-rewrite mode, which is the quieter and
## nastier of the two: `disp(%anchor,%idx,scale)` is re-anchored only when the
## index is not ALSO an anchor, because two anchors are ambiguous. A stale anchor
## on the index trips that bail, so the access keeps its RAW i386 displacement and
## resolves to translated_anchor + i386_disp — inside __TEXT.
##
## ARM 1 is the real blocker, reproduced as a STORE. Portal 2 localize.dylib i386
## 0xbc8f, in a static initializer:
##     movzx 0x6bfa(%esi),%ebx          ## %ebx := a small count
##     mov   %eax,0x791e(%esi,%ebx,8)   ## store into a global table
## kept `0x791e` and wrote into its own read-only __TEXT -> SIGBUS, 6 runs of 6.
## Every one of the seven sibling accesses in that same basic block — all without
## an index — WAS re-anchored correctly, which is precisely why static audits read
## the function as clean: only the two indexed forms were left raw.
##
## Exit 42 = all three arms correct. 1/2/3 = which arm is wrong. 9 = the anchor
## itself is broken, so the arms would prove nothing either way.
##
## ⚠ POP IS DELIBERATELY NOT TESTED HERE, because `pop %reg` is EXEMPT from the
## rule while a forward branch target is still ahead. That is not an oversight in
## the rule, it is a measurement: `pop` is how an epilogue restores callee-saved
## registers, and a linear walk reaches the epilogue BEFORE every block the
## function only enters by a branch taken earlier. Portal 2 engine.dylib 0x2d1c1
## pops %esi four instructions before 0x2d1c6 `dec 0x5f67ce(%esi)`, a slow-path
## block where %esi really is still the anchor; killing on pop left 182 such sites
## raw across five images. The exemption is gated exactly like the RET clear.
##
## Kill switch: translate with M64_NO_PIC_ANCHOR_MEMLOAD_KILL=1 for the old
## behaviour. ⚠ The OFF arm then dies with SIGBUS (exit 138) rather than returning
## a wrong exit code, because arm 1's store lands in read-only __TEXT — that IS
## the blocker, so it is the faithful reproduction.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$32, %esp

	## ── ARM 1: the index anchor is killed by a BYTE LOAD (movzx) ───────────
	calll	Lpic1b
Lpic1b:
	popl	%ebx                        ## %ebx = an anchor, and it must DIE below
	calll	Lpic1
Lpic1:
	popl	%esi                        ## %esi = the anchor that stays LIVE

	## A real anchored read first, so the anchor is genuinely live and tracked
	## (and so this test also proves the fix does not disarm a LEGITIMATE use).
	movl	(_tag - Lpic1)(%esi), %eax
	cmpl	$0x00C0FFEE, %eax
	jne	Lfail9

	## Redefine %ebx with a zero-extending BYTE load. This is the instruction
	## the iform-shaped fix could not see.
	movzbl	(_idxb - Lpic1)(%esi), %ebx ## %ebx = 2; its anchor DIES here

	## Anchored STORE with %ebx as the INDEX. Rewritten only if %ebx is no
	## longer considered an anchor; otherwise the two-anchor bail leaves the raw
	## i386 displacement and this writes into __TEXT.
	movl	$0x5A5A5A5A, %eax
	movl	%eax, (_wtable - Lpic1)(%esi,%ebx,4)

	## Read it back through an INDEX-FREE form, which is always re-anchored
	## correctly — so this compares the store's landing site, not the read's.
	movl	(_wtable + 8 - Lpic1)(%esi), %eax
	cmpl	$0x5A5A5A5A, %eax
	jne	Lfail1

	## ── ARM 2: the index anchor is killed with NO MEMORY OPERAND AT ALL ────
	calll	Lpic2b
Lpic2b:
	popl	%ebx                        ## %ebx = an anchor again
	calll	Lpic2
Lpic2:
	popl	%edi                        ## %edi = the live base anchor

	movl	(_tag - Lpic2)(%edi), %eax
	cmpl	$0x00C0FFEE, %eax
	jne	Lfail9

	xorl	%ebx, %ebx                  ## %ebx = 0; anchor DIES, no memory read
	incl	%ebx                        ## %ebx = 1

	movl	(_table - Lpic2)(%edi,%ebx,4), %eax
	cmpl	$0x11111111, %eax
	jne	Lfail2

	## ── ARM 3: killed by a SIGN-extending byte load (a third iform family) ─
	calll	Lpic3b
Lpic3b:
	popl	%ebx                        ## %ebx = an anchor again
	calll	Lpic3
Lpic3:
	popl	%edi                        ## %edi = the live base anchor

	movl	(_tag - Lpic3)(%edi), %eax
	cmpl	$0x00C0FFEE, %eax
	jne	Lfail9

	movsbl	(_idxb3 - Lpic3)(%edi), %ebx ## %ebx = 3; anchor DIES

	movl	(_table - Lpic3)(%edi,%ebx,4), %eax
	cmpl	$0x44444444, %eax
	jne	Lfail3

	pushl	$42
	calll	_exit
	ud2

Lfail1:
	pushl	$1
	calll	_exit
	ud2
Lfail2:
	pushl	$2
	calll	_exit
	ud2
Lfail3:
	pushl	$3
	calll	_exit
	ud2
Lfail9:
	pushl	$9
	calll	_exit
	ud2

	## Cross-section from every anchor, so each displacement above is a full
	## disp32 — the width the re-anchoring rewrite handles.
	.section __DATA,__data
	.p2align 4
_tag:
	.long	0x00C0FFEE
_idxb:
	.byte	2                           ## the index arm 1 loads as a BYTE
	.byte	0, 0, 0
_idxb3:
	.byte	3                           ## the index arm 3 loads as a SIGNED byte
	.byte	0, 0, 0
	.p2align 4
_table:
	.long	0x00000000                  ## [0]
	.long	0x11111111                  ## [1]  <- arm 2 reads
	.long	0x22222222                  ## [2]
	.long	0x44444444                  ## [3]  <- arm 3 reads
	.p2align 4
_wtable:
	.long	0                           ## [0]
	.long	0                           ## [1]
	.long	0                           ## [2]  <- arm 1 stores here
	.long	0                           ## [3]
