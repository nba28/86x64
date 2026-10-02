#!/usr/bin/env python3
"""bink-emu.py — run Portal 2's libbinkmachox86 under Unicorn: the ORIGINAL i386
image (ground truth, bit-exact with ffmpeg) or OUR TRANSLATED x86_64 image, in
the same deterministic sandbox, so the two can be diffed state for state.

  bink-emu.py decode LIB FILE.bik NFRAMES OUT.yuv
      decode into registered I420 planes like probes/binkdec.c (BK_YUV) and
      Portal 2's CBIKMaterial. LIB may be the i386 original or a translation.
  bink-emu.py diff ORIG.i386 TRANS.x86_64 FILE.bik NFRAMES
      run both side by side; report the first frame whose decoder heap state
      differs, then the first differing heap WRITE inside it (translated pc +
      the i386 pc it maps to via __86x64_pcmap).

Each image is mapped at its link address (no rebasing needed: i386 base 0,
translation 0x10000000). Imports are Python hooks reached through hlt pads: the
i386 __IMPORT,__jump_table stubs, and for the translation the bound __jt_ptrs
slots (translated code keeps i386 stack conventions: 4-byte return slot, args
at sp+4, so one set of handlers serves both). Only what the decode path reaches
is implemented; anything else stops with the import's name.
Needs `unicorn` + `numpy` (+ `capstone` for diff).
"""
import collections, os, struct, subprocess, sys, time
from unicorn import (Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_MODE_64, UC_HOOK_CODE,
                     UC_HOOK_MEM_WRITE)
from unicorn import x86_const as X

HEAP, HEAP_SZ = 0x40000000, 0x30000000
STACK_TOP = 0x7f000000
MAGIC = 0x30000000                        # return pad + callbacks + import pads

def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True).stdout

