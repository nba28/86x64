## 99_pic_anchor_imm_text_alias — an integer CONSTANT passed as a stack argument
## from PIC-anchored code must NOT be relocated just because its value aliases a
## section.
##
## The translator classifies an i386 imm32 as a pointer when its VALUE lands in
## some segment's vmaddr range, because a fixed-load-address image really does
## reach its own globals that way. The stack-argument arm of that heuristic
## deliberately accepts ANY segment ("stack-arg setup is overwhelmingly
## string/pointer args"), so it is not covered by the non-PIE MH_EXECUTE gate the
## register arms use — and a SMALL constant aliases low __TEXT very easily.
##
## Portal 2, engine.dylib i386 0x2e4d5f, CVoxelTree::CVoxelTree:
##     movl $0x1000, 0x4(%esp)          # 4096 — a page size
## became
##     lea r11,[rip-0x475d43] ; mov %r11d,0x4(%rsp)
## so CMemoryStack::Init received a CODE ADDRESS as its size. The allocator was
## then asked for image_base + 0x1460 — about 300 MB, tracking ASLR, with the low
## 12 bits always 0x460. It was SERVED, so nothing failed, nothing crashed, and no
## signal-based tool could see it; only `ABICONV_HEAP_TRACE` showed a size that
## moved between runs.
##
## The cure is the rule that already governed the zerofill case, applied without
## the zerofill restriction: PIC code NEVER embeds an absolute-address immediate,
## whatever section the value happens to alias. A live get_pc_thunk anchor proves
## the enclosing function is PIC codegen, so the immediate is a CONSTANT.
##
## The constant here is 0x3000, which lands inside this binary's own __DATA,__data
## (padded below so the alias does not depend on exact code layout) — the same
## "an integer that happens to name a mapped address" situation as Portal 2's
## 0x1000 landing in engine.dylib's __TEXT. ⚠A constant equal to a SEGMENT START
## (0x1000 here = the Mach-O header) does NOT work: there is no blob at that
## address, so nothing resolves and the guard would pass either way — the first
## version of this fixture was inert for exactly that reason. The function is unambiguously PIC: it establishes an anchor
## with call/pop and makes a real anchored read through it before passing the
## constant, so the cancel is being tested where it must apply — not in a
## non-PIC function, where a bare absolute immediate IS genuine and the
## heuristic must be kept (guards 95_abs32_imm_const, 98_abs32_imm_group).
##
## Exit 42 = the callee received 0x3000 through both the stack-slot store and the
## push form. 1 = the c7 stack-slot form was relocated. 2 = the push form was.
## 3 = the anchor itself is broken, so the arms prove nothing.
##
## Kill switch: translate with M64_PIC_ANCHOR_IMM_ZEROFILL_ONLY=1 to restore the
## old zerofill-only cancel; both arms then receive a __TEXT address.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

## int takes_two(void *ignored, int n) — returns n, so the caller can see exactly
## what arrived. Deliberately NOT anchored itself: the value must survive the
## CALLER's translation, and a plain callee keeps the oracle independent.
	.p2align 4, 0x90
_takes_two:
	movl	8(%esp), %eax
	ret

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$40, %esp
	pushl	%ebx

	## PIC anchor: ebx = vmaddr of Lpic0. Establishes this function as PIC.
	calll	Lpic0
Lpic0:
	popl	%ebx

	## A real anchored read, so the anchor is live and tracked (and so this test
	## also proves the cancel does not disturb a LEGITIMATE anchored access).
	movl	(_tag - Lpic0)(%ebx), %eax
	cmpl	$0x00C0FFEE, %eax
	jne	Lfail3

	## --- arm 1: the c7 stack-slot form, exactly Portal 2's shape -----------
	##     movl $imm32, 0x4(%esp)
	movl	$0, (%esp)
	movl	$0x3000, 4(%esp)
	calll	_takes_two
	## ⚠THE ORACLE MUST NOT SHARE THE DEFECT. `cmpl $0x3000,%eax` is the SAME
	## aliasing immediate, so the broken translation relocated the comparison
	## identically to the argument and the two matched — the first version of
	## this fixture passed both ways for exactly that reason. Build the expected
	## value by SHIFTING instead: 3 and 12 are far below __PAGEZERO's end, which
	## the heuristic excludes, so neither can be mistaken for an address.
	movl	$3, %ecx
	shll	$12, %ecx                 ## 3 << 12 = 0x3000
	cmpl	%ecx, %eax
	jne	Lfail1

	## --- arm 2: the push form of the same constant ------------------------
	pushl	$0x3000
	pushl	$0
	calll	_takes_two
	addl	$8, %esp
	movl	$3, %ecx
	shll	$12, %ecx                 ## independent oracle, as above
	cmpl	%ecx, %eax
	jne	Lfail2

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

	.section __DATA,__data
	.p2align 4
_tag:
	.long	0x00C0FFEE
	## Pad __data out so the constant 0x3000 lands INSIDE a real section's
	## vmaddr range regardless of small code-size changes above.
	.space	0x3000, 0
