## 25_pic_anchor_branch_store — PIC-anchored data STORE on a branch target that
## sits after `ret` + alignment nops.  Mirrors iPhoto UpgradeChecker
## -[checkOSVersion:]'s `osVersion = SystemVersion()`: a function-local static
## lazily initialized on a COLD path.  clang places the init block after the hot
## path's `ret`, 16-byte-aligned, so nop padding separates the `ret` from the
## branch target.
##
## DetectPicAnchoredDisps must see THROUGH that nop padding when deciding whether
## the branch target has a live fall-through predecessor: the real predecessor
## is the RET (no fall-through), so the target's anchor snapshot is authoritative
## and must be ADOPTED, not intersected with the dead post-RET state.  The hot
## path restores the caller's %ebx via a FRAME-SLOT load (`mov -4(%ebp),%ebx`),
## which correctly clears the anchor on the linear path — so the carried state at
## the branch target is empty, and intersecting it (the bug) drops the anchor.
## The `disp(%ebx)` store then keeps its stale i386 displacement and, after the
## M32->M64 layout grows, writes to the wrong place (read-only __TEXT in iPhoto
## -> SIGBUS).  See section.cc DetectPicAnchoredDisps block (0b) / last_flow_cat.
##
## Written in GAS/AT&T (not nasm): the anchor+disp `(_osv - L0)(%ebx)` is a
## cross-section (__DATA vs __TEXT) symbol difference needing a Mach-O
## SECTDIFF/PAIR scattered reloc, which nasm's macho32 backend can't emit but
## clang's integrated assembler can (same reason tests/04 is in C).
##
## Validation via exit code: cached() lazily sets osVersion=42 on the cold path;
## main reads osVersion + the call count back via its OWN (fall-through, always-
## rewritten) anchor.  exit(42) iff the store landed correctly AND compute() ran
## exactly once.  A dropped anchor makes the store miss (osVersion keeps its
## 0xdead sentinel, compute re-runs -> exit 0xad / g_calls=2) or fault (signal).

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_compute:                              ## ++g_calls; return 42
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	calll	Lc0
Lc0:	popl	%ebx
	movl	(_gcalls - Lc0)(%ebx), %eax
	incl	%eax
	movl	%eax, (_gcalls - Lc0)(%ebx)
	movl	$42, %eax
	popl	%ebx
	popl	%ebp
	retl

	.p2align 4, 0x90
_cached:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$8, %esp
	movl	%ebx, -4(%ebp)             ## SAVE caller ebx to a FRAME SLOT (before anchor)
	calll	L0
L0:	popl	%ebx                       ## PIC anchor = L0
	movl	(_osv - L0)(%ebx), %eax    ## load osVersion (fall-through: rewritten)
	cmpl	$0xdead, %eax
	je	Linit                         ## cold init -> forward branch
Lret:
	movl	-4(%ebp), %ebx             ## RESTORE ebx from frame slot (MOV) -> clears anchor
	movl	%ebp, %esp
	popl	%ebp
	retl                              ## RET
	.p2align 4, 0x90                  ## nop padding -> Linit is a branch target after RET
Linit:
	calll	_compute                   ## eax = 42 (ebx callee-saved: anchor value survives)
	movl	%eax, (_osv - L0)(%ebx)    ## STORE osVersion = 42  <-- anchored store under test
	jmp	Lret

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$16, %esp
	calll	_cached                    ## first: runs cold init
	calll	_cached                    ## second: cached
	calll	Lm0
Lm0:	popl	%ebx
	movl	(_osv - Lm0)(%ebx), %eax      ## osVersion (42 if store landed right)
	movl	(_gcalls - Lm0)(%ebx), %edx   ## g_calls (1 if compute ran once)
	cmpl	$42, %eax
	jne	Lfail
	cmpl	$1, %edx
	jne	Lfail
	movl	$42, %eax
Lfail:
	movl	%eax, (%esp)
	calll	_exit
	ud2

	.section __DATA,__data
	.p2align 2
_osv:	.long	0xdead
_gcalls: .long	0
