#!/usr/bin/env python3
"""raw-i386-memform-scan.py — find translated instructions that kept a RAW i386
memory form, i.e. a candidate SUPPRESSED PIC-anchor rewrite.

WHY THIS SHAPE IS THE INTERESTING ONE
-------------------------------------
i386 PIC parks a get_pc_thunk base in a register and reads globals as
`disp(%anchor)`. The translator re-anchors each such access as rip-relative. When
it declines to — most often the two-anchor ambiguity bail on
`disp(%anchor,%idx,scale)`, where a STALE anchor on the INDEX trips the bail —
the instruction is emitted byte-identically, which in 64-bit mode needs a 0x67
address-size prefix to keep the 32-bit effective-address arithmetic. The result
resolves to `translated_anchor + i386_disp`, a wild address, and a STORE through
one lands in read-only __TEXT (Portal 2 localize.dylib i386 0xbc8f -> SIGBUS).

So a 0x67-prefixed instruction with a 32-bit GPR base is the whole candidate set.
It is not by itself a bug — plenty of genuine non-anchored accesses translate
byte-identically — so the scan classifies:

  SUSPECT  a 32-bit GPR base with a displacement ENCODED AS disp32 whose value
           lands inside this image's own span. The disp32 encoding is the sharp
           half of the test, not the value: the re-anchoring rewrite only ever
           fires on `dwidth == sizeof(uint32_t)`, so a suppressed rewrite is
           always disp32, while an ordinary object-field access is assembled as
           disp8 (`[ecx + eax*4 + 0x34]`) and can be excluded outright.
  raw      everything else: disp8 or no displacement, non-GPR bases.

⚠ SUSPECT is a HINT, not a verdict — a genuinely large struct or array offset is
assembled as disp32 too and its value can alias the image span, exactly as an
integer constant can alias a section (the value-alias immediate heuristic has the
same shape). Confirm one by reading it next to its i386 original with
pcmap-diff.py, which names the anchor.

★ The high-value use is DIFFERENTIAL: run it on a before and an after build of
the same image and watch SUSPECT go to zero. That is a whole-image static proof
of a translator fix with no run at all.

usage:
  raw-i386-memform-scan.py <translated-macho>...          # per-image tally
  raw-i386-memform-scan.py --show 20 <macho>              # list the suspects
  raw-i386-memform-scan.py --all <macho>                  # list every raw form
"""
import argparse, struct, sys

try:
    import capstone
except ImportError:
    sys.exit("needs capstone (pip3 install capstone)")

FAT_MAGICS = (0xcafebabe, 0xbebafeca)
MH_MAGIC_64 = 0xfeedfacf
LC_SEGMENT_64 = 0x19


def slice_offset(data, want_cpu):
    magic, = struct.unpack_from('>I', data, 0)
    if magic not in FAT_MAGICS:
        return 0
    n, = struct.unpack_from('>I', data, 4)
    for i in range(n):
        cpu, _sub, off, _size, _al = struct.unpack_from('>5I', data, 8 + i * 20)
        if cpu == want_cpu:
            return off
    sys.exit("no x86_64 slice")


def segments(data, base):
    magic, _cpu, _sub, _ft, ncmds, _sz, _fl, _rs = struct.unpack_from('<8I', data, base)
    if magic != MH_MAGIC_64:
        sys.exit("not a 64-bit Mach-O")
    off = base + 32
    segs, text = [], None
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from('<2I', data, off)
        if cmd == LC_SEGMENT_64:
            name = data[off + 8:off + 24].rstrip(b'\0').decode('ascii', 'replace')
            vmaddr, vmsize, fileoff, filesize = struct.unpack_from('<4Q', data, off + 24)
            nsects, = struct.unpack_from('<I', data, off + 64)
            segs.append((vmaddr, vmsize))
            so = off + 72
            for _s in range(nsects):
                sname = data[so:so + 16].rstrip(b'\0').decode('ascii', 'replace')
                addr, size = struct.unpack_from('<2Q', data, so + 32)
                fo, = struct.unpack_from('<I', data, so + 48)
                if name == '__TEXT' and sname == '__text':
                    text = (addr, size, fo + base)
                so += 80
        off += cmdsize
    if text is None:
        sys.exit("no __TEXT,__text")
    lo = min(a for a, _ in segs)
    hi = max(a + s for a, s in segs)
    return text, lo, hi


GPR32 = {'eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi'}


def scan(path):
    data = open(path, 'rb').read()
    base = slice_offset(data, 0x01000007)
    (taddr, tsize, tfo), lo, hi = segments(data, base)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True

    suspects, raws = [], 0
    code = data[tfo:tfo + tsize]
    for ins in md.disasm(code, taddr):
        # 0x67 is the only way a byte-identical i386 memory form survives.
        if 0x67 not in ins.prefix:
            continue
        mem = None
        for op in ins.operands:
            if op.type == capstone.x86.X86_OP_MEM:
                mem = op.mem
                break
        if mem is None:
            continue
        raws += 1
        breg = ins.reg_name(mem.base) if mem.base else None
        if breg not in GPR32:
            continue
        # The rewrite only fires on a disp32, so a suppressed one is always
        # disp32. This is what separates a real candidate from the disp8 struct
        # accesses that dominate the raw count.
        try:
            if ins.encoding.disp_size != 4:
                continue
        except AttributeError:
            sys.exit("needs capstone >= 4 for instruction encoding detail")
        # And a section-relative PIC displacement is an offset into the image.
        d = mem.disp
        if not (0 <= d < (hi - lo) or lo <= d < hi):
            continue
        suspects.append((ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str,
                         'index' if mem.index else 'base-only'))
    return raws, suspects, lo, hi


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('images', nargs='+')
    ap.add_argument('--show', type=int, default=0,
                    help='list up to N suspects per image')
    ap.add_argument('--all', action='store_true',
                    help='list every raw i386 memory form, not just suspects')
    a = ap.parse_args()

    total_raw = total_susp = 0
    for p in a.images:
        try:
            raws, susp, lo, hi = scan(p)
        except SystemExit as e:
            print("%-34s SKIP (%s)" % (p.split('/')[-1], e))
            continue
        total_raw += raws
        total_susp += len(susp)
        print("%-34s raw=%-6d SUSPECT=%-5d  span=[0x%x,0x%x)"
              % (p.split('/')[-1], raws, len(susp), lo, hi))
        n = a.show if a.show else (len(susp) if a.all else 0)
        for row in susp[:n]:
            print("    0x%08x  %-24s %s %s   [%s]"
                  % (row[0], row[1], row[2], row[3], row[4]))
    if len(a.images) > 1:
        print("\nTOTAL raw=%d  SUSPECT=%d  over %d images"
              % (total_raw, total_susp, len(a.images)))
    return 1 if total_susp else 0


if __name__ == '__main__':
    sys.exit(main())
