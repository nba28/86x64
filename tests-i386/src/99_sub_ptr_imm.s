## 99_sub_ptr_imm — `sub $global, %reg` in a fixed-load image: the pointer
## difference `p - &array`.
##
## A fixed-load i386 executable names its globals by literal address, so
## `mov $X,%reg` / `add $X,%reg` / `cmp $X,%reg` are relocated when X is an
## in-image address. `sub` was not: GCC's `(p - array)` and `strstr(buf,s) - buf`
## kept the raw i386 address while `p` held a translated one, so the index was
## off by the relocation delta (Call of Duty 4: 8 sites, Civilization IV: 9).
## Both encodings are covered: `2d imm32` (sub %eax) and `81 /5 imm32`.
##
## Exit code 8 on success, 1 without the fix.
## OFF arm: M64_NO_SUB_PTR_IMM=1 at translate time.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	movl	$_array+12, %eax
	.byte	0x2d                          ## subl $_array, %eax (2d form): 12
	.long	_array
	movl	$_array+20, %ecx
	subl	$_array+16, %ecx              ## 81 /5: 4
	movl	$1, %edx
	cmpl	$12, %eax
	jne	Lout
	cmpl	$4, %ecx
	jne	Lout
	movl	$8, %edx
Lout:
	pushl	%edx
	calll	_exit
	ud2

	.section __DATA,__data
	.p2align 4
	.space	64
_array:
	.long	1, 2, 3, 4, 5, 6, 7, 8
