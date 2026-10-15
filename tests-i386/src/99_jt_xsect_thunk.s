## 99_jt_xsect_thunk — a GCC PIC switch table survives the x86_64 re-parse when
## its get_pc_thunk lives in another section.
##
## Halo (stripped GCC, thunk in __textcoal_nt) dispatches `movl d(%ebx,%eax,4),
## %eax; addl %ebx,%eax; jmp *%eax` over inline tables. The transform claims them,
## but the later x86_64 passes (modify, convert) re-parse our own output and
## found their thunk only by scanning the CURRENT section, so __text got no
## anchor, the tables were decoded as code, and a table byte like 74 04 (`je +4`)
## re-encoded as a 6-byte jcc, shifting every entry after it. Halo's digit-key
## switch at 0x2ee904 then jumped into its own table (SIGSEGV).
##
## Same shape here: the thunk in __picthunk (renamed to __textcoal_nt after the
## link, as in 62_pic_xsection_thunk_stripped) and 24 six-case switches whose
## case bodies sit at spread-out anchor offsets (0x180..0x600), so some
## translated tables contain bytes that decode as short branches, as Halo's did.
## Which ones do depends on the translated layout; the set is generated once
## (seed 3 of a small generator) and checked in. _main runs every case of every
## switch; exit 42 iff the sum of the picked bodies matches. OFF:
## M64_NO_JT_XSECT_THUNK=1 at TRANSLATE time (measured: SIGBUS).
	.section __TEXT,__text,regular,pure_instructions
	.globl _main
	.p2align 4, 0x90
_main:
	pushl %ebp
	movl %esp, %ebp
	pushl %esi
	pushl %edi
	pushl %ebx
	subl $12, %esp
	xorl %edi, %edi
	xorl %esi, %esi
L0loop:
	movl %esi, (%esp)
	call _pick0
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L0loop
	xorl %esi, %esi
L1loop:
	movl %esi, (%esp)
	call _pick1
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L1loop
	xorl %esi, %esi
L2loop:
	movl %esi, (%esp)
	call _pick2
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L2loop
	xorl %esi, %esi
L3loop:
	movl %esi, (%esp)
	call _pick3
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L3loop
	xorl %esi, %esi
L4loop:
	movl %esi, (%esp)
	call _pick4
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L4loop
	xorl %esi, %esi
L5loop:
	movl %esi, (%esp)
	call _pick5
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L5loop
	xorl %esi, %esi
L6loop:
	movl %esi, (%esp)
	call _pick6
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L6loop
	xorl %esi, %esi
L7loop:
	movl %esi, (%esp)
	call _pick7
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L7loop
	xorl %esi, %esi
L8loop:
	movl %esi, (%esp)
	call _pick8
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L8loop
	xorl %esi, %esi
L9loop:
	movl %esi, (%esp)
	call _pick9
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L9loop
	xorl %esi, %esi
L10loop:
	movl %esi, (%esp)
	call _pick10
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L10loop
	xorl %esi, %esi
L11loop:
	movl %esi, (%esp)
	call _pick11
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L11loop
	xorl %esi, %esi
L12loop:
	movl %esi, (%esp)
	call _pick12
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L12loop
	xorl %esi, %esi
L13loop:
	movl %esi, (%esp)
	call _pick13
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L13loop
	xorl %esi, %esi
L14loop:
	movl %esi, (%esp)
	call _pick14
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L14loop
	xorl %esi, %esi
L15loop:
	movl %esi, (%esp)
	call _pick15
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L15loop
	xorl %esi, %esi
L16loop:
	movl %esi, (%esp)
	call _pick16
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L16loop
	xorl %esi, %esi
L17loop:
	movl %esi, (%esp)
	call _pick17
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L17loop
	xorl %esi, %esi
L18loop:
	movl %esi, (%esp)
	call _pick18
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L18loop
	xorl %esi, %esi
L19loop:
	movl %esi, (%esp)
	call _pick19
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L19loop
	xorl %esi, %esi
L20loop:
	movl %esi, (%esp)
	call _pick20
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L20loop
	xorl %esi, %esi
L21loop:
	movl %esi, (%esp)
	call _pick21
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L21loop
	xorl %esi, %esi
L22loop:
	movl %esi, (%esp)
	call _pick22
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L22loop
	xorl %esi, %esi
L23loop:
	movl %esi, (%esp)
	call _pick23
	addl %eax, %edi
	incl %esi
	cmpl $6, %esi
	jl L23loop
	cmpl $16863, %edi
	movl $42, %eax
	movl $1, %ecx
	cmovne %ecx, %eax
	movl %eax, (%esp)
	call _exit
	.p2align 4, 0x90
