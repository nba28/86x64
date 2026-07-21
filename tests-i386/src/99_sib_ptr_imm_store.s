## 99_sib_ptr_imm_store — pointer-IMMEDIATE stores/compares through a
## SIB-with-base destination (`movl/cmpl $&data, disp(%base,%index,scale)`)
## in a non-PIE i386 executable, including the NEGATIVE-INDEX EA wrap.
##
## Two gaps this guards (matrix findings, 9510f29/07e7ef0/99_cmp_mem_ptr_imm
## family):
##
## 1. CAPTURE: the genbase c7 arm excluded rm==4 wholesale ("esp/ebp handled
##    above") but the stack arm only keys the exact esp-no-index encodings,
##    so a SIB with a REAL register base (`movl $&blk, 0x1c(%ebx,%esi,1)` —
##    array-of-structs field install) fell through BOTH arms and the raw
##    i386 imm shipped verbatim (matrix bit3). Fix admits SIB-with-base
##    under the same policy gates; no-base [disp32+idx] stays with the
##    absolute-table machinery.
##
## 2. EA-WRAP FIDELITY: the shared MR rewrite (lea r11 + `op [mem],r11d`)
##    copies the i386 ModR/M+SIB into 64-bit addressing, losing the i386
##    mod-2^32 wrap a negative/sentinel index relies on (the
##    55_sib_index_neg_wrap pathology): index=-4 at scale 8 computes
##    base+0x7fffffe0-ish >4GB -> SIGSEGV instead of base-0x20. Fix
##    prepends 0x67 (addr32) on scaled-index dests, mirroring the M64
##    copy-ctor policy (esp/ebp-involving operands excluded).
##
## Exit 0 iff all cases hold; distinct exit code per first failure
## (pre-fix: exit 2 on the SIB store, or 139 on the wrap cases).

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$0x40, %esp
	movl	$_obj, %ebx
	xorl	%esi, %esi

	## ---- D: SIB-with-base pointer-imm STORE ----
	movl	$_blk+8, 0x1c(%ebx,%esi,1)  ## c7 44 33 1c imm32
	movl	0x1c(%ebx), %ecx
	movl	$_blk+8, %eax               ## relocated ground truth
	cmpl	%ecx, %eax
	jne	Lbad_d

	## ---- F: SIB-with-base pointer-imm CMP over a reg-stored slot ----
	movl	%eax, 0x20(%ebx)
	cmpl	$_blk+8, 0x20(%ebx,%esi,1)  ## 81 7c 33 20 imm32
	jne	Lbad_f

	## ---- W: NEGATIVE-index SIB pointer-imm STORE (i386 EA wrap) ----
	movl	$_arr+0x20, %ebx            ## base = one-past-end (relocated)
	movl	$-4, %edx                   ## index sentinel
	movl	$_blk+8, (%ebx,%edx,8)      ## EA = base - 0x20 = _arr (wrap)
	movl	_arr, %ecx                  ## read back directly
	cmpl	%ecx, %eax                  ## eax still = &_blk+8 ground truth
	jne	Lbad_w

	## ---- X: NEGATIVE-index SIB pointer-imm CMP (i386 EA wrap) ----
	cmpl	$_blk+8, (%ebx,%edx,8)      ## same wrapped slot must MATCH
	jne	Lbad_x

	## ---- integer negative control through SIB ----
	movl	$_obj, %ebx
	movl	$0x30, 0x24(%ebx,%esi,1)
	cmpl	$0x30, 0x24(%ebx)
	jne	Lbad_i

	pushl	$0
	calll	_exit
	ud2

Lbad_d:
	pushl	$2
	calll	_exit
	ud2
Lbad_f:
	pushl	$3
	calll	_exit
	ud2
Lbad_w:
	pushl	$4
	calll	_exit
	ud2
Lbad_x:
	pushl	$5
	calll	_exit
	ud2
Lbad_i:
	pushl	$6
	calll	_exit
	ud2

	.section __DATA,__data
	.globl _obj
	.p2align 2
_obj:
	.space	48

	## wrap-target array: the negative-index EA lands at _arr (= base-0x20)
	.p2align 4
_arr:
	.long	0, 0, 0, 0, 0, 0, 0, 0      ## 0x20 bytes

	.p2align 4
_blk:
	.long	0, 0, 0, 0
