#!/usr/bin/env python3
"""pcmap-diff.py — read a TRANSLATED function's real x86_64 code next to the
i386 original it came from, using the translator's own __DATA,__86x64_pcmap.

WHY THIS EXISTS.  When a translated program misbehaves inside a function we did
not write (a statically linked library, say), the only way to settle "did the
translator emit this correctly?" is to look at both instruction streams.  The
symtab cannot do it: translation is non-linear inside a function (instructions
grow, split and move), so `symbol + offset` arithmetic is wrong by construction.
__86x64_pcmap is the one artefact that records the real per-instruction
correspondence, and it is emitted for every image carrying EH data.

WHAT IT IS NOT.  This does not decide correctness for you.  It shows two
streams; you read them.  A missing row is not proof of a dropped instruction
(the row set covers Instruction blobs, not padding or data-in-code).

USAGE
  pcmap-diff.py --orig <i386 binary> --trans <translated dylib> --addr 0x246a5a
  pcmap-diff.py ... --addr 0x246a5a --len 0x80      # explicit byte length
  pcmap-diff.py ... --addr 0x246a5a --map-only      # just the address rows
  pcmap-diff.py ... --audit 0x2d0000-0x2e0000       # sweep: flag DIVERGENT rows

AUDIT MODE is the one to reach for when you do not yet know which function is
wrong.  Every pcmap row names an instruction start in BOTH address spaces, so
each row can be disassembled on both sides and compared without a linear sweep
(no alignment guessing).  Translation is near-identity for the vast majority of
instructions -- same bytes, reinterpreted with 64-bit base registers -- so the
rows whose mnemonic or shape CHANGED are a short list, and that list is what you
read.  Known-benign rewrites (short jcc widened to near, call/jmp retargeted,
esp/ebp arithmetic) are classified, not hidden: they are printed under their own
heading so a real defect cannot be buried by suppressing a category.

FAT BINARIES: --orig may be fat; the i386 slice is selected automatically.  A
fat file's section offsets are SLICE-relative and reading them against the whole
file yields plausible garbage from the wrong architecture, so this is handled
here rather than left to the caller.
"""
import argparse, re, struct, sys

try:
    import capstone
except ImportError:
    sys.exit("capstone is required: python3 -m pip install capstone")

FAT_MAGIC = 0xcafebabe
MH_MAGIC, MH_MAGIC_64 = 0xfeedface, 0xfeedfacf
LC_SEGMENT, LC_SEGMENT_64 = 0x1, 0x19
CPU_I386, CPU_X86_64 = 7, 0x1000007
PCMAP_MAGIC = 0x366d6370  # "pcm6"


def slice_offset(data, want_cpu):
    """Return the file offset of the requested slice (0 for a thin file)."""
    magic, = struct.unpack_from('>I', data, 0)
    if magic not in (FAT_MAGIC, 0xcafebabf):
        return 0
    nfat, = struct.unpack_from('>I', data, 4)
    wide = magic == 0xcafebabf
    step = 32 if wide else 20
    for i in range(nfat):
        base = 8 + i * step
        cpu, = struct.unpack_from('>i', data, base)
        if wide:
            off, = struct.unpack_from('>Q', data, base + 16)
        else:
            off, = struct.unpack_from('>I', data, base + 8)
        if cpu == want_cpu:
            return off
    raise SystemExit("no slice for cputype %#x in this fat binary" % want_cpu)


