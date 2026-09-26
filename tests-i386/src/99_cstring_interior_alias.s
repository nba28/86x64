## 99_cstring_interior_alias — a STATICALLY-INITIALIZED __data word whose value
## ALIASES the INTERIOR of a __cstring (i.e. equals a byte address inside a C
## string, not at its start) must NOT be relocated as a `char *` when the image
## is translated. Regression guard for the Civ IV (Steam) AppKit-era SIGSEGV
## rooted in section.cc DataParser over-relocation.
##
## ROOT: Civ's GCompactDeclInfoNodeArray carries a byte-classification table
## `00 01 00 01 …` — read as `movzbl table[c&0xf]` (bytes 0/1), NEVER as a
## pointer. Each 4-byte word of it is the integer 0x01000100, which happens to
## land inside __TEXT,__cstring (mid-string "…eWidgetType, int iData1, i…"). The
## Steam binary is LOCALS-STRIPPED and carries ZERO relocations, so every
## discriminating gate in DataParser is disarmed (code_alias_is_constant returns
## false with no local text syms; the classic-reloc gate needs a reloc table).
## The permissive in-range heuristic then "rebased" each table word to the
## TRANSLATED __text address of that cstring (0x11a536a0), corrupting the table:
## table[c&0xf] returned a wild index -> node[8+idx*4] read a NULL name pointer
## -> `movzbl (%rdx),%ebx` with rdx=0 -> EXC_BAD_ACCESS at 0x0, reached inside
## Find/LowerBoundSearch running under x64_cb_dispatch (a bridged callback).
##
## FIX: DataParser rejects a value landing in a __cstring INTERIOR (preceding
## byte != NUL and value != section start) as a constant — a genuine baked
## `char *` always targets a string START. This needs no symbols/reloc table, so
## it is the ONLY discriminator that can arm on a stripped, reloc-less exec.
##
## Shape MATCHES Civ: fixed-address (-no_pie) + `strip -x` (locals stripped) so
## have_local_text_syms is false and the other gates stay disarmed; the build
## rule asserts the interior alias really lands mid-__cstring (fail the BUILD
## loudly if layout drifts).
##
## A (positive): read the interior-aliasing word; it must have stayed the i386
## LITERAL (small vmaddr), NOT the translated cstring interior. exit 0 = correct;
## exit 8 = wrongly rebased (the bug).
## B (negative control): a genuine `char *` to the string START must STILL be
## relocated to the translated cstring — so the fix NARROWS, not disables,
## cstring data-pointer relocation. exit 9 = the START pointer was left literal
## (fix over-reached).

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp

	## ---- A: interior alias — must stay the i386 LITERAL (not rebased).
	## leal gives the correctly-relocated TRANSLATED interior address = what a
	## wrong rebase would have written into _probe; they must therefore DIFFER.
	movl	_probe, %eax
	leal	_cstr, %ecx
	addl	$8, %ecx                     ## translated interior (= buggy value)
	cmpl	%eax, %ecx
	je	Lbad_a                       ## _probe == translated interior -> REBASED

	## ---- B (negative control): START pointer MUST still be relocated.
	movl	_pstart, %eax
	leal	_cstr, %ecx                  ## translated string START
	cmpl	%eax, %ecx
	jne	Lbad_b                       ## _pstart != translated start -> left literal

	pushl	$0
	calll	_exit
	ud2
Lbad_a:
	pushl	$8
	calll	_exit
	ud2
Lbad_b:
	pushl	$9
	calll	_exit
	ud2

	.section __TEXT,__cstring,cstring_literals
_cstr:
	.asciz	"ABCDEFGHIJKLMNOP"           ## interior +8 = 'I'; byte before ('H') != NUL

	.section __DATA,__data
	.p2align 2
	.globl _probe
	.long	_cstr + 8                    ## Civ-faithful: the byte table REPEATS the
_probe:                                      ## same word, so no neighbour is a string
	.long	_cstr + 8                    ## pointer of its own (contrast
	.long	_cstr + 8                    ## 99_cstring_tailmerge_ptr); _pstart sits
	.long	0                            ## beyond the +-8 neighbour window
	.long	0
	.globl _pstart
_pstart:
	.long	_cstr                        ## genuine char* to the string START (must rebase)
