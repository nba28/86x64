## 99_identity_nop_anchor — an assembler's in-function alignment filler
## (`lea 0(%esi),%esi`) rewrites the PIC anchor register with itself; taken for
## a write, it ended the anchor and the next anchored load kept its raw i386
## displacement. Exit 42.
## OFF arm: M64_NO_IDENTITY_NOP=1 at translate time.
	.text
	.globl _main
_main:
	calll	L0
L0:	popl	%esi
	.byte	0x8d, 0xb6, 0, 0, 0, 0     ## leal 0x0(%esi),%esi
	.byte	0x89, 0xf6                 ## movl %esi,%esi
	movl	Lval-L0(%esi), %eax
	pushl	%eax
	calll	_exit
	.data
Lval:	.long	42
