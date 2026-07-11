## 94_const_interior_movswl — SIGNED interior MEMORY-OPERAND read (`movswl
## [const+2]`) of a {short width, short height} struct in a READ-ONLY
## __TEXT,__const table, in a non-PIE i386 executable.
##
## This is the read half of the Quinn Preferences blank-piece-style-previews
## bug (-[QuinnPieceStylePreviewCell drawInteriorWithFrame:inView:]):
##
##   _pieceSize1 = 0xb3a40  {w, h}   movswl 0xb3a42 -> h1 (interior, 2 mod 4)
##   _pieceSize3 = 0xb3a44  {w, h}
##   _pieceSize2 = 0xb3a48  {w, h}   movswl 0xb3a4a -> h2 (interior, 2 mod 4)
##
## Quinn's real layout orders the siblings 1,3,2 (pieceSize3 sits BETWEEN
## pieceSize1 and pieceSize2), reproduced here. That ordering matters: an
## interior read that MISRELOCATES by a datum lands on a VALID-but-WRONG
## sibling (mapped memory, no fault) and silently returns the wrong field
## value — exactly the "reads height 3 instead of 1 -> blank preview" symptom.
##
## A `movswl [abs32]` read decodes as a `[disp32]` memory operand with no
## trailing immediate, so instruction.cc parses its disp32 through
## Immediate<bits>::Parse(is_pointer=true). Immediate's mid-blob
## resolve_containing fallback was gated to WRITABLE __DATA only, so an interior
## address inside a read-only __TEXT,__const blob was silently left
## unrelocated: the translated dylib emitted `movswl 0xb3a42(%rip)` — the raw
## i386 vmaddr used as a rip-relative displacement, reading garbage far outside
## the dylib (EXC_BAD_ACCESS / wrong bytes) -> blank previews.
##
## Fix (commit 7a28d47): ParseEnv::vmaddr_in_readonly_opaque_data OR'd into the
## Immediate/NonLazySymbolPointer mid-blob gate, so a read-only const interior
## relocates to (containing blob + offset) like writable __DATA. 93 guards the
## POINTER-IMMEDIATE interior; this guards the interior signed-READ path and
## additionally asserts the read returns the RIGHT sibling's field VALUE (a
## silent wrong-datum misrelocation returns a wrong value with NO crash — which
## a mere "non-crash" guard would miss).
##
## Signed movswl is deliberate: height1 = -1 (0xFFFF) sign-extends to
## 0xFFFFFFFF, so a wrong sibling (a positive height) is unambiguously caught
## and the signedness of the load is exercised too.
##
## Exit 0 iff every interior signed read yields its OWN struct's height; exit 1
## on any wrong value (stale, wrong sibling, or lost sign).

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90

_main:
	## ---- control: 4-ALIGNED read of pieceSize1.width (blob start) ----
	## Always worked (exact-key resolve hits the blob start). w1 = 4.
	movswl	_pieceSize1, %eax
	cmpl	$4, %eax
	jne	Lbad

	## ---- interior read 1: pieceSize1.height (0xb3a40+2, 2 mod 4) ----
	## Correct value is -1 (0xFFFF sign-extended). A wrong sibling would give
	## +0x1234 (pieceSize3.h) or +0x5678 (pieceSize2.h); a lost sign would
	## give +0xFFFF. All are != -1.
	movswl	_pieceSize1+2, %eax
	cmpl	$-1, %eax
	jne	Lbad

	## ---- interior read 2: pieceSize2.height (0xb3a48+2, 2 mod 4) ----
	## The exact Quinn shape whose sibling (pieceSize3) sits BETWEEN 1 and 2.
	## Correct value is 0x5678.
	movswl	_pieceSize2+2, %eax
	cmpl	$0x5678, %eax
	jne	Lbad

	## ---- interior read 3: pieceSize3.height (0xb3a44+2, 2 mod 4) ----
	## The in-between sibling; correct value 0x1234.
	movswl	_pieceSize3+2, %eax
	cmpl	$0x1234, %eax
	jne	Lbad

	## ---- control: 4-aligned read of pieceSize2.width (blob start) ----
	movswl	_pieceSize2, %eax
	cmpl	$2, %eax
	jne	Lbad

	pushl	$0
	calll	_exit
	ud2

Lbad:
	pushl	$1
	calll	_exit
	ud2

	## {short width, short height} structs, ordered 1,3,2 like Quinn, each
	## 4-aligned at its start so the .height field is a 2-mod-4 interior. Kept
	## in one contiguous table so a datum-off misrelocation lands on a real
	## neighbor (valid memory, wrong value) rather than faulting.
	.section __TEXT,__const
	.p2align 2
_pieceSize1:
	.short	4                        ## +0  width  = 4
	.short	-1                       ## +2  height = -1 (0xFFFF, signed)
_pieceSize3:
	.short	2                        ## +0  width  = 2
	.short	0x1234                   ## +2  height = 0x1234
_pieceSize2:
	.short	2                        ## +0  width  = 2
	.short	0x5678                   ## +2  height = 0x5678
