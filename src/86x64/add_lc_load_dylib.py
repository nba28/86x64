#!/usr/bin/env python3
"""Splice one or more LC_LOAD_DYLIB load commands into a 64-bit Mach-O.

Uses the headerpad slack reserved by the 86x64 translator (MACHO_HEADERPAD=1024).
The target file must be thin (not FAT) x86_64 Mach-O.

Usage:
   add_lc_load_dylib.py <macho> <path-to-add> [<path-to-add> ...]
"""
import struct, sys, os

MH_MAGIC_64 = 0xFEEDFACF
LC_LOAD_DYLIB = 0xC
LC_REEXPORT_DYLIB = 0x8000001F  # LC_REQ_DYLD | 0x1F
LC_SEGMENT_64 = 0x19
HEADER_SIZE = 32  # mach_header_64
LC_TYPES = {"load": LC_LOAD_DYLIB, "reexport": LC_REEXPORT_DYLIB}

def fail(msg):
    print(f"error: {msg}", file=sys.stderr)
    sys.exit(1)

def build_dylib_lc(path: str, cmd: int) -> bytes:
    """dylib_command + name string, padded to 8-byte alignment.
    cmd(4) cmdsize(4) name_offset(4) timestamp(4) current(4) compat(4)  = 24 bytes header
    + name (nul-terminated) padded to 8."""
    name_bytes = path.encode("utf-8") + b"\x00"
    fixed = 24
    total = fixed + len(name_bytes)
    padded = (total + 7) & ~7
    pad = padded - total
    out = struct.pack(
        "<IIIIII",
        cmd,
        padded,
        24,           # name offset within this command
        0,            # timestamp
        0x00010000,   # current_version 1.0.0
        0x00010000,   # compatibility_version 1.0.0
    ) + name_bytes + (b"\x00" * pad)
    assert len(out) == padded
    return out

def main():
    argv = sys.argv[1:]
    cmd = LC_LOAD_DYLIB
    if argv and argv[0].startswith("--cmd="):
        kind = argv.pop(0).split("=", 1)[1]
        if kind not in LC_TYPES:
            fail(f"unknown --cmd type: {kind} (use one of {list(LC_TYPES)})")
        cmd = LC_TYPES[kind]
    if len(argv) < 2:
        print(__doc__)
        sys.exit(2)
    target = argv[0]
    new_paths = argv[1:]
    with open(target, "r+b") as f:
        data = bytearray(f.read())

    # Header
    magic = struct.unpack_from("<I", data, 0)[0]
    if magic != MH_MAGIC_64:
        fail(f"not a thin 64-bit little-endian Mach-O (magic=0x{magic:x})")
    ncmds, sizeofcmds = struct.unpack_from("<II", data, 16)
    print(f"before: ncmds={ncmds} sizeofcmds={sizeofcmds}")

    # End of existing LCs
    lc_end = HEADER_SIZE + sizeofcmds

    # Find first segment's file offset to bound the headerpad
    off = HEADER_SIZE
    first_seg_fileoff = None
    for _ in range(ncmds):
        ic, cmdsize = struct.unpack_from("<II", data, off)
        if ic == LC_SEGMENT_64:
            # struct segment_command_64: cmd(4) cmdsize(4) segname(16) vmaddr(8) vmsize(8) fileoff(8)
            segname = data[off + 8 : off + 24].rstrip(b"\x00").decode("ascii", "replace")
            fileoff = struct.unpack_from("<Q", data, off + 32)[0]
            if segname == "__TEXT":
                first_seg_fileoff = fileoff
                # __TEXT's first section's data starts at offset = fileoff + sectionheaders + headerpad
                # but the binding restriction is just: LC bytes must fit before any segment's
                # file data, which for __TEXT typically starts at fileoff + sizeof(headers).
                # The translator's MACHO_HEADERPAD=1024 reserves room within __TEXT before
                # the first section. The first section's offset is what really bounds us.
                break
        off += cmdsize

    # Look up the first section's file offset within __TEXT for the real cap
    cap = None
    off = HEADER_SIZE
    for _ in range(ncmds):
        ic, cmdsize = struct.unpack_from("<II", data, off)
        if ic == LC_SEGMENT_64:
            segname = data[off + 8 : off + 24].rstrip(b"\x00").decode("ascii", "replace")
            if segname == "__TEXT":
                nsects = struct.unpack_from("<I", data, off + 64)[0]
                if nsects > 0:
                    # section_64: sectname(16) segname(16) addr(8) size(8) offset(4) ...
                    # Each section header is 80 bytes. First section header starts at off + 72.
                    sect0_off = struct.unpack_from("<I", data, off + 72 + 48)[0]
                    cap = sect0_off
                break
        off += cmdsize
    if cap is None:
        fail("could not locate __TEXT first-section offset for headerpad cap")
    print(f"headerpad cap (first __TEXT section file offset): {cap}")

    # Build new LCs
    new_lcs = b"".join(build_dylib_lc(p, cmd) for p in new_paths)
    needed = len(new_lcs)
    print(f"adding {len(new_paths)} LC_LOAD_DYLIB(s), {needed} bytes total")
    if lc_end + needed > cap:
        fail(f"insufficient headerpad: have {cap - lc_end} bytes, need {needed}")

    # Splice in
    data[lc_end : lc_end + needed] = new_lcs
    # Update header
    new_ncmds = ncmds + len(new_paths)
    new_sizeofcmds = sizeofcmds + needed
    struct.pack_into("<II", data, 16, new_ncmds, new_sizeofcmds)
    print(f"after:  ncmds={new_ncmds} sizeofcmds={new_sizeofcmds}")

    with open(target, "wb") as f:
        f.write(bytes(data))
    print("wrote", target)

if __name__ == "__main__":
    main()
