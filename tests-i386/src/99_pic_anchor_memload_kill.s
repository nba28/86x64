## 99_pic_anchor_memload_kill — a PIC anchor register RE-PURPOSED by a load from a
## NON-FRAME address must stop being treated as an anchor.
##
## i386 PIC code parks a get_pc_thunk base in a callee-saved register and reads
## globals as `disp(%anchor)`. The translator tracks which register holds that
## anchor so it can re-anchor each such access as rip-relative. It therefore has to
## know when the anchor DIES. It watched the frame-slot spill/reload form
##     mov %anchor, disp(%ebp)   /   mov disp(%ebp), %reg
## but a function re-purposes an anchor register just as often by loading an OBJECT
## FIELD into it, and that load has an arbitrary base register, so it fell straight
## through the EBP/ESP gate and the DEAD anchor stayed live for the rest of the
## function.
##
## One stale anchor, two opposite failure modes — both are armed here:
##
##  ARM 1  STALE ANCHOR ON THE BASE -> A FALSE REWRITE.
##    `lea 0x7ff(%esi), %edi` after %esi was reloaded from an object field becomes
##    `lea rip+..., %edi`, i.e. an IMAGE ADDRESS where an integer belongs.
##    Portal 2, libvstdlib CKeyValuesSystem::CKeyValuesSystem (i386 0x11fb1): %esi
##    held the anchor 0x11e6f, then `mov 0x64(%ebx),%esi` loaded m_HashTable.m_Size,
##    and `lea 0x7ff(%esi),%edi` (m_Size + 2047) turned into &(0x11e6f + 0x7ff).
##    That reached CUtlVector::GrowVector as `num`, so CUtlMemory::Grow doubled up
##    past it and asked for next_pow2(load_base) * 8 — 1 GiB or 2 GiB depending only
##    on where ASLR had put the image. The allocator returned NULL and Source exited
##    0 in silence, which is why this presented as a load-address-dependent flake.
##
##  ARM 2  STALE ANCHOR ON THE INDEX -> A SUPPRESSED REWRITE.
##    `mov disp(%anchor,%idx,4), %eax` is rewritten only when the index is NOT also
##    an anchor, because two anchors are ambiguous. A STALE anchor on the index trips
##    that bail, so the access keeps its RAW i386 displacement and reads
##    translated_anchor + i386_disp — inside __TEXT. Six such sites in libvstdlib,
##    all CRC-table reads that were silently indexing code.
##
## Exit 42 = both arms correct. 1/2 = which arm is wrong. 3 = the anchor itself is
## broken, so the arms below would prove nothing.
##
## Kill switch: translate with M64_NO_PIC_ANCHOR_MEMLOAD_KILL=1 to get the old
## behaviour back; arm 1 then compares an image address against 0x863 and fails (or
## faults), and arm 2 reads code as a table.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$32, %esp

	## ── ARM 1 ──────────────────────────────────────────────────────────────
	calll	Lpic1
Lpic1:
	popl	%esi                        ## %esi = anchor

	## A real anchored read first, so the anchor is genuinely live and tracked
	## (and so this test also proves the fix does not disarm a LEGITIMATE use).
	leal	(_tag - Lpic1)(%esi), %eax
	movl	(%eax), %eax
	cmpl	$0x00C0FFEE, %eax
	jne	Lfail3

	## Re-purpose %esi through a NON-FRAME base: %ebx points at an object, and
	## the field load is what the EBP/ESP gate could not see.
	leal	(_obj - Lpic1)(%esi), %ebx
	movl	4(%ebx), %esi               ## %esi = _obj.count = 100; anchor DIES

	## disp32 LEA off the re-purposed register: plain integer arithmetic.
	leal	0x7ff(%esi), %edi
	cmpl	$0x863, %edi                ## 100 + 0x7ff
	jne	Lfail1

	## ── ARM 2 ──────────────────────────────────────────────────────────────
	calll	Lpic2
Lpic2:
	popl	%edi                        ## %edi = anchor (stays the BASE)
	movl	%edi, %esi                  ## %esi = a COPY of the anchor

	leal	(_idxsrc - Lpic2)(%edi), %ebx
	movl	(%ebx), %esi                ## %esi = 2; its anchor must DIE here

	## Legitimate anchored table read. It is rewritten only if %esi is no longer
	## considered an anchor — otherwise the two-anchor bail leaves the raw disp.
	movl	(_table - Lpic2)(%edi,%esi,4), %eax
	cmpl	$0x33333333, %eax
	jne	Lfail2

	pushl	$42
	calll	_exit
	ud2

Lfail1:
	pushl	$1
	calll	_exit
	ud2
Lfail2:
	pushl	$2
	calll	_exit
	ud2
Lfail3:
	pushl	$3
	calll	_exit
	ud2

	## Cross-section from both anchors, so every displacement above is a full
	## disp32 — the width the re-anchoring rewrite handles.
	.section __DATA,__data
	.p2align 4
_tag:
	.long	0x00C0FFEE
_obj:
	.long	0xDEADBEEF                  ## +0: something that is NOT the count
	.long	100                         ## +4: the field arm 1 loads
_idxsrc:
	.long	2                           ## the index arm 2 loads
	.p2align 4
_table:
	.long	0x00000000                  ## [0]
	.long	0x11111111                  ## [1]
	.long	0x33333333                  ## [2]  <- the read
	.long	0x44444444                  ## [3]
