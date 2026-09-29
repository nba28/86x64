# 99_disp_reloc_accept — an instruction's disp32 that carries a classic local
# reloc IS an address, in a relocatable (non-fixed-load) classic image too.
#
# Portal 2 libbinkmachox86.dylib is classic non-PIC code with local relocs; its
# slot allocator returns `leal 0x30e60(%edx),%eax` (base register + absolute
# __bss address). Only a fixed-load image had that shape captured, so the disp
# stayed the raw i386 vmaddr and BinkOpen zeroed unmapped memory (SIGSEGV).
# Linked -pie -read_only_relocs suppress for 10.5: classic relocs, no LC_DYLD_INFO.
# ON: exit 42. OFF (M64_NO_DISP_RELOC_ACCEPT=1 at TRANSLATE time): the store
# through the raw address faults.
        .text
        .globl  _main
_main:
        xorl    %edx, %edx
        leal    _slots(%edx), %eax      # base + absolute disp32 (classic local reloc)
        movl    $42, (%eax)
        movl    _slots, %ecx            # absolute read-back (already relocated)
        subl    $12, %esp
        pushl   %ecx
        call    _exit

        .zerofill __DATA,__bss,_slots,64,4
