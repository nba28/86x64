## code_thunk_entry.s — fixture for code_thunk_entry_test.sh
## (core: ParseEnv::code_target_has_entry_evidence, adjustor-thunk entry shape.)
##
## THE BUG THIS GUARDS AGAINST.  The CODE-ENTRY gate demotes a __DATA word that
## aliases an instructions section unless the target carries positive
## function-ENTRY evidence.  Its first two evidence forms — an nlist at the
## value, or the `55 89 e5` frame-setup prologue — miss the i386 C++ ABI
## ADJUSTOR THUNK: the compiler emits one per multiple-inheritance or
## covariant-return override, STORES IT IN A VTABLE, and gives it neither a
## surviving symbol (it is local, so `strip -x` removes it) nor a frame setup —
## a thunk is just `add|sub $imm, disp8(%esp)` + tail-`jmp`.
##
## Measured on the real i386 Civ IV: without the thunk shape the gate demoted
## 7883 genuine vtable slots, which would then have kept i386 addresses and
## sent every such virtual call to an unmapped address.
##
## THE FIXTURE is Civ's shape: a locals-stripped, reloc-less, fixed-address
## (-no_pie) i386 exec whose sentinel-bracketed __DATA word is patched with the
## plain integer value of an adjustor-thunk ENTRY.  That word MUST be treated as
## a pointer and relocated.  The 0xA5A5xxxx sentinels are >= 0x80000000 and so
## are never pointer-detected themselves.

	.section __TEXT,__text,regular,pure_instructions

	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp
	call	_thunk_fn              ## keep the thunk referenced/reachable
	movl	$0, (%esp)
	call	_exit

	.p2align 4, 0x90
_thunk_fn:                         ## local + stripped -> no func_syms nlist
	addl	$-8, 4(%esp)           ## 83 44 24 04 f8   adjust `this` in place
	jmp	_real_fn               ## e9/eb rel        tail-jmp to the override

	.p2align 4, 0x90
_real_fn:
	pushl	%ebp                   ## the thunk's target DOES have a prologue;
	movl	%esp, %ebp             ## the thunk ENTRY itself does not, which is
	leave                          ## exactly what the gate used to miss
	ret

	.section __DATA,__data
	.globl _g_vslot
	.p2align 2
_g_vslot:
	.long	0xA5A51111             ## sentinel (>= 0x80000000: never pointer-detected)
	.long	0x00000000             ## <- harness patches _thunk_fn here
	.long	0xA5A52222             ## sentinel
