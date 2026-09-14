## 99_jt_fused_add_dispatch — the switch dispatch clang FUSES into one instruction.
##
## DetectJumpTables recognised the dispatch only as a three-step chain:
##     (c) mov %reg,[%tblbase + idx*4]     ; reg = table entry
##     (d) add %reg,%anchor                ; reg = anchor + entry = target
##     (e) jmp %reg
## But clang also emits the load and the add as ONE instruction, with the anchor
## acting as table base AND addend and the result landing back in the anchor:
##
##     add edi,[edi + edx*4 + 0x2a7]       ; edi = anchor + table[idx]
##     jmp edi
##
## (c) wants MOV_GPRv_MEMv and (d) wants a register-to-register ADD, so the fused
## form matched neither, the table was never claimed, and its bytes were parsed
## as CODE — the same end state as an unbounded table-base lea
## (99_jt_lea_out_of_section), reached by a different route.
##
## MEASURED, Portal 2 libtogl `IDirect3D9::CheckDeviceFormat` (i386 0x15b67,
## anchor 0x15a31, table 0x15cd8, 9 entries). The raw entry values
## 0x272/0x13f/0x20e/0x215/0x29d were disassembled as a junk `jb`/`aas`/`add`
## stream — and they HAD pcmap rows, which is the tell that the translator
## decoded them as instructions. The dispatch then left %rip in the low-4GB
## arena on a `prot=rw-` page: SIGBUS with err=0x15, instruction fetch plus
## protection. Fixing it took libtogl from 97 to 114 detected tables.
##
## Both roles must be the SAME live anchor and the scale must be 4, or it is an
## ordinary indexed add; case (e) still validates every entry against the section
## before recording slots. Kill switch M64_NO_JT_FUSED_ADD=1.
##
## Exit 42 = the table was relocated and case 1 ran. With the kill switch the
## entries stay raw i386 offsets, so `anchor + entry` lands mid-instruction:
## a wrong exit code or a signal death, both FAILs.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$16, %esp
	pushl	%esi

	calll	Lpic0
Lpic0:
	popl	%eax                     ## eax = the PIC anchor

	movl	$1, %edx                 ## the switch value -> case 1
	cmpl	$3, %edx
	ja	Ldefault

	## THE FUSED DISPATCH: load the entry and add the anchor in one insn,
	## result back in the anchor register.
	addl	(Ltable - Lpic0)(%eax,%edx,4), %eax
	jmp	*%eax

Lcase0:
	movl	$10, %esi
	jmp	Lend
Lcase1:
	movl	$42, %esi
	jmp	Lend
Lcase2:
	movl	$20, %esi
	jmp	Lend
Lcase3:
	movl	$30, %esi
	jmp	Lend
Ldefault:
	movl	$1, %esi

Lend:
	pushl	%esi
	calll	_exit
	ud2

	.p2align 2
Ltable:
	.long	Lcase0 - Lpic0
	.long	Lcase1 - Lpic0
	.long	Lcase2 - Lpic0
	.long	Lcase3 - Lpic0
