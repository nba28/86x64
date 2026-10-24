# Byte-level fixture for rex-seg-null-trap (translate-time only, never run).
#   2e 89 ec      a reg-only %esp op behind a segment override: the widening
#                 REX.W must follow the 2e, or the CPU ignores it
#   call/jmp *_fp an absolute 4-byte fn-ptr slot: MACHO_NULL_TRAP=1 must trap
#                 both like the lower_mem forms
	.text
	.globl _main
_main:
	pushl %ebp
	movl %esp, %ebp
	.byte 0x2e
	movl %ebp, %esp
	call *_fp
	jmp *_fp
_tgt:
	ret
	.data
_fp:	.long _tgt
