#!/usr/bin/env python3
"""Resolve __jt_tramp trampoline index -> imported symbol name for a translated dylib.

Structural: __DATA,__jt_ptrs is S_NON_LAZY_SYMBOL_POINTERS whose reserved1 indexes a
contiguous run in the indirect symbol table, one entry per slot; trampoline i in
__TEXT,__jt_tramp uses slot i.
"""
import struct, sys

path = sys.argv[1]
d = open(path, 'rb').read()

magic, cputype, cpusub, filetype, ncmds, sizeofcmds, flags, res = struct.unpack_from('<IiiIIIII', d, 0)
assert magic == 0xfeedfacf, hex(magic)
off = 32
symoff = nsyms = stroff = strsize = 0
indirectsymoff = nindirectsyms = 0
sections = {}
for _ in range(ncmds):
    cmd, cmdsize = struct.unpack_from('<II', d, off)
    if cmd == 0x19:  # LC_SEGMENT_64
        segname = d[off+8:off+24].rstrip(b'\0').decode()
        nsects = struct.unpack_from('<I', d, off+64)[0]
        so = off + 72
        for i in range(nsects):
            sn = d[so:so+16].rstrip(b'\0').decode()
            addr, size = struct.unpack_from('<QQ', d, so+32)
            offs, align, reloff, nreloc, sflags, r1, r2 = struct.unpack_from('<IIIIIII', d, so+48)
            sections[(segname, sn)] = dict(addr=addr, size=size, off=offs, flags=sflags, r1=r1, r2=r2)
            so += 80
    elif cmd == 0x2:  # LC_SYMTAB
        symoff, nsyms, stroff, strsize = struct.unpack_from('<IIII', d, off+8)
    elif cmd == 0xb:  # LC_DYSYMTAB
        vals = struct.unpack_from('<' + 'I'*18, d, off+8)
        indirectsymoff, nindirectsyms = vals[12], vals[13]
    off += cmdsize

def symname(idx):
    strx, ntype, nsect, ndesc, nvalue = struct.unpack_from('<IBBHQ', d, symoff + idx*16)
    end = d.index(b'\0', stroff+strx)
    return d[stroff+strx:end].decode()

ptrs = sections[('__DATA', '__jt_ptrs')]
tramp = sections[('__TEXT', '__jt_tramp')]
nslots = ptrs['size'] // 8
stride = 21
print('# __jt_tramp base=0x%x stride=%d nslots=%d reserved1=%d' % (tramp['addr'], stride, nslots, ptrs['r1']))
want = [int(x, 16) for x in sys.argv[2:]]
for i in range(nslots):
    isym = struct.unpack_from('<I', d, indirectsymoff + (ptrs['r1']+i)*4)[0]
    if isym & 0xc0000000:
        nm = '<local/abs>'
    else:
        nm = symname(isym)
    a = tramp['addr'] + i*stride
    if not want or a in want:
        print('0x%08x  slot[%3d] @0x%08x  %s' % (a, i, ptrs['addr']+i*8, nm))
