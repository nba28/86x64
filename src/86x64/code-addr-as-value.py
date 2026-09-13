#!/usr/bin/env python3
"""code-addr-as-value.py — find translated sites where a CODE ADDRESS is used as
a VALUE, which is how a mis-relocated integer constant shows up in the artifact.

WHY THIS EXISTS.  The translator relocates an i386 imm32 whose value lands inside
a segment's vmaddr range, because a fixed-load-address image really does reach its
own globals that way.  The false positive is an ordinary integer constant that
merely ALIASES a section -- and small constants alias low __TEXT very easily.
Portal 2, engine.dylib i386 0x2e4d5f (CVoxelTree::CVoxelTree):

    movl $0x1000, 0x4(%esp)          # 4096: a page size

became `lea r11,[rip-0x475d43]; mov %r11d,0x4(%rsp)`, so CMemoryStack::Init was
handed a CODE ADDRESS as its size.  The request tracked ASLR (size ==
image_base + 0x1460) and was SERVED, so nothing failed, nothing crashed, and no
signal-based tool could see it.  Only the SHAPE in the artifact gives it away.

WHAT IT LOOKS FOR.  `lea r11,[rip+X]` (the translator's own re-anchoring scratch
register) whose target lands in __TEXT,__text, immediately followed by a store of
%r11d.  A pointer into CODE is not a plausible value for an argument or a field:
a genuine relocated table lives in __const/__cstring/__data, and a genuine code
pointer is taken by name, not re-anchored into a scratch register and spilled.

  --args-only   only outgoing-argument stores (`mov %r11d,N(%rsp)`) -- the
                highest-confidence shape, and the one that cost us a day
  (default)     any store of %r11d, which also catches field/global writes

NOT A PROOF either way: read each hit against the i386 original (pcmap-diff.py)
before calling it a bug, and remember a jump table CAN legitimately live in
__text -- which is why only STORES of the value, never reads through it, count.
"""
import argparse, os, re, struct, sys

try:
    import capstone
except ImportError:
    sys.exit("capstone is required: python3 -m pip install capstone")

RIP = re.compile(r'rip \+ (0x[0-9a-f]+)|rip - (0x[0-9a-f]+)')


def sections(data):
    if struct.unpack_from('<I', data, 0)[0] != 0xfeedfacf:
        return None
    ncmds = struct.unpack_from('<I', data, 16)[0]
    off, out = 32, {}
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from('<II', data, off)
        if cmd == 0x19:  # LC_SEGMENT_64
            nsects = struct.unpack_from('<I', data, off + 64)[0]
            so = off + 72
            for _s in range(nsects):
                sn = data[so:so + 16].rstrip(b'\0').decode()
                sg = data[so + 16:so + 32].rstrip(b'\0').decode()
                addr, size = struct.unpack_from('<QQ', data, so + 32)
                fo = struct.unpack_from('<I', data, so + 48)[0]
                out[(sg, sn)] = (addr, size, fo)
                so += 80
        off += cmdsize
    return out


def scan(path, args_only, show):
    data = open(path, 'rb').read()
    sects = sections(data)
    if sects is None or ('__TEXT', '__text') not in sects:
        return 0
    ta, tz, tf = sects[('__TEXT', '__text')]
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    hits, pend = [], None
    for i in md.disasm(data[tf:tf + tz], ta):
        if i.mnemonic == 'lea' and i.op_str.startswith('r11,') and 'rip' in i.op_str:
            m = RIP.search(i.op_str)
            if m:
                v = int(m.group(1), 16) if m.group(1) else -int(m.group(2), 16)
                pend = (i.address, i.address + i.size + v)
            else:
                pend = None
            continue
        if pend is not None:
            if i.mnemonic == 'mov' and 'r11d' in i.op_str:
                is_arg = 'rsp' in i.op_str
                if (not args_only or is_arg) and ta <= pend[1] < ta + tz:
                    hits.append((pend[0], pend[1], i.op_str))
            pend = None
    if hits:
        print("%-26s %d site(s)%s" % (os.path.basename(path), len(hits),
                                      " [args only]" if args_only else ""))
        for at, tgt, op in hits[:show]:
            print("    %#012x  lea r11 -> %#x (in __text)   store: %s" % (at, tgt, op))
        if len(hits) > show:
            print("    ... %d more (--show N)" % (len(hits) - show))
    return len(hits)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('images', nargs='+')
    ap.add_argument('--args-only', action='store_true',
                    help='only outgoing-argument stores mov %%r11d,N(%%rsp)')
    ap.add_argument('--show', type=int, default=5, help='hits to print per image')
    a = ap.parse_args()
    total = sum(scan(p, a.args_only, a.show) for p in a.images if os.path.isfile(p))
    print("TOTAL %d" % total)
    return 0


if __name__ == '__main__':
    sys.exit(main())
