## 98_abs32_imm_group — IMMEDIATE-group ops (C6/C7/80/81/83/F6/F7) whose memory
## operand is an ABSOLUTE disp32 (`[disp32+idx*scale]` or `[base+disp32]`) AND
## which carry a trailing immediate.
##
## Mirrors the deployed-Halo renderer crash (EXC_BAD_ACCESS write ~0x453cc1):
## `movb $0x0, 0x453cc0(%rdx)` = `C6 82 C0 3C 45 00 00` shipped BYTE-IDENTICAL
## from the i386 source — disp32 0x453cc0 is the raw i386 __DATA-zerofill
## address, never rebased to the M64 layout and never listed in the
## __DATA,__86x64_abs32 slide table. Two parse-side gaps produced that:
##
##  (1) `[disp32+idx*scale]` + trailing imm32 (C7 04/81 /r/F7 /0 with SIB
##      base=101): instruction.cc's `[disp32+idx]` parser skipped ANY form
##      with a trailing imm32 (`has_trailing_imm32` gate, deferring to the
##      runtime byte-scan that 2b7076a's exact table SUPERSEDED) -> memdisp
##      never captured -> raw i386 disp32 shipped.
##  (2) `[base+disp32]` (mod=10, non-PIE MH_EXECUTE absolute-table gate):
##      memdisp was captured but resolved with EXACT-KEY resolve only; a
##      dest INSIDE a multi-byte blob — every ZEROFILL (__bss/__common)
##      variable, per the ZeroBlob spanning extent — missed, memdisp stayed
##      null, and the transform's slide-correct `lea r11,[rip+..]` rewrite
##      silently degraded to the byte-identical copy (Halo: 0x453cc0 sits in
##      the __DATA zerofill tail; all 5 sites C6 82/C6 86/80 BE/80 B8 shipped
##      raw while the NEIGHBORING no-imm `8B 1C 95` load of the same table
##      was rebased fine).
##
## Every op verifies through a KNOWN-GOOD access (bare `[disp32]` load — the
## rip-relative-translated class), so a consistently-wrong address cannot
## self-consistently false-pass. Exit 99 = all forms hit the real datum.
## (The no-imm ALU family is guarded by 94_alu_abs_table; the bare-[disp32]
## imm forms C6 05/C7 05/80 05 by 12_static_byte_flag & friends.)

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	subl	$16, %esp

## ---- (1) [disp32+idx*scale] + trailing imm32: C7 04, 81 /4, 81 /1, F7 /0,
##          83 /7 (imm8 twin) — the has_trailing_imm32 skip class ----
	movl	$1, %eax
	movl	$0x5555aaaa, _tab(,%eax,4)    ## C7 04 85: tab[1] = 0x5555aaaa
	movl	_tab+4, %ecx                  ## known-good readback
	cmpl	$0x5555aaaa, %ecx
	jne	Lbad0
	andl	$0x00ff00ff, _tab(,%eax,4)    ## 81 24 85: tab[1] = 0x005500aa
	movl	_tab+4, %ecx
	cmpl	$0x005500aa, %ecx
	jne	Lbad1
	orl	$0x30003000, _tab(,%eax,4)    ## 81 0C 85: tab[1] = 0x305530aa
	movl	_tab+4, %ecx
	cmpl	$0x305530aa, %ecx
	jne	Lbad2
	testl	$0x00300000, _tab(,%eax,4)    ## F7 04 85: 0x305530aa & 0x300000 != 0
	jz	Lbad3
	cmpl	$0x7f, _tab(,%eax,4)          ## 83 3C 85 imm8: 0x305530aa > 0x7f
	jbe	Lbad4

