/*
 * 83_pic_anchor_int3.s — PIC anchor survives past int3 (0xCC breakpoint trap).
 *
 * Pattern from Portal 2 engine.dylib CVoxelTree::CVoxelTree():
 *
 *   call Lpic; pop %edi          ; PIC anchor establishes anchors[EDI]
 *   ...
 *   calll SomeFunc               ; some function call
 *   testb %al, %al
 *   je   Lslow                   ; not-in-debug path: goto Lslow (calls raise)
 *   int3                         ; abort trap (dead fall-through)
 *   jmp  Lafter                  ; dead: snapshot[Lafter] = {} without fix
 * Lslow:
 *   calll _raise                 ; system call
 * Lafter:                        ; snapshot intersect kills anchors[EDI] without fix
 *   movl disp(%edi), %eax        ; MUST be pic_anchored: EDI is still the anchor!
 *
 * BUG: DetectPicAnchoredDisps treated int3 as XED_CATEGORY_INTERRUPT and called
 * anchors.clear() / anchor_slots.clear(). The subsequent `jmp Lafter` then
 * recorded an EMPTY branch_anchor_snap[Lafter], which intersected with the
 * Lslow-path snapshot and cleared it. At Lafter, anchors[EDI] was lost and the
 * load was left with a raw i386 disp32 -> wrong address or crash.
 *
 * FIX: int3 (XED_ICLASS_INT3) is excluded from the INTERRUPT anchor-clear in
 * DetectPicAnchoredDisps (section.cc step 6). int3 is a no-op for anchor
 * tracking; `jmp Lafter` then captures {EDI:anchor} and the intersection at
 * Lafter preserves it. A PIC-anchored load AFTER an int3/jmp bypass reads the
 * correct relocated value, not garbage.
 *
 * Validation: _main calls exit(0) on success (read returns 83 as expected).
 */

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi

	/* PIC anchor: anchors[EDI] = L83pic */
	calll	L83pic
L83pic:
	popl	%edi

	/*
	 * Assertion-guard idiom (mirrors engine.dylib):
	 *
	 *   testb %al, %al
	 *   je   L83slow     ; branch taken (eax=0) -> Lslow -> Lafter
	 *   int3             ; dead: anchor-clear BUG fired here
	 *   jmp  L83after    ; dead: was recording {} snapshot for L83after
	 * L83slow:
	 *   (real code: would call raise(), but we skip it to keep the test clean)
	 * L83after:
	 *   movl ...(%edi), %eax   ; BUG: anchor was lost -> wrong load
	 */
	xorl	%eax, %eax           /* eax = 0: simulate IsInDebugSession() = false */
	testb	%al, %al
	je	L83slow              /* always taken */
	int3                         /* dead path: this is where anchors.clear() fired */
	jmp	L83after             /* dead fall-through; snapshot[L83after] was {} */
L83slow:
	/* normal continuation (skips int3); anchor[EDI] survives since callee-saved */
L83after:
	/*
	 * anchors[EDI] = L83pic must still be live after the fix.
	 * The translator rewrites this to rip-relative so it reads L83data.
	 * Without the fix the raw i386 displacement (L83data - L83pic) is kept
	 * in x86_64 code -> garbage address -> wrong value.
	 */
	movl	(L83data - L83pic)(%edi), %eax   /* should load 83 */

	/* exit(0) on success, exit(eax) on failure */
	subl	$83, %eax
	pushl	%eax
	calll	_exit
	ud2

	.section __DATA,__data
	.p2align 2
L83data:
	.long	83
