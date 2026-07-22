## 99_jmptbl_index_live — a memory-indirect jump-table dispatch
## (`jmpl *table(,%reg,4)`) writes NO general register in i386 semantics; the
## switch INDEX register (and every other register) is live-in at the case
## handlers. GCC relies on this: the canonical switch shape keeps the scrutinee
## in the index register and the case handlers consume it.
##
## RED repro for the Civ IV (Steam) HBITMAP_Mac garbage-size abort ("operator
## new[] (~3-4GB) cannot throw"): the translator lowers the one-step i386
## dispatch to `movl table(,%rax,4),%eax ; jmpq *%rax` — CLOBBERING %eax with
## the translated TARGET ADDRESS. Civ's HBITMAP_Mac ctor dispatches on
## biBitCount (`movzwl %dx,%eax ; jmpl *0x103254c(,%eax,4)`) and case 8 stores
## %eax as the bits-per-pixel local (`movl %eax,-0x3c(%ebp)`); the translated
## run stores table[8]'s SLID ADDRESS (0x1013d4c4+slide) instead of 8, so
## size = align4(bpp*W/8)*H computes ~3-4GB (low bits stable 0x…6200, high bits
## = the per-run ASLR slide — the "Heisenbug" garbage was never in the
## BITMAPINFO at all). Every one of Civ's 983 one-step table dispatches gets
## this lowering (848 with index==eax, the rest clobber live %eax as the
## scratch), so the family is app-universal.
##
## Contract asserted here, mirroring the Civ shape byte-for-byte
## (cmp/ja bounds guard + `ff 24 85 <table>` + table in __TEXT,__const):
##   A: dispatch via %eax     -> case handler still sees %eax == index (8)
##   B: dispatch via %ecx     -> handler sees %ecx == index (2) AND a live
##      %eax canary preserved (the translated form loads the target into %eax
##      even when the index is another register).
## exit 0 = both preserved; exit 8 = A clobbered (the Civ bug); exit 9/10 = B.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp

	## ---- A: index register IS %eax (Civ HBITMAP_Mac biBitCount shape) ----
	movzwl	_bitcount, %eax          ## eax = 8 (like movzwl 0xe(%ecx),%edx; movzwl %dx,%eax)
	cmpl	$0x18, %eax
	ja	Ldefault
	jmpl	*_tbl(,%eax,4)           ## ff 24 85 <abs32> — writes NO register

Lcase8:
	## i386 contract: %eax still holds the switch value (Civ stores it as bpp)
	cmpl	$8, %eax
	jne	Lbad_index_eax

	## ---- B: index in %ecx, live canary in %eax ----
	movl	$2, %ecx
	movl	$0x0c0ffee0, %eax        ## live across the dispatch in i386 semantics
	cmpl	$0x18, %ecx
	ja	Ldefault
	jmpl	*_tbl2(,%ecx,4)

Lcase2:
	cmpl	$2, %ecx                 ## index register preserved?
	jne	Lbad_index_ecx
	cmpl	$0x0c0ffee0, %eax        ## bystander %eax preserved?
	jne	Lbad_canary_eax

	pushl	$0
	calll	_exit
	ud2

Ldefault:                                ## wrong table slot taken
	pushl	$7
	calll	_exit
	ud2
Lbad_index_eax:                          ## the Civ bug: %eax = slid target addr
	pushl	$8
	calll	_exit
	ud2
Lbad_index_ecx:
	pushl	$9
	calll	_exit
	ud2
Lbad_canary_eax:
	pushl	$10
	calll	_exit
	ud2

	## GCC places switch tables in __TEXT,__const with absolute entries.
	.section __TEXT,__const
	.p2align 2
_tbl:                                    ## 25 entries, [8] = Lcase8
	.long	Ldefault, Ldefault, Ldefault, Ldefault, Ldefault
	.long	Ldefault, Ldefault, Ldefault, Lcase8,   Ldefault
	.long	Ldefault, Ldefault, Ldefault, Ldefault, Ldefault
	.long	Ldefault, Ldefault, Ldefault, Ldefault, Ldefault
	.long	Ldefault, Ldefault, Ldefault, Ldefault, Ldefault
_tbl2:                                   ## 25 entries, [2] = Lcase2
	.long	Ldefault, Ldefault, Lcase2,   Ldefault, Ldefault
	.long	Ldefault, Ldefault, Ldefault, Ldefault, Ldefault
	.long	Ldefault, Ldefault, Ldefault, Ldefault, Ldefault
	.long	Ldefault, Ldefault, Ldefault, Ldefault, Ldefault
	.long	Ldefault, Ldefault, Ldefault, Ldefault, Ldefault

	.section __DATA,__data
	.p2align 2
_bitcount:
	.short	8                        ## biBitCount = 8 (the Civ case)
	.short	0
