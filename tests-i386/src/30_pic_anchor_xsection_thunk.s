## 30_pic_anchor_xsection_thunk — PIC anchor established by a get_pc_thunk that
## lives in a DIFFERENT text section from its caller.
##
## Root cause (Civ IV, 2026-06-26): GCC emits the separate-thunk PIC idiom
## `call ___i686.get_pc_thunk.bx` and the i386 linker places the thunk in a
## text section OTHER than the caller's __text (Civ IV: __textcoal_nt).
## Section::DetectPicAnchoredDisps / DetectJumpTables byte-scanned only the
## CURRENT section for the thunk's `mov %reg,(%esp); ret` body, so a cross-
## section call found no thunk, established no anchor, and the anchored store
## `mov %eax, disp(%ebx)` fell through to the generic [base+disp32] rewrite —
## which KEEPS the anchor base: `lea r11,[rip+target]; mov [rbx+r11]`.  That
## double-counts the base (eff. addr = anchor + target ≈ 2x image base) → the
## store either SIGSEGVs (Civ IV crt `start` faulted on its first PIC store) or,
## when the disp stays disp32-but-unrewritten, writes its STALE i386 offset into
## read-only __TEXT → SIGBUS.
##
## Fix: the Symtab ctor records every `___i686.get_pc_thunk.<r>` symbol by NAME
## into ParseEnv::pic_thunks (global, cross-section), and both detectors seed
## their section-local thunk map from it.
##
## NOTE on coverage: the modern clang assembler/linker MERGES custom i386 code
## sections (the deprecated __textcoal_nt, and __StaticInit) back into __text, so
## a minimal binary can't reproduce the literal cross-SECTION split — this test
## therefore runs the thunk and caller in one __text (it passes pre- and post-
## fix).  Its enduring value is guarding the NAMED separate-thunk anchor path
## (`call ___i686.get_pc_thunk.bx` to a non-external thunk -> anchor -> rip-rel
## store), which neither 25 (inline call$+0;pop) nor 26 (SIB index) covers, and
## which the symbol-table seed now also feeds.  The genuine cross-section miss is
## validated on the real Civ IV binary (its __textcoal_nt survives as a separate
## section), where the fix turns the crt `start` SIGSEGV into a clean launch.
## The thunk is LOCAL (no .globl) so the call is DIRECT (no stub) and its target
## equals the thunk's symbol vmaddr, exactly as in Civ IV's non-external thunk.
##
## _main anchors via the named thunk, STOREs 42 into _g through it, then
## _verify reads _g back via its OWN inline `call $+0; pop` anchor (always
## detected, even pre-fix).  exit(_verify()) == 42 iff the cross-section-anchored
## store landed on _g.  Pre-fix: store faults / misses -> not 42.

	## The PIC thunk in a SEPARATE code section, LOCAL (no .globl => direct call).
	.section __TEXT,__StaticInit,regular,pure_instructions
	.p2align 2, 0x90
___i686.get_pc_thunk.bx:
	movl	(%esp), %ebx
	retl

	.section __TEXT,__text,regular,pure_instructions

	## _verify: read _g via a SELF (inline) PIC anchor; return it in eax.
	.p2align 4, 0x90
_verify:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	calll	Lv0
Lv0:	popl	%ebx                          ## inline anchor (detected pre-fix too)
	movl	(_g - Lv0)(%ebx), %eax        ## eax = _g
	popl	%ebx
	popl	%ebp
	retl

	.globl	_main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$8, %esp
	pushl	%ebx
	calll	___i686.get_pc_thunk.bx       ## CROSS-SECTION thunk -> ebx = L0 anchor
L0:
	movl	$42, %eax
	movl	%eax, (_g - L0)(%ebx)         ## anchored STORE under test (-> _g)
	popl	%ebx
	calll	_verify                       ## eax = _g read via inline anchor
	movl	%eax, (%esp)
	calll	_exit                         ## exit(_g)  == 42 iff store landed
	ud2

	.section __DATA,__data
	.p2align 2
_g:	.long	0
