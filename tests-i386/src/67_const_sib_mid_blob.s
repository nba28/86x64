## 67_const_sib_mid_blob — absolute SIB table access whose disp32 lands MID-BLOB
## inside a READ-ONLY __TEXT,__const table.
##
## Mirrors Quinn's -[QuinnGame incrementScoreWithLastHeight:rowsKilled:clearedBoard:]:
##   movswl 0xb2f42(,%eax,8),%eax        (crashlog Quinn-2026-07-03-020407.ips)
## where 0xb2f42 = <const table base> + 2 (a nonzero field offset folded into
## the displacement), so the disp32 addresses the MIDDLE of the __const blob,
## not its start symbol.
##
## The translator rebases an absolute `[disp32+index*scale]` table base via
## Resolver::resolve() (exact-key). A mid-blob disp32 misses the exact key and
## must fall back to resolve_containing() (nearest blob + offset). That fallback
## was gated to WRITABLE __DATA, so a mid-blob reference into read-only __const
## was left UN-REBASED: the raw i386 vmaddr shipped verbatim and, dereferenced in
## the x86_64 process, points at unmapped low memory -> SIGSEGV (Quinn crashed on
## a line-clear reading 0xb2f62). Fix: admit any non-__OBJC segment (incl.
## read-only const/literal tables) to the containing fallback — an indexed
## operand's disp32 is always a real dereferenced table base.
##
## _ctbl is 8-byte rows of 4 shorts. Read (_ctbl+2)[idx*8] with idx=1:
##   EA = _ctbl + 2 + 1*8 = _ctbl + 10  = short index 5 = 99.
## Exit code = 99 on success; SIGSEGV (exit 139) or a wrong value without the fix.

	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	movl	$1, %eax                  ## idx = 1
	movswl	_ctbl+2(,%eax,8), %eax    ## mid-blob const load: (_ctbl+2)+idx*8
	pushl	%eax                      ## exit code = loaded short
	calll	_exit
	ud2

	.section __TEXT,__const
	.p2align 3
_ctbl:
	.short	0, 0, 0, 0                ## row 0  (bytes  0..7)
	.short	0, 99, 0, 0               ## row 1  (bytes  8..15): short[5] = 99 @ _ctbl+10