def macho_sections(data, base):
    """{(seg,sect): (vmaddr, size, fileoff)} — offsets are SLICE-relative, so
    every caller must add `base` before indexing `data`."""
    magic, = struct.unpack_from('<I', data, base)
    is64 = magic == MH_MAGIC_64
    if magic not in (MH_MAGIC, MH_MAGIC_64):
        raise SystemExit("not a little-endian Mach-O at %#x (magic %#x)" % (base, magic))
    ncmds, = struct.unpack_from('<I', data, base + 16)
    off = base + (32 if is64 else 28)
    out = {}
    for _ in range(ncmds):
        cmd, csz = struct.unpack_from('<II', data, off)
        if cmd in (LC_SEGMENT, LC_SEGMENT_64):
            wide = cmd == LC_SEGMENT_64
            nsects, = struct.unpack_from('<I', data, off + (64 if wide else 48))
            soff = off + (72 if wide else 56)
            for _ in range(nsects):
                sect = data[soff:soff+16].rstrip(b'\0').decode()
                seg = data[soff+16:soff+32].rstrip(b'\0').decode()
                if wide:
                    addr, size = struct.unpack_from('<QQ', data, soff + 32)
                    fo, = struct.unpack_from('<I', data, soff + 48)
                else:
                    addr, size = struct.unpack_from('<II', data, soff + 32)
                    fo, = struct.unpack_from('<I', data, soff + 40)
                out[(seg, sect)] = (addr, size, fo)
                soff += 80 if wide else 68
        off += csz
    return out


# Section types whose entries are POINTER SLOTS: 4 bytes in i386, 8 in x86_64.
# The translated section therefore holds the same entries at DOUBLE the intra-
# section offset, and a naive same-offset expectation reports every one of them
# as a mismatch. (S_SYMBOL_STUBS entries are code, not slots, and are excluded
# from the audit's address checks elsewhere.)
PTR_SLOT_TYPES = {0x6, 0x7, 0x9, 0xa}   # non-lazy, lazy, mod_init, mod_term


def read_pcmap(data, sects):
    """[(orig_vmaddr, trans_vmaddr)] — trans_off rows are relative to __text."""
    key = ('__DATA', '__86x64_pcmap')
    if key not in sects:
        raise SystemExit("no __DATA,__86x64_pcmap in the translated image.\n"
                         "  It is only injected for images carrying EH data, so this\n"
                         "  says the gate did not fire — not that translation failed.")
    text = sects.get(('__TEXT', '__text'))
    if text is None:
        raise SystemExit("translated image has no __TEXT,__text to anchor rows on")
    _, size, fo = sects[key]
    magic, count = struct.unpack_from('<II', data, fo)
    if magic != PCMAP_MAGIC:
        raise SystemExit("pcmap magic %#x != %#x" % (magic, PCMAP_MAGIC))
    rows = []
    for i in range(count):
        t, o = struct.unpack_from('<iI', data, fo + 8 + i * 8)
        rows.append((o, text[0] + t))
    return rows


def symbols(data, base, is64):
    """{vmaddr: name} from LC_SYMTAB — sparse symtabs are normal; offsets rule."""
    magic, = struct.unpack_from('<I', data, base)
    ncmds, = struct.unpack_from('<I', data, base + 16)
    off = base + (32 if is64 else 28)
    syms = {}
    for _ in range(ncmds):
        cmd, csz = struct.unpack_from('<II', data, off)
        if cmd == 0x2:  # LC_SYMTAB
            symoff, nsyms, stroff, _strsize = struct.unpack_from('<IIII', data, off + 8)
            ent = 16 if is64 else 12
            for i in range(nsyms):
                p = base + symoff + i * ent
                strx, typ = struct.unpack_from('<IB', data, p)
                if typ & 0x0e != 0x0e:      # N_SECT only
                    continue
                if is64:
                    value, = struct.unpack_from('<Q', data, p + 8)
                else:
                    value, = struct.unpack_from('<I', data, p + 8)
                end = data.index(b'\0', base + stroff + strx)
                syms[value] = data[base + stroff + strx:end].decode('utf8', 'replace')
        off += csz
    return syms


def disasm(md, blob, addr, limit):
    out = []
    for insn in md.disasm(blob, addr):
        out.append((insn.address, insn.size, insn.mnemonic, insn.op_str,
                    insn.bytes.hex()))
        if insn.address - addr >= limit:
            break
        if insn.mnemonic == 'ret' and insn.address - addr >= limit - 16:
            break
    return out



