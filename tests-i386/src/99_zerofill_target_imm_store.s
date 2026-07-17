## 99_zerofill_target_imm_store — `movl $&__bss_global, disp(%reg)` pointer
## IMMEDIATE (whose VALUE is a ZEROFILL __bss/__common address) stored through
## a GENERAL register base in a non-PIE i386 executable.
##
## This is the OPEN sibling of the 92_ptr_imm_field_store / 9510f29 fix. That
## fix relocates a genbase mem-dest pointer immediate ONLY when the immediate
## VALUE lands in FILE-BACKED writable __DATA (`!vmaddr_in_zerofill(value)`).
## The Civ IV census (todo_gaps gap (2)) found 5158 sibling sites whose stored
## pointer VALUE is a __bss/__common (ZEROFILL) address, e.g.
## `movl $0x154d828,(%rax)` — deliberately EXCLUDED by that gate, so the raw
## i386 zerofill address ships VERBATIM into __text. Once translated to a dylib
## (which slides and whose sections move as __cstring/etc. expand), that raw
## immediate points at the stale i386 __bss vmaddr -> a later deref reads/writes
## unmapped memory (latent EXC_BAD_ACCESS).
##
## ★CROSS-FORM CLUE the fix leans on: the reg-dest form of the SAME pointer DOES
## relocate today — `movl $&__bss_global, %reg` (MOV_GPRv_IMMv, instruction.cc
## ~896) admits ANY non-pagezero/linkedit segment INCLUDING zerofill, with NO
## `!vmaddr_in_zerofill` exclusion. So the reg path is the re-parse-safe GROUND
## TRUTH here and the mem-dest path is the one under test. A correct translator
## must agree between the two forms.
##
## Structure mirrors 92_ptr_imm_field_store exactly, but the pointer target is a
## __bss (zerofill) symbol instead of a file-backed __data one:
##   - store &_bss_blk (a zerofill address) into an object field via genbase;
##   - read the field back and compare against the covered reg-imm ground truth;
##   - actually WRITE THROUGH the stored pointer and read it back (a raw stale
##     i386 __bss address is unmapped -> SIGSEGV / mismatch);
##   - a small-integer field store stays verbatim (negative control).
##
## Exit 0 iff the stored field equals the relocated address, the round-trip
## through it works, and the integer control is untouched. Pre-fix: the field
## holds the raw i386 __bss address while the reg-imm ground truth relocates
## -> the compare fails (exit 1) or the deref faults.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	## base object pointer via the covered reg-imm path
	movl	$_obj, %ebx

	## G core: pointer-to-__bss (ZEROFILL) stored into a field through a
	## general base register — the gap (2) form `movl $&zerofill, disp(%reg)`
	movl	$_bss_blk, 0x8(%ebx)       ## c7 43 08 imm32 (imm = __bss vmaddr)

	## negative control: small integer field store stays verbatim
	movl	$0x30, 0xc(%ebx)

	## field readback vs covered ground truth (reg-dest relocates today)
	movl	0x8(%ebx), %ecx
	movl	$_bss_blk, %eax            ## covered: lea eax,[rip+...]
	cmpl	%ecx, %eax
	jne	Lbad

	## round-trip THROUGH the stored pointer: it must address mapped __bss.
	## Write a sentinel via the field-held pointer, read it back via the
	## independently-relocated ground-truth pointer. A stale raw i386 __bss
	## address is unmapped -> this faults or reads garbage.
	movl	$0x5a5a5a5a, (%ecx)        ## *stored_ptr = sentinel
	movl	(%eax), %edx              ## reload via ground-truth ptr
	cmpl	$0x5a5a5a5a, %edx
	jne	Lbad

	## negative control intact?
	cmpl	$0x30, 0xc(%ebx)
	jne	Lbad

	pushl	$0
	calll	_exit
	ud2

Lbad:
	pushl	$1
	calll	_exit
	ud2

	.section __DATA,__data
	.globl _obj
	.p2align 2
_obj:
	.space	16                        ## the object written at runtime

	## ZEROFILL (__bss) block — the stored pointer targets it. No file bytes;
	## its address is a genuine zerofill vmaddr (the gap (2) discriminator).
	.zerofill __DATA,__bss,_bss_blk,16,4
