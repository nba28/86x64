## 99_sscanf_many_args — an i386 sscanf with more than 16 arguments must parse
## every conversion and preserve the caller's callee-saved %esi.
##
## vararg-conv-t.asm (the scanf/sscanf/__sprintf_chk bridge) reserved 16 slots:
## args 17+ overflowed args64 into argtypes, and their type tags (Q = 3) landed on
## the saved rsi -> %esi came back as 3. Portal 2 materialsystem
## CMaterialSubRect::ParseMaterialVars sscanf's a 16-float matrix (18 args); %esi
## held its KeyValues iterator -> GetName(3) -> SIGSEGV at 0x15.
##
## Exit 42 = 18 values parsed in order and %esi intact. No kill switch (a buffer
## size); proven non-inert against the 16-slot template.
	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	pushl	%ebx
	subl	$104, %esp               ## 20 outgoing args (80 bytes), 16-aligned
	calll	Lpic0
Lpic0:
	popl	%ebx
	movl	$0x5eed1234, %esi        ## callee-saved: must survive the call
	leal	(_str - Lpic0)(%ebx), %eax
	movl	%eax, (%esp)
	leal	(_fmt - Lpic0)(%ebx), %eax
	movl	%eax, 4(%esp)
	leal	(_vals + 0 - Lpic0)(%ebx), %eax
	movl	%eax, 8(%esp)
	leal	(_vals + 4 - Lpic0)(%ebx), %eax
	movl	%eax, 12(%esp)
	leal	(_vals + 8 - Lpic0)(%ebx), %eax
	movl	%eax, 16(%esp)
	leal	(_vals + 12 - Lpic0)(%ebx), %eax
	movl	%eax, 20(%esp)
	leal	(_vals + 16 - Lpic0)(%ebx), %eax
	movl	%eax, 24(%esp)
	leal	(_vals + 20 - Lpic0)(%ebx), %eax
	movl	%eax, 28(%esp)
	leal	(_vals + 24 - Lpic0)(%ebx), %eax
	movl	%eax, 32(%esp)
	leal	(_vals + 28 - Lpic0)(%ebx), %eax
	movl	%eax, 36(%esp)
	leal	(_vals + 32 - Lpic0)(%ebx), %eax
	movl	%eax, 40(%esp)
	leal	(_vals + 36 - Lpic0)(%ebx), %eax
	movl	%eax, 44(%esp)
	leal	(_vals + 40 - Lpic0)(%ebx), %eax
	movl	%eax, 48(%esp)
	leal	(_vals + 44 - Lpic0)(%ebx), %eax
	movl	%eax, 52(%esp)
	leal	(_vals + 48 - Lpic0)(%ebx), %eax
	movl	%eax, 56(%esp)
	leal	(_vals + 52 - Lpic0)(%ebx), %eax
	movl	%eax, 60(%esp)
	leal	(_vals + 56 - Lpic0)(%ebx), %eax
	movl	%eax, 64(%esp)
	leal	(_vals + 60 - Lpic0)(%ebx), %eax
	movl	%eax, 68(%esp)
	leal	(_vals + 64 - Lpic0)(%ebx), %eax
	movl	%eax, 72(%esp)
	leal	(_vals + 68 - Lpic0)(%ebx), %eax
	movl	%eax, 76(%esp)
	calll	_sscanf
	cmpl	$18, %eax
	jne	Lfail
	cmpl	$0x5eed1234, %esi
	jne	Lfail
	cmpl	$1, (_vals + 0 - Lpic0)(%ebx)
	jne	Lfail
	cmpl	$2, (_vals + 4 - Lpic0)(%ebx)
	jne	Lfail
	cmpl	$3, (_vals + 8 - Lpic0)(%ebx)
	jne	Lfail
	cmpl	$4, (_vals + 12 - Lpic0)(%ebx)
	jne	Lfail
	cmpl	$5, (_vals + 16 - Lpic0)(%ebx)
	jne	Lfail
	cmpl	$6, (_vals + 20 - Lpic0)(%ebx)
	jne	Lfail
	cmpl	$7, (_vals + 24 - Lpic0)(%ebx)
	jne	Lfail
	cmpl	$8, (_vals + 28 - Lpic0)(%ebx)
	jne	Lfail
	cmpl	$9, (_vals + 32 - Lpic0)(%ebx)
	jne	Lfail
	cmpl	$10, (_vals + 36 - Lpic0)(%ebx)
	jne	Lfail
	cmpl	$11, (_vals + 40 - Lpic0)(%ebx)
	jne	Lfail
	cmpl	$12, (_vals + 44 - Lpic0)(%ebx)
	jne	Lfail
	cmpl	$13, (_vals + 48 - Lpic0)(%ebx)
	jne	Lfail
	cmpl	$14, (_vals + 52 - Lpic0)(%ebx)
	jne	Lfail
	cmpl	$15, (_vals + 56 - Lpic0)(%ebx)
	jne	Lfail
	cmpl	$16, (_vals + 60 - Lpic0)(%ebx)
	jne	Lfail
	cmpl	$17, (_vals + 64 - Lpic0)(%ebx)
	jne	Lfail
	cmpl	$18, (_vals + 68 - Lpic0)(%ebx)
	jne	Lfail
	movl	$42, (%esp)
	calll	_exit
Lfail:
	movl	$1, (%esp)
	calll	_exit

	.cstring
_str:
	.asciz	"1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18"
_fmt:
	.asciz	"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d"

	.data
	.p2align 2
_vals:
	.space	72
