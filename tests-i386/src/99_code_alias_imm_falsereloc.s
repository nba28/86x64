## 99_code_alias_imm_falsereloc — an INTEGER CONSTANT whose value happens to
## ALIAS a __text address that contains function-PROLOGUE bytes (55 89 e5)
## must NOT be relocated when stored into a field. Regression guard for the
## 31de727 over-relocation that turned the Civ IV (Steam) rc=139 boost-registry
## STARTUP crash from ~15-20% intermittent into ~100% deterministic.
##
## ROOT (bisected — flips RED at 31de727, GREEN on 7542bc8/ffba14a/4e964cc/
## 3621bf3): 31de727 admits a CODE-target pointer IMMEDIATE stored into a field
## through a general base (`movl $imm32, disp(%reg)`) whenever the value aliases
## an instructions-flagged section AND imm32_code_alias_is_constant() returns
## false. That predicate returns false (=> "it's a function pointer, relocate
## it") when the aliased __text address carries the i386 frame prologue bytes
## `55 89 e5` (push ebp; mov esp,ebp) — the stripped-binary ProcPtr recovery.
## But an INTEGER CONSTANT (a hash multiplier / mask / enum / small size) that
## merely aliases such an address is NOT a function pointer: relocating it
## rewrites the store to `lea r11,[rip+slid]; mov [mem],r11d`, replacing the
## integer with a slide-dependent address. In Civ's ~16MB __text, a boost
## type-registry hash/index constant aliases a coincidental `55 89 e5` run ->
## the constant is corrupted -> the bucket index / node link computes wrong ->
## the per-token bucket walk reads a NULL head and faults
## `movl 0x88(%rbx),%ebx` (rbx=0, addr=0x88) — the exact deterministic crash.
##
## LOCALS-STRIPPED (Civ Steam shape) so imm32_code_alias_is_constant's symboled
## discriminator is disarmed and only the prologue-byte evidence remains. The
## build rule asserts 0x1f90 really aliases __text AND that 55 89 e5 sits at it,
## so the alias trap is armed (fail the BUILD loudly if layout drifts).
##
## A/B: store the integer 0x1f90 into a field, read it back, require it equals
## the LITERAL 0x1f90. Exit 0 = preserved (correct). exit 8 = wrongly relocated
## (the 31de727 regression: the field holds a slid __text address, not 0x1f90).
## A negative control stores a genuine data-symbol pointer that MUST still
## relocate (so the fix must narrow, not blanket-disable, the code-target admit).

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	movl	$_node, %ebx

	## ---- A: integer constant aliasing __text @ 55 89 e5 — must stay literal
	movl	$0x1f90, 0x88(%ebx)          ## c7 83 88 00 00 00  90 1f 00 00
	movl	0x88(%ebx), %ecx
	cmpl	$0x1f90, %ecx                ## must still be the literal integer
	jne	Lbad_a

	## ---- B (negative control): a genuine DATA-symbol pointer stored into a
	## field MUST still relocate (the 92_ptr_imm_field_store contract) — the fix
	## must not blanket-disable code/data-target field stores.
	movl	$_dtarget, 0x8(%ebx)
	movl	0x8(%ebx), %ecx
	movl	$_dtarget, %eax              ## reg-imm ground truth (relocated)
	cmpl	%ecx, %eax
	jne	Lbad_b
	movl	(%eax), %edx                 ## deref: reads _dtarget's sentinel word
	cmpl	$0x0c0ffee0, %edx
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

	## The prologue-byte marker MUST land at vmaddr 0x1f90 (asserted by the build
	## rule). 0x40 of padding after _main's short body places it there; if the
	## body length drifts the build rule fails and prints the actual address.
	.p2align 4
_pad:
	.space 0x40
	.globl _marker
_marker:
	.byte 0x55, 0x89, 0xe5, 0x90, 0x90    ## push ebp; mov esp,ebp; nop nop

	.section __DATA,__data
	.globl _node
	.p2align 4
_node:
	.space 0x100

	.p2align 2
	.globl _dtarget
_dtarget:
	.long 0x0c0ffee0                      ## sentinel the negative control derefs
