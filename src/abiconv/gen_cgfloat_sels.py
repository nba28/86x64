#!/usr/bin/env python3
"""gen_cgfloat_sels.py — derive the CGFloat-argument selector table from the
i386 system frameworks' own ObjC1 metadata.

On i386 CGFloat is `float` ('f'); on x86_64 it is `double` ('d'), so the forward
bridge cannot tell from the native encoding that a translated caller pushed a
4-byte float. The i386 method lists (__OBJC,__inst_meth/__cls_meth and the
category variants) say so exactly. For each selector, bit k of the mask marks
explicit arg k as 'f'. The table is keyed by bare SEL, so a selector whose
definitions disagree anywhere (e.g. 'f' in one class, 'd' in another) is left
out. Objects/structs/ints never set bits.

usage: gen_cgfloat_sels.py <i386 framework binary>... > cgfloat_sels.inc
Inputs: i386 slices of the Snow Leopard AppKit/Foundation/QuartzCore (the DVD's
own /System, `lipo -thin i386`; kept in
$M64_FRAMEWORKS/i386-originals/sl-objc/).
"""
import struct
import sys

def sections(data):
    magic, _, _, _, ncmds, _, _ = struct.unpack_from("<7I", data, 0)
    assert magic == 0xFEEDFACE, "not a thin i386 Mach-O"
    off, out = 28, []
    for _ in range(ncmds):
        cmd, size = struct.unpack_from("<2I", data, off)
        if cmd == 0x1:  # LC_SEGMENT
            nsects = struct.unpack_from("<I", data, off + 48)[0]
            s = off + 56
            for _ in range(nsects):
                name = data[s:s + 16].rstrip(b"\0")
                seg = data[s + 16:s + 32].rstrip(b"\0")
                addr, sz, foff = struct.unpack_from("<3I", data, s + 32)
                out.append((seg, name, addr, sz, foff))
                s += 68
        off += size
    return out


def reader(data, sects):
    def off(vm):
        for _, _, addr, sz, foff in sects:
            if addr <= vm < addr + sz and foff:
                return foff + vm - addr
        return None

    def cstr(vm):
        p = off(vm)
        return None if p is None else data[p:data.index(b"\0", p)].decode("latin-1")
    return off, cstr


METH_SECTS = {b"__inst_meth", b"__cls_meth", b"__cat_inst_meth", b"__cat_cls_meth"}


def method_lists(data, sects, off):
    """vmaddrs of every objc_method_list, enumerated exactly as the runtime
    does: __module_info -> symtab -> class/category defs (the lists are not
    packed back to back, and __category has no fixed stride)."""
    def u32(vm, k=0):
        return struct.unpack_from("<I", data, off(vm) + k)[0]

    for seg, name, addr, sz, foff in sects:
        if seg != b"__OBJC" or name != b"__module_info":
            continue
        for p in range(foff, foff + sz - 15, 16):   # {version, size, name, symtab}
            symtab = struct.unpack_from("<I", data, p + 12)[0]
            if not symtab or off(symtab) is None:
                continue
            q = off(symtab)
            ncls, ncat = struct.unpack_from("<2H", data, q + 8)
            defs = struct.unpack_from("<%dI" % (ncls + ncat), data, q + 12)
            for cls in defs[:ncls]:
                # methodLists (+28) points straight at ONE list on disk; the
                # runtime builds the array form at load time.
                yield u32(cls, 28)
                yield u32(u32(cls, 0), 28)           # isa -> metaclass
            for cat in defs[ncls:]:
                yield u32(cat, 8)                    # instance_methods
                yield u32(cat, 12)                   # class_methods


def skip_type(enc, i):
    """Index just past the single type starting at enc[i]."""
    while i < len(enc) and enc[i] in "rnNoORV":      # qualifiers
        i += 1
    c = enc[i]
    if c == "^":
        return skip_type(enc, i + 1)
    if c in "{([":
        close, depth = {"{": "}", "(": ")", "[": "]"}[c], 0
        while True:
            if enc[i] == c:
                depth += 1
            elif enc[i] == close:
                depth -= 1
                if depth == 0:
                    return i + 1
            i += 1
    if c == "@" and enc[i + 1:i + 2] == '"':
        return enc.index('"', i + 2) + 1
    if c == "b":                                     # bitfield width
        i += 1
        while i < len(enc) and enc[i].isdigit():
            i += 1
        return i
    return i + 1


