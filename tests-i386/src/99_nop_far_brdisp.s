## 99_nop_far_brdisp — a NOP-substituted far call/jmp must not get its branch
## target back.
##
## Far call/jmp (0x9A/0xEA ptr16:32) has no 64-bit form; it is always data the
## linear sweep decoded as code, so the M32->M64 copy ctor replaces it with NOPs
## and clears brdisp. But the ctor had already registered a DEFERRED resolve of
## brdisp. When the parser-computed target is a FORWARD blob, the Resolver fills
## brdisp after the clear, and Emit dies patching a branch displacement into a
## NOP: "xed_patch_brdisp: failed to patch instruction ... iform NOP_90".
##
## MEASURED, Portal 2 libcef.dylib: 18 far-transfer substitutions; the one at
## rebased src 0x84049 aborted the whole translate.
##
## Exit 42 = the image translated and ran. Before the fix the translate aborts
## (no binary at all).

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	$42
	calll	_exit
	ud2

	## Never executed: data the linear sweep decodes as `jmp far 0004:<rel>`,
	## with the "displacement" landing on the forward label below.
Lfar:
	.byte	0xea
	.long	Lfwd - Lfar_end
	.word	4
Lfar_end:
	nop
	nop
Lfwd:
	ret
