#!/usr/bin/env python3
"""fault-symbolize.py — turn a fault report's `image+0xoff` lines into i386
function names.

WHY THIS EXISTS. The fault reporter resolves every stack word to
`<image>+<translated offset>`, which is exactly the wrong address space to read:
translation is non-linear inside a function, so `symbol + offset` arithmetic on
the TRANSLATED image is wrong by construction and the translated symtab is
sparse anyway. `__DATA,__86x64_pcmap` carries the real per-instruction
correspondence, so each translated offset can be mapped back to its i386
address and THEN resolved against the ORIGINAL binary's symbol table, which is
complete.

This is the same mapping `pcmap-diff.py` does for one address; this does it for
a whole fault report, in stack order, so a backtrace falls out of a log that
the unwinder and the crash reporter could not read at all.

    fault-symbolize.py <fault log> --orig <i386 dir> --trans <translated dir>

⚠ A pcmap row is the LARGEST row <= the address, so an address inside an
instruction's expansion resolves to that instruction's original — which is what
you want for a return address, and is approximate for anything else.
"""
import argparse, bisect, os, re, struct, subprocess, sys

FAT_MAGIC = (0xcafebabe, 0xcafebabf)
CPU_I386, CPU_X86_64 = 7, 0x1000007
PCMAP_MAGIC = 0x366d6370


def slice_offset(data, want):
    magic, = struct.unpack_from('>I', data, 0)
    if magic not in FAT_MAGIC:
        return 0
    nfat, = struct.unpack_from('>I', data, 4)
    wide = magic == 0xcafebabf
    step = 32 if wide else 20
    for i in range(nfat):
        off = 8 + i * step
        cpu, = struct.unpack_from('>i', data, off)
        if cpu == want:
            if wide:
                return struct.unpack_from('>Q', data, off + 16)[0]
            return struct.unpack_from('>I', data, off + 8)[0]
    return 0


def sections(data, base):
    """{(seg, sect): (vmaddr, size, fileoff)} for the slice at `base`."""
    magic, = struct.unpack_from('<I', data, base)
    is64 = magic == 0xfeedfacf
    ncmds, = struct.unpack_from('<I', data, base + 16)
    off = base + (32 if is64 else 28)
    out = {}
    for _ in range(ncmds):
        cmd, size = struct.unpack_from('<II', data, off)
        if cmd in (0x1, 0x19):
            nsects, = struct.unpack_from('<I', data, off + (64 if is64 else 48))
            so = off + (72 if is64 else 56)
            for _s in range(nsects):
                name = data[so:so + 16].split(b'\0')[0].decode()
                seg = data[so + 16:so + 32].split(b'\0')[0].decode()
                if is64:
                    addr, sz = struct.unpack_from('<QQ', data, so + 32)
                    fo, = struct.unpack_from('<I', data, so + 48)
                    so += 80
                else:
                    addr, sz, fo = struct.unpack_from('<III', data, so + 32)
                    so += 68
                out[(seg, name)] = (addr, sz, fo)
        off += size
    return out


def pcmap(path):
    """sorted [(trans_vmaddr, orig_vmaddr)] for a translated image."""
    data = open(path, 'rb').read()
    base = slice_offset(data, CPU_X86_64)
    sects = sections(data, base)
    s = sects.get(('__DATA', '__86x64_pcmap')) or sects.get(('__TEXT', '__86x64_pcmap'))
    if not s:
        return [], 0
    fo = base + s[2]
    magic, = struct.unpack_from('<I', data, fo)
    if magic != PCMAP_MAGIC:
        n, = struct.unpack_from('<I', data, fo)
        rows = [struct.unpack_from('<iI', data, fo + 8 + i * 8) for i in range(n)]
    else:
        n, = struct.unpack_from('<I', data, fo + 4)
        rows = [struct.unpack_from('<iI', data, fo + 8 + i * 8) for i in range(n)]
    text = sects[('__TEXT', '__text')][0]
    # trans_off in the table is relative to __text
    return sorted((text + t, o) for t, o in rows), text


def symbols(path):
    """sorted [(i386 addr, name)] from the ORIGINAL binary's symtab."""
    try:
        out = subprocess.run(['nm', '-n', path], capture_output=True,
                             text=True, errors='replace').stdout
    except OSError:
        return []
    syms = []
    for line in out.splitlines():
        f = line.split()
        if len(f) >= 3 and f[1] in 'tT':
            try:
                syms.append((int(f[0], 16), f[2]))
            except ValueError:
                pass
    syms.sort()
    return syms


class Image:
    def __init__(self, trans_path, orig_path):
        self.rows, _ = pcmap(trans_path)
        self.keys = [r[0] for r in self.rows]
        self.syms = symbols(orig_path) if orig_path else []
        self.saddr = [s[0] for s in self.syms]

    def name_for(self, trans_off):
        """translated file offset (unslid, image-relative) -> 'sym+0x..'"""
        t = 0x10000000 + trans_off
        i = bisect.bisect_right(self.keys, t)
        if not i:
            return None
        orig = self.rows[i - 1][1] + (t - self.keys[i - 1])
        j = bisect.bisect_right(self.saddr, orig)
        if not j:
            return "i386 %#x" % orig
        return "%s+%#x  [i386 %#x]" % (self.syms[j - 1][1],
                                       orig - self.syms[j - 1][0], orig)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('log')
    ap.add_argument('--orig', required=True, help='dir of i386 originals')
    ap.add_argument('--trans', required=True, help='dir of translated output')
    a = ap.parse_args()

    text = open(a.log, errors='replace').read()
    cache = {}

    def image(nm):
        if nm not in cache:
            tp = os.path.join(a.trans, nm)
            op = os.path.join(a.orig, nm)
            cache[nm] = Image(tp, op if os.path.exists(op) else None) \
                if os.path.exists(tp) else None
        return cache[nm]

    # Every line the reporter already resolved: "... 0xXXXXXXXX   name+0xoff"
    pat = re.compile(r'^\s*(\[[^\]]+\]|#\d+\s*\S*)?\s*.*?0x[0-9a-f]+\s+'
                     r'([A-Za-z0-9_.+-]+\.dylib|portal2_osx\S*)\+(0x[0-9a-f]+)')
    seen, shown = set(), 0
    for line in text.splitlines():
        m = pat.match(line)
        if not m:
            continue
        nm, off = m.group(2), int(m.group(3), 16)
        im = image(nm)
        who = im.name_for(off) if im else None
        key = (nm, off)
        tag = '  ' if key in seen else '* '
        seen.add(key)
        print("%s%-28s %s" % (tag, nm + '+' + hex(off), who or '(no pcmap)'))
        shown += 1
    if not shown:
        print("no resolved image+offset lines in", a.log, file=sys.stderr)


if __name__ == '__main__':
    main()