# --- audit -------------------------------------------------------------------
# A row is BENIGN when the difference is one the translator makes BY
# CONSTRUCTION.  Every rule below states which one; anything unmatched is
# reported.  Mnemonics alone are too coarse -- the two defects this project has
# actually shipped were REBASED CONSTANTS, which leave the mnemonic untouched
# and change only an operand -- so operands are normalised and compared too.
R32 = ['eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi']
R64 = ['rax', 'rcx', 'rdx', 'rbx', 'rsp', 'rbp', 'rsi', 'rdi']


def norm_mem(ops):
    """Canonicalise addressing-mode base/index registers to their 64-bit names.
    Only inside brackets: a 32-bit DATA operand stays 32-bit on both sides, and
    collapsing those would hide a genuine operand-size change."""
    out, i = [], 0
    while i < len(ops):
        c = ops[i]
        if c == '[':
            j = ops.index(']', i) if ']' in ops[i:] else len(ops) - 1
            inner = ops[i:j + 1]
            for a, b in zip(R32, R64):
                inner = inner.replace(a, b)
            out.append(inner)
            i = j + 1
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def classify(oi, ti):
    """None = indistinguishable. lowercase = explained rewrite. UPPERCASE = read it."""
    omn, tmn = oi.mnemonic, ti.mnemonic
    oops, tops = norm_mem(oi.op_str), norm_mem(ti.op_str)
    if omn == tmn and oops == tops:
        return None
    # Control transfer: the target address MUST change (the body moved), and a
    # short displacement usually has to widen. The condition may not change.
    if omn[0] == 'j' or omn in ('call', 'loop', 'loope', 'loopne'):
        if omn != tmn:
            return 'BRANCH-KIND-CHANGED'
        return 'branch-retarget'
    if omn != tmn:
        return 'MNEMONIC-CHANGED'
    # String instructions: capstone prints the implicit ES: prefix in 32-bit
    # mode and drops it in 64-bit mode. Same instruction, different printing.
    if omn.split()[-1] in ('movsb', 'movsw', 'movsd', 'cmpsb', 'cmpsw',
                           'scasb', 'scasw', 'stosb', 'stosw', 'lodsb') \
       or oops.replace('es:', '') == tops:
        return 'string-op-printing'
    # An absolute address operand becomes RIP-relative in PIC output.
    if 'rip' in tops and 'rip' not in oops:
        return 'abs-to-riprel'
    # Stack/frame arithmetic is rewritten when the translator re-frames.
    if ('rsp' in tops or 'rbp' in tops) and ('esp' in oi.op_str or 'ebp' in oi.op_str):
        return 'frame-rewrite'
    # A changed absolute DISPLACEMENT is an ordinary data rebase: the operand is
    # an address by construction, so relocating it is right. A changed IMMEDIATE
    # is the dangerous case -- an immediate may be a plain NUMBER that merely
    # aliases an address, and rebasing it silently corrupts a constant. That is
    # the family behind both shipped defects, so it gets its own loud class.
    onums = set(re.findall(r'0x[0-9a-f]+', oi.op_str))
    tnums = set(re.findall(r'0x[0-9a-f]+', ti.op_str))
    changed = onums - tnums
    if changed:
        mem_part = oi.op_str.split('[', 1)[1] if '[' in oi.op_str else ''
        in_mem = all(n in mem_part for n in changed)
        return 'disp-rebased' if in_mem else 'IMMEDIATE-CHANGED'
    return 'OPERAND-CHANGED'


