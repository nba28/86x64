## 99_stripped_fnptr_field_start — a code-target imm32 stored into a struct
## FIELD through a general base register must be relocated when it is a real
## function START, and must stay a LITERAL when it merely aliases a
## prologue-shaped run that something falls through into.
##
## TWO ARMS, one fixture, both markers LOCAL so `strip -x` removes their nlists
## (the Halo CE / Civ IV Steam shape). No symbol survives at either value, so
## the translator sees only two integers and must discriminate them
## STRUCTURALLY — which is the whole point of the guard. Deliberately no
## __text-geometry assertion in the Makefile: both immediates are produced by
## the assembler from local labels, so the trap cannot drift out of position.
##
## ---- ARM A (positive): the Halo crash, minimised -------------------------
## Halo CE is fully locals-stripped: LC_DYSYMTAB nlocalsym=0, and of the 236
## defined symbols only 222 land in __text, every one a C++ COALESCED template
## instantiation — the game's own functions carry NO nlist. b4b2848 narrowed
## the field-store code-target admit to func_syms-only evidence on the premise
## that "a genuine callback target is symboled even in a locals-stripped image
## (globals survive strip -x)". For Halo that premise is false, so the admit is
## INERT: measured, ZERO of Halo's 207 in-__text field-store immediates (and
## ZERO of Civ IV Steam's 517) carry an nlist at the target. Halo's
##     movl $0x250ff8, 0x10(%ebx)      ## i386 0x2532ca and 0x256106
## therefore shipped VERBATIM into the translated dylib (0x1031e4ff /
## 0x10321c82) and the first indirect call through the field jumped to the
## stale i386 vmaddr: SIGSEGV, rip = fault addr = 0x250FF8.
## Pre-fix this arm is exit 139 (SIGSEGV); post-fix exit 0.
##
## ---- ARM B (negative): 31de727's over-relocation ------------------------
## The class b4b2848 protects against, in its own words, is an integer aliasing
## "an anonymous MID-FUNCTION `55 89 e5` run" (Civ IV boost type-registry
## rc=139). Mid-function means FALL-THROUGH REACHABLE. _mid_alias sits directly
## after 0x40 bytes of ZERO filler, so the byte before it is 0x00 — not a
## terminator, not alignment padding — i.e. execution runs into it and it is
## not a function start. Its value must survive the field round trip as the
## literal integer. Exit 9 if it was relocated.
##
## Both arms are required: ARM A alone would be satisfied by 31de727's
## prologue-only admit (which regressed Civ), ARM B alone by b4b2848's
## symbol-only admit (which strands Halo). Only entry SHAPE **and**
## function-START evidence together satisfy both.
##
## Kill switches for A/B bisection:
##   M64_NO_FIELD_FNPTR_SHAPE_EVIDENCE=1 -> ARM A goes RED (b4b2848 behaviour)
##   M64_NO_FUNCTION_START_EVIDENCE=1    -> ARM B goes RED (31de727 behaviour)

	.section __TEXT,__text,regular,pure_instructions

	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	movl	$_obj, %ebx

	## ---- ARM A: install an UNSYMBOLED handler into a field, then CALL it.
	## _local_handler is a genuine function START: the byte before it is the
	## 0x90 alignment fill of the .p2align below.
	movl	$_local_handler, 0x14(%ebx)   ## c7 43 14 imm32, code target
	calll	*0x14(%ebx)                   ## stale i386 addr faults here pre-fix
	cmpl	$0x2a, %eax
	jne	Lbad_a

	## ---- ARM B: an integer aliasing a prologue-shaped run that is NOT a
	## function start must round-trip through the field as a LITERAL.
	movl	$_mid_alias, 0x88(%ebx)
	movl	0x88(%ebx), %ecx
	cmpl	$_mid_alias, %ecx
	jne	Lbad_b

	pushl	$0
	calll	_exit
	ud2

Lbad_a:
	pushl	$8
	calll	_exit
	ud2
Lbad_b:
	pushl	$9
	calll	_exit
	ud2

	## A REAL function start: reached only by the indirect call, preceded by
	## 0x90 alignment fill. LOCAL (no .globl) so `strip -x` drops its nlist and
	## the only available evidence is structural.
	.p2align 4, 0x90
_local_handler:
	pushl	%ebp
	movl	%esp, %ebp
	movl	$0x2a, %eax
	popl	%ebp
	ret

	## NOT a function start: prologue BYTES, but preceded by zero filler that
	## falls straight into them. Also LOCAL, so no nlist rescues or condemns it.
	.p2align 4
_zfill:
	.space	0x40
_mid_alias:
	.byte	0x55, 0x89, 0xe5, 0x90, 0x90    ## push ebp; mov esp,ebp; nop; nop

	.section __DATA,__data
	.globl _obj
	.p2align 2
_obj:
	.space	0x100
