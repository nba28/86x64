#!/usr/bin/env python3
"""text_addr_of.py <macho32> <hexbytes> — print the vmaddr (hex) of the first
occurrence of <hexbytes> inside __TEXT,__text of a thin 32-bit Mach-O. Fixtures
use it to find where a linker placed a marker. Fails if it isn't there."""
import struct, sys

path, needle = sys.argv[1], bytes.fromhex(sys.argv[2])
b = open(path, "rb").read()
off = 28                                   # sizeof(mach_header)
for _ in range(struct.unpack_from("<I", b, 16)[0]):
    cmd, size = struct.unpack_from("<II", b, off)
    if cmd == 1:                           # LC_SEGMENT
        for i in range(struct.unpack_from("<I", b, off + 48)[0]):
            h = off + 56 + 68 * i
            if b[h:h + 16].rstrip(b"\0") == b"__text":
                addr, size_, fileoff = struct.unpack_from("<III", b, h + 32)
                o = b[fileoff:fileoff + size_].find(needle)
                if o < 0:
                    sys.exit(f"{path}: {sys.argv[2]} not in __text")
                print(hex(addr + o))
                sys.exit(0)
    off += size
sys.exit(f"{path}: no __text")
