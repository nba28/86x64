#!/usr/bin/env python3
"""const_drift.py — find CONSTANTS the translator "rebased" as if they were
pointers.

    const_drift.py <i386-original> <translated-x86_64-dylib> [label]

For each 4-byte aligned word in a file-backed DATA-ish section that exists in
BOTH the i386 original and the x86_64 translated output at the same intra-
section offset: if the i386 value is NOT inside any i386 section (=> the M32
pass correctly classified it a CONSTANT) yet the translated word DIFFERS, that
word was reclassified as a pointer downstream => an instance of the bug.

This is the measurement that sized task #30 (the M64 re-parse reclassifying M32
constants, cured by __DATA,__86x64_cpin): Civ IV Steam 738, iPhoto 143,
iMovie 10, Halo CE 1, Quinn/Pages/Numbers/iWeb 0. Keep it around — it is the
regression check for the whole DataParser pointer-detection family, and it runs
against DEPLOYED artifacts with no rebuild.

TWO TRAPS, both hit while writing it:
  * A FAT i386 original (Halo is ppc750+i386) needs the slice offset added to
    every file-offset computation. Without it you read the PowerPC slice and get
    ~98,000 bogus "instances" — PPC opcodes look exactly like small in-range
    i386 addresses.
  * Same-offset comparison is only valid where the section survives 1:1.
    Excluded: __DATA,__cfstring (16->32 B record growth), 4->8 B slot tables
    (__mod_init_func/__mod_term_func/symbol pointers), instruction-flagged
    sections, cstring literals, zerofill, and our own __86x64_* metadata. Any
    other size change is reported and skipped — Civ IV RETAIL is unmeasurable
    this way because its output coalesces __const_coal/__datacoal_nt into
    __TEXT,__const/__DATA,__const.
"""
import struct, sys, os

FAT_MAGIC   = 0xcafebabe
FAT_CIGAM   = 0xbebafeca
MH_MAGIC    = 0xfeedface
MH_MAGIC_64 = 0xfeedfacf

CPU_I386   = 7
CPU_X86_64 = 0x01000007

S_ZEROFILL = 0x1
S_CSTRING_LITERALS = 0x2
S_GB_ZEROFILL = 0xc
S_THREAD_LOCAL_ZEROFILL = 0x12
S_ATTR_PURE_INSTRUCTIONS = 0x80000000
S_ATTR_SOME_INSTRUCTIONS = 0x00000400


def slices(path):
    data = open(path, 'rb').read()
    magic = struct.unpack('>I', data[:4])[0]
    out = []
    if magic == FAT_MAGIC:
        n = struct.unpack('>I', data[4:8])[0]
        for i in range(n):
            cputype, cpusub, off, size, align = struct.unpack('>5I', data[8 + i * 20: 28 + i * 20])
            out.append((cputype, off, size))
    else:
        out.append((None, 0, len(data)))
    return data, out


def parse_macho(data, base):
    magic = struct.unpack('<I', data[base:base + 4])[0]
    if magic == MH_MAGIC:
        is64 = False
        hdrsz = 28
    elif magic == MH_MAGIC_64:
        is64 = True
        hdrsz = 32
    else:
        return None
    cputype, cpusub, filetype, ncmds, sizeofcmds, flags = struct.unpack('<6I', data[base + 4:base + 28])
    p = base + hdrsz
    sects = []
    segs = []
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack('<2I', data[p:p + 8])
        if cmd == 0x1:   # LC_SEGMENT
            segname = data[p + 8:p + 24].rstrip(b'\0').decode('ascii', 'replace')
            vmaddr, vmsize, fileoff, filesize, maxprot, initprot, nsects, sflags = struct.unpack('<8I', data[p + 24:p + 56])
            segs.append((segname, vmaddr, vmsize, initprot))
            q = p + 56
            for _s in range(nsects):
                sn = data[q:q + 16].rstrip(b'\0').decode('ascii', 'replace')
                sg = data[q + 16:q + 32].rstrip(b'\0').decode('ascii', 'replace')
                addr, size, offset, align, reloff, nreloc, secflags = struct.unpack('<7I', data[q + 32:q + 60])
                sects.append(dict(seg=sg, sect=sn, addr=addr, size=size, off=offset, flags=secflags))
                q += 68
        elif cmd == 0x19:  # LC_SEGMENT_64
            segname = data[p + 8:p + 24].rstrip(b'\0').decode('ascii', 'replace')
            vmaddr, vmsize, fileoff, filesize = struct.unpack('<4Q', data[p + 24:p + 56])
            maxprot, initprot, nsects, sflags = struct.unpack('<4I', data[p + 56:p + 72])
            segs.append((segname, vmaddr, vmsize, initprot))
            q = p + 72
            for _s in range(nsects):
                sn = data[q:q + 16].rstrip(b'\0').decode('ascii', 'replace')
                sg = data[q + 16:q + 32].rstrip(b'\0').decode('ascii', 'replace')
                addr, size = struct.unpack('<2Q', data[q + 32:q + 48])
                offset, align, reloff, nreloc, secflags = struct.unpack('<5I', data[q + 48:q + 68])
                sects.append(dict(seg=sg, sect=sn, addr=addr, size=size, off=offset, flags=secflags))
                q += 80
        p += cmdsize
    return dict(base=base, is64=is64, sects=sects, segs=segs, filetype=filetype)


