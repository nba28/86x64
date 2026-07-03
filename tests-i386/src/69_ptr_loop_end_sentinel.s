## 69_ptr_loop_end_sentinel — a pointer LOOP whose BASE is a relocated absolute
## immediate and whose END is a `cmp reg, $&table_end` sentinel.
##
## Mirrors Halo's static-init table walk (crashlog Halo-2026-07-03-020440.ips):
##   mov  $_tbl, %ebx           ## base -> relocated to the translated layout
## L:
##   mov  (%ebx), %edx          ## entry (an object pointer, or 0)
##   test %edx, %edx
##   je   next
##   ...call *[edx+0x10]...     ## virtual method through the entry
## next:
##   add  $4, %ebx
##   cmp  $_tbl_end, %ebx       ## END SENTINEL = base + table_size
##   jne  L
##
## The translator relocates the base (`mov $_tbl,%ebx` -> `lea ebx,[rip+disp]`)
## but historically left the `cmp $_tbl_end` immediate as the raw i386 vmaddr.
## The slid iterator then never equals the stale sentinel -> the loop overruns
## the table and dereferences adjacent memory (Halo: a NULL vtable slot ->
## `jmp *0`). The fix relocates the sentinel by the same delta.
##
## This test sums the (4-entry) integer table by walking base..end. Without the
## sentinel fix the loop runs past _tbl_end into whatever follows and either
## reads a wrong value or faults. Exit code = sum = 10 on success.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	movl	$_tbl, %ebx              ## base (relocated absolute immediate)
	xorl	%eax, %eax               ## sum = 0
L:
	addl	(%ebx), %eax             ## sum += *ebx
	addl	$4, %ebx
	cmpl	$_tbl_end, %ebx          ## END SENTINEL (must move with the base)
	jne	L
	pushl	%eax                     ## exit code = sum
	calll	_exit
	ud2

	.section __DATA,__data
	.p2align 2
_tbl:
	.long	1, 2, 3, 4               ## sum = 10
_tbl_end:
	.long	0xdeadbeef              ## overrun guard: adding this -> wrong code
