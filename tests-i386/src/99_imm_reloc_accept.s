# 99_imm_reloc_accept — an instruction's imm32 that carries a classic local
# reloc IS an address, whatever the instruction shape, in a relocatable
# (non-fixed-load) classic image too. The imm sibling of 99_disp_reloc_accept.
#
# Portal 2 libbinkmachox86.dylib passes its IO callback by literal address
# (`pushl $0x30aa`); the push heuristic only ran in fixed-load images, so the
# imm stayed the raw i386 vmaddr and Bink's IO thread called 0x30aa (SIGSEGV).
# Linked -pie -read_only_relocs suppress for 10.5: classic relocs, no LC_DYLD_INFO.
# ON: exit 42. OFF (M64_NO_IMM_RELOC_ACCEPT=1 at TRANSLATE time): the call
# through the raw address faults.
        .text
        .globl  _main
_main:
        pushl   $cb                     # imm32 code address (classic local reloc)
        popl    %eax
        call    *%eax
        subl    $12, %esp
        pushl   %eax
        call    _exit

        .p2align 4
cb:
        movl    $42, %eax
        ret
