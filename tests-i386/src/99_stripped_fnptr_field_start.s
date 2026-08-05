## 99_stripped_fnptr_field_start — a code-target imm32 stored into a struct
## FIELD through a general base register must be relocated when, and only when,
## it is unambiguously a function-pointer install.
##
## FOUR ARMS, one fixture. Every marker is a LOCAL label resolved by the
## assembler and the binary is `strip -x`'d (the Halo CE / Civ IV Steam /
## iPhoto shape), so no symbol survives at any of the four values: the
## translator sees only integers and must discriminate them STRUCTURALLY.
## Deliberately no __text-geometry assertion — the immediates come from local
## labels, so the traps cannot drift out of position.
##
## ---- ARM A (positive): the Halo crash, minimised -------------------------
## Halo CE is fully locals-stripped: LC_DYSYMTAB nlocalsym=0, and of the 236
## defined symbols only 222 land in __text, every one a C++ COALESCED template
## instantiation — the game's own functions carry NO nlist. b4b2848 narrowed
## the field-store code-target admit to func_syms-only evidence on the premise
## that "a genuine callback target is symboled even in a locals-stripped image
## (globals survive strip -x)". For Halo that premise is false, so the admit is
## INERT: ZERO of Halo's 207 in-__text field-store immediates carry an nlist at
## the target (Civ IV Steam: zero of 517; iPhoto has 7 defined symbols total).
## Halo's `movl $0x250ff8, 0x10(%ebx)` (i386 0x2532ca / 0x256106) therefore
## shipped VERBATIM into the translated dylib and the first indirect call
## through the field jumped to the stale i386 vmaddr: SIGSEGV, rip = 0x250FF8.
##
## ---- ARM B (negative): 31de727's over-relocation ------------------------
## The class b4b2848 protects against is an integer aliasing "an anonymous
## MID-FUNCTION 55 89 e5 run" — mid-function = FALL-THROUGH REACHABLE.
## _mid_alias sits directly after 0x40 bytes of ZERO filler, so the byte before
## it is 0x00: not a terminator, not padding. Must stay LITERAL.
##
## ---- ARM C (positive): MULTI-BYTE NOP padding ---------------------------
## Most alignment padding is not 0x90 but the multi-byte NOP family, and
## `0f 1f 80 00 00 00 00` / `66 0f 1f 84 00 00 00 00 00` END IN 0x00 — so a
## "look at the previous byte" test misses them. MEASURED on iPhoto that miss
## split ONE eight-slot handler table (installs into +0x8..+0x24 at
## 0x412d44..0x412d78) into 2 admitted and 6 refused purely by which NOP
## encoding happened to precede each target, and lost 32 of 42 genuine targets
## image-wide. _mbnop_handler is preceded by an explicit 7-byte `nopl 0(%eax)`.
##
## ---- ARM D (negative): the PAGE-MULTIPLE ambiguity refusal ---------------
## Structure cannot separate `obj->cap = 0x100000` from `obj->fn = &f` when a
## real function genuinely begins at 0x100000 — and iPhoto contains exactly
## that collision (0x3c49ff stores 0x100000 into a capacity field while 0x3c4a06
## passes the SAME value as a malloc size, and iPhoto's 0x100000 is a properly
## padded genuine function entry). The tie-break is asymmetric risk: refusing a
## real fn-ptr costs a missed relocation (status quo, inert until called);
## admitting an integer CORRUPTS a live constant. So a 4 KiB-multiple value is
## refused. _page_handler is a REAL function start at a page boundary and its
## address must nonetheless stay LITERAL. (Measured cost of the refusal on real
## images: Halo 0 of 4 admits, Civ IV 0 of 349, iPhoto 2 of 42 — and both
## iPhoto refusals are verified integers.)
##
## Arms A+C alone would be satisfied by 31de727's prologue-only admit (which
## regressed Civ); arms B+D alone by b4b2848's symbol-only admit (which strands
## Halo). Only entry SHAPE **and** function-START evidence **and** the
## page-multiple refusal satisfy all four.
##
## Kill switches for A/B bisection:
##   M64_NO_FIELD_FNPTR_SHAPE_EVIDENCE=1 -> arms A,C go RED (b4b2848 behaviour)
##   M64_NO_FUNCTION_START_EVIDENCE=1    -> arm  B  goes RED (31de727 behaviour)

	.section __TEXT,__text,regular,pure_instructions

	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	movl	$_obj, %ebx

	## ---- ARM A: unsymboled handler preceded by 0x90 align fill -> CALL it.
	movl	$_local_handler, 0x14(%ebx)   ## c7 43 14 imm32, code target
	calll	*0x14(%ebx)                   ## stale i386 addr faults here pre-fix
	cmpl	$0x2a, %eax
	jne	Lbad_a

	## ---- ARM B: prologue bytes that something FALLS THROUGH into -> literal.
	movl	$_mid_alias, 0x88(%ebx)
	movl	0x88(%ebx), %ecx
	cmpl	$_mid_alias, %ecx
	jne	Lbad_b

	## ---- ARM C: unsymboled handler preceded by a MULTI-BYTE NOP -> CALL it.
	movl	$_mbnop_handler, 0x18(%ebx)
	calll	*0x18(%ebx)
	cmpl	$0x3b, %eax
	jne	Lbad_c

	## ---- ARM D: a REAL function start at a PAGE boundary -> refused, literal.
	movl	$_page_handler, 0x8c(%ebx)
	movl	0x8c(%ebx), %ecx
	cmpl	$_page_handler, %ecx
	jne	Lbad_d

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
Lbad_c:
	pushl	$10
	calll	_exit
	ud2
Lbad_d:
	pushl	$11
	calll	_exit
	ud2

	## A REAL function start, reached only by the indirect call and preceded by
	## 0x90 alignment fill. LOCAL (no .globl) so `strip -x` drops its nlist and
	## the only available evidence is structural.
	.p2align 4, 0x90
_local_handler:
	pushl	%ebp
	movl	%esp, %ebp
	movl	$0x2a, %eax
	popl	%ebp
	ret

	## A REAL function start preceded by an explicit 7-byte multi-byte NOP,
	## `nopl 0(%eax)` — note it ENDS IN 0x00, so only a padding-aware start test
	## sees it.
	.byte	0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00
_mbnop_handler:
	pushl	%ebp
	movl	%esp, %ebp
	movl	$0x3b, %eax
	popl	%ebp
	ret

	## NOT a function start: prologue BYTES, but preceded by zero filler that
	## falls straight into them. Also LOCAL, so no nlist rescues or condemns it.
	.p2align 4
_zfill:
	.space	0x40
_mid_alias:
	.byte	0x55, 0x89, 0xe5, 0x90, 0x90    ## push ebp; mov esp,ebp; nop; nop

	## A REAL function start that lands on a PAGE boundary — indistinguishable
	## from a size/capacity constant, so it must be refused.
	.p2align 12
_page_handler:
	pushl	%ebp
	movl	%esp, %ebp
	movl	$0x4c, %eax
	popl	%ebp
	ret

	.section __DATA,__data
	.globl _obj
	.p2align 2
_obj:
	.space	0x100
