## 99_pic_push_mem — `pushl disp(%anchor)` and `pushl disp(%anchor,%idx,4)`
## through a GCC PIC anchor must read the translated slot.
##
## PvZ's libbass: `pushl key(%ebx); call _pthread_getspecific`. The PUSH_MEMv
## rewrite returned before the generic PIC-anchor rewrite, so the push kept
## `[rbx+disp]` with rbx = the TRANSLATED anchor -> it pushed code bytes
## (0x24648d48) as the key. exit 42 = both pushes read their slots.
## OFF arm: M64_NO_PIC_PUSH_MEM=1 at translate time.
	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	calll	L1
L1:
	popl	%ebx
	pushl	(_val-L1)(%ebx)             ## base form
	popl	%eax
	movl	$1, %ecx
	pushl	(_tbl-L1)(%ebx,%ecx,4)      ## indexed form
	popl	%edx
	addl	%edx, %eax
	pushl	%eax
	calll	_exit
	ud2
	.section __DATA,__data
	.p2align 2
_val:
	.long	20
_tbl:
	.long	0
	.long	22
