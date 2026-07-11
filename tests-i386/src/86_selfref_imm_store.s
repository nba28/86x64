## 86_selfref_imm_store — same-object SELF-REFERENTIAL pointer stored by a
## `movl $abs32, abs32` immediate store (MOV_MEMv_IMMz, ABS32 destination) in a
## non-PIE i386 executable.
##
## Mirrors the GCC-4 -O2 inlined empty-container header init that gates Civ IV
## Steam's boost.python converter registry (std::set<registration> static init,
## _Rb_tree_impl::_M_initialize):
##
##   _M_header._M_left  = &_M_header;   ->  movl $_hdr, _hdr+8   (c7 05 disp32 imm32)
##   _M_header._M_right = &_M_header;   ->  movl $_hdr, _hdr+12
##
## The M32->M64 transform translates this to `mov dword [rip+disp], imm32`,
## re-emitting the pointer as a RAW imm32 in __text (instruction.cc
## XED_IFORM_MOV_MEMv_IMMz, abs-dest arm). Unlike every other pointer-immediate
## form (MOV_GPRv/PUSH/ADD/CMP -> rip-relative lea), that embedded imm32 is NOT
## re-detected by the M64 re-parses of the later pipeline stages
## (modify --insert / static-interpose / convert): the rip-relative DESTINATION
## re-resolves against each stage's fresh layout, but the imm32 keeps the
## transform-stage address. When a later stage shifts __DATA (convert grew
## __TEXT by a page on Civ), every such store goes stale by exactly the shift:
## Civ's registry header ended up with _M_left/_M_right == &_M_header - 0x1000,
## so _M_insert_unique's `__j == begin()` leftmost guard failed and
## _Rb_tree_decrement dereferenced the empty header's NULL parent (crash 0x4).
##
## The fix rewrites the abs-dest arm like the reg-dest arm: lea r11,[rip+target];
## mov [rip+dest], r11d — no pointer immediate survives in __text at all.
##
## Ground truth here is `movl $_hdr, %eax` (MOV_GPRv_IMMv -> lea eax,[rip+disp]),
## which is re-parse-safe by construction. Exit code 0 iff both stored fields
## equal the ground-truth address.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	## the inlined _M_initialize shape (self-referential immediate stores)
	movl	$_hdr, _hdr+8            ## _M_left  = &_M_header
	movl	$_hdr, _hdr+12           ## _M_right = &_M_header

	## independent recomputation of &_hdr via the lea-based immediate path
	movl	$_hdr, %eax

	cmpl	_hdr+8, %eax             ## left == &header ?
	jne	Lbad
	cmpl	_hdr+12, %eax            ## right == &header ?
	jne	Lbad
	pushl	$0
	calll	_exit
	ud2

Lbad:
	## exit code = 1 (a stale store: field != &header)
	pushl	$1
	calll	_exit
	ud2

	.section __DATA,__data
	.globl _hdr                      ## exported so selfref_imm_stage_shift_test.sh
	                                 ## can locate the object in the final dylib
	.p2align 2
_hdr:
	.long	0                        ## _M_color
	.long	0                        ## _M_parent (empty tree: NULL)
	.long	0                        ## _M_left   (must become &_hdr)
	.long	0                        ## _M_right  (must become &_hdr)
