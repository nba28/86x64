## 36_stripped_func_pad — regression for the STRIPPED-binary linear-sweep
## padding resync (Halo CE static-init crash, Halo.dylib+0x37fd95).
##
## Sibling of 27_func_boundary_pad, but for binaries with NO function symbols.
## 27 keeps .globl symbols so env.func_syms catches the inter-function boundary;
## this binary is STRIPPED of local symbols (only _main survives, exactly like
## Halo's __text, which carries a single _main symbol over the whole section).
##
## Root cause: with func_syms empty the linear code sweep has no boundary to
## anchor on.  After a function's `ret` the linker inserts a 0x00 alignment pad
## before the next function; the vanilla sweep decodes `00 55 89` (the pad byte
## + the next function's `55` = push ebp prologue) as `add [ebp-0x77],dl`,
## ABSORBING the entry byte.  The eax/ebp-based misdecodes need no PIC fixup, so
## they survive M32->M64 byte-identical and the whole function body is emitted as
## raw i386 garbage.  A direct `call` to that swallowed entry then resolves to a
## mid-instruction address; at runtime it executes raw i386 bytes as x86_64 and
## faults (Halo: rax=1 from the absorbed `mov eax,1`, write to 0x0).
##
## The fix (Section::Parse1): after a no-fall-through terminator (RET / uncond
## JMP) a 0x00 byte is alignment padding -> emit a 1-byte DataBlob and re-sync at
## the true entry.  Needs no symbol table, so it cures the stripped case too.
##
## Layout (functions _h10/_h20 are LOCAL and stripped via `strip -x`, leaving
## only the .globl _main in the symtab so env.func_syms is effectively empty):
##   _h10: returns 10, ends in `ret`
##   .byte 0x00   inter-function alignment padding
##   _h20: returns 20  <- entry swallowed by the vanilla sweep
##   _main: _exit(_h10() + _h20()) == _exit(30) on success.
##
## Without the fix _h20 is mistranslated and `call _h20` lands mid-garbage ->
## crash or wrong sum (exit != 30).  With the fix it returns 20 -> exit 30.

	.section __TEXT,__text,regular,pure_instructions

	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$8, %esp
	calll	_h10               ## eax = 10
	movl	%eax, -4(%ebp)     ## save first result
	calll	_h20               ## eax = 20 (the swallowed-entry victim)
	addl	-4(%ebp), %eax     ## eax = 10 + 20 = 30
	movl	%eax, (%esp)
	calll	_exit              ## _exit(30)
	ud2

	## --- local (stripped): returns 10; terminator is `ret` ---
_h10:
	pushl	%ebp
	movl	%esp, %ebp
	movl	$10, %eax
	popl	%ebp
	retl

	## A single 0x00 alignment byte between the two functions.  Vanilla
	## stripped-binary sweep absorbs it together with _h20's `55` entry.
	.byte 0x00

	## --- local (stripped): returns 20; entry MUST survive the sweep ---
_h20:
	pushl	%ebp
	movl	%esp, %ebp
	movl	$20, %eax
	popl	%ebp
	retl
