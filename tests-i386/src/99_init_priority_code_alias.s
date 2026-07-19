## 99_init_priority_code_alias — GCC static-init PRIORITY immediate 0xffff
## mis-relocated in a binary STRIPPED of local text symbols (the Civ IV
## Steam shape), including the parse-ORDER inconsistency that skipped a TU's
## static ctors ("Launch in Window" NULL std::list registry crash).
##
## The i386 GCC unit-at-a-time regparm(3) static-init dispatch:
##     __GLOBAL__I_*:  mov $0xffff,%edx      ; BA FF FF 00 00 (priority 65535)
##                     mov $1,%eax           ; initialize_p = 1
##                     jmp __static_initialization_and_destruction_0
##     __static_...._0: dec %eax; jne skip
##                     cmp $0xffff,%edx      ; 81 FA FF FF 00 00
##                     jne skip
##                     <construct the TU's file-scope objects; __cxa_atexit>
## 0xffff numerically ALIASES the i386 __text vmaddr range in any binary
## whose __text spans past 0x10000 (Civ: [0x23b0, 0xdd2176) — ALL 1087 sites).
## With locals stripped, code_alias_is_constant is DISARMED, so the
## MOV_GPRv_IMMv pointer probe relocated every stub's priority to
## `lea edx,[rip+..]`; each relocated value entered relocated_ptr_imms, which
## flipped imm_bounds_relocated_table for every dispatcher `cmp $0xffff,%edx`
## parsed AFTER a stub — 1095/1096 dispatchers relocated too (ACCIDENTALLY
## consistent, ctors still ran) — but the FIRST dispatcher in sweep order
## parsed before any relocated base existed, kept the literal 0xffff, and its
## TU's ctors were silently SKIPPED -> a never-constructed std::list registry
## -> EXC_BAD_ACCESS addr=0x8 at game launch.
##
## The fix (imm32_code_alias_is_constant): a code-section-aliasing imm32 is a
## CONSTANT on both the symboled and the locals-stripped paths — for the
## MOV/PUSH/ADD register-imm family, the CMP_GPRv_IMMz twin, the [abs32]-store
## trailing imm and the stack-arg store alike — unless POSITIVE fn-pointer
## evidence exists at the value (nlist symbol, or the `55 89 e5` prologue).
##
## Three assertions (distinct failure exits):
##   exit 10 — ctor1 skipped: dispatcher BEFORE stub in sweep order (the Civ
##             crash shape: literal cmp vs relocated mov). RED pre-fix.
##   exit 11 — genuine fn-POINTER immediate broken: `movl $_helper,%ecx;
##             call *%ecx` where _helper has the 55 89 e5 prologue must STILL
##             relocate on the stripped path (no over-suppression).
##   exit 12 — ctor2 skipped: dispatcher AFTER the stub (+ after the relocated
##             _helper base, which imm_bounds_relocated_table would otherwise
##             latch onto) — guards the CMP-side classification.
## Exit 99 = all three hold. The binary is `strip -x`ed (Makefile rule) and
## the Makefile asserts 0xffff really falls inside __text at build time.

	.section __TEXT,__text,regular,pure_instructions

## Dispatcher FIRST in the section: its `cmp $0xffff,%edx` parses before ANY
## relocatable immediate exists (the Civ first-TU shape).
	.p2align 4, 0x90
_disp1:
	decl	%eax
	jne	Ld1out
	cmpl	$0xffff, %edx
	jne	Ld1out
	movl	$0x5a5, _sentinel1	# construction block (imm < 0x1000: never probed)
Ld1out:
	ret

## A real function with the canonical i386 frame-setup prologue (55 89 e5) —
## the positive-evidence target for the fn-pointer immediate control. Its
## address (< 0xffff) also becomes a relocated base in relocated_ptr_imms,
## arming the imm_bounds_relocated_table trap for _disp2's cmp below.
	.p2align 4, 0x90
_helper:
	pushl	%ebp
	movl	%esp, %ebp
	movl	$0x77, %eax
	popl	%ebp
	ret

	.globl _main
	.p2align 4, 0x90
_main:
	subl	$16, %esp

	## --- 1: THE CIV SHAPE, dispatcher-before-stub ---
	movl	$0xffff, %edx		# BA FF FF 00 00 — the priority immediate
	movl	$1, %eax
	calll	_disp1
	movl	_sentinel1, %ecx
	cmpl	$0x5a5, %ecx
	jne	Lbad0

	## --- 2: genuine fn-pointer immediate (same MOV_GPRv_IMMv iform) ---
	movl	$_helper, %ecx		# prologue target: must STILL relocate
	calll	*%ecx
	cmpl	$0x77, %eax
	jne	Lbad1

	## --- 3: stub-before-dispatcher (CMP-side classification) ---
	movl	$0xffff, %edx
	movl	$1, %eax
	calll	_disp2
	movl	_sentinel2, %ecx
	cmpl	$0x5a5, %ecx
	jne	Lbad2

	movl	$99, %ecx
	jmp	Lout
## Distinct failure exits so a FAIL names the broken half.
Lbad0:	movl	$10, %ecx
	jmp	Lout
Lbad1:	movl	$11, %ecx
	jmp	Lout
Lbad2:	movl	$12, %ecx
	jmp	Lout
Lout:
	movl	%ecx, (%esp)
	calll	_exit
	ud2

## Dispatcher AFTER the stubs: pre-fix its cmp parsed with 0xffff (and
## _helper) already in relocated_ptr_imms -> relocated -> accidentally
## consistent; post-fix BOTH sides stay the literal value.
	.p2align 4, 0x90
_disp2:
	decl	%eax
	jne	Ld2out
	cmpl	$0xffff, %edx
	jne	Ld2out
	movl	$0x5a5, _sentinel2
Ld2out:
	ret

	## PAD: extend __text well past 0xffff so the priority immediate is
	## guaranteed inside the section's vmaddr range (suite binaries' __text
	## starts ~0x1f40; the Makefile rule asserts containment at build time
	## and errors loudly if layout drifts).
	.space	0xf400, 0x90

	.section __DATA,__data
	.p2align 2
_sentinel1:
	.long	0
_sentinel2:
	.long	0
