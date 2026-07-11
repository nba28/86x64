## 93_const_interior_imm — pointer IMMEDIATE into the INTERIOR (non-4-aligned
## offset) of a READ-ONLY __TEXT,__const table, in a non-PIE i386 executable.
##
## Mirrors the Quinn Preferences crash (-[QuinnPieceStylePreviewCell
## drawInteriorWithFrame:inView:] -> QuinnDrawCells(_pieceMatrix2, ...)):
##
##   _pieceMatrix3 = 0xb3a30   (4-aligned)   movl $imm,(%esp) -> lea, RELOCATED
##   _pieceMatrix1 = 0xb3a3c   (4-aligned)   movl $imm,(%esp) -> lea, RELOCATED
##   _pieceMatrix2 = 0xb3a36   (2 mod 4)     movl $imm,(%esp) -> STALE raw imm32
##
## The parse marks all three immediates as pointers (fixed-load-address
## heuristic), but Immediate<bits>'s pointee resolution only exact-key-resolves
## blob STARTS; the mid-blob resolve_containing fallback was gated to WRITABLE
## __DATA, so an interior address inside a read-only __TEXT,__const blob was
## silently left unrelocated -> the translated dylib passes the ORIGINAL
## pre-slide vmaddr (0xb3a36) to the callee -> EXC_BAD_ACCESS on first deref
## (Quinn: native loadregs callee, fault addr == the stale imm).
##
## Fix: extend the fallback gate to read-only NON-CODE sections
## (ParseEnv::vmaddr_in_readonly_opaque_data) — __OBJC stays excluded
## (structurally-parsed fragile metadata), instruction sections stay excluded
## (mid-instruction offsets don't survive the M32->M64 transform).
##
## Ground truth is the absolute MEMORY-OPERAND read (`movzwl _tbl+2`), whose
## containing-blob fallback has always been ungated. Exit 0 iff the interior
## pointer immediates (reg form AND Quinn's arg-store form) reach the same
## bytes the direct read sees.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90

## int check(const unsigned short *p): cdecl; returns p[0] (halts on NULL-ish)
_check:
	movl	4(%esp), %ecx
	movzwl	(%ecx), %eax
	retl

_main:
	## ---- ground truth: direct absolute read of the interior halfword ----
	movzwl	_tbl+2, %ebx             ## abs [disp32] read (ungated fallback)

	## ---- form 1: MOV_GPRv_IMMv with an INTERIOR (2 mod 4) const address ----
	movl	$_tbl+2, %eax            ## was: stale raw imm32 (unrelocated)
	movzwl	(%eax), %ecx
	cmpl	%ebx, %ecx
	jne	Lbad

	## ---- form 2 (Quinn's exact shape): arg-store `movl $imm,(%esp)` ----
	subl	$12, %esp
	movl	$_tbl+2, (%esp)          ## MOV_MEMv_IMMz esp-slot arg store
	calll	_check
	addl	$12, %esp
	cmpl	%ebx, %eax
	jne	Lbad

	## ---- control: 4-ALIGNED interior offset (worked before the fix) ----
	movl	$_tbl+4, %eax
	movzwl	(%eax), %ecx
	movzwl	_tbl+4, %edx
	cmpl	%edx, %ecx
	jne	Lbad

	pushl	$0
	calll	_exit
	ud2

Lbad:
	pushl	$1
	calll	_exit
	ud2

	.section __TEXT,__const
	.p2align 2
_tbl:
	.short	0x1111                   ## +0
	.short	0x2222                   ## +2  <- the _pieceMatrix2-shaped interior
	.short	0x3333                   ## +4  <- 4-aligned control
	.short	0x4444                   ## +6
