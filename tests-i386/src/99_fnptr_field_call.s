## 99_fnptr_field_call — `movl $_handler, disp(%reg)` FUNCTION-pointer
## IMMEDIATE stored into a struct FIELD through a general register base,
## then CALLED through the field.
##
## Matrix finding (crash-class): the genbase c7 arm's probes (const-section,
## vtable double-indirection, 9510f29 fixed-load WRITABLE-DATA) all reject
## an INSTRUCTIONS-section target, so the classic C manual-dispatch idiom
##     obj->cb = &handler;        // movl $_handler, 0x14(%ebx)
##     obj->cb(...);              // call *0x14(%ebx)
## shipped the raw i386 code address verbatim into the field and the first
## indirect call through it jumped to the stale i386 vmaddr -> SIGSEGV
## (probe exit 139 pre-fix). The STACK-ARG twin (`movl $_handler,(%esp)` —
## Halo Carbon ProcPtr registration) was already admitted with
## function-entry positive evidence; this extends the same policy to field
## stores: instructions-section target AND (func_syms nlist OR `55 89 e5`
## prologue) via the shared imm32_code_alias_is_constant classifier. An
## integer merely aliasing __text stays literal.
##
## The compare sibling (`cmpl $_handler, field` handler-identity test) is
## case (g) of guard 99_cmp_mem_ptr_imm.
##
## Exit 0 iff the called handler returns its sentinel and the identity
## compare matches; pre-fix: SIGSEGV (exit 139).

	.section __TEXT,__text,regular,pure_instructions
	.globl _handler_fn
	.p2align 4, 0x90
_handler_fn:
	pushl	%ebp
	movl	%esp, %ebp
	movl	$0x2a, %eax
	popl	%ebp
	ret

	.globl _main
	.p2align 4, 0x90
_main:
	movl	$_obj, %ebx

	## field install of the callback (the uncaptured shape)
	movl	$_handler_fn, 0x14(%ebx)    ## c7 43 14 imm32, code target

	## identity test against the relocated reg ground truth
	movl	$_handler_fn, %eax          ## covered reg-imm path
	cmpl	%eax, 0x14(%ebx)
	jne	Lbad

	## call through the stored field — stale i386 addr faults here pre-fix
	calll	*0x14(%ebx)
	cmpl	$0x2a, %eax
	jne	Lbad

	pushl	$0
	calll	_exit
	ud2

Lbad:
	pushl	$1
	calll	_exit
	ud2

	.section __DATA,__data
	.globl _obj
	.p2align 2
_obj:
	.space	32
