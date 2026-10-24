## 99_push_pop_esp — `push %esp` stores esp from BEFORE the decrement, and
## `pop %esp` loads esp from the slot with no increment after. The 4-byte
## push/pop expansion stored esp-4 and left esp 4 high. Exit 42 / 7.
## OFF arm: M64_NO_PUSH_POP_ESP=1 at translate time.
	.text
	.globl _main
_main:
	movl	%esp, %eax
	pushl	%esp
	popl	%ecx                    ## the esp before the push
	cmpl	%eax, %ecx
	jne	1f
	pushl	%esp
	popl	%esp                    ## esp unchanged
	cmpl	%eax, %esp
	jne	1f
	pushl	$42
	calll	_exit
1:	movl	%eax, %esp
	pushl	$7
	calll	_exit