## ---- (2a) [base+disp32] + trailing imm, FILE-BACKED __data dest ----
	movl	$8, %edi
	movl	$0x0badf00d, _tab(%edi)       ## C7 87: tab[2] = 0x0badf00d
	movl	_tab+8, %ecx
	cmpl	$0x0badf00d, %ecx
	jne	Lbad5
	andl	$0xffff0000, _tab(%edi)       ## 81 A7: tab[2] = 0x0bad0000
	movl	_tab+8, %ecx
	cmpl	$0x0bad0000, %ecx
	jne	Lbad6
	movl	$2, %edx
	movb	$0x5a, _bytes+2(%edx)         ## C6 82 (the Halo shape): bytes[4]=0x5a
	movzbl	_bytes+4, %ecx
	cmpl	$0x5a, %ecx
	jne	Lbad7
	cmpb	$0x5a, _bytes+2(%edx)         ## 80 BA /7
	jne	Lbad8
	testb	$0x10, _bytes+2(%edx)         ## F6 82 /0: 0x5a & 0x10 != 0
	jz	Lbad9

## ---- (2b) [base+disp32] + trailing imm, ZEROFILL interior dest (exact
##           Halo class: disp32 lands inside the __bss ZeroBlob extent) ----
	movl	$3, %esi
	movb	$0x77, _zflags+6(%esi)        ## C6 86: zflags[9] = 0x77
	movzbl	_zflags+9, %ecx
	cmpl	$0x77, %ecx
	jne	Lbad10
	cmpb	$0x77, _zflags+6(%esi)        ## 80 BE /7
	jne	Lbad11
	testb	$0x04, _zflags+6(%esi)        ## F6 86 /0: 0x77 & 0x04 != 0
	jz	Lbad12

## ---- (1b) [disp32+idx*scale] + imm32 into ZEROFILL (both gaps at once) ----
	movl	$4, %ebx
	movl	$0x13572468, _zwords+4(,%ebx,4) ## C7 04 9D: zwords[5] = 0x13572468
	movl	_zwords+20, %ecx
	cmpl	$0x13572468, %ecx
	jne	Lbad13

## ---- guards: index-form imm8 (C6 04/80 3C w/ SIB) — memdisp was already
##      captured for these (imm8 passes the old width==32 gate); keep green ----
	movl	$6, %ebx
	movb	$0x5e, _bytes(,%ebx,1)        ## C6 04 1D: bytes[6] = 0x5e
	movzbl	_bytes+6, %ecx
	cmpl	$0x5e, %ecx
	jne	Lbad14
	cmpb	$0x5e, _bytes(,%ebx,1)        ## 80 3C 1D /7
	jne	Lbad15

	movl	$99, %ecx
	jmp	Lout
## Distinct failure exit codes (10+N) so a FAIL names the broken form.
Lbad0:	movl	$10, %ecx
	jmp	Lout
Lbad1:	movl	$11, %ecx
	jmp	Lout
Lbad2:	movl	$12, %ecx
	jmp	Lout
Lbad3:	movl	$13, %ecx
	jmp	Lout
Lbad4:	movl	$14, %ecx
	jmp	Lout
Lbad5:	movl	$15, %ecx
	jmp	Lout
Lbad6:	movl	$16, %ecx
	jmp	Lout
Lbad7:	movl	$17, %ecx
	jmp	Lout
Lbad8:	movl	$18, %ecx
	jmp	Lout
Lbad9:	movl	$19, %ecx
	jmp	Lout
Lbad10:	movl	$20, %ecx
	jmp	Lout
Lbad11:	movl	$21, %ecx
	jmp	Lout
Lbad12:	movl	$22, %ecx
	jmp	Lout
Lbad13:	movl	$23, %ecx
	jmp	Lout
Lbad14:	movl	$24, %ecx
	jmp	Lout
Lbad15:	movl	$25, %ecx
	jmp	Lout
Lout:
	movl	%ecx, (%esp)
	calll	_exit
	ud2

	.section __DATA,__data
	.p2align 2
_tab:
	.long	0x11111111
	.long	0x22222222
	.long	0x33333333
	.long	0x44444444
_bytes:
	.space	8, 0

	.zerofill __DATA,__bss,_zpad,16,4
	.zerofill __DATA,__bss,_zflags,16,4
	.zerofill __DATA,__bss,_zwords,32,4
