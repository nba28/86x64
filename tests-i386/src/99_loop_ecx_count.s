## 99_loop_ecx_count — LOOP/LOOPE/LOOPNE/JECXZ count in the address-size
## register: ECX on i386, RCX in x86_64 unless the translation adds 0x67.
## Exercises every form taken and not taken (the added prefix grows each
## rel8 branch by a byte). Exit 0 iff every branch landed where i386 says.
## Static half (every translated form carries 0x67): `make loop-ecx-count`.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	## loop: 5 iterations
	xorl	%eax, %eax
	movl	$5, %ecx
L1:	incl	%eax
	loop	L1
	cmpl	$5, %eax
	jne	Lbad

	## loope: stops when ZF clears (at eax == 3), ecx left at 10-3
	xorl	%eax, %eax
	movl	$10, %ecx
L2:	incl	%eax
	cmpl	$3, %eax
	setae	%dl
	testb	%dl, %dl
	loope	L2
	cmpl	$3, %eax
	jne	Lbad
	cmpl	$7, %ecx
	jne	Lbad

	## loopne: stops when ZF sets (at eax == 4)
	xorl	%eax, %eax
	movl	$10, %ecx
L3:	incl	%eax
	cmpl	$4, %eax
	loopne	L3
	cmpl	$4, %eax
	jne	Lbad
	cmpl	$6, %ecx
	jne	Lbad

	## jecxz taken / not taken
	xorl	%ecx, %ecx
	jecxz	L4
	jmp	Lbad
L4:	movl	$1, %ecx
	jecxz	Lbad

	pushl	$0
	calll	_exit
	ud2

Lbad:
	pushl	$1
	calll	_exit
	ud2
