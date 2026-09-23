## 99_pic_anchor_jt_targets — a PIC jump table's CASE BODIES are branch targets,
## so the anchor snapshot must be recorded for every one of them.
##
## DetectPicAnchoredDisps snapshots the anchor map (and, since round 4, the
## anchor SPILL-SLOT map) at a forward DIRECT branch, keyed by the branch
## displacement's target, and JOINs it back when the linear walk reaches that
## target. An INDIRECT dispatch `jmp *%reg` has no displacement, so nothing was
## recorded — even though DetectJumpTables has already resolved the table's
## entire target set (anchor + each claimed slot's delta) while auto-sizing it.
## The linear walk therefore carried state into exactly ONE case body: the
## instruction that happens to follow the dispatch. Every other case body is
## reachable ONLY through the table, so whatever killed the anchor register in
## between — typically a CALL in the first case, ECX/EDX being caller-saved —
## left them with their RAW i386 displacement.
##
## ★MEASURED — Portal 2 shaderapidx9 `CShaderShadowDX8::DepthFunc` (i386
## 0x35a60), wall 8; anchor `popl %ecx` at 0x35a6a:
##     0x35a91  movl 0xb2(%ecx,%edx,4),%edx   ## fused PIC table, 8 entries
##     0x35a9a  jmpl *%edx
##     0x35a9c  movl 0x3c5c2(%ecx),%eax       ## case 1 -> movl ...(%rip),%eax
##     0x35abc  movl 0x3c5c2(%ecx),%eax       ## case 2 -> KEPT THE RAW DISP
##     0x35ad8  movl 0x3c5c2(%ecx),%eax       ## case 3 -> KEPT THE RAW DISP
##     0x35af8  movl 0x3c5c2(%ecx),%eax       ## case 4 -> KEPT THE RAW DISP
## At runtime translated_anchor + i386_disp landed inside the translated __text
## and the following `movl (%eax),%eax` dereferenced code bytes — 0x76654472,
## ASCII "rDev" — and faulted.
##
## THE FIXTURE reproduces the three ingredients:
##   (i)   a PIC anchor in a CALLER-SAVED register (ECX),
##   (ii)  a fused PIC dispatch `movl tbl(%ecx,%edx,4),%edx; addl %ecx,%edx;
##         jmpl *%edx` over a 2-entry table, taken with index 1,
##   (iii) case 0 — the only body the linear walk reaches by falling through —
##         ending in a CALL, which erases ECX for the walk but NEVER EXECUTES,
##         so case 1 still holds the anchor at runtime.
##   ON  (gate armed):     the load in case 1 is rewritten rip-relative -> 42.
##   OFF (M64_NO_PIC_ANCHOR_JT_TARGETS=1): it keeps its i386 displacement and
##       reads the __text 0x90 padding (0x90909090) instead of _magic -> exit 9.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp

	## (i) PIC anchor in ECX — caller-saved, so a CALL kills it in the walk.
	calll	Lpic0
Lpic0:
	popl	%ecx

	## (ii) Fused PIC dispatch, index 1 -> Lcase1.
	movl	$1, %edx
	movl	(Ltbl - Lpic0)(%ecx,%edx,4), %edx
	addl	%ecx, %edx
	jmpl	*%edx

	## Case 0: the linear walk's next instruction, so it still carries the
	## anchor either way. Its CALL erases ECX for the walk; at runtime this
	## block never runs.
Lcase0:
	movl	(_magic - Lpic0)(%ecx), %eax
	calll	Lclobber
	movl	$7, %eax
	jmp	Lfinish

	## Case 1: reachable ONLY through the table. The `jmp Lfinish` above
	## leaves no live fall-through, so the JOIN must ADOPT the dispatch's
	## snapshot.
Lcase1:
	movl	(_magic - Lpic0)(%ecx), %eax
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

	## Callee for case 0's anchor-killing CALL. Deliberately NOT a
	## get_pc_thunk (no `movl (%esp),%reg`), so it establishes no anchor.
Lclobber:
	ret

	## The table lives AFTER the case bodies (the layout DepthFunc uses), so
	## auto-sizing stops at the first entry whose target leaves the section —
	## the 0x90 padding below reads as 0x90909090, a large negative delta.
	.p2align 2
Ltbl:
	.long	Lcase0 - Lpic0
	.long	Lcase1 - Lpic0

	## Padding so the OFF arm's stale-displacement read lands in MAPPED __text
	## (0x90 nops) and reports a deterministic exit 9 rather than a fault.
	.p2align 4, 0x90
	.space	0x2000, 0x90

	.section __DATA,__data
	.p2align 2
_magic:
	.long	0x5A17C0DE
