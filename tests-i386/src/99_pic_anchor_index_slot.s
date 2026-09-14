## 99_pic_anchor_index_slot — the PIC anchor in the SIB *INDEX* slot.
##
## i386 PIC reaches a global as `disp(%anchor)`, and with a live subscript as
## `disp(%anchor,%idx,scale)`. But SIB is SYMMETRIC at scale 1, and clang emits
## the two fields in whatever order its addressing-mode matcher produced, so the
## anchor turns up just as often in the INDEX slot with the live value in the
## BASE:
##
##     lea  edx, [eax + edi + 0xa4b0]      # edi = get_pc_thunk anchor
##                                         # eax = a live counter
##
## DetectPicAnchoredDisps used to test only the BASE for an anchor and `continue`
## otherwise, so this form kept its RAW i386 displacement.
##
## ★A RAW DISPLACEMENT IS NEVER SAFE. Translated code is bigger than the i386 it
## came from, so the translated image's section layout differs and `anchor + disp`
## reaches a DIFFERENT SECTION than it did originally. On Portal 2's libsteam_api
## (i386 0x61f6) the address that had pointed at a 0x3ff-byte path buffer in
## __common came out inside the image's own read-only __TEXT, and the loop's
## `movb $0, (%rcx)` took a write-protect SIGBUS (err=0x6). Three instructions
## earlier the sibling `lea esi,[edi + 0xa4b1]` — same buffer, anchor in the BASE
## slot, one byte along — was rewritten CORRECTLY, which is why an audit of the
## region read clean.
##
## The fix accepts either slot as the anchor, requiring scale 1 (`anchor*2` is not
## an anchor) and a base that can be re-encoded AS a SIB index — field 100 means
## "no index", so an ESP base still bails. Both slots anchored stays ambiguous and
## still bails. The transform swaps the roles:
##
##     lea  r11, [rip + disp32]            # r11 = anchor + disp, resolved
##     lea  edx, [r11 + eax*1]             # live value moves to the INDEX field
##
## ORACLE. The store goes through the index-slot form; the read-back goes through
## the BASE-slot form, which was always rewritten correctly, so the oracle cannot
## share the defect. `_cells[0]` is checked untouched, which also proves the live
## BASE survived the swap rather than being dropped (a dropped base would write
## _cells[0] instead of _cells[1]).
##
## Exit 42 = correct. With M64_NO_PIC_ANCHOR_INDEX=1 the raw displacement is kept
## and the store lands outside _cells: exit 7 (the untouched initialiser) or a
## signal death if the wild address is unmapped or read-only. Both are FAILs.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$16, %esp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	## Establish the PIC anchor: edi = vmaddr of Lpic0.
	calll	Lpic0
Lpic0:
	popl	%edi

	## eax = a LIVE byte offset (selects _cells[1]), sitting in the SIB BASE.
	movl	$4, %eax
	## The anchor is the INDEX, scale 1. This is the form under test.
	movl	$42, (_cells - Lpic0)(%eax,%edi,1)

	## Read back through the BASE-slot form — the always-correct path.
	movl	(_cells - Lpic0 + 4)(%edi), %esi
	## _cells[0] must be untouched: a dropped base would have landed here.
	movl	(_cells - Lpic0)(%edi), %edx
	testl	%edx, %edx
	jne	Lbad

	pushl	%esi
	calll	_exit
	ud2

Lbad:
	pushl	$1
	calll	_exit
	ud2

	.section __DATA,__data
	.p2align 2
_cells:
	.long	0	## must stay 0 — catches a dropped live base
	.long	7	## the store's target; 7 survives if the rewrite is suppressed
