## 92_ptr_imm_field_store — `movl $&writable_data, disp(%reg)` pointer
## IMMEDIATE stored through a GENERAL register base in a non-PIE i386 exec.
##
## Mirrors the Civ IV (Steam) pointer-immediate wall: the game stores the
## interior pointer of an ANONYMOUS ZEROED static string block into ~97
## object fields (`movl $0x145ea60, 0x18(%ebx)`, c7 43 18 imm32; the block
## carries no symbol and no content — consumers read its zero length field
## via `movzbl -2(%eax)`). The general-base c7 heuristic admitted only
## CONSTANT-section targets and the vtable double-indirection probe, both of
## which fail for anonymous zeroed __data — so the raw i386 __data address
## shipped unrelocated into the translated field and the first consumer
## dereferenced the unmapped original address (EXC_BAD_ACCESS).
##
## The fix admits the store under the SAME fixed-load-image gate that
## already justifies the MOV/PUSH/ADD-to-REG data-immediate family (only a
## non-PIE MH_EXECUTE bakes absolute data addresses), narrowed structurally:
## value 4-aligned AND file-backed writable data (zerofill spans excluded —
## the section.cc (2c) lesson; __OBJC excluded by the predicate). The
## MOV_MEMv_IMMz transform then emits the slide-correct lea+store.
##
## Exit 0 iff the stored field equals the independently-relocated address
## (MOV_GPRv_IMMv ground truth), the Civ-style -2 deref reads the zeroed
## block, and a small-integer field store stays verbatim.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	## base object pointer via the covered reg-imm path
	movl	$_obj, %ebx

	## G4 core: pointer-to-anonymous-zeroed-__data stored into a field
	movl	$_blk+8, 0x8(%ebx)          ## c7 43 08 imm32

	## negative control: small integer field store stays verbatim
	movl	$0x30, 0xc(%ebx)

	## field readback vs covered ground truth
	movl	0x8(%ebx), %ecx
	movl	$_blk+8, %eax               ## covered: lea eax,[rip+...]
	cmpl	%ecx, %eax
	jne	Lbad

	## Civ-style consumer: length field 2 bytes before the pointer is zero
	movzbl	-2(%ecx), %edx
	testl	%edx, %edx
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
	.space	16                          ## the object written at runtime

	## ANONYMOUS zeroed block (no .globl — like Civ's static string block);
	## the stored pointer targets its interior at +8
	.p2align 4
_blk:
	.long	0, 0, 0, 0