_pick0:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La0:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld0
	movl Lt0-La0(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt0:
	.long Lc0_0-La0
	.long Lc0_1-La0
	.long Lc0_2-La0
	.long Lc0_1-La0
	.long Lc0_2-La0
	.long Lc0_2-La0
Ld0:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 747 - (. - La0), 0x90
Lc0_0:
	movl $1, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1218 - (. - La0), 0x90
Lc0_1:
	movl $2, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1293 - (. - La0), 0x90
Lc0_2:
	movl $3, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick1:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La1:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld1
	movl Lt1-La1(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt1:
	.long Lc1_1-La1
	.long Lc1_1-La1
	.long Lc1_2-La1
	.long Lc1_0-La1
	.long Lc1_0-La1
	.long Lc1_2-La1
Ld1:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 402 - (. - La1), 0x90
Lc1_0:
	movl $11, %eax
	popl %ebx
	popl %ebp
	ret
	.space 483 - (. - La1), 0x90
Lc1_1:
	movl $12, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1314 - (. - La1), 0x90
Lc1_2:
	movl $13, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick2:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La2:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld2
	movl Lt2-La2(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt2:
	.long Lc2_1-La2
	.long Lc2_1-La2
	.long Lc2_2-La2
	.long Lc2_0-La2
	.long Lc2_0-La2
	.long Lc2_2-La2
Ld2:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1104 - (. - La2), 0x90
Lc2_0:
	movl $21, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1212 - (. - La2), 0x90
Lc2_1:
	movl $22, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1227 - (. - La2), 0x90
Lc2_2:
	movl $23, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick3:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La3:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld3
	movl Lt3-La3(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt3:
	.long Lc3_2-La3
	.long Lc3_0-La3
	.long Lc3_2-La3
	.long Lc3_0-La3
	.long Lc3_0-La3
	.long Lc3_2-La3
Ld3:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 615 - (. - La3), 0x90
Lc3_0:
	movl $31, %eax
	popl %ebx
	popl %ebp
	ret
	.space 981 - (. - La3), 0x90
Lc3_1:
	movl $32, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1185 - (. - La3), 0x90
Lc3_2:
	movl $33, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick4:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La4:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld4
	movl Lt4-La4(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt4:
	.long Lc4_1-La4
	.long Lc4_1-La4
	.long Lc4_2-La4
	.long Lc4_2-La4
	.long Lc4_1-La4
	.long Lc4_2-La4
Ld4:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 429 - (. - La4), 0x90
Lc4_0:
	movl $41, %eax
	popl %ebx
	popl %ebp
	ret
	.space 447 - (. - La4), 0x90
Lc4_1:
	movl $42, %eax
	popl %ebx
	popl %ebp
	ret
	.space 846 - (. - La4), 0x90
Lc4_2:
	movl $43, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick5:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La5:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld5
	movl Lt5-La5(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt5:
	.long Lc5_2-La5
	.long Lc5_1-La5
	.long Lc5_0-La5
	.long Lc5_1-La5
	.long Lc5_0-La5
	.long Lc5_0-La5
Ld5:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 990 - (. - La5), 0x90
Lc5_0:
	movl $51, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1038 - (. - La5), 0x90
Lc5_1:
	movl $52, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1500 - (. - La5), 0x90
Lc5_2:
	movl $53, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick6:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La6:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld6
	movl Lt6-La6(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt6:
	.long Lc6_1-La6
	.long Lc6_2-La6
	.long Lc6_1-La6
	.long Lc6_2-La6
	.long Lc6_1-La6
	.long Lc6_1-La6
Ld6:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 591 - (. - La6), 0x90
Lc6_0:
	movl $61, %eax
	popl %ebx
	popl %ebp
	ret
	.space 717 - (. - La6), 0x90
Lc6_1:
	movl $62, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1143 - (. - La6), 0x90
Lc6_2:
	movl $63, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick7:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La7:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld7
	movl Lt7-La7(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt7:
	.long Lc7_1-La7
	.long Lc7_2-La7
	.long Lc7_2-La7
	.long Lc7_1-La7
	.long Lc7_2-La7
	.long Lc7_0-La7
Ld7:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 975 - (. - La7), 0x90
Lc7_0:
	movl $71, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1161 - (. - La7), 0x90
Lc7_1:
	movl $72, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1263 - (. - La7), 0x90
Lc7_2:
	movl $73, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick8:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La8:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld8
	movl Lt8-La8(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt8:
	.long Lc8_1-La8
	.long Lc8_2-La8
	.long Lc8_2-La8
	.long Lc8_2-La8
	.long Lc8_0-La8
	.long Lc8_2-La8
Ld8:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 426 - (. - La8), 0x90
Lc8_0:
	movl $81, %eax
	popl %ebx
	popl %ebp
	ret
	.space 900 - (. - La8), 0x90
Lc8_1:
	movl $82, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1431 - (. - La8), 0x90
Lc8_2:
	movl $83, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick9:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La9:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld9
	movl Lt9-La9(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt9:
	.long Lc9_2-La9
	.long Lc9_0-La9
	.long Lc9_2-La9
	.long Lc9_2-La9
	.long Lc9_0-La9
	.long Lc9_2-La9
Ld9:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 885 - (. - La9), 0x90
Lc9_0:
	movl $91, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1215 - (. - La9), 0x90
Lc9_1:
	movl $92, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1260 - (. - La9), 0x90
Lc9_2:
	movl $93, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick10:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La10:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld10
	movl Lt10-La10(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt10:
	.long Lc10_0-La10
	.long Lc10_0-La10
	.long Lc10_1-La10
	.long Lc10_2-La10
	.long Lc10_1-La10
	.long Lc10_0-La10
Ld10:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 792 - (. - La10), 0x90
Lc10_0:
	movl $101, %eax
	popl %ebx
	popl %ebp
	ret
	.space 819 - (. - La10), 0x90
Lc10_1:
	movl $102, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1263 - (. - La10), 0x90
Lc10_2:
	movl $103, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick11:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La11:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld11
	movl Lt11-La11(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt11:
	.long Lc11_0-La11
	.long Lc11_0-La11
	.long Lc11_1-La11
	.long Lc11_1-La11
	.long Lc11_1-La11
	.long Lc11_0-La11
Ld11:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 486 - (. - La11), 0x90
Lc11_0:
	movl $111, %eax
	popl %ebx
	popl %ebp
	ret
	.space 912 - (. - La11), 0x90
Lc11_1:
	movl $112, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1014 - (. - La11), 0x90
Lc11_2:
	movl $113, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick12:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La12:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld12
	movl Lt12-La12(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt12:
	.long Lc12_0-La12
	.long Lc12_1-La12
	.long Lc12_2-La12
	.long Lc12_2-La12
	.long Lc12_1-La12
	.long Lc12_2-La12
Ld12:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 450 - (. - La12), 0x90
Lc12_0:
	movl $121, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1311 - (. - La12), 0x90
Lc12_1:
	movl $122, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1326 - (. - La12), 0x90
Lc12_2:
	movl $123, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick13:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La13:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld13
	movl Lt13-La13(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt13:
	.long Lc13_0-La13
	.long Lc13_1-La13
	.long Lc13_0-La13
	.long Lc13_0-La13
	.long Lc13_0-La13
	.long Lc13_2-La13
Ld13:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 744 - (. - La13), 0x90
Lc13_0:
	movl $131, %eax
	popl %ebx
	popl %ebp
	ret
	.space 810 - (. - La13), 0x90
Lc13_1:
	movl $132, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1158 - (. - La13), 0x90
Lc13_2:
	movl $133, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick14:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La14:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld14
	movl Lt14-La14(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt14:
	.long Lc14_1-La14
	.long Lc14_1-La14
	.long Lc14_2-La14
	.long Lc14_1-La14
	.long Lc14_0-La14
	.long Lc14_2-La14
Ld14:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 432 - (. - La14), 0x90
Lc14_0:
	movl $141, %eax
	popl %ebx
	popl %ebp
	ret
	.space 687 - (. - La14), 0x90
Lc14_1:
	movl $142, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1206 - (. - La14), 0x90
Lc14_2:
	movl $143, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick15:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La15:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld15
	movl Lt15-La15(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt15:
	.long Lc15_1-La15
	.long Lc15_0-La15
	.long Lc15_1-La15
	.long Lc15_1-La15
	.long Lc15_1-La15
	.long Lc15_2-La15
Ld15:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 447 - (. - La15), 0x90
Lc15_0:
	movl $151, %eax
	popl %ebx
	popl %ebp
	ret
	.space 864 - (. - La15), 0x90
Lc15_1:
	movl $152, %eax
	popl %ebx
	popl %ebp
	ret
	.space 903 - (. - La15), 0x90
Lc15_2:
	movl $153, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick16:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La16:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld16
	movl Lt16-La16(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt16:
	.long Lc16_2-La16
	.long Lc16_2-La16
	.long Lc16_0-La16
	.long Lc16_2-La16
	.long Lc16_2-La16
	.long Lc16_1-La16
Ld16:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 975 - (. - La16), 0x90
Lc16_0:
	movl $161, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1296 - (. - La16), 0x90
Lc16_1:
	movl $162, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1371 - (. - La16), 0x90
Lc16_2:
	movl $163, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick17:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La17:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld17
	movl Lt17-La17(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt17:
	.long Lc17_2-La17
	.long Lc17_0-La17
	.long Lc17_1-La17
	.long Lc17_1-La17
	.long Lc17_1-La17
	.long Lc17_2-La17
Ld17:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1044 - (. - La17), 0x90
Lc17_0:
	movl $171, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1356 - (. - La17), 0x90
Lc17_1:
	movl $172, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1488 - (. - La17), 0x90
Lc17_2:
	movl $173, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick18:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La18:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld18
	movl Lt18-La18(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt18:
	.long Lc18_0-La18
	.long Lc18_1-La18
	.long Lc18_2-La18
	.long Lc18_1-La18
	.long Lc18_0-La18
	.long Lc18_1-La18
Ld18:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 849 - (. - La18), 0x90
Lc18_0:
	movl $181, %eax
	popl %ebx
	popl %ebp
	ret
	.space 903 - (. - La18), 0x90
Lc18_1:
	movl $182, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1224 - (. - La18), 0x90
Lc18_2:
	movl $183, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick19:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La19:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld19
	movl Lt19-La19(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt19:
	.long Lc19_0-La19
	.long Lc19_0-La19
	.long Lc19_2-La19
	.long Lc19_2-La19
	.long Lc19_1-La19
	.long Lc19_1-La19
Ld19:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1287 - (. - La19), 0x90
Lc19_0:
	movl $191, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1329 - (. - La19), 0x90
Lc19_1:
	movl $192, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1353 - (. - La19), 0x90
Lc19_2:
	movl $193, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick20:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La20:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld20
	movl Lt20-La20(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt20:
	.long Lc20_2-La20
	.long Lc20_1-La20
	.long Lc20_2-La20
	.long Lc20_1-La20
	.long Lc20_0-La20
	.long Lc20_2-La20
Ld20:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 924 - (. - La20), 0x90
Lc20_0:
	movl $201, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1317 - (. - La20), 0x90
Lc20_1:
	movl $202, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1425 - (. - La20), 0x90
Lc20_2:
	movl $203, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick21:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La21:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld21
	movl Lt21-La21(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt21:
	.long Lc21_1-La21
	.long Lc21_1-La21
	.long Lc21_2-La21
	.long Lc21_1-La21
	.long Lc21_1-La21
	.long Lc21_2-La21
Ld21:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 414 - (. - La21), 0x90
Lc21_0:
	movl $211, %eax
	popl %ebx
	popl %ebp
	ret
	.space 477 - (. - La21), 0x90
Lc21_1:
	movl $212, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1422 - (. - La21), 0x90
Lc21_2:
	movl $213, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick22:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La22:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld22
	movl Lt22-La22(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt22:
	.long Lc22_1-La22
	.long Lc22_0-La22
	.long Lc22_1-La22
	.long Lc22_1-La22
	.long Lc22_2-La22
	.long Lc22_1-La22
Ld22:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 654 - (. - La22), 0x90
Lc22_0:
	movl $221, %eax
	popl %ebx
	popl %ebp
	ret
	.space 873 - (. - La22), 0x90
Lc22_1:
	movl $222, %eax
	popl %ebx
	popl %ebp
	ret
	.space 1305 - (. - La22), 0x90
Lc22_2:
	movl $223, %eax
	popl %ebx
	popl %ebp
	ret
	.p2align 4, 0x90
_pick23:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	call ___i686.get_pc_thunk.bx
La23:
	movl 8(%ebp), %eax
	cmpl $5, %eax
	ja Ld23
	movl Lt23-La23(%ebx,%eax,4), %eax
	addl %ebx, %eax
	jmp *%eax
	.p2align 2, 0x90
Lt23:
	.long Lc23_0-La23
	.long Lc23_2-La23
	.long Lc23_2-La23
	.long Lc23_2-La23
	.long Lc23_0-La23
	.long Lc23_1-La23
Ld23:
	xorl %eax, %eax
	popl %ebx
	popl %ebp
	ret
	.space 543 - (. - La23), 0x90
Lc23_0:
	movl $231, %eax
	popl %ebx
	popl %ebp
	ret
	.space 843 - (. - La23), 0x90
Lc23_1:
	movl $232, %eax
	popl %ebx
	popl %ebp
	ret
	.space 963 - (. - La23), 0x90
Lc23_2:
	movl $233, %eax
	popl %ebx
	popl %ebp
	ret
	.section __TEXT,__picthunk,regular,pure_instructions
___i686.get_pc_thunk.bx:
	movl (%esp), %ebx
	ret