def arg_types(enc):
    """Explicit-arg types of an i386 method encoding (after ret, self, _cmd)."""
    toks, i = [], 0
    while i < len(enc):
        j = skip_type(enc, i)
        t = enc[i:j].lstrip("rnNoORV")
        toks.append(t)
        i = j
        while i < len(enc) and (enc[i].isdigit() or enc[i] == "-"):  # frame offset
            i += 1
    return toks[3:]


def masks_of(enc):
    """(float_mask, double_mask) over the explicit args; None if the encoding
    does not parse. Only these two kinds matter: the runtime applies a bit only
    where the NATIVE arg is 'd'/'f', so an object/int/struct at the same
    position elsewhere is never affected."""
    try:
        types = arg_types(enc)
    except (IndexError, ValueError):
        return None
    if len(types) > 32:
        return None
    f = sum(1 << k for k, t in enumerate(types) if t == "f")
    d = sum(1 << k for k, t in enumerate(types) if t == "d")
    return f, d


def main(paths):
    fmask, dmask, bad = {}, {}, set()   # sel -> OR of 'f' / 'd' positions
    skipped = 0                          # list pointers outside the method sections
    for path in paths:
        data = open(path, "rb").read()
        sects = sections(data)
        off, cstr = reader(data, sects)
        lists = [(a, a + sz) for seg, n, a, sz, _ in sects
                 if seg == b"__OBJC" and n in METH_SECTS]
        for ml in set(method_lists(data, sects, off)):
            if not ml:
                continue
            if not any(lo <= ml < hi for lo, hi in lists):
                skipped += 1
                continue
            p = off(ml)
            count = struct.unpack_from("<I", data, p + 4)[0]
            for k in range(count):
                sel_vm, types_vm, _ = struct.unpack_from("<3I", data, p + 8 + 12 * k)
                sel, types = cstr(sel_vm), cstr(types_vm)
                if not (sel and types):
                    continue
                m = masks_of(types)
                if m is None:
                    bad.add(sel)
                    continue
                fmask[sel] = fmask.get(sel, 0) | m[0]
                dmask[sel] = dmask.get(sel, 0) | m[1]
    print("/* GENERATED by gen_cgfloat_sels.py from the i386 Snow Leopard")
    print(" * system frameworks' __OBJC method lists. Do not edit. */")
    kept = conflicts = 0
    for sel in sorted(fmask):
        f = fmask[sel]
        if not f or sel in bad:
            continue
        if f & dmask[sel]:          # a real double where another def has CGFloat
            conflicts += 1
            continue
        print('   { "%s", 0x%x },' % (sel, f))
        kept += 1
    print("/* %d selectors; %d left out (float vs double at one position) */"
          % (kept, conflicts))
    print("gen_cgfloat_sels: %d selectors, %d conflicting, %d stray list pointers"
          % (kept, conflicts, skipped), file=sys.stderr)


def selftest():
    for enc, want in [("v12@0:4f8", (0x1, 0)), ("v20@0:4@8f12@16", (0x2, 0)),
                      ("v24@0:4{_NSRect={_NSPoint=ff}{_NSSize=ff}}8", (0, 0)),
                      ("v16@0:4d8", (0, 0x1)), ("@28@0:4f8f12f16f20f24", (0x1f, 0)),
                      ("v16@0:4^f8", (0, 0)), ("c12@0:4r*8", (0, 0)),
                      ("v12@0:4{bad", None)]:
        assert masks_of(enc) == want, (enc, masks_of(enc), want)
    print("selftest ok")


if __name__ == "__main__":
    selftest() if sys.argv[1:] == ["--selftest"] else main(sys.argv[1:])
