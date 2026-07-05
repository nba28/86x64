## stripped_stackarg_procptr.s — fixture for stripped_stackarg_procptr_test.sh
## (NOT a numbered suite test: the bug is only observable structurally in the
## translated binary, so a shell harness inspects the output rather than
## diffing stdout).
##
## Regression for the STRIPPED-binary stack-arg ProcPtr relocation gap
## (core: instruction.cc stack-arg path; Halo CE renderer-check crash 2026-07-05).
##
## Halo registers its Carbon renderer-check event handlers with the idiom
##   movl $handler, (%esp)          ; c7 04 24 <i386 __text addr>
##   call _InstallEventHandler
## In a locals-STRIPPED binary (Halo keeps only _main) the parser's
## stackarg_imm_is_code_constant conservatively calls EVERY code-section-aliasing
## stack-arg immediate a constant (no symbol to prove it is a function entry), so
## the raw i386 __text address is emitted UNRELOCATED. At runtime Halo installs
## the handler; when Carbon later invokes it the reverse callback bridge
## (_86x64_call_i386) jmps straight to the raw i386 address (NOT pcmap-translated,
## unlike a translated `call *reg`) -> EXC_BAD_ACCESS at an unmapped low address.
##
## The fix recognizes a genuine ProcPtr STRUCTURALLY — its target is a function
## ENTRY, identified without symbols by the standard i386 frame-setup prologue
## `55 89 e5` (push ebp; mov ebp,esp) — and relocates it to the translated layout.
##
## _handler is LOCAL (stripped via `strip -x`) so it carries no func_syms nlist,
## exactly like Halo's handlers. It opens with the 55 89 e5 prologue.

	.section __TEXT,__text,regular,pure_instructions

	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp
	movl	$_handler, (%esp)      ## the Halo idiom: ProcPtr as a stack argument
	call	_exit                  ## (never returns; the store above is the site under test)

	.p2align 4, 0x90
_handler:                          ## local + stripped -> no func_syms; 55 89 e5 prologue
	pushl	%ebp
	movl	%esp, %ebp
	movl	$42, %eax
	popl	%ebp
	ret
