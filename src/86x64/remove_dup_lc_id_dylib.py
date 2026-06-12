#!/usr/bin/env python3
"""Remove duplicate LC_ID_DYLIB from a Mach-O 64 file. Keeps the FIRST one
(the original install_name), removes any extras (the pipeline's @rpath/X.dylib).
Updates ncmds and sizeofcmds in the mach_header_64. Re-signs ad-hoc.
"""
import struct, sys, subprocess

LC_ID_DYLIB = 0xD
LC_CODE_SIGNATURE = 0x1d
MAGIC_64 = 0xfeedfacf

def main():
    if len(sys.argv) != 2:
        print(__doc__); sys.exit(2)
    path = sys.argv[1]
    data = bytearray(open(path, 'rb').read())
    magic = struct.unpack_from('<I', data, 0)[0]
    if magic != MAGIC_64:
        print(f"not a 64-bit Mach-O (magic=0x{magic:x})"); sys.exit(1)
    # mach_header_64: magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved
    ncmds = struct.unpack_from('<I', data, 16)[0]
    sizeofcmds = struct.unpack_from('<I', data, 20)[0]
    print(f"in: ncmds={ncmds} sizeofcmds={sizeofcmds}")
    off = 32
    seen_id = False
    removals = []  # (offset, cmdsize) to remove
    kept_cmds = 0
    for _ in range(ncmds):
        cmd, sz = struct.unpack_from('<II', data, off)
        if cmd == LC_ID_DYLIB:
            if seen_id:
                removals.append((off, sz))
                print(f"  duplicate LC_ID_DYLIB at offset {off:#x} size {sz}, will remove")
            else:
                seen_id = True
                kept_cmds += 1
        elif cmd == LC_CODE_SIGNATURE:
            # Stripping CS lets install_name_tool work afterward; we re-sign
            # ad-hoc at end.
            removals.append((off, sz))
            print(f"  LC_CODE_SIGNATURE at offset {off:#x} size {sz}, will remove (will re-sign)")
        else:
            kept_cmds += 1
        off += sz
    if not removals:
        print("no duplicates to remove"); return
    # Build new commands area by skipping the removed offsets.
    # All LCs sit in [32, 32+sizeofcmds). After removal we shift later LCs left
    # and zero-fill the tail.
    cmds_start = 32
    cmds_end = 32 + sizeofcmds
    old_cmds = bytes(data[cmds_start:cmds_end])
    removed_bytes = 0
    skip_ranges = sorted([(o - cmds_start, sz) for o, sz in removals])
    new_cmds = bytearray()
    cursor = 0
    for skip_off, skip_sz in skip_ranges:
        new_cmds.extend(old_cmds[cursor:skip_off])
        cursor = skip_off + skip_sz
        removed_bytes += skip_sz
    new_cmds.extend(old_cmds[cursor:])
    # Pad tail with zeros so total cmds area still occupies sizeofcmds bytes
    # (otherwise sections that immediately follow shift, breaking offsets).
    # We update sizeofcmds in header to the NEW (smaller) value, but the
    # bytes between new sizeofcmds and old sizeofcmds become padding before
    # the first section. dyld ignores trailing zeros in cmds area when ncmds
    # counts only the real LCs. Some tools may complain — we accept it.
    new_sizeofcmds = sizeofcmds - removed_bytes
    pad = bytearray(removed_bytes)
    final_cmds = new_cmds + pad
    data[cmds_start:cmds_end] = final_cmds
    # Update header
    struct.pack_into('<I', data, 16, ncmds - len(removals))
    struct.pack_into('<I', data, 20, new_sizeofcmds)
    print(f"out: ncmds={ncmds - len(removals)} sizeofcmds={new_sizeofcmds}")
    open(path, 'wb').write(bytes(data))
    subprocess.run(['codesign', '--force', '--sign', '-', path], check=False)
    print("done")

if __name__ == '__main__':
    main()
