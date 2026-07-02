## 62_pic_xsection_thunk_stripped — UNNAMED cross-section get_pc_thunk in a
## fully-stripped binary (Halo root cause, 2026-07-02).
##
## Sibling of 30_pic_anchor_xsection_thunk. Test 30 guards the NAMED seed:
## the Symtab ctor records every `___i686.get_pc_thunk.<r>` nlist into
## ParseEnv::pic_thunks so a cross-section `call thunk` establishes the PIC
## anchor. A fully STRIPPED GCC binary (Halo: 0 defined text nlists) defeats
## that seed; the detectors' byte-scans only cover their OWN section's blobs,
## so an anchored access in __text whose thunk lives elsewhere established no
## anchor. Consequence (Halo.dylib+0x1e36): `leal 0x5b2208(%ebx),%eax` (disp
## aliasing a segment) fell through to the absolute-table rewrite
## `lea r11,[rip+target]; leal (%rbx,%r11),%eax` — KEEPING the dead anchor
## base — and the follow-on deref faulted at anchor+target. Small-disp
## anchored accesses are equally wrong (stale i386 disp emitted verbatim).
##
## The fix triggers on STRUCTURE, not names: Archive's parse (archive.cc)
## byte-scans every executable text section for the canonical thunk body
## `8b {04..3c} 24 c3` (mov (%esp),%r32; ret) and seeds ParseEnv::pic_thunks
## before any Parse1 — CALLing an address with that body IS a get_pc_thunk
## whether or not a symbol names it.
##
## This test reproduces the structure with modern tools: the thunk sits in a
## CUSTOM `__TEXT,__picthunk` section (unknown names survive ld's merging of
## the deprecated __textcoal_nt/__StaticInit into __text), the caller in
## __text, and the Makefile rule runs `strip -x` so the thunk label is gone.
## _main anchors via the unnamed cross-section thunk, STOREs 42 into _g
## through the anchor, then _verify reads _g back via its own inline
## `call $+0; pop` anchor (always detected). exit(_verify()) == 42 iff the
## anchored store landed. Pre-fix: the store misses _g (stale disp) or
## faults -> exit != 42.

	## The PIC thunk: LOCAL label (stripped by `strip -x`), custom section.
	.section __TEXT,__picthunk,regular,pure_instructions
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
Lv0:	popl	%ebx                          ## inline anchor (always detected)
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
	calll	___i686.get_pc_thunk.bx       ## UNNAMED cross-section thunk after strip
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