def audit(a, odata, tdata, obase, osects, tsects, rows, osyms):
    lo_s, _, hi_s = a.audit.partition('-')
    lo, hi = int(lo_s, 0), int(hi_s, 0)
    otext = osects[('__TEXT', '__text')]
    ttext = tsects[('__TEXT', '__text')]
    md32 = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md64 = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)

    sel = sorted(((o, t) for (o, t) in rows if lo <= o < hi))
    print("# audit orig [%#x, %#x): %d pcmap rows" % (lo, hi, len(sel)))
    if not sel:
        print("# NO ROWS in that range -- it is not translated code, or the\n"
              "# range is wrong. This is not evidence of anything else.")
        return 1

    buckets = {}
    for o, t in sel:
        ofo = obase + otext[2] + (o - otext[0])
        tfo = ttext[2] + (t - ttext[0])
        oi = next(md32.disasm(odata[ofo:ofo + 16], o), None)
        ti = next(md64.disasm(tdata[tfo:tfo + 16], t), None)
        if oi is None or ti is None:
            buckets.setdefault('UNDECODABLE', []).append(
                (o, t, '?' if oi is None else oi.mnemonic + ' ' + oi.op_str,
                 '?' if ti is None else ti.mnemonic + ' ' + ti.op_str))
            continue
        cls = classify(oi, ti)
        if cls is None:
            continue
        buckets.setdefault(cls, []).append(
            (o, t, (oi.mnemonic + ' ' + oi.op_str).strip(),
             (ti.mnemonic + ' ' + ti.op_str).strip()))

    total = sum(len(v) for v in buckets.values())
    order = sorted(buckets, key=lambda k: (k.islower(), -len(buckets[k])))
    print("# %d of %d row%s differ:" % (total, len(sel), '' if total == 1 else 's'))
    for k in order:
        print("#   %-24s %d" % (k, len(buckets[k])))
    want = a.show or 'unexplained'
    print("# UPPERCASE classes are UNEXPLAINED -- read those. lowercase are\n"
          "# rewrites the translator makes by construction.")

    # ROW GAPS.  The class counts above can only speak for rows that EXIST, and a
    # REWRITTEN instruction has no row: the PIC-anchor re-anchoring replaces one
    # Instruction blob with a fresh `lea r11,[rip+t]; op [r11+idx]` pair, and
    # inject_pcmap_section only records blobs that are still an Instruction with a
    # non-zero orig_vmaddr.  So "0 unexplained" WITHOUT this scan means "0 among
    # the rows that exist" -- which is how a whole-image audit of Portal 2's
    # libvstdlib (22675 rows, 0 unexplained) exonerated an image whose actual bug
    # was a mis-anchored `lea` the audit never saw.  Walking the ORIGINAL stream
    # between consecutive rows finds those instructions.  Resynchronising at every
    # row start (rather than sweeping linearly) keeps data-in-code from derailing
    # the walk.
    def dis_at(at):
        fo = obase + otext[2] + (at - otext[0])
        return next(md32.disasm(odata[fo:fo + 16], at), None)
    gaps, desync = [], 0
    for (o, _t0), (nxt, _t1) in zip(sel, sel[1:]):
        if nxt <= o:
            continue
        cur, steps = o, 0
        while True:
            ins = dis_at(cur)
            if ins is None or ins.size == 0:
                desync += 1
                break
            cur += ins.size
            steps += 1
            if cur >= nxt or steps > 64:
                if cur != nxt:
                    desync += 1
                break
            gaps.append((cur, dis_at(cur)))
    print("# %d original instruction start%s in range carry NO pcmap row%s" %
          (len(gaps), '' if len(gaps) == 1 else 's',
           (" (+%d resync failures)" % desync) if desync else ""))
    if gaps:
        print("#   A rewritten instruction LOSES its row, so this list is where the\n"
              "#   class counts above are blind -- PIC-anchor re-anchored accesses\n"
              "#   land here, and so do function prologues (which never had a row).\n"
              "#   Read it: an anchored rewrite that used the WRONG anchor looks\n"
              "#   exactly like one that used the right anchor from the row set.")
        shown = gaps if want == 'all' else gaps[:40]
        for at, gi in shown:
            print("  %#010x   %s" % (
                at, (gi.mnemonic + ' ' + gi.op_str).strip() if gi else '?'))
        if len(shown) < len(gaps):
            print("  ... %d more (--show all)" % (len(gaps) - len(shown)))
    for k in order:
        if want == 'unexplained' and k.islower():
            continue
        if want not in ('all', 'unexplained') and k != want:
            continue
        print("\n=== %s (%d) ===" % (k, len(buckets[k])))
        for o, t, otxt, ttxt in buckets[k]:
            print("  %#010x -> %#012x   %-34s | %s" % (o, t, otxt, ttxt))
    return 0



