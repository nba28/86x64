## 99_jt_spill_slot — a PIC relative-offset switch jump table whose TABLE BASE is
## SPILLED TO A FRAME SLOT and reloaded into a different register before use.
##
## Under register pressure GCC computes the PIC table base once, parks it in a
## local, and reloads it at each dispatch:
##
##      call  $+0 ; pop %ebx            # %ebx = PIC anchor
##      lea   disp(%ebx),%eax           # table base
##      mov   %eax,-0x14(%ebp)          # *** SPILL ***
##      ...
##      mov   -0x14(%ebp),%edi          # *** RELOAD, different register ***
##      mov   (%edi,%eax,4),%eax        # entry = table[index]
##      add   %ebx,%eax                 # target = anchor + entry
##      jmp   *%eax
##
## DetectJumpTables tracked the base only through REGISTERS, so it lost it across
## the spill, never recognised the dispatch, and the linear sweep then
## disassembled the inline table AS CODE — destroying the entries and shifting
## every later one (an entry byte 0x53 decodes as `pushl %ebx` and is re-encoded
## into a 7-byte x86_64 push). Worse, the entries are relative offsets into code
## that the translation MAKES LONGER, so even an unmangled table is stale: every
## `anchor + entry` lands at the wrong place.
##
## Real-world: Civ IV "Init Python". Python 2.6's marshal r_object switch (126
## entries, table 0x9b268) is exactly this shape. The shifted read produced an
## entry whose low 16 bits were 0x0000, so `add %anchor` left the anchor's low
## half intact and corrupted only its high half — jump to unmapped memory,
## SIGBUS. 27 of Python's 99 PIC dispatches use the spill form.
##
## Two dispatches, so a green ON arm cannot just mean "the translator changed
## nothing":
##   CONTROL — base kept in a register the whole time. Already handled before
##             this fix, so it must stay correct in BOTH arms.
##   SPILL   — base parked in a frame slot. Correct only when the detector
##             follows it through the slot.
##
## exit 0 = both dispatches landed on their intended case.
## exit 7 = CONTROL took the wrong case (the guard itself is broken).
## exit 8 = SPILL took the wrong case (the defect).
## A crash/abnormal termination in the SPILL arm is also the defect.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%edi
	subl	$0x30, %esp

	## ---------------- CONTROL: table base lives in a register --------------
	calll	LpicA
LpicA:
	popl	%ebx                      ## %ebx = PIC anchor
	leal	(LtblA-LpicA)(%ebx), %edi ## base in a register, never spilled
	movl	$1, %eax                  ## index
	movl	(%edi,%eax,4), %eax
	addl	%ebx, %eax
	jmpl	*%eax
LtblA:
	.long	LbadA-LpicA
	.long	LokA-LpicA
	.long	LbadA-LpicA
	.long	LbadA-LpicA

LbadA:
	pushl	$7
	calll	_exit
	ud2

	## ---------------- SPILL: table base parked in a frame slot -------------
LokA:
	calll	LpicB
LpicB:
	popl	%ebx                      ## %ebx = PIC anchor
	leal	(LtblB-LpicB)(%ebx), %eax ## table base ...
	movl	%eax, -0x14(%ebp)         ## *** SPILLED to a frame slot ***
	movl	$2, %eax                  ## index
	movl	-0x14(%ebp), %edi         ## *** RELOADED into another register ***
	movl	(%edi,%eax,4), %eax
	addl	%ebx, %eax
	jmpl	*%eax
LtblB:
	.long	LbadB-LpicB
	.long	LbadB-LpicB
	.long	LokB-LpicB
	.long	LbadB-LpicB

LbadB:
	pushl	$8
	calll	_exit
	ud2

LokB:
	pushl	$0
	calll	_exit
	ud2
