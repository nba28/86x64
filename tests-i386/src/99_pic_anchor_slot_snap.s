## 99_pic_anchor_slot_snap — a PIC anchor SPILL SLOT must survive a store that
## only the LINEAR WALK thinks kills it.
##
## DetectPicAnchoredDisps already tracks an anchor through `mov %anchor,slot` /
## `mov slot,%reg` (step 2b), and already snapshots the REGISTER map at a
## forward branch so a block whose sibling clobbered the register still sees the
## anchor (branch_anchor_snap). The SLOT map was not snapshotted, so a store of a
## NON-anchor into the slot on ONE branch killed it for EVERY later block.
##
## ★MEASURED — Portal 2 libtogl `CGLMFBO::TexAttach` (i386 0x2260), wall 5:
##     0x226e  popl %eax                 ## anchor
##     0x226f  movl %eax,-0x10(%ebp)     ## spilled
##     0x2309  jne 0x23b5                ## forward branch
##     0x230f  movl -0x10(%ebp),%eax     ## reload  (rewritten correctly)
##     0x2312  movl 0x44daa(%eax),%eax
##     0x2318  movl (%eax),%eax
##     0x231a  movl %eax,-0x10(%ebp)     ## NON-anchor stored -> slot erased
##     0x23b0  jmp 0x25a3                ## no fall-through into 0x23b5
##     0x23b5: ...
##     0x23ce  movl -0x10(%ebp),%eax     ## reload -> no longer an anchor
##     0x23d1  movl 0x44daa(%eax),%eax   ## KEPT THE RAW i386 DISPLACEMENT
##     0x23da  movl (%eax),%eax          ## SIGSEGV
## The two blocks are MUTUALLY EXCLUSIVE at runtime (0x2309 jumps over the
## kill), but a linear walk cannot know that. At runtime
## translated_anchor + i386_disp landed inside the translated __text, so the
## loaded "pointer" was code bytes (0xc700) and the dereference faulted — three
## instructions before the GL dispatch slots 0x1c8/0x1d4/0x1d8 it was fetching.
##
## THE FIXTURE reproduces the three ingredients:
##   (i)   an anchor spilled to a frame slot,
##   (ii)  a forward branch OVER a block that reloads the slot and then stores a
##         NON-anchor back into it,
##   (iii) the branch target preceded by an unconditional `jmp`, so it has no
##         live fall-through and must ADOPT the branch's snapshot.
##   ON  (gate armed):     the load at Lreload is rewritten rip-relative -> 42.
##   OFF (M64_NO_PIC_ANCHOR_SLOT_SNAP=1): it keeps its i386 displacement and
##       reads the __text nop padding instead of _magic -> exit 7.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp

	## (i) Establish the PIC anchor and SPILL it to a frame slot.
	calll	Lpic0
Lpic0:
	popl	%eax
	movl	%eax, -16(%ebp)

	## (ii) Always-taken forward branch over the killing block. The snapshot of
	## the slot map has to be taken HERE, where the slot still holds the anchor.
	movl	$1, %edx
	testl	%edx, %edx
	jne	Lreload

	## Never executed at runtime — but the LINEAR walk sees it first. Its last
	## instruction stores a NON-anchor into the slot, which is what erased the
	## slot for Lreload below.
	movl	-16(%ebp), %eax
	movl	(_magic - Lpic0)(%eax), %eax
	movl	%eax, -16(%ebp)
	jmp	Ldone

	## (iii) Reached only by the branch above (the `jmp Ldone` leaves no live
	## fall-through), so the JOIN must ADOPT the branch's slot snapshot.
Lreload:
	movl	-16(%ebp), %eax
	movl	(_magic - Lpic0)(%eax), %eax
	cmpl	$0x5A17C0DE, %eax
	jne	Lbad
	movl	$42, %eax
	jmp	Lfinish
Lbad:
	movl	$7, %eax
	jmp	Lfinish
Ldone:
	movl	$8, %eax

Lfinish:
	pushl	%eax
	calll	_exit
	ud2

	## Padding so the OFF arm's stale-displacement read lands in MAPPED __text
	## (0x90 nops) and reports a deterministic exit 7 rather than a fault.
	.p2align 4, 0x90
	.space	0x2000, 0x90

	.section __DATA,__data
	.p2align 2
_magic:
	.long	0x5A17C0DE
