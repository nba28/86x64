## 99_pic_anchor_sib_prefixed_rex — PIC-anchored SIB access at a PREFIXED operand
## width (0x66 16-bit and 0xF3 SSE-scalar), where the REX.B for the substituted
## %r11 base used to be emitted in the WRONG PLACE and silently dropped.
##
## The transform for `op [anchor + idx*scale + disp32]` is:
##     lea  r11, [rip + disp32]     ; r11 = table base
##     op   [r11 + idx*scale]       ; index into the table
## and %r11 needs REX.B, because register 11 encodes in the SIB base field as its
## low three bits, 011 — which alone is %rbx.
##
## REX must be the LAST prefix, immediately before the opcode. The rewrite used to
## insert it at the FRONT of the instruction, ahead of any mandatory 0x66/0xF2/0xF3
## prefix, and a REX followed by another prefix is IGNORED by the CPU:
##     66 41 66 c7 04 03 ...   -> REX dead, base decodes as %rbx   (was emitted)
##     66 41 c7 04 03 ...      -> REX honoured, base is %r11       (correct)
## Plain `movl`/`movq` forms carry no mandatory prefix, so REX landed correctly and
## they always worked — which is why this hid for so long while every 16-bit and
## SSE-scalar table access through a PIC anchor addressed a STALE %rbx.
##
## On Portal 2's engine.dylib that was 292 sites; one of them stored 16-bit 0xFFFF
## through the stale base into dyld's own allocations, corrupting a Loader so that
## dyld aborted the process (`magic == kMagic`). Reads were equally wrong, silently
## returning garbage.
##
## Three arms, each a form that carries a mandatory prefix:
##   1. 66-prefixed STORE  `movw $imm16, disp(%edi,%ebx,2)`, verified by reading the
##      slot back with a NON-prefixed load, so the oracle cannot share the defect.
##   2. 66-prefixed LOAD   `movw disp(%edi,%ebx,2), %ax`, summed over the table.
##   3. F3-prefixed LOAD   `movss disp(%edi,%ebx,4), %xmm0`.
##
## Exit 42 = all three correct. 1/2/3 = which arm read or wrote the wrong place.
## With the defect the wild base typically faults (SIGSEGV/SIGBUS) instead of
## returning a value at all, so a signal death is also a FAIL, not a flake.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$32, %esp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	## PIC anchor: edi = vmaddr of Lpic0
	calll	Lpic0
Lpic0:
	popl	%edi

	## --- arm 1: 66-prefixed STORE through the anchored SIB form -------------
	## 66 C7 /0 imm16 — exactly Portal 2's `movw $0xffff, (%rbx,%rax)` site.
	movl	$1, %ebx
	movw	$0x5a5a, (_w16 - Lpic0)(%edi,%ebx,2)
	## Read it back with movzwl (0F B7: no mandatory prefix, so this load was
	## always encoded correctly and is an INDEPENDENT oracle for the store).
	movzwl	(_w16 - Lpic0)(%edi,%ebx,2), %eax
	cmpl	$0x5a5a, %eax
	jne	Lfail1

	## --- arm 2: 66-prefixed LOAD through the anchored SIB form --------------
	xorl	%esi, %esi
	xorl	%ebx, %ebx
Lloop:
	movw	(_w16b - Lpic0)(%edi,%ebx,2), %ax
	movzwl	%ax, %eax
	addl	%eax, %esi
	incl	%ebx
	cmpl	$4, %ebx
	jl	Lloop
	cmpl	$0x1AAA, %esi              ## 0x1111+0x0222+0x0333+0x0444
	jne	Lfail2

	## --- arm 3: F3-prefixed SSE scalar LOAD --------------------------------
	movl	$1, %ebx
	movss	(_f32 - Lpic0)(%edi,%ebx,4), %xmm0
	movss	%xmm0, -4(%ebp)
	movl	-4(%ebp), %eax
	cmpl	$0x3F800000, %eax         ## 1.0f
	jne	Lfail3

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

	## Writable, so arm 1 can store into it. Cross-section from the anchor, so
	## the displacement is a full disp32 (the form the rewrite handles).
	.section __DATA,__data
	.p2align 4
_w16:
	.short	0, 0, 0, 0
_w16b:
	.short	0x1111, 0x0222, 0x0333, 0x0444
	.p2align 4
_f32:
	.long	0x00000000                ## [0] 0.0f
	.long	0x3F800000                ## [1] 1.0f  <- the read
	.long	0x40000000                ## [2] 2.0f
	.long	0x40400000                ## [3] 3.0f
