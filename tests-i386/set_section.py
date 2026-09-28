#!/usr/bin/env python3
"""set_section.py <macho32> <sectname> <new_sectname> <flags> — rename a section
and set its flags in a thin 32-bit Mach-O, in place. Fixtures use it to recreate
a real binary's section header that the Snow Leopard linker won't emit itself
(it folds known code-section names into __text and drops the attributes of a
custom-named one). Fails if the section isn't found."""
import struct, sys

path, old, new, flags = sys.argv[1], sys.argv[2].encode(), sys.argv[3].encode(), int(sys.argv[4], 0)
assert len(new) <= 16
b = bytearray(open(path, "rb").read())
ncmds = struct.unpack_from("<I", b, 16)[0]
off, hit = 28, False                       # sizeof(mach_header)
for _ in range(ncmds):
    cmd, size = struct.unpack_from("<II", b, off)
    if cmd == 1:                           # LC_SEGMENT
        for i in range(struct.unpack_from("<I", b, off + 48)[0]):
            h = off + 56 + 68 * i          # sizeof(segment_command), section
            if b[h:h + 16].rstrip(b"\0") == old:
                b[h:h + 16] = new.ljust(16, b"\0")
                struct.pack_into("<I", b, h + 56, flags)   # section.flags
                hit = True
    off += size
if not hit:
    sys.exit(f"{path}: no section {old.decode()}")
open(path, "wb").write(b)