def verify_rebases(odata, tdata, obase, osects, tsects, rows):
    """Every ABSOLUTE address operand must land on the right translated address.

    This is the check that found the literal-pool defect. An absolute operand is
    an address by construction -- there is no constant-vs-pointer ambiguity to
    argue about -- so its translated form has exactly one correct value, and any
    other value is a defect, not a judgement call. Both encodings count: a plain
    `[disp32]` becomes RIP-relative in 64-bit (mod=00 r/m=101 changes meaning),
    while a `[reg*N + disp32]` stays absolute and must carry the full translated
    address.
    """
    otext = osects[('__TEXT', '__text')]
    ttext = tsects[('__TEXT', '__text')]

    def sect_of(va):
        for k, (a, sz, f) in osects.items():
            if a <= va < a + sz:
                return k, a
        return None, None

    def expect(va):
        k, a = sect_of(va)
        if k is None or k not in tsects:
            return None
        stype = None
        off = va - a
        # pointer-slot sections widen 4 -> 8 bytes per entry
        if k[1] in ('__nl_symbol_ptr', '__la_symbol_ptr', '__pointers',
                    '__mod_init_func', '__mod_term_func'):
            off *= 2
        return tsects[k][0] + off

    md32 = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md32.detail = True
    md64 = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64); md64.detail = True
    ok, unmapped, bad = 0, 0, []
    for o, t in rows:
        ofo = obase + otext[2] + (o - otext[0])
        tfo = ttext[2] + (t - ttext[0])
        oi = next(md32.disasm(odata[ofo:ofo + 16], o), None)
        ti = next(md64.disasm(tdata[tfo:tfo + 16], t), None)
        if oi is None or ti is None or oi.mnemonic != ti.mnemonic:
            continue
        for oop, top in zip(oi.operands, ti.operands):
            if oop.type != capstone.x86.X86_OP_MEM or oop.mem.base != 0:
                continue
            if not oop.mem.disp:
                continue
            exp = expect(oop.mem.disp & 0xffffffff)
            if exp is None:
                unmapped += 1
                continue
            if top.type != capstone.x86.X86_OP_MEM:
                continue
            if top.mem.base == capstone.x86.X86_REG_RIP:
                got = t + ti.size + top.mem.disp
            elif top.mem.base == 0:
                got = top.mem.disp & 0xffffffffffff
            else:
                continue
            if got == exp:
                ok += 1
            else:
                bad.append((o, t, oop.mem.disp & 0xffffffff, exp, got,
                            (oi.mnemonic + ' ' + oi.op_str).strip(),
                            (ti.mnemonic + ' ' + ti.op_str).strip()))
    print("# %d pcmap rows" % len(rows))
    print("# absolute address operands verified CORRECT: %d" % ok)
    print("# not in any i386 section (skipped, not a verdict): %d" % unmapped)
    print("# MISMATCHED: %d" % len(bad))
    for o, t, d, exp, got, otxt, ttxt in bad:
        print("  orig %#010x -> %#012x" % (o, t))
        print("      disp %#x  expected %#x  got %#x" % (d, exp, got))
        print("      i386:  %s" % otxt)
        print("      x64 :  %s" % ttxt)
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--orig', required=True)
    ap.add_argument('--trans', required=True)
    ap.add_argument('--addr', required=True)
    ap.add_argument('--len', dest='length', default=None)
    ap.add_argument('--map-only', action='store_true')
    ap.add_argument('--audit', default=None,
                    help='LO-HI original range: report divergent rows instead')
    ap.add_argument('--verify-rebases', action='store_true',
                    help='check every absolute address operand lands on the '
                         'right translated address')
    ap.add_argument('--show', default=None,
                    help='audit: a class name, "all", or "unexplained" (default)')
    a = ap.parse_args()

    addr = int(a.addr, 0)
    odata = open(a.orig, 'rb').read()
    tdata = open(a.trans, 'rb').read()

    obase = slice_offset(odata, CPU_I386)
    osects = macho_sections(odata, obase)
    tsects = macho_sections(tdata, 0)
    rows = read_pcmap(tdata, tsects)

    osyms = symbols(odata, obase, False)
    if a.verify_rebases:
        return verify_rebases(odata, tdata, obase, osects, tsects, rows)
    if a.audit:
        return audit(a, odata, tdata, obase, osects, tsects, rows, osyms)
    # Function extent: next symbol after addr, else the explicit --len, else 256.
    if a.length:
        length = int(a.length, 0)
    else:
        later = sorted(v for v in osyms if v > addr)
        length = min(later[0] - addr, 4096) if later else 256
    lo, hi = addr, addr + length

    sel = sorted(((o, t) for (o, t) in rows if lo <= o < hi), key=lambda r: r[1])
    print("# orig [%#x, %#x)  (%d byte%s, %s)" %
          (lo, hi, length, '' if length == 1 else 's',
           osyms.get(addr, 'no symbol at this address')))
    print("# %d pcmap row%s" % (len(sel), '' if len(sel) == 1 else 's'))
    if not sel:
        print("#\n# NO ROWS. Either this range holds no translated instructions\n"
              "# (data, or never reached by the parser), or the address is wrong.")
        return 1
    print("# trans [%#x, %#x)" % (sel[0][1], sel[-1][1]))
    if a.map_only:
        for o, t in sel:
            print("  %#010x -> %#012x" % (o, t))
        return 0

    o2t = {}
    for o, t in sel:
        o2t.setdefault(o, []).append(t)
    t2o = {t: o for (o, t) in sel}

    # --- original i386 ---
    otext = osects[('__TEXT', '__text')]
    ofo = obase + otext[2] + (lo - otext[0])
    md32 = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    o_ins = disasm(md32, odata[ofo:ofo + length + 32], lo, length)

    # --- translated x86_64 ---
    ttext = tsects[('__TEXT', '__text')]
    tlo, thi = sel[0][1], sel[-1][1]
    tfo = ttext[2] + (tlo - ttext[0])
    tlen = thi - tlo + 32
    md64 = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    t_ins = disasm(md64, tdata[tfo:tfo + tlen + 32], tlo, thi - tlo)

    print("\n=== i386 ORIGINAL ===")
    for ad, sz, mn, ops, by in o_ins:
        mark = '' if ad in o2t else '   <-- NO PCMAP ROW'
        print("  %#010x  %-22s %-30s %s%s" % (ad, by, mn + ' ' + ops, '', mark))

    print("\n=== x86_64 TRANSLATED ===")
    cur = None
    for ad, sz, mn, ops, by in t_ins:
        o = t2o.get(ad)
        if o is not None:
            cur = o
            tag = "%#010x" % o
        else:
            tag = "        ^" if cur is not None else "        ?"
        print("  %#012x  [%s]  %-24s %s" % (ad, tag, by, mn + ' ' + ops))
    print("\n# '^' = no row of its own: emitted as part of the preceding original\n"
          "# instruction's expansion. '?' = before the first mapped instruction.")
    return 0


if __name__ == '__main__':
    sys.exit(main())
