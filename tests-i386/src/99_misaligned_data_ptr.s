## 99_misaligned_data_ptr — a __data pointer slot whose VALUE is a MISALIGNED
## address inside FILE-BACKED __data.
##
## THE DEFECT. A fixed-address i386 exec carries no relocations (nlocrel=0), so
## the translator must INFER which 4-byte __data words are pointers. One of its
## discriminators was: "a real pointer into a non-executable segment points at an
## ALIGNED global, so a misaligned data-range value is a constant". That is FALSE
## for a pointer into a packed BYTE-RECORD table, which legitimately begins at
## any offset. Such a pointer is then left UNREBASED and ships into the
## translated image as a raw i386 address; the first deref through it touches
## whatever happens to be mapped at that low address, or nothing.
##
## MEASURED on Halo CE (2026-08-20). Its versioned-schema pointer at i386
## 0x3798ec holds 0x00379ec6 — misaligned by 2, and a genuine pointer to a table
## of 10-byte records. Every OTHER pointer in the same struct was rebased; this
## one survived verbatim into __DATA,__data of the translated image. The scan at
## 0x19f0a4 walks that table looking for a tag-9 terminator, so it ran off the
## end of the low mapping and took SIGSEGV at exactly 0x379ec6, on the Campaign
## "load an existing profile" path.
##
## WHY THE GATE STAYS FOR ZERO-FILL. Every false positive it was written for
## targets zero-fill space (photocd's 0x30a91 in __bss; Halo's 0x0048021C /
## 0x005802D0 in __common), where the pointee has no file content and alignment
## is the only signal left. A target with real file content is a different
## situation, and rejecting it on alignment alone throws away a true pointer.
##
## THE FIXTURE. `_tbl` is deliberately placed at an ODD-BY-2 address inside
## __data, and `_ptr` holds its address. main reads the pointer back out of
## __data and dereferences it. If the slot was rebased, the record's tag reads
## back as 0x2A. If it was not, the raw i386 address is dereferenced instead:
## unmapped in the translated image -> SIGSEGV, or garbage if something is
## mapped. A separate ALIGNED pointer is the positive control (it must keep
## working, proving the fixture reaches the pointer path at all), and a plain
## small integer is the negative control (it must NOT be rebased).
##
## Exit 0 iff: the misaligned pointer resolves and reads 0x2A, the aligned
## control reads 0x5B, and the integer control still reads 0x1234.
## OFF arm (M64_NO_MISALIGN_ZF_NARROW=1) restores the unconditional gate and
## must FAIL — a clean OFF arm would mean the fixture never exercised the gate.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	## --- misaligned pointer under test ---
	movl	_ptr, %eax             ## load the __data pointer SLOT
	movzbl	(%eax), %ecx           ## deref: tag byte, expect 0x2A
	cmpl	$0x2A, %ecx
	jne	Lfail

	## --- aligned pointer: positive control ---
	movl	_ptr_aligned, %eax
	movzbl	(%eax), %ecx           ## expect 0x5B
	cmpl	$0x5B, %ecx
	jne	Lfail

	## --- integer constant: negative control (must NOT be rebased) ---
	movl	_int_const, %ecx
	cmpl	$0x1234, %ecx
	jne	Lfail

	## success
	pushl	$0
	call	_exit
Lfail:
	pushl	$1
	call	_exit

	.section __DATA,__data
	.p2align 2
## Two filler bytes so _tbl lands at (base + 2): misaligned by construction.
_pad:
	.byte	0xAA, 0xBB
_tbl:
	.byte	0x2A                   ## the tag the test reads back
	.byte	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09

	.p2align 2
_tbl_aligned:
	.byte	0x5B
	.byte	0x00, 0x00, 0x00

	.p2align 2
_ptr:
	.long	_tbl                   ## MISALIGNED value -> the gate under test
_ptr_aligned:
	.long	_tbl_aligned           ## aligned value -> always accepted
_int_const:
	.long	0x1234                 ## must stay a constant
