# 99_entry_pad_fallthrough — code that FALLS THROUGH into a function symbol.
#
# The translator pads a function entry to an even address (Itanium member-
# function-pointer rule). The pad was 0x00: harmless after a ret, but an
# alternate entry just before the label (Portal 2 libbink: `movl 4(%esp),%eax`
# falls into _RADCB_resume_handler) executes it, and `00 48 8d ...` decodes as
# `addb %cl,-0x73(%rax)`, swallowing the translated `pushl %esi` behind it. The
# frame is then off by 4 and `ret` pops a local.
# Four entry pairs with 1..4-byte lead-ins, so translated layouts put at least
# one label at an odd address. Each lead-in loads %eax (the stack arg) and falls
# into a function that pushes/pops %esi and adds a constant.
# ON: exit 42. OFF (M64_ZERO_ENTRY_PAD=1 at TRANSLATE time): wrong sum or fault.
        .text
        .globl  _main
_main:
        movl    %esp, %ebp
        andl    $-16, %esp
        xorl    %ebx, %ebx
        pushl   $1
        call    _alt1
        addl    $4, %esp
        addl    %eax, %ebx          # 1+1
        pushl   $2
        call    _alt2
        addl    $4, %esp
        addl    %eax, %ebx          # 2+2
        pushl   $3
        call    _alt3
        addl    $4, %esp
        addl    %eax, %ebx          # 3+3
        pushl   $4
        call    _alt4
        addl    $4, %esp
        addl    %eax, %ebx          # 4+4 -> ebx = 20
        addl    $22, %ebx           # 42
        subl    $12, %esp
        pushl   %ebx
        call    _exit

        .globl  _alt1
_alt1:  movl    4(%esp), %eax
        .globl  _f1
_f1:    pushl   %esi
        movl    $1, %esi
        addl    %esi, %eax
        popl    %esi
        ret

        .globl  _alt2
_alt2:  nop
        movl    4(%esp), %eax
        .globl  _f2
_f2:    pushl   %esi
        movl    $2, %esi
        addl    %esi, %eax
        popl    %esi
        ret

        .globl  _alt3
_alt3:  nop
        nop
        movl    4(%esp), %eax
        .globl  _f3
_f3:    pushl   %esi
        movl    $3, %esi
        addl    %esi, %eax
        popl    %esi
        ret

        .globl  _alt4
_alt4:  nop
        nop
        nop
        movl    4(%esp), %eax
        .globl  _f4
_f4:    pushl   %esi
        movl    $4, %esi
        addl    %esi, %eax
        popl    %esi
        ret
