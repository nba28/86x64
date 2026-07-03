## 70_sse_sib_const_table — SSE-prefixed absolute SIB table load (movsd).
##
## Mirrors Quinn's -[QuinnGame enableTimer]:
##   movsd 0xb2ed8(,%eax,8), %xmm0          (67 F2 0F 10 04 C5 disp32 translated)
## loading the repeating drop-timer's NSTimeInterval from the per-level speed
## table in __TEXT (doubles [2.0, 0.8, 0.65, ...]).
##
## The translator rebases the absolute `[disp32+index*8]` table base at
## transform time, but the RUNTIME slide patchers (objc_slide.c
## patch_text_abs32 + wrapper_setup.c) matched only bare opcodes and the plain
## 0F escape (movsx/movzx) — the SSE mandatory prefix F2/F3/66 before 0F was
## not decoded, so the rebased disp32 was never slid. The movsd then read from
## the PRE-slide vmaddr: garbage bytes -> a tiny-positive double (e.g.
## 0x3CCCCCCD00000000 ~ 4e-16) -> NSTimer interval < 1 TSR tick on a REPEATING
## timer -> CoreFoundation's "A CFRunLoopTimer with an interval of 0 is set to
## repeat" ud2 trap in __CFRunLoopDoTimer (deterministic SIGILL at fire time;
## crashlogs Quinn-2026-07-03-044024/34/40, r14=0 = the ToTSR==0 leg).
##
## Read _dtbl[1] = 0.8 and compare the raw IEEE-754 words. Exit 99 on match;
## 1 on a garbage read (unslid disp32), SIGSEGV if the unslid page is unmapped.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	subl	$16, %esp
	movl	$1, %eax                  ## idx = 1
	movsd	_dtbl(,%eax,8), %xmm0     ## F2 0F 10 04 C5 disp32: dtbl[idx]
	movsd	%xmm0, (%esp)
	movl	(%esp), %eax              ## low word of the double
	movl	4(%esp), %edx             ## high word
	movl	$99, %ecx
	cmpl	$0x9999999A, %eax         ## 0.8 = 0x3FE999999999999A
	jne	Lbad
	cmpl	$0x3FE99999, %edx
	je	Lout
Lbad:
	movl	$1, %ecx
Lout:
	movl	%ecx, (%esp)
	calll	_exit
	ud2

	.section __TEXT,__const
	.p2align 3
_dtbl:
	.quad	0x4000000000000000        ## [0] 2.0
	.quad	0x3FE999999999999A        ## [1] 0.8   <- the read
	.quad	0x3FE4CCCCCCCCCCCD        ## [2] 0.65
	.quad	0x3FDEB851EB851EB8        ## [3] 0.48
