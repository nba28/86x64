## 99_pic_anchor_ret_call_dead — A CALL DOES NOT REVIVE A DEAD FALL-THROUGH.
##
## DetectPicAnchoredDisps joins a forward branch's anchor/spill-slot snapshot
## back at the branch TARGET. If the instruction before the target ended control
## flow (RET or unconditional JMP) the target has no fall-through predecessor,
## so the join ADOPTs the snapshot outright; otherwise it INTERSECTS it with the
## state the linear walk carried in. That test looked at exactly ONE preceding
## instruction, and the compiler's NORETURN-TRAP idiom slips through it:
##
##     ret                        ## function exit
##     call ___stack_chk_fail     ## branch-reachable, and NEVER RETURNS
## L:  ...                        ## branch target
##
## At L the previous category is CALL, so the join INTERSECTED L's (correct,
## non-empty) snapshot with the EPILOGUE's state — which describes no execution
## path that reaches L — and the anchor was lost for the whole rest of the
## function.
##
## ★MEASURED — Portal 2 wall 9, materialsystem `ImageLoader::ConvertImageFormat`
## (i386 0x79760, anchor `popl %ebx` at 0x7976e):
##     0x799ea  movl %ebx,-0x20(%ebp)          ## anchor SPILL  (slot live)
##     0x79a06  ja 0x79a84                     ## snapshot for 0x79a84 = {slot}
##     0x79a68  <join>                         ## isect with an EMPTY snapshot
##                                             ##   recorded at 0x797ca, before
##                                             ##   the spill -> slot dies
##     0x79a7e  retl                           ## mid-function RET
##     0x79a7f  calll ___stack_chk_fail        ## NORETURN
##     0x79a84  movl 0xc(%ebp),%ebx            ## join: isect cur=0 snap=1 -> 0
##     ...
##     0x79d24  movl -0x20(%ebp),%eax          ## reload of the spilled anchor
##     0x79d27  movl 0x4b912(%eax,%ebx,4),%eax ## KEPT THE RAW i386 DISPLACEMENT
##     0x79d8f  calll *-0x28(%ebp)             ## -> rip=0x87058646, a HEAP page
## Those two displacements address `ImageLoader`'s per-format conversion
## function-pointer tables in __DATA,__const (0xc5080 and 0xc5120); left raw,
## the call went through a garbage pointer and died SIGBUS, err=0x15
## (instruction fetch on a rw- page), under CMaterialSystem::SetMode.
##
## THE FIXTURE reproduces the four ingredients:
##   (i)   a PIC anchor (EBX) SPILLED to an EBP frame slot,
##   (ii)  a join that intersects that slot away, because an earlier forward
##         branch to it was recorded BEFORE the spill (snapshot = {}),
##   (iii) a mid-function RET followed by a CALL — the noreturn-trap idiom,
##   (iv)  a branch target after that CALL, reached ONLY by a branch taken
##         while the slot was still live.
##   ON  (gate armed):  the join ADOPTs -> the slot reload re-establishes the
##       anchor -> the read is rewritten rip-relative -> _magic -> exit 42.
##   OFF (M64_NO_PIC_ANCHOR_RET_CALL_DEAD=1): the join intersects -> the slot is
##       dead -> the read keeps its i386 displacement and lands in the __text
##       0x90 padding (0x90909090) -> exit 9.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp

	## (i) PIC anchor in EBX.
	calll	Lpic0
Lpic0:
	popl	%ebx

	## (ii-a) A forward branch to Ljoin recorded BEFORE the spill, so Ljoin's
	##        slot snapshot is EMPTY. Never taken at runtime.
	xorl	%eax, %eax
	testl	%eax, %eax
	jne	Ljoin

	## (i) The anchor SPILL. From here the slot holds the anchor.
	movl	%ebx, -20(%ebp)

	## (iv) The branch to Ltarget, recorded while the slot IS live, so
	##      Ltarget's snapshot = {slot}. TAKEN at runtime.
	xorl	%eax, %eax
	testl	%eax, %eax
	je	Ltarget

	## (ii-b) Ljoin is reached here by FALL-THROUGH, so the join intersects
	##        its empty snapshot with the live state and the slot dies for
	##        the rest of the linear walk. Dead at runtime.
Ljoin:
	movl	$1, %eax

	## (iii) The noreturn-trap idiom: a mid-function RET (Ltarget is still a
	##       pending forward target, so this does not end the function for the
	##       walk) immediately followed by a CALL. Neither runs at runtime.
	ret
	calll	Lnoreturn

	## (iv) The branch target. Its only real predecessor is the `je` above.
Ltarget:
	movl	-20(%ebp), %eax
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

	## Callee for the trap call. Deliberately NOT a get_pc_thunk (no
	## `movl (%esp),%reg`), so it establishes no anchor of its own.
Lnoreturn:
	ret

	## Padding so the OFF arm's stale-displacement read lands in MAPPED __text
	## (0x90 nops) and reports a deterministic exit 9 rather than faulting.
	.p2align 4, 0x90
	.space	0x2000, 0x90

	.section __DATA,__data
	.p2align 2
_magic:
	.long	0x5A17C0DE
