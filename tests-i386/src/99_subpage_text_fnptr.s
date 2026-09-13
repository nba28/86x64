## 99_subpage_text_fnptr — a function pointer whose target lives in the first page
## must still be recognised as a pointer.
##
## Pointer detection rejected any value below 0x1000 up front, as a cheap
## "obviously not a pointer" filter. That is true of a non-PIE EXECUTABLE, where
## __PAGEZERO owns the first page and __TEXT starts at 0x1000. It is FALSE of a
## DYLIB: its __TEXT is based at vmaddr 0, so __text routinely starts a couple of
## KiB in and every function in that sub-page window was invisible to pointer
## detection. 16 of Portal 2's 42 i386 modules have __text below 0x1000
## (libtier0 0x840, vaudio_* 0x7b0, inputsystem 0xd80).
##
## The consequence is silent and total for the affected slots. inputsystem.dylib's
## CInputSystem vtable (i386 __DATA,__const 0x10168) came out of translation with
## 9 of 12 slots correctly rebased and three left as RAW i386 addresses — 0xed0,
## 0xef0 and 0xf50, every one of them below the threshold. The launcher's virtual
## call
##     mov (%ecx),%eax ; mov 0x14(%eax),%eax ; call *%eax
## then jumped to 0xed0, which is unmapped in the translated image: SIGSEGV on the
## instruction fetch, 3 runs of 3.
##
## The floor is now the image's OWN lowest section vmaddr, so there is no magic
## number to be wrong. std::min keeps 0x1000 where it was already tighter, so the
## change can only add candidates, and only for an image that really has a section
## down there.
##
## This fixture is linked `-pagezero_size 0 -seg1addr 0` (see the Makefile rule) so
## __TEXT is based at vmaddr 0 and __text starts at 0xf30, the layout every i386
## DYLIB has. The default -no_pie executable layout CANNOT reproduce it: __PAGEZERO
## owns the first page, __text starts at 0x1000, and the test would pass for the
## wrong reason. _early lands at 0xf30, below the old threshold; _late is padded
## above 0x1000 as the control. Neither is referenced by any instruction, so the
## __DATA table slot is the only evidence that they are code addresses -- exactly a
## vtable's situation.
##
## Exit 42 = the sub-page pointer was rebased and the call landed. 1 = the table
## slot still holds a raw i386 address (it will usually fault instead, which is
## the real blocker's signature). 9 = the control pointer above 0x1000 is broken,
## so the test proves nothing.

	.section __TEXT,__text,regular,pure_instructions

	## ── A function deliberately placed in the FIRST PAGE ──────────────────
	## .org is relative to the start of __text, and the linker places __text
	## a few hundred bytes into the image, so this lands well below 0x1000.
	.p2align 4, 0x90
_early:
	movl	$7, %eax
	ret

	## Pad so the CONTROL function lands ABOVE 0x1000 while _early stays below.
	## Without this both sit in the first page and the control controls nothing.
	.space	0x1000, 0x90

	.p2align 4, 0x90
_late:
	movl	$35, %eax
	ret

	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp

	## Control first: the slot holding a NORMAL (higher) code address must
	## work, so a failure below is specifically about the sub-page one.
	calll	Lpic
Lpic:
	popl	%esi
	movl	(_fntab - Lpic + 4)(%esi), %eax
	calll	*%eax
	cmpl	$35, %eax
	jne	Lfail9

	## The sub-page pointer. Read it through the same table and call it.
	movl	(_fntab - Lpic)(%esi), %eax
	calll	*%eax
	cmpl	$7, %eax
	jne	Lfail1

	pushl	$42
	calll	_exit
	ud2

Lfail1:
	pushl	$1
	calll	_exit
	ud2
Lfail9:
	pushl	$9
	calll	_exit
	ud2

	## A function-pointer table in __DATA, the shape a C++ vtable has. Nothing
	## references _early or _late directly, so the table slot is the only way
	## the translator can learn these are code addresses.
	.section __DATA,__const
	.p2align 4
_fntab:
	.long	_early                      ## below 0x1000 — the regression
	.long	_late                       ## the control
