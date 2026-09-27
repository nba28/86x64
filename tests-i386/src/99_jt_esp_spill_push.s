## 99_jt_esp_spill_push — a PIC switch table base spilled to an %ESP slot must
## survive argument pushes and a call before the dispatch reloads it.
##
## PvZ's libbass MOD effect switch (i386 0x127f6): `lea 0x3ef(%ebx),%edx;
## mov %edx,0x14(%esp)`, later `push x4; call; add $0x10,%esp`, then
## `mov 0x14(%esp),%ecx; mov (%ecx,%eax,4),%eax; add %ebx,%eax; jmp *%eax`.
## The detector dropped every %esp slot on any push/pop/call, never claimed the
## table, swept it as code, and the game jumped into a data mapping.
## exit 0 = intended case. OFF (M64_NO_JT_ESP_TRACK=1 at TRANSLATE time): the
## table keeps stale i386 offsets -> wrong case (exit 8) or a crash.
	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebx
	subl	$0x28, %esp
	calll	Lpic
Lpic:
	popl	%ebx                      ## %ebx = PIC anchor
	leal	(Ltbl-Lpic)(%ebx), %edx   ## table base ...
	movl	%edx, 0x14(%esp)          ## *** SPILLED to an %esp slot ***
	pushl	%eax                      ## four argument pushes ...
	pushl	%eax
	pushl	%eax
	pushl	%eax
	calll	Lleaf                     ## ... a call ...
	addl	$0x10, %esp               ## ... and the caller's cleanup
	movl	$2, %eax                  ## index
	movl	0x14(%esp), %ecx          ## *** RELOADED ***
	movl	(%ecx,%eax,4), %eax
	addl	%ebx, %eax
	jmpl	*%eax
Ltbl:
	.long	Lbad-Lpic
	.long	Lbad-Lpic
	.long	Lok-Lpic
	.long	Lbad-Lpic
Lbad:
	pushl	$8
	calll	_exit
	ud2
Lok:
	pushl	$0
	calll	_exit
	ud2
Lleaf:
	retl
