## 89_abs_base_index_disp — `<op> disp32(%base,%idx,scale)` where disp32 is
## an ABSOLUTE global-table address and BOTH base and index registers are
## live (i386 mod=10 r/m=100 + SIB with base and index).
##
## A fixed-address i386 exec indexing a 2-D array / array-of-structs emits
## the table's absolute vmaddr as the SIB displacement with the row offset in
## the base register and the element index in the index register:
##
##     movl _tab(%eax,%ecx,4), %edx        ; 8b 94 88 disp32
##
## The parser's absolute-operand branches covered [disp32] (no base, no
## index), [disp32+idx*scale] (index only — guard 98) and [base+disp32]
## (base only — photocd), but the base+index+disp32 shape matched NEITHER
## (the indexed arm requires base=INVALID, the base arm requires
## index=INVALID), so the raw i386 disp32 shipped verbatim: no rebase to the
## M64 layout, no __86x64_abs32 runtime-slide entry — the translated load
## dereferences the unmapped original address.
##
## The fix adds the sibling parse branch (same non-PIE MH_EXECUTE +
## disp-in-segment gates, esp/ebp bases excluded); the byte-identical default
## rule then patches the disp32 to the M64 table vmaddr (memdisp_absolute
## re-derives true: base != RIP) and inject_abs32_section registers the field
## for the runtime ASLR slide, exactly like the [disp32+idx*scale] family.
##
## Covers the load (8b), a store-direction rmw add (01), a byte load
## (0f b6 movzbl), and an INDIRECT CALL (ff /2) through a base+index
## fn-ptr table — the CALL_NEAR_MEMv narrowing rewrite must keep the
## captured table disp ABSOLUTE (register-carrying operand), not patch it
## rip-relative. Exit 0 iff every access saw the relocated table.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	## row 1 (base = 8 bytes), element 1 (idx*4) -> _tab[3] = 0x44444444
	movl	$8, %eax
	movl	$1, %ecx
	movl	_tab(%eax,%ecx,4), %edx     ## 8b 94 88 disp32
	cmpl	$0x44444444, %edx
	jne	Lbad

	## store-direction rmw through the same shape: _tab[2] += edx
	movl	$0, %ecx
	addl	%edx, _tab(%eax,%ecx,4)     ## 01 94 88 disp32 -> 0x33333333+0x44444444
	movl	_tab(%eax,%ecx,4), %edx     ## read back
	cmpl	$0x77777777, %edx
	jne	Lbad

	## byte load (0f b6) through base+index+disp32: _btab[5] = 0x66
	movl	$4, %eax
	movl	$1, %ecx
	movzbl	_btab(%eax,%ecx,1), %edx    ## 0f b6 94 08 disp32
	cmpl	$0x66, %edx
	jne	Lbad

	## indirect call through a base+index fn-ptr table: _ftab[1]
	movl	$0, %esi                    ## sentinel the callee must set
	movl	$4, %eax                    ## row offset (bytes)
	movl	$0, %ecx                    ## element index
	calll	*_ftab(%eax,%ecx,4)         ## ff 94 88 disp32
	cmpl	$0x5e77ed, %esi             ## callee ran?
	jne	Lbad

	pushl	$0
	calll	_exit
	ud2

Lbad:
	pushl	$1
	calll	_exit
	ud2

	.globl _hit
	.p2align 4, 0x90
_hit:
	pushl	%ebp
	movl	%esp, %ebp
	movl	$0x5e77ed, %esi
	popl	%ebp
	ret

	.section __DATA,__data
	.globl _tab
	.p2align 2
_tab:
	.long	0x11111111                  ## [0]
	.long	0x22222222                  ## [1]
	.long	0x33333333                  ## [2]
	.long	0x44444444                  ## [3]
	.globl _btab
_btab:
	.byte	0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
	.globl _ftab
	.p2align 2
_ftab:
	.long	0                           ## [0] unused
	.long	_hit                        ## [1] target (DataParser relocates)
