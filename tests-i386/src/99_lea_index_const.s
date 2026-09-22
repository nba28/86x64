## 99_lea_index_const — in a POSITION-INDEPENDENT image, the disp32 of
## `lea disp32(,%index,scale)` is an INTEGER CONSTANT, not a table base.
##
## The `[disp32 + index*scale]` arm of Instruction::Parse rewrites disp32 to the
## translated vmaddr of whatever the i386 address named, because for a LOAD or a
## STORE that disp32 really is a dereferenced table base (Quinn's
## `movswl 0xb2f42(,%eax,8)`, Halo's `movl $imm32,tab(,%esi,4)`).
##
## ★LEA DOES NOT DEREFERENCE. `lea C(,%reg,4), %reg` is the compiler's ordinary
## idiom for the ARITHMETIC `C + 4*reg`, and in a position-independent image C
## can never be an address at all: a dylib / PIE exec forms every real address
## through a PIC anchor (`lea tab(%ebx,%eax,4)`, a different operand shape), and
## has no text relocation that could slide a literal.
##
## ★MEASURED — Portal 2 wall 6, libtogl `CGLMBuffer::CGLMBuffer` picking the GL
## buffer usage enum:
##     8d 04 85 e4 88 00 00    lea 0x88e4(,%eax,4), %eax
##   = GL_STATIC_DRAW(0x88E4) + 4*(bool) = GL_DYNAMIC_DRAW(0x88E8).
## 0x88e4 aliases libtogl's own __text, so it was rewritten to the translated
## address 0x1000c626 AND given a rebase; at run time the usage argument arrived
## as 0x4fec626 (= libtogl's load base + 0xc626), glBufferDataARB raised
## GL_INVALID_ENUM, the index buffer kept a 0-byte store, glMapBufferARB returned
## NULL, and GLMContext::GenDebugFontTex stored through it
## (`movdqu %xmm3,(%ecx,%eax,2)` with ecx=0) -> SIGSEGV.
##
## THE FIXTURE links -pie (a position-independent image, like every Portal 2
## dylib) and computes 42 as `MAGIC + 4*index` where MAGIC is an integer that
## deliberately ALIASES this image's own __text.
##   ON  (gate armed):  the disp32 is left alone -> the sum is 42.
##   OFF (M64_NO_LEA_INDEX_CONST=1): the disp32 is relocated to the translated
##       __text address, so the sum is a huge pointer -> exit 7.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp

	## A real PIC anchor, so the image looks exactly like the compiler output
	## this rule is about (and so rebasify has a thunk site to seed from — with
	## none it copies the input through and the guard would be inert).
	calll	Lpic0
Lpic0:
	popl	%ebx

	## index = 1 (kept opaque to the translator via memory)
	movl	$1, -4(%ebp)
	movl	-4(%ebp), %eax

	## THE INSTRUCTION UNDER TEST: 8d 04 85 <disp32>.
	## No base register, index*4, disp32 = 0x1b80 — an integer that aliases
	## this image's own __text (which is linked at 0x1b50..0x1f90), so
	## the un-gated heuristic resolves it to a blob and rewrites it.
	## 0x1b80 + 4*1 = 0x1b84; 0x1b84 - 0x1b5a = 42.
	leal	0x1b80(,%eax,4), %eax
	subl	$0x1b5a, %eax

	cmpl	$42, %eax
	je	Lok
	movl	$7, %eax
	jmp	Lfinish
Lok:
	movl	$42, %eax

Lfinish:
	pushl	%eax
	calll	_exit
	ud2

	## Padding so 0x1b80 lands inside a real, mapped __text blob rather than
	## past the end of the section (past the end, resolve would miss and the
	## OFF arm would pass for the wrong reason — an inert guard).
	.p2align 4, 0x90
	.space	0x400, 0x90