class Machine:
    def __init__(self, lib, bik):
        self.lib, self.img = lib, open(lib, 'rb').read()
        self.x64 = 'x86_64' in run(['file', '-b', lib])
        self.mu = Uc(UC_ARCH_X86, UC_MODE_64 if self.x64 else UC_MODE_32)
        self.SP = X.UC_X86_REG_RSP if self.x64 else X.UC_X86_REG_ESP
        self.PC = X.UC_X86_REG_RIP if self.x64 else X.UC_X86_REG_EIP
        self.syms = {}
        for line in run(['nm', lib]).splitlines():
            p = line.split()
            if len(p) == 3:
                self.syms[p[2]] = int(p[0], 16)
        self.pads = {}                    # pad address -> import name
        mu = self.mu
        mu.mem_map(HEAP, HEAP_SZ)
        mu.mem_map(STACK_TOP - 0x400000, 0x400000)
        mu.mem_map(MAGIC, 0x10000)
        mu.mem_write(MAGIC, b'\xf4' * 0x10000)
        self.RET, self.CB_ALLOC, self.CB_FREE = MAGIC, MAGIC + 0x10, MAGIC + 0x20
        self.load_x64() if self.x64 else self.load_i386()
        mu.hook_add(UC_HOOK_CODE, self.on_pad, begin=MAGIC, end=MAGIC + 0x10000)
        self.heap_top, self.fds, self.t0, self.stop = HEAP, {}, time.monotonic(), []
        self.bik = bik

    def load_i386(self):
        mu = self.mu
        mu.mem_map(0, 0x40000)
        for seg in self.segments():
            mu.mem_write(seg['vmaddr'], self.img[seg['fileoff']:seg['fileoff'] + seg['filesize']])
        # __jump_table stubs -> hlt pads
        on = False
        for line in run(['otool', '-Iv', self.lib]).splitlines():
            if line.startswith('Indirect symbols for'):
                on = '__jump_table' in line
                continue
            p = line.split()
            if on and len(p) == 3 and p[0].startswith('0x'):
                stub = int(p[0], 16)
                pad = MAGIC + 0x100 + 8 * len(self.pads)
                self.pads[pad] = p[2]
                mu.mem_write(stub, b'\xe9' + struct.pack('<i', pad - (stub + 5)))   # jmp pad
            elif on and len(p) == 3 and p[1] == 'LOCAL':
                pass
        # non-lazy pointers to symbols this image defines itself (dyld binds them)
        on = False
        for line in run(['otool', '-Iv', self.lib]).splitlines():
            if line.startswith('Indirect symbols for'):
                on = '__nl_symbol_ptr' in line
                continue
            p = line.split()
            if on and len(p) == 3 and p[0].startswith('0x') and p[2] in self.syms:
                mu.mem_write(int(p[0], 16), struct.pack('<I', self.syms[p[2]]))

    def load_x64(self):
        mu = self.mu
        for seg in self.segments():
            if seg['name'] in ('__PAGEZERO', '__LINKEDIT'):
                continue
            mu.mem_map(seg['vmaddr'], (seg['vmsize'] + 0xfff) & ~0xfff)
            mu.mem_write(seg['vmaddr'], self.img[seg['fileoff']:seg['fileoff'] + seg['filesize']])
        for line in run(['dyld_info', '-fixups', self.lib]).splitlines():
            p = line.split()
            if len(p) >= 5 and p[3] == 'bind':
                name = p[4].split('/')[-1]
                if name.startswith('___'):
                    name = name[2:]           # libabiconv bridge ___X -> _X
                pad = MAGIC + 0x100 + 8 * len(self.pads)
                self.pads[pad] = name
                mu.mem_write(int(p[2], 16), struct.pack('<Q', pad))

    def segments(self):
        segs, cur = [], None
        for line in run(['otool', '-l', self.lib]).splitlines():
            p = line.split()
            if not p:
                continue
            if p[0] == 'cmd':
                cur = None
                if p[1] in ('LC_SEGMENT', 'LC_SEGMENT_64'):
                    cur = {}
                    segs.append(cur)
            elif cur is not None and p[0] in ('segname', 'vmaddr', 'vmsize', 'fileoff', 'filesize'):
                cur['name' if p[0] == 'segname' else p[0]] = p[1] if p[0] == 'segname' else int(p[1], 0)
        return segs

    # ---- memory / ABI helpers -------------------------------------------
    def rd32(self, a): return struct.unpack('<I', self.mu.mem_read(a, 4))[0]
    def wr32(self, a, v): self.mu.mem_write(a, struct.pack('<I', v & 0xffffffff))
    def arg(self, i): return self.rd32(self.mu.reg_read(self.SP) + 4 + 4 * i)
    def malloc(self, n):
        p = (self.heap_top + 15) & ~15
        self.heap_top = p + max(n, 1)
        assert self.heap_top < HEAP + HEAP_SZ, 'emu heap exhausted'
        return p
    def cstr(self, a):
        s = b''
        while (c := self.mu.mem_read(a, 1)) != b'\0':
            s += c; a += 1
        return s.decode()

    def imp(self, name):
        """One import, cdecl (caller pops). Returns eax."""
        a = self.arg
        if name == '_open':               # per-machine fd numbers: both sides must agree
            fd = 3 + len(self.fds); self.fds[fd] = os.open(self.cstr(a(0)), os.O_RDONLY); return fd
        if name == '_read':
            data = os.read(self.fds[a(0)], a(2)); self.mu.mem_write(a(1), data); return len(data)
        if name == '_lseek':
            off = struct.unpack('<i', struct.pack('<I', a(1)))[0]
            return os.lseek(self.fds[a(0)], off, a(2))
        if name == '_close':
            os.close(self.fds[a(0)]); return 0
        if name == '_MPCreateCriticalRegion':
            self.wr32(a(0), 0x1234); return 0
        if name in ('_MPEnterCriticalRegion', '_MPExitCriticalRegion', '_MPDeleteCriticalRegion',
                    '_InstallTimeTask', '_PrimeTime', '_RemoveTimeTask', '_DisposeTimerUPP',
                    '_DisposeSndCallBackUPP'):
            return 0
        if name in ('_NewTimerUPP', '_NewSndCallBackUPP'):
            return a(0)
        if name == '_SndNewChannel':      # no audio: the video decodes regardless
            self.wr32(a(0), 0); return -202
        if name == '_OTAtomicAdd32':
            v = (self.rd32(a(1)) + a(0)) & 0xffffffff; self.wr32(a(1), v); return v
        if name == '_UpTime':             # deterministic clock: both sides must agree
            self.mu.reg_write(X.UC_X86_REG_EDX, 0); return 0
        if name == '_AbsoluteToNanoseconds':
            self.mu.reg_write(X.UC_X86_REG_EDX, a(1)); return a(0)
        if name == '_Gestalt':
            return -5551                  # gestaltUndefSelectorErr
        raise RuntimeError(f'unimplemented import {name} (add it to bink-emu.py)')

    def on_pad(self, uc, addr, size, _):
        if addr == self.RET:
            uc.emu_stop(); return
        sp = uc.reg_read(self.SP)
        if addr in (self.CB_ALLOC, self.CB_FREE):
            eax = self.malloc(self.rd32(sp + 4)) if addr == self.CB_ALLOC else 0
        elif addr in self.pads:
            try:
                eax = self.imp(self.pads[addr])
            except Exception as e:
                self.stop.append(str(e)); uc.emu_stop(); return
        else:
            return
        uc.reg_write(X.UC_X86_REG_RAX if self.x64 else X.UC_X86_REG_EAX, eax & 0xffffffff)
        uc.reg_write(self.PC, self.rd32(sp))          # 4-byte return slot on both sides
        uc.reg_write(self.SP, sp + 4)

    def call(self, name, *args):
        sp = STACK_TOP - 0x1000 - 4 * len(args)
        self.mu.mem_write(sp, b''.join(struct.pack('<I', v & 0xffffffff) for v in args))
        sp -= 4
        self.wr32(sp, self.RET)
        self.mu.reg_write(self.SP, sp)
        try:
            self.mu.emu_start(self.syms[name], self.RET)
        except UcError as e:
            raise SystemExit(f'{self.lib}: {name}: {e} at pc={self.mu.reg_read(self.PC):#x}')
        if self.stop:
            raise SystemExit(f'{self.lib}: {name}: {self.stop[0]}')
        return self.mu.reg_read(X.UC_X86_REG_RAX if self.x64 else X.UC_X86_REG_EAX) & 0xffffffff

    # ---- Portal 2's CBIKMaterial sequence -----------------------------------
    def open(self):
        path = self.malloc(len(self.bik) + 1)
        self.mu.mem_write(path, self.bik.encode() + b'\0')
        self.call('_BinkSetMemory', self.CB_ALLOC, self.CB_FREE)
        self.bink = self.call('_BinkOpen', path, 0x400)      # BINKNOFRAMEBUFFERS
        if not self.bink:
            raise SystemExit(f'{self.lib}: BinkOpen failed')
        self.W, self.H, self.F = struct.unpack('<III', self.mu.mem_read(self.bink, 12))
        fb = self.fb = self.malloc(0x18 + 2 * 0x30)
        self.call('_BinkGetFrameBuffersInfo', self.bink, fb)
        total, yw, yh, cw, ch = struct.unpack('<iIIII', self.mu.mem_read(fb, 20))
        for f in range(min(total, 2)):
            for k, (w, h) in enumerate(((yw, yh), (cw, ch), (cw, ch))):   # Y, cR, cB
                pl = fb + 0x18 + f * 0x30 + k * 12
                if self.rd32(pl):                                         # Allocate
                    pitch = (w + 15) & ~15
                    self.mu.mem_write(pl + 4, struct.pack('<II', self.malloc(pitch * h), pitch))
        self.call('_BinkRegisterFrameBuffers', self.bink, fb)

    def plane(self, pl, w, h):
        buf, pitch = struct.unpack('<II', self.mu.mem_read(pl + 4, 8))
        raw = self.mu.mem_read(buf, pitch * h)
        return b''.join(raw[y * pitch:y * pitch + w] for y in range(h))

    def frame(self):
        """Decode one frame; return it as I420 bytes."""
        self.call('_BinkDoFrame', self.bink)
        cur = self.fb + 0x18 + self.rd32(self.fb + 0x14) * 0x30            # Frames[FrameNum]
        W, H = self.W, self.H
        out = self.plane(cur, W, H) + self.plane(cur + 24, W // 2, H // 2) + \
              self.plane(cur + 12, W // 2, H // 2)
        self.call('_BinkNextFrame', self.bink)
        return out

    def heap(self):
        return bytes(self.mu.mem_read(HEAP, self.heap_top - HEAP))


def decode(lib, bik, n, out):
    m = Machine(lib, bik)
    m.open()
    with open(out, 'wb') as o:
        for i in range(min(n, m.F)):
            o.write(m.frame())
    print(f'{m.W}x{m.H}: {min(n, m.F)} frames -> {out}', file=sys.stderr)


def heap_diffs(a, b):
    """Heap word offsets whose values really differ: a pair of code addresses
    (i386 __text vs translated __text) is a pointer to the same code, not a diff."""
    ha, hb = a.heap(), b.heap()
    if len(ha) != len(hb):
        return ['heap size %#x vs %#x' % (len(ha), len(hb))]
    wa = struct.unpack(f'<{len(ha) // 4}I', ha[:len(ha) // 4 * 4])
    wb = struct.unpack(f'<{len(hb) // 4}I', hb[:len(hb) // 4 * 4])
    out = []
    for o, (x, y) in enumerate(zip(wa, wb)):
        if x != y and not (0x1000 <= x < 0x25000 and 0x10000000 <= y < 0x10030000):
            out.append(o * 4)
    return out


def first_divergent_write(a, b, words):
    """Re-run one frame on both machines recording every write into `words`;
    return the earliest write (by order on each side) whose value differs."""
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_MODE_64
    watch = {HEAP + o for o in words}
    recs = []
    for m in (a, b):
        rec = []
        def w(uc, acc, addr, size, val, _, m=m, rec=rec):
            for o in range(addr & ~3, addr + size, 4):
                if o in watch:
                    rec.append((o, uc.reg_read(m.PC), size, val & ((1 << (8 * size)) - 1)))
        h = m.mu.hook_add(UC_HOOK_MEM_WRITE, w)
        m.frame()
        m.mu.hook_del(h)
        recs.append(rec)
    # per watched word: the k-th write on each side should agree
    seq = collections.defaultdict(lambda: ([], []))
    for side, rec in enumerate(recs):
        for i, r in enumerate(rec):
            seq[r[0]][side].append((i, r))
    best = None
    for word, (ra, rb) in seq.items():
        for (ia, xa), (ib, xb) in zip(ra, rb):
            if (xa[2], xa[3]) != (xb[2], xb[3]):
                if best is None or ia < best[0]:
                    best = (ia, word, xa, xb)
                break
        else:
            if len(ra) != len(rb):
                k = min(len(ra), len(rb))
                ia = ra[k][0] if k < len(ra) else rb[k][0]
                xa = ra[k][1] if k < len(ra) else None
                xb = rb[k][1] if k < len(rb) else None
                if best is None or ia < best[0]:
                    best = (ia, word, xa, xb)
    return best


def diff(orig, trans, bik, n):
    a, b = Machine(orig, bik), Machine(trans, bik)
    a.open(); b.open()
    d = heap_diffs(a, b)
    print(f'after open: {len(d)} differing heap words')
    for i in range(min(n, a.F)):
        sa = (a.heap_top, a.heap())            # snapshot to replay the bad frame
        fa, fb = a.frame(), b.frame()
        d = heap_diffs(a, b)
        print(f'frame {i}: output {"same" if fa == fb else "DIFF"}, '
              f'{len(d)} differing heap words')
        if d:
            print('  first differing words: ' + ' '.join(f'heap+{o:#x}' for o in d[:12]))
            explain(orig, trans, bik, i, d)
            return i, d
    return None, []


def trans_to_orig(trans):
    """translated pc -> (i386 pc of the nearest pcmap row at or before it, delta)."""
    import bisect, importlib.util
    spec = importlib.util.spec_from_file_location(
        'pcmap_diff', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pcmap-diff.py'))
    pd = importlib.util.module_from_spec(spec); spec.loader.exec_module(pd)
    data = open(trans, 'rb').read()
    rows = sorted((t, o) for o, t in pd.read_pcmap(data, pd.macho_sections(data, 0)))
    keys = [t for t, _ in rows]
    def f(pc):
        k = bisect.bisect_right(keys, pc) - 1
        return (rows[k][1], pc - rows[k][0]) if k >= 0 else (None, None)
    return f


def exact_rows(trans):
    import importlib.util
    spec = importlib.util.spec_from_file_location(
        'pcmap_diff', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pcmap-diff.py'))
    pd = importlib.util.module_from_spec(spec); spec.loader.exec_module(pd)
    data = open(trans, 'rb').read()
    return {t: o for o, t in pd.read_pcmap(data, pd.macho_sections(data, 0))}


def first_divergent_path(a, b, trans, limit=50_000_000):
    """Run one frame on both, tracing the i386 address of every instruction that
    has a pcmap row (translated pcs mapped exactly). Return the index and the
    context where the two i386-address streams first differ."""
    import array
    t2o = exact_rows(trans)
    rowset = set(t2o.values())
    ta, tb = array.array('I'), array.array('I')
    ha = a.mu.hook_add(UC_HOOK_CODE, lambda uc, addr, sz, _: ta.append(addr) if addr in rowset else None,
                       begin=0x1000, end=0x25000)
    hb = b.mu.hook_add(UC_HOOK_CODE, lambda uc, addr, sz, _: tb.append(t2o[addr]) if addr in t2o else None,
                       begin=0x10000000, end=0x10030000)
    a.frame(); b.frame()
    a.mu.hook_del(ha); b.mu.hook_del(hb)
    n = min(len(ta), len(tb))
    k = next((j for j in range(n) if ta[j] != tb[j]), n)
    return k, len(ta), len(tb), ta[max(0, k - 12):k + 4], tb[max(0, k - 12):k + 4]


REGS32 = ('eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi')


def first_divergent_regs(a, b, trans, upto):
    """Lockstep over the first `upto` pcmap-row instructions of one frame:
    the GPRs (low 32 bits) entering each row must agree. Returns the first row
    where any register differs, with both register files and the rows before."""
    t2o = exact_rows(trans)
    rowset = set(t2o.values())
    ids32 = [getattr(X, 'UC_X86_REG_' + r.upper()) for r in REGS32]
    ids64 = [getattr(X, 'UC_X86_REG_R' + r[1:].upper()) for r in REGS32]
    ra, rb = [], []
    def ha_(uc, addr, sz, _):
        if addr in rowset and len(ra) < upto:
            ra.append((addr, tuple(uc.reg_read(r) for r in ids32)))
    def hb_(uc, addr, sz, _):
        if addr in t2o and len(rb) < upto:
            rb.append((t2o[addr], tuple(uc.reg_read(r) & 0xffffffff for r in ids64)))
    h1 = a.mu.hook_add(UC_HOOK_CODE, ha_, begin=0x1000, end=0x25000)
    h2 = b.mu.hook_add(UC_HOOK_CODE, hb_, begin=0x10000000, end=0x10030000)
    a.frame(); b.frame()
    a.mu.hook_del(h1); b.mu.hook_del(h2)
    for j, (x, y) in enumerate(zip(ra, rb)):
        if x != y:
            return j, ra[max(0, j - 6):j + 1], rb[max(0, j - 6):j + 1]
    return None, [], []


def explain(orig, trans, bik, i, words):
    """Replay frame i on fresh machines and name the first divergent write."""
    a, b = Machine(orig, bik), Machine(trans, bik)
    a.open(); b.open()
    for _ in range(i):
        a.frame(); b.frame()
    best = first_divergent_write(a, b, words)
    if best is None:
        print('  no divergent write found among the watched words'); return
    _, word, xa, xb = best
    t2o = trans_to_orig(trans)
    print(f'  first divergent write, heap+{word - HEAP:#x}:')
    if xa: print(f'    i386  pc={xa[1]:#07x} size={xa[2]} value={xa[3]:#x}')
    if xb:
        o, dlt = t2o(xb[1])
        print(f'    trans pc={xb[1]:#x} size={xb[2]} value={xb[3]:#x}'
              f'  (pcmap: i386 {o:#x} +{dlt:#x})' if o is not None else '')
    a, b = Machine(orig, bik), Machine(trans, bik)
    a.open(); b.open()
    for _ in range(i):
        a.frame(); b.frame()
    k, na, nb, ca, cb = first_divergent_path(a, b, trans)
    print(f'  instruction streams (pcmap rows only): i386 {na}, trans {nb}; first split at #{k}')
    print('    i386 : ' + ' '.join(f'{x:#x}' for x in ca))
    print('    trans: ' + ' '.join(f'{x:#x}' for x in cb))
    a, b = Machine(orig, bik), Machine(trans, bik)
    a.open(); b.open()
    for _ in range(i):
        a.frame(); b.frame()
    j, wa, wb = first_divergent_regs(a, b, trans, k + 16)
    if j is None:
        print('  registers agree at every row up to the split'); return
    print(f'  first register divergence at row #{j} (i386 {wa[-1][0]:#x}); rows leading up to it:')
    for (pa, va), (pb, vb) in zip(wa, wb):
        diffs = [f'{n}: {x:#x} vs {y:#x}' for n, x, y in zip(REGS32, va, vb) if x != y]
        print(f'    {pa:#07x}' + (f'  trans pc-row {pb:#x}' if pa != pb else '') +
              ('   ' + ', '.join(diffs) if diffs else ''))


if __name__ == '__main__':
    if len(sys.argv) == 6 and sys.argv[1] == 'decode':
        decode(sys.argv[2], sys.argv[3], int(sys.argv[4]), sys.argv[5])
    elif len(sys.argv) == 6 and sys.argv[1] == 'diff':
        diff(sys.argv[2], sys.argv[3], sys.argv[4], int(sys.argv[5]))
    else:
        sys.exit(__doc__)
