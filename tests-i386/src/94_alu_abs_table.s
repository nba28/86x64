## 94_alu_abs_table — ALU ops on an absolute-indexed __DATA table.
##
## Mirrors Civ IV (Steam)'s CRC-32 implementation: the table BUILD stores
## through `movl %ecx, tab(,%esi,4)` (67 89 0c b5 disp32 translated) and the
## checksum READS back through `xorl tab(,%eax,4), %edx` (67 33 14 85 disp32).
## macho-tool rebases BOTH disp32s to the image's pre-slide M64 vmaddr at
## convert time (the `[disp32+idx*scale]` handler in instruction.cc, which
## fires for these no-trailing-immediate forms). The RUNTIME slide patchers
## (objc_slide.c patch_text_abs32 + wrapper_setup.c) then add the ASLR slide
## on top — but their opcode table matched the 89 STORE and NOT the 33 XOR
## load (nor most of the single-byte ALU family), so the xor's disp stayed at
## the pre-slide M64 vmaddr while the store's was slid. Proven from the live
## Civ process: store disp slid, xor disp raw -> EXC_BAD_ACCESS KERN_INVALID
## at the preferred vmaddr on the FIRST byte checksummed (crashlog
## 2026-07-11-154748, fault 0x11fb5d1c = unslid &tab[0xff]).
##
## Exercises the single-byte ALU load/store forms with NO trailing immediate
## (the exact class the runtime slider must cover): xor/or/and/sub/cmp loads
## (33/0B/23/2B/3B), the store-direction add/xor rmw (01/31), inc (FF /0), and
## an 8B load readback. Unfixed, the first xorl reads the unslid disp32 ->
## SIGSEGV (or a wrong value if the page happens to be mapped). Exit 99 = every
## op saw the slid table. (The imm-combo forms 81/7 and C7/0 are a SEPARATE
## translate-time gap — skipped by instruction.cc's has_trailing_imm32 guard —
## and are intentionally NOT exercised here.)

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	subl	$16, %esp

	## xor-load chain (opcode 33) — the exact Civ CRC read shape
	xorl	%edx, %edx
	movl	$0, %eax
	xorl	_tab(,%eax,4), %edx       ## edx = 0x11111111
	movl	$2, %eax
	xorl	_tab(,%eax,4), %edx       ## ^= 0x33333333 -> 0x22222222

	## or-load (0B), and-load (23), sub-load (2B)
	movl	$1, %eax
	orl	_tab(,%eax,4), %edx       ## | 0x22222222 -> 0x22222222
	movl	$2, %eax
	andl	_tab(,%eax,4), %edx       ## & 0x33333333 -> 0x22222222
	movl	$0, %eax
	subl	_tab(,%eax,4), %edx       ## - 0x11111111 -> 0x11111111

	## cmp-load (3B)
	cmpl	_tab(,%eax,4), %edx       ## vs tab[0] = 0x11111111
	jne	Lbad

	## store-direction add (01) + inc (FF /0), then read back via 8B load
	movl	$3, %eax
	addl	%edx, _tab(,%eax,4)       ## tab[3] = 0x44444444 + 0x11111111
	incl	_tab(,%eax,4)             ## -> 0x55555556
	movl	_tab(,%eax,4), %ecx
	cmpl	$0x55555556, %ecx
	jne	Lbad

	## store-direction xor (31) + readback through the 8B load
	movl	$0, %eax
	xorl	%edx, _tab(,%eax,4)       ## tab[0] ^= 0x11111111 -> 0
	movl	_tab(,%eax,4), %ecx
	testl	%ecx, %ecx
	jnz	Lbad

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
_tab:
	.long	0x11111111
	.long	0x22222222
	.long	0x33333333
	.long	0x44444444
