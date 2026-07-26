## code_interior_alias.s — fixture for code_interior_alias_test.sh
## (NOT a numbered suite test: the bug is only observable STRUCTURALLY in the
## translated binary, so a shell harness inspects the output bytes rather than
## diffing stdout.)
##
## Regression for the CODE-INTERIOR ALIAS gate (core: section.cc DataParser +
## ParseEnv::code_interior_alias; Civ IV STEAM static-init SIGSEGV, 2026-07-26).
##
## THE BUG.  A fixed-address (-no_pie) i386 executable carries no rebase stream
## and no local reloc table, so macho-tool cannot KNOW which 4-byte __DATA words
## are pointers.  Its heuristic calls a word a pointer whenever the value lands
## in the image's vmaddr span; for an EXECUTABLE target it deliberately skips the
## 4-byte-alignment test (function entries are not 4-aligned).  Every other
## discriminator is disarmed for this binary shape:
##   * code_alias_is_constant    needs local text symbols (stripped away)
##   * the classic-reloc gate    needs LC_DYSYMTAB local relocs (a fixed-address
##                               exec has none)
##   * cstring_interior_alias    only covers __cstring targets
## So ANY constant whose value happens to alias __text is falsely "rebased" to a
## translated __text address and corrupted.
##
## Civ IV hit this on a 4-byte STRING constant in __DATA,__data: the strtok
## delimiter set " ._" = 0x005F2E20, which aliased the LAST BYTE of a 5-byte
## `call` at i386 __text 0x5F2E1C.  Rebased, its bytes became "mT\xa9\x10", so
## strtok split the registry path "Game" on the 'm' into "Ga"/"e"; "Ga" is not a
## child of the root node, the walk kept a NULL node, and the next component
## dereferenced it at +0x88 -> EXC_BAD_ACCESS during the translated static
## initializers.
##
## THE FIX.  A genuine pointer into code always targets a decoded INSTRUCTION —
## a function entry, or at worst a basic-block head (switch table).  Nothing can
## point at the middle of an instruction, nor at bytes the disassembler could not
## decode at all.  So a data word landing on a non-instruction byte of a code
## section is a constant, whatever it aliases.  Symbol-free, reloc-free, exact.
##
## THE FIXTURE.  Civ's 0x5F2E20 sits 8 bytes into the 11-byte span that starts at
## the instruction 0x5F2E18 — i.e. on bytes the linear sweep emitted as raw
## DataBlobs, NOT on an instruction start.  That distinction matters: a target
## with NO blob at all is simply left unrelocated (Immediate only does an
## exact-key resolve for code targets), so it is the DataBlob case that actually
## produces the corruption.  `_probe_pad` reproduces it exactly: one decodable
## instruction (nop) followed by bytes xed cannot decode, which the sweep emits
## as DataBlobs.  The harness reads _probe_pad's vmaddr with nm BEFORE stripping
## and patches _probe_pad+PROBE_DATA_OFF into the middle slot of _g_blob, so the
## __DATA word is a plain integer literal with NO relocation — exactly Civ's
## shape.  _probe_pad is LOCAL, so `strip -x` removes it and leaves the binary
## with no local text symbols (func-entry gate disarmed), again exactly Civ's
## shape.  The 0xA5A5xxxx sentinels bracket the slot so the harness can find it
## in the translated output; they are >= 0x80000000 and therefore outside the
## pointer-detection window themselves.

	.section __TEXT,__text,regular,pure_instructions

	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp
	call	_probe_pad             ## keep _probe_pad referenced/reachable
	movl	$0, (%esp)
	call	_exit

	.p2align 4, 0x90
_probe_pad:                        ## local + stripped -> no func_syms
	.byte	0x90                                 ## +0 nop  (a decodable instruction)
	.byte	0xff, 0xff, 0xff, 0xff               ## +1..+4 `ff /7`: no such opcode ->
	.byte	0xff, 0xff, 0xff, 0xff               ## +5..+8 the sweep emits DataBlobs
	ret                                          ## (harness targets _probe_pad+2)

	.section __DATA,__data
	.globl _g_blob
	.p2align 2
_g_blob:
	.long	0xA5A51111             ## sentinel (>= 0x80000000: never pointer-detected)
	.long	0x00000000             ## <- harness patches _probe_pad+2 here
	.long	0xA5A52222             ## sentinel
