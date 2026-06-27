## 33_pic_anchor_regreg_overwrite — a PIC-anchor register reassigned by a
## register-to-register move must LOSE its anchor: a later base+index access
## off the reassigned register is a plain pointer deref, NOT a PIC-anchored
## table reference.
##
## Mirrors Portal 2 CLoggingSystem::LogDirect (the s18 SIGSEGV):
##     calll Lpic0; popl %edi            ; %edi = get_pc_thunk PIC anchor
##     leal  (_obj - Lpic0)(%edi), %eax  ; legit PIC ref off the anchor
##     movl  %eax, %edi                  ; %edi REASSIGNED from a real pointer
##     movl  0x80(%edi,%ebx,4), %esi     ; base+index off the pointer  <-- plain deref
##
## DetectPicAnchoredDisps seeds %edi as an anchor at the pop. The final access
## uses a disp32 (0x80 > 127) so it matches the implemented SIB-indexed PIC
## rewrite, which — if the stale anchor is not cleared — rewrites it to a
## rip-relative absolute ref `lea r11,[rip+(anchor_vmaddr+0x80)] ; mov
## (%r11,%ebx,4)` reading the __TEXT padding (0x90 nops) instead of _obj+0x80.
## The reg->reg `movl %eax,%edi` must clear the stale anchor so the access stays
## a base+index deref off %edi (= &_obj).
##
## _obj is 0x84 bytes; _obj+0x80 = 0x42.  Exit code = 0x42 = 66 on success;
## without the fix the read lands in the __TEXT nop pad (0x90909090 -> exit 144)
## or faults.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main

	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$16, %esp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	## Establish PIC anchor: %edi = vmaddr of Lpic0
	calll	Lpic0
Lpic0:
	popl	%edi

	## Legit PIC ref off the anchor: %eax = &_obj (anchor is real here, so
	## DetectPicAnchoredDisps definitely tracks %edi as a PIC anchor).
	leal	(_obj - Lpic0)(%edi), %eax

	## Reassign %edi from %eax via a register-to-register move. %edi is now a
	## plain data pointer (&_obj), NOT the PIC base. This is the bug trigger.
	movl	%eax, %edi

	## Base+index access off the reassigned %edi, disp32 0x80: %esi = _obj+0x80.
	xorl	%ebx, %ebx
	movl	0x80(%edi,%ebx,4), %esi

	## exit(%esi)
	pushl	%esi
	calll	_exit
	ud2

	## Pad __TEXT so (Lpic0 + 0x80) is a valid in-section target — that is where
	## a stale-anchor mis-rewrite would read from (0x90 nop bytes).
	.p2align 4, 0x90
	.space	0x100, 0x90

	.section __DATA,__data
	.p2align 2
_obj:
	.space	0x80, 0x00
	.long	0x00000042
