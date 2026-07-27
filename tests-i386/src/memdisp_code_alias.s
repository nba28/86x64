## memdisp_code_alias.s — fixture for memdisp_code_alias_test.sh
## (core: instruction.cc memdisp_code_alias_is_constant;
##  Civ IV STEAM "launch in window" SIGBUS, 2026-07-28.)
##
## THE BUG.  For a fixed-load-address (-no_pie MH_EXECUTE) i386 image,
## instruction.cc treats a `[base + disp32]` memory displacement as an ABSOLUTE
## GLOBAL-TABLE vmaddr whenever disp32 lands inside any real segment, and
## relocates it to the translated layout.  With a base register live, though,
## disp32 is far more often an ordinary INTEGER — a struct-member offset, an
## array extent, a loop bound — and on a large i386 image __text spans a range
## (Civ IV: 0x23b0 .. 0xdd2176) that ordinary small integers alias constantly.
##
## Civ IV's GMemory free-list builder allocates a 0x2800-byte chunk and computes
## its one-past-the-last-node terminator with
##      lea 0x27ec(%ecx), %edx          ## 0x27ec == 0x2800 - 0x14
## 0x27ec aliases __text, so the arm relocated it: the translated code became
## `lea r11,[rip+..]; lea (%rcx,%r11),%edx` -> end = block + 0x599400d.  The
## 0x14-stride iterator never reached the terminator, the node-zeroing loop ran
## off the end of the mmap'd chunk and died writing the first non-writable page
## (KERN_PROTECTION_FAILURE at 0x10382008, 8 bytes past a 1540K rwx region whose
## neighbour is a read-only file mapping).
##
## THE DISCRIMINATOR.  Exactly the one the IMMEDIATE family already uses
## (imm32_code_alias_is_constant): a value that aliases an instructions-flagged
## section is an integer CONSTANT unless there is positive function-ENTRY
## evidence at it (an nlist, the `55 89 e5` prologue, or an adjustor-thunk
## shape).  That classifier's own contract requires the answer be a pure
## function of value+image, "never of parse ORDER or of WHICH IFORM CARRIES the
## immediate" — before the fix, 0x27ec was a CONSTANT as `mov $0x27ec,%edx` but
## a POINTER as `lea 0x27ec(%ecx),%edx`.
##
## THE FIXTURE has both polarities so the gate cannot be "fixed" by simply
## disarming the whole heuristic:
##
##   POSITIVE (must be PRESERVED): `leal 0x11223344(%ecx), %edx` — the harness
##   patches the disp32 to _probe_fn+3, a real instruction boundary MID-function
##   with no nlist (local + `strip -x`) and no entry shape.  Civ's shape.
##
##   NEGATIVE control (must still be RELOCATED): `movl %eax, _g_table(%edx)` —
##   a genuine absolute global-table store into __DATA, the photocd/Halo case
##   this heuristic exists for.  __DATA is not an instructions section, so the
##   code-alias gate must not touch it.

	.section __TEXT,__text,regular,pure_instructions

	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp
	call	_probe_fn              ## keep _probe_fn referenced/reachable
	call	_uses_disp
	movl	$0, (%esp)
	call	_exit

## The function whose MID-BODY address the positive case's disp32 aliases.
## Local + stripped -> no func_syms nlist, and have_local_text_syms stays false
## (the Civ IV Steam shape).
	.p2align 4, 0x90
_probe_fn:
	pushl	%ebp                   ## +0  55        \ frame-setup prologue =
	movl	%esp, %ebp             ## +1  89 e5     / positive ENTRY evidence
	subl	$0x18, %esp            ## +3  83 ec 18  <- harness targets THIS:
	leave                          ##               real instruction boundary,
	ret                            ##               mid-function, no evidence

	.p2align 4, 0x90
_uses_disp:
	pushl	%ebp
	movl	%esp, %ebp
	xorl	%ecx, %ecx
	xorl	%edx, %edx
	## POSITIVE: base register live + disp32 patched to a mid-__text address.
	## Encodes as 8d 91 44 33 22 11 (mod=10, reg=edx, r/m=ecx, disp32).
	leal	0x11223344(%ecx), %edx
	## NEGATIVE control: a real absolute __DATA table base with a live base
	## register — must keep being relocated.
	movl	%edx, _g_table(%ecx)
	popl	%ebp
	ret

	.section __DATA,__data
	.globl _g_table
	.p2align 2
_g_table:
	.long	0
	.long	0
	.long	0
	.long	0
