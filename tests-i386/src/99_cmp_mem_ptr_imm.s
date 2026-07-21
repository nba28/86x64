## 99_cmp_mem_ptr_imm — `cmpl $&writable_data, mem` pointer-IMMEDIATE
## COMPARES against a memory destination in a non-PIE i386 executable.
##
## The OPEN sibling of the 92_ptr_imm_field_store (9510f29) / 99_zerofill_
## target_imm_store (07e7ef0) STORE fixes: those relocate the pointer-imm
## STORES (`movl $&data, disp(%reg)` / `movl $&data, [abs32]`), but the
## matching COMPARES shipped RAW:
##
##   (a) `cmpl $&data, disp(%reg)`  (81 /7, general reg base)  — Civ IV live
##       at 0x114cf20b: `cmpl $0x145ea6c,-0x54(%rbp)`, 22/22 __data-target
##       sites raw;
##   (b) `cmpl $&data, disp(%ebp)`  (81 7d, frame slot)        — same;
##   (c) `cmpl $&data, [abs32]`     (81 3d) — the 31b8a39 CMP/TEST gate
##       admits a pointer ONLY on a func_syms hit, so a DATA-target identity
##       compare stays literal;
##   (d) zerofill-target variants of (a)  — 17/18 raw in the Civ census.
##
## Once the store side relocates and the compare side doesn't, a sentinel-
## IDENTITY test (`obj->field = &sentinel; ... if (obj->field == &sentinel)`)
## is ALWAYS-FALSE in the translated binary: the field holds the slid
## address, the compare imm holds the raw i386 one. That is a SILENT
## corruption class (copy-on-write/free-the-sentinel decisions invert), not
## a crash — exactly why it survived so long.
##
## The fix mirrors the EXACT 9510f29 discriminator on the COMPARE side, for
## CMP only: fixed-load image (non-PIE MH_EXECUTE) + imm32 4-aligned +
## vmaddr_in_writable_data (zerofill included per 07e7ef0; __OBJC excluded
## by the predicate). Deliberately NOT the looser const-section policy: live
## integer compares like `$0x1000000` alias __TEXT,__const and must stay
## literal (guard 99_cmp_abs32_imm_notptr covers the __text-aliasing side).
## The CMP_MEMv_IMMz transform (lea r11 + `cmp [mem], r11d`, 39 /r) already
## existed — this is purely the missing parse-capture.
##
## Exit 0 iff every store/compare pair MATCHES (and the mismatch control
## correctly differs). Distinct exit codes name the failing case.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$0x60, %esp

	## base object pointer via the covered reg-imm path
	movl	$_obj, %ebx

	## ---- case (a): general-reg-base store + IDENTITY COMPARE, __data ----
	movl	$_blk+8, 0x8(%ebx)          ## store: relocates (9510f29, guard 92)
	cmpl	$_blk+8, 0x8(%ebx)          ## 81 7b 08 imm32 — must MATCH
	jne	Lbad_a

	## ---- case (b): ebp frame-slot store + IDENTITY COMPARE, __data ----
	movl	$_blk+8, -0x54(%ebp)        ## store: relocates (c7 45 stack arm)
	cmpl	$_blk+8, -0x54(%ebp)        ## 81 7d ac imm32 — must MATCH
	jne	Lbad_b

	## ---- case (c): abs32-dest store + IDENTITY COMPARE, __data ----
	movl	$_blk+8, _gfield            ## c7 05: relocates (86_selfref family)
	cmpl	$_blk+8, _gfield            ## 81 3d: 31b8a39 forces literal today
	jne	Lbad_c

	## ---- case (d): general-reg-base store + IDENTITY COMPARE, zerofill ----
	movl	$_bss_blk, 0x10(%ebx)       ## store: relocates (07e7ef0, guard 99)
	cmpl	$_bss_blk, 0x10(%ebx)       ## 81 7b 10 imm32 — must MATCH
	jne	Lbad_d

	## ---- case (g): CODE-pointer identity compare (func_syms evidence) ----
	## `movl $_handler_fn, field` relocates via the const-section genbase
	## arm (+ code-alias positive evidence); the compare sibling must admit
	## the same func_syms-symboled code target (mirrors the abs32-dest CMP
	## arm's func_syms admit, test 87) or the handler-identity test
	## (`field == &default_handler`) is always-false.
	movl	$_handler_fn, 0x14(%ebx)
	cmpl	$_handler_fn, 0x14(%ebx)    ## 81 7b 14 imm32, code target
	jne	Lbad_g

	## ---- mismatch control: a DIFFERENT pointer must still differ ----
	cmpl	$_blk, 0x8(%ebx)            ## field holds _blk+8, imm is _blk
	je	Lbad_e

	## ---- integer negative control: small imm stays verbatim both sides ----
	movl	$0x30, 0xc(%ebx)
	cmpl	$0x30, 0xc(%ebx)            ## 83 /7 imm8 — untouched
	jne	Lbad_f

	pushl	$0
	calll	_exit
	ud2

Lbad_a:
	pushl	$2
	calll	_exit
	ud2
Lbad_b:
	pushl	$3
	calll	_exit
	ud2
Lbad_c:
	pushl	$4
	calll	_exit
	ud2
Lbad_d:
	pushl	$5
	calll	_exit
	ud2
Lbad_e:
	pushl	$6
	calll	_exit
	ud2
Lbad_f:
	pushl	$7
	calll	_exit
	ud2
Lbad_g:
	pushl	$8
	calll	_exit
	ud2

	## symboled function target for case (g) — never called, just compared
	.globl _handler_fn
	.p2align 4, 0x90
_handler_fn:
	pushl	%ebp
	movl	%esp, %ebp
	popl	%ebp
	ret

	.section __DATA,__data
	.globl _obj
	.p2align 2
_obj:
	.space	32                          ## the object written at runtime

	## abs32-dest global field for case (c)
	.globl _gfield
	.p2align 2
_gfield:
	.long	0

	## ANONYMOUS zeroed block (no .globl — like Civ's static string block);
	## the stored/compared pointer targets its interior at +8
	.p2align 4
_blk:
	.long	0, 0, 0, 0

	## ZEROFILL (__bss) block for case (d)
	.zerofill __DATA,__bss,_bss_blk,16,4
