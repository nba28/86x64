## 99_jt_case_in_func — a PIC jump table's case bodies lie inside the
## dispatching function. The table scan read one word PAST a `cmpl $3 ; ja`
## table (Portal 2 server: the `nopl` alignment after it) as a 5th "case" whose
## target was 4 MB away inside another function. That target's join took the
## dispatcher's anchor snapshot ({%ebx}) and erased that function's own %esi
## anchor mid-run, so its remaining PIC stores stayed raw and wrote into
## __TEXT (SIGBUS in server.dylib static init).
##
## Here the stray word after the table is a delta to _victim_mid. ON: the case
## scan stops at the function boundary, _victim stays anchored, exit 42.
## OFF (M64_NO_JT_CASE_IN_FUNC=1 at translate time): the store at _victim_mid
## stays raw -> fault (or a wrong value), never 42.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	$1
	calll	_dispatch
	addl	$4, %esp
	calll	_victim
	pushl	%eax
	calll	_exit
	ud2

	.globl _dispatch
	.p2align 4, 0x90
_dispatch:
	pushl	%ebx
	calll	1f
1:	popl	%ebx
	movl	8(%esp), %eax
	cmpl	$3, %eax
	ja	Ldef
	movl	Ltbl-1b(%ebx,%eax,4), %eax
	addl	%ebx, %eax
	jmpl	*%eax
Lc0:	movl	$10, %eax
	jmp	Ldef
Lc1:	movl	$11, %eax
	jmp	Ldef
Lc2:	movl	$12, %eax
	jmp	Ldef
Lc3:	movl	$13, %eax
Ldef:	popl	%ebx
	retl
	.p2align 2
Ltbl:
	.long	Lc0-1b
	.long	Lc1-1b
	.long	Lc2-1b
	.long	Lc3-1b
	.long	_victim_mid-1b          ## NOT a case: the word after the table

	.globl _victim
	.p2align 4, 0x90
_victim:
	pushl	%esi
	calll	2f
2:	popl	%esi
	movl	$0, _cell-2b(%esi)
_victim_mid:
	movl	$42, _cell-2b(%esi)
	movl	_cell-2b(%esi), %eax
	popl	%esi
	retl

	.section __DATA,__data
	.p2align 2
_cell:
	.long	0
