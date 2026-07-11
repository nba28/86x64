## 95_abs32_imm_const — integer imm32 constants that ALIAS the translated
## image's pre-slide vmaddr window must survive load untouched.
##
## Mirrors the Civ IV (Steam) boost.python wall's root cause: the runtime
## slide patchers (objc_slide.c patch_text_abs32 pass 2 + wrapper_setup.c)
## byte-scanned __text for `c7 /0 ... imm32` mov-imm stores and slid ANY
## imm32 that fell inside the image span [vmaddr_lo,vmaddr_hi). But since
## instruction.cc rewrites every RECOGNIZED pointer imm32 to a slide-correct
## lea+store at transform time, the C7 imm32s remaining in a fresh
## translation are integer CONSTANTS — on Civ the scan slid 33 of them
## (four MD5 IVs 0x10325476 among them) and, worse, 5 PHANTOM matches that
## straddled real instructions corrupted the code itself (a call-sim
## `jmp rel32` tail `..c7 45 00` + the landing `movl %eax,disp32(%rip)`
## patched as `mov [rbp+0],imm32` -> landing decayed to garbage -> the
## boost.python registration crash, three ASLR faces).
##
## The fix: macho-tool emits __DATA,__86x64_abs32 (the EXACT table of 4-byte
## __TEXT fields that hold pre-slide absolute addresses); the runtime walks
## the table and never scans. This test plants in-window-looking constants
## in all three C7 shapes the scanners matched (rsp-form stack arg,
## rbp+disp8 local, rip-dest global store) and fails if any got "slid".
## The values are NOT i386 image addresses, so the translator correctly
## ships them verbatim — but 0x100001xx lands inside the M64 image span
## (translated images rebase to 0x10000000), which is exactly the alias
## trap. Exit 99 = every constant read back intact.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp

	## rsp-form stack-arg store (c7 04 24 imm32)
	movl	$0x10000123, (%esp)
	## rbp+disp8 local store (c7 45 f8 imm32)
	movl	$0x10000456, -8(%ebp)
	## absolute global store (i386 c7 05 abs32 imm32 -> M64 rip-dest c7 05)
	movl	$0x10000789, _gslot

	movl	(%esp), %eax
	cmpl	$0x10000123, %eax
	jne	Lbad
	movl	-8(%ebp), %eax
	cmpl	$0x10000456, %eax
	jne	Lbad
	movl	_gslot, %eax
	cmpl	$0x10000789, %eax
	jne	Lbad

	movl	$99, %ecx
	jmp	Lout
Lbad:
	movl	$1, %ecx
Lout:
	movl	%ecx, (%esp)
	calll	_exit
	ud2

	.section __DATA,__data
	.p2align 2
_gslot:
	.long	0
