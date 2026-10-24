## 99_pic_cfg_fixpoint — the PIC anchor state at a loop head must be the
## fixpoint over every edge, not what a linear walk of the code saw first.
##
## clang -O2 (differential fuzzer, _t28): the anchor is spilled on one path
## only, a block reached only from a loop entered by a backward branch reloads
## it, and the next join is the head of a loop:
##         jne   LA                  ; esi = anchor, slot not written
##         movl  %esi,-0x10(%ebp)    ; spill (other path)
##         jmp   LBbody
##   LO:   movl  -0x10(%ebp),%esi    ; reached only by the back edge
##         je    LX
##   LBbody: ... jmp LO
##   LA:   jmp   LH
##   LX:   movl  -0x10(%ebp),%esi    ; reached only from LO
##   LH:   addl  disp(%esi),%eax     ; loop head: stayed RAW
##         jne   LH
## The linear walk ran LX on the state before it (LA's: no slot), lost %esi
## at LH, and its first pass's back-edge snapshot kept the loss on the second.
##
## Exit 42 = both PIC reads hit _val. Kill switch M64_NO_PIC_CFG=1 (translate
## time) runs the linear walk: a raw disp read, a fault or a wrong exit.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	subl	$44, %esp

	calll	Lpic0
Lpic0:
	popl	%esi                     ## esi = the PIC anchor
	movl	$2, %edi                 ## loop counter
	xorl	%eax, %eax
	testl	%esp, %esp               ## always taken
	jne	LA
	movl	%esi, -0x10(%ebp)        ## spill on this path only
	jmp	LBbody

LO:                                      ## reached only by the back edge
	movl	-0x10(%ebp), %esi
	decl	%ecx
	je	LX
LBbody:
	decl	%ebx
	jmp	LO                       ## back edge

LA:
	jmp	LH

LX:                                      ## reached only from LO
	movl	-0x10(%ebp), %esi

LH:                                      ## join and loop head
	addl	(_val - Lpic0)(%esi), %eax
	decl	%edi
	jne	LH                       ## back edge
	movl	%eax, (%esp)
	calll	_exit
	ud2

	.data
	.p2align 2
_val:
	.long	21
