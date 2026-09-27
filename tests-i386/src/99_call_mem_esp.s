## 99_call_mem_esp — `call *disp(%esp)` reads its target BEFORE the call pushes
## the return address. PvZ libbass: `push x5; call *0x184(%esp)`; the
## translation pushed the return address first and then loaded 0x184(%rsp),
## 4 bytes too low -> jumped onto its own stack.
## exit 0 = reached Ltarget with the right args. OFF
## (M64_CALL_MEM_LOAD_AFTER_PUSH=1 at TRANSLATE time): wrong slot -> crash.
	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	subl	$0x1c, %esp
	calll	Lpic
Lpic:
	popl	%eax
	leal	(Ltarget-Lpic)(%eax), %eax
	movl	%eax, 0x10(%esp)          ## fn ptr in a stack slot
	movl	$0x51, 0x0c(%esp)         ## a non-pointer neighbour
	pushl	$7                        ## argument
	calll	*0x14(%esp)               ## = the fn-ptr slot (0x10 + 4 pushed)
	addl	$4, %esp
	pushl	%eax
	calll	_exit
	ud2
Ltarget:
	movl	4(%esp), %eax             ## arg 7
	subl	$7, %eax                  ## -> 0
	retl
