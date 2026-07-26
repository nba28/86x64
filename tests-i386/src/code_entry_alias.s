## code_entry_alias.s — fixture for code_entry_alias_test.sh
## (core: section.cc DataParser + ParseEnv::code_alias_lacks_entry_evidence;
## Civ IV STEAM static-init SIGSEGV, 2026-07-26.)
##
## THE BUG.  A locals-STRIPPED, reloc-less, fixed-address (-no_pie) i386 exec
## gives macho-tool no metadata for deciding which 4-byte __DATA words are
## pointers, so its in-range heuristic rebases any constant whose value aliases
## __text.  Civ IV's strtok delimiter string " ._" = 0x005F2E20 was rewritten to
## a translated __text address whose bytes are "mT\xa9\x10"; strtok then split
## the registry path "Game" on the 'm' and the failed lookup NULL-dereferenced
## at +0x88 during the translated static initializers.
##
## WHY THE BOUNDARY GATE IS NOT ENOUGH.  code_interior_alias rejects values that
## are not instruction starts.  Civ's 0x005F2E20 IS one: it is `sub $0x18,%esp`,
## three bytes into the function entered at 0x5F2E1D with `55 89 e5`.  Measured
## on the real binary — the parse has a decoded Instruction blob at exactly that
## address, so the boundary test cannot see it.
##
## THE DISCRIMINATOR.  A genuine DATA-resident code pointer (vtable slot, fn-ptr
## table entry, ObjC1 IMP) targets a function ENTRY, not just any boundary.
## Entry-ness is recoverable WITHOUT local symbols: an nlist at the value (GLOBAL
## text symbols survive `strip -x`) or the `55 89 e5` frame-setup prologue there.
## That is the same positive-evidence rule instruction.cc already applies to
## code-aliasing imm32s, now shared via ParseEnv::code_target_has_entry_evidence.
##
## THE FIXTURE reproduces Civ's shape exactly: _probe_fn is a LOCAL function
## (removed by `strip -x`, so no nlist and have_local_text_syms stays false)
## opening with the `55 89 e5` prologue; the harness patches _probe_fn+3 — the
## `subl $0x18,%esp` boundary, mid-function, no symbol, no prologue — into the
## sentinel-bracketed __DATA slot as a plain integer literal with NO relocation.
## The 0xA5A5xxxx sentinels are >= 0x80000000 and so are never pointer-detected.

	.section __TEXT,__text,regular,pure_instructions

	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp
	call	_probe_fn              ## keep _probe_fn referenced/reachable
	movl	$0, (%esp)
	call	_exit

	.p2align 4, 0x90
_probe_fn:                         ## local + stripped -> no func_syms nlist
	pushl	%ebp                   ## +0  55        \ the frame-setup prologue:
	movl	%esp, %ebp             ## +1  89 e5     / positive ENTRY evidence
	subl	$0x18, %esp            ## +3  83 ec 18  <- harness targets THIS:
	leave                          ##               a real instruction boundary,
	ret                            ##               mid-function, no evidence

	.section __DATA,__data
	.globl _g_blob
	.p2align 2
_g_blob:
	.long	0xA5A51111             ## sentinel (>= 0x80000000: never pointer-detected)
	.long	0x00000000             ## <- harness patches _probe_fn+3 here
	.long	0xA5A52222             ## sentinel
