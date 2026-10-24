## 99_enter_flags — `enter` leaves EFLAGS alone; its expansion used `sub` for
## the frame size, which rewrote CF. Exit 42 / 7.
## OFF arm: M64_NO_ENTER_LEA=1 at translate time.
	.text
	.globl _main
_main:
	movl	$1, %eax
	cmpl	$2, %eax                ## CF=1
	enter	$16, $0
	jb	1f
	leave
	pushl	$7
	calll	_exit
1:	leave
	pushl	$42
	calll	_exit