def pick(path, want):
    data, sl = slices(path)
    for cputype, off, size in sl:
        if cputype is None or cputype == want:
            m = parse_macho(data, off)
            if m and ((want == CPU_I386) == (not m['is64'])):
                return data, m
    return None, None


def datalike(s):
    stype = s['flags'] & 0xff
    if stype in (S_ZEROFILL, S_GB_ZEROFILL, S_THREAD_LOCAL_ZEROFILL):
        return False
    if stype == S_CSTRING_LITERALS:
        return False
    if s['flags'] & (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS):
        return False
    if s['sect'] in ('__cfstring',):
        return False           # 16 -> 32 byte record growth
    if s['sect'].startswith('__86x64_'):
        return False           # our own synthesized metadata
    if s['sect'] in ('__text', '__stubs', '__symbol_stub', '__picsymbol_stub',
                     '__stub_helper', '__StaticInit', '__textcoal_nt',
                     '__coalesced', '__jt_tramp', '__eh_frame'):
        return False
    if s['seg'] == '__TEXT' and s['sect'] not in ('__const', '__const_coal'):
        return False           # literals/ustring/etc: not DataParser routed
    if s['seg'] not in ('__DATA', '__TEXT', '__OBJC'):
        return False
    return True


def sect_of(sects, v):
    for s in sects:
        if s['size'] and s['addr'] <= v < s['addr'] + s['size']:
            return s
    return None


def main(p32, p64, label):
    d32, m32 = pick(p32, CPU_I386)
    d64, m64 = pick(p64, CPU_X86_64)
    if m32 is None or m64 is None:
        print(f'{label}: SKIP (could not read slices)')
        return
    by64 = {(s['seg'], s['sect']): s for s in m64['sects']}
    total_words = 0
    const_words = 0
    hits = []
    for s in m32['sects']:
        if not datalike(s):
            continue
        t = by64.get((s['seg'], s['sect']))
        if t is None:
            continue
        n = min(s['size'], t['size'])
        if s['size'] != t['size']:
            # size changed -> offset equality invalid; note and skip
            print(f'  [skip size-mismatch] {s["seg"]},{s["sect"]} {s["size"]} vs {t["size"]}')
            continue
        for off in range(0, n - 3, 4):
            b32 = m32['base'] + s['off'] + off
            v32 = struct.unpack('<I', d32[b32:b32 + 4])[0]
            total_words += 1
            if sect_of(m32['sects'], v32) is not None:
                continue           # in-range in the i386 image -> not our class
            const_words += 1
            b64 = m64['base'] + t['off'] + off
            v64 = struct.unpack('<I', d64[b64:b64 + 4])[0]
            if v64 != v32:
                tgt = sect_of(m64['sects'], v64)
                hits.append((s['seg'], s['sect'], off, s['addr'] + off, v32, v64,
                             f'{tgt["seg"]},{tgt["sect"]}' if tgt else '?'))
    print(f'{label}: {len(hits)} instance(s); constants examined {const_words} of {total_words} data words')
    for h in hits[:100000]:
        print(f'    {h[0]},{h[1]} +0x{h[2]:x} (i386 addr 0x{h[3]:08X})  '
              f'0x{h[4]:08X} -> 0x{h[5]:08X}  [{h[6]}]')
    if len(hits) > 100000:
        print(f'    ... {len(hits)-60} more')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else sys.argv[1])
