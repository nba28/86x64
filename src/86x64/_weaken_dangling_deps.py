#!/usr/bin/env python3
"""Weaken dangling (zero-bind) LC_LOAD_DYLIB dependencies of a translated Mach-O.

THE PROBLEM (structural, universal — found via Portal 2's engine.dylib):
Old linkers happily record LC_LOAD_DYLIB load commands for libraries the binary
never actually binds a single symbol from (over-linking; Xcode-era projects link
whole SDK framework sets).  When such a dependency has since been REMOVED from
macOS (QuickTime.framework being the canonical example), dyld aborts the whole
dlopen/load with "Library not loaded" even though nothing in the image ever
references it.  The translated binary is then unloadable on a modern system for
no semantic reason.

THE FIX: after translation, flip every LC_LOAD_DYLIB whose dependency
contributes ZERO symbol binds to LC_LOAD_WEAK_DYLIB (same dylib_command layout,
only the cmd field differs — an in-place 4-byte patch, no re-layout).  Weak
semantics are identical for a present library (dyld still loads it); for an
absent one dyld now skips it instead of failing the load.  Since no bind
references the library, no lookup can ever miss.

Safety gates:
  - only absolute-path deps ('/...') are considered: @rpath/@loader_path deps
    are deploy-managed (bundled) and left alone;
  - libabiconv/libinterpose (our own runtime, inserted by the pipeline) are
    never touched;
  - images without MH_TWOLEVEL are skipped entirely (flat-namespace lookup
    cannot attribute binds to a specific dependency, so every dep must stay);
  - two deps sharing a leaf name are conservatively treated as bound.

Bind attribution comes from `nm -m` two-level "(from X)" annotations, which is
the same source static-interpose.sh already relies on.

★DELIBERATELY ABSENT DEPENDENCIES — `M64_ABSENT_DEPS=leaf1,leaf2`
The zero-bind rule above cannot help when we CHOOSE not to translate a dependency
that the dependent really does bind symbols from. Portal 2: the translate script
skips libcef.dylib (29 MB of embedded Chromium, only the in-game browser UI), but
vguimatsurface.dylib binds 19 cef_* symbols from it, so dyld refuses to load
vguimatsurface AT ALL -- which made CSourceAppSystemGroup::Create() fail,
AddSystems() fail, and the whole app tear down, with no error text anywhere. A
skipped dependency silently breaks every dependent, and nothing enforced otherwise.

For each leaf named in M64_ABSENT_DEPS this pass does BOTH halves, because either
alone still fails the load:
  1. LC_LOAD_DYLIB -> LC_LOAD_WEAK_DYLIB, so dyld tolerates the missing file;
  2. every bind attributed to that dylib ordinal gets BIND_SYMBOL_FLAGS_WEAK_IMPORT,
     so the unresolved symbols become NULL instead of "Symbol not found".
The result is honest rather than a stub: the module loads, and an actual call into
the absent library faults at 0x0 -- loudly, and exactly where the real dependency
would have been needed -- instead of silently doing nothing. Policy (which dep is
absent) lives in the per-target translate script, which is where that knowledge is;
this pass only carries it out.
"""
import os
import re
import struct
import subprocess
import sys

LC_LOAD_DYLIB = 0x0C
LC_LOAD_WEAK_DYLIB = 0x80000018
LC_REEXPORT_DYLIB = 0x8000001F
LC_LAZY_LOAD_DYLIB = 0x20
LC_LOAD_UPWARD_DYLIB = 0x80000023
LC_DYLD_INFO = 0x22
LC_DYLD_INFO_ONLY = 0x80000022
MH_MAGIC_64 = 0xFEEDFACF
MH_TWOLEVEL = 0x80

# Every load command that occupies a dylib ORDINAL slot, in load-command order.
DYLIB_CMDS = (LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB, LC_REEXPORT_DYLIB,
              LC_LAZY_LOAD_DYLIB, LC_LOAD_UPWARD_DYLIB)

BIND_OPCODE_MASK = 0xF0
BIND_IMM_MASK = 0x0F
BIND_SYMBOL_FLAGS_WEAK_IMPORT = 0x1
OP_DONE = 0x00
OP_SET_DYLIB_ORDINAL_IMM = 0x10
OP_SET_DYLIB_ORDINAL_ULEB = 0x20
OP_SET_DYLIB_SPECIAL_IMM = 0x30
OP_SET_SYMBOL_TRAILING_FLAGS_IMM = 0x40
OP_SET_TYPE_IMM = 0x50
OP_SET_ADDEND_SLEB = 0x60
OP_SET_SEGMENT_AND_OFFSET_ULEB = 0x70
OP_ADD_ADDR_ULEB = 0x80
OP_DO_BIND = 0x90
OP_DO_BIND_ADD_ADDR_ULEB = 0xA0
OP_DO_BIND_ADD_ADDR_IMM_SCALED = 0xB0
OP_DO_BIND_ULEB_TIMES_SKIPPING_ULEB = 0xC0


def _uleb(b, i):
    v = s = 0
    while True:
        c = b[i]
        i += 1
        v |= (c & 0x7F) << s
        s += 7
        if not c & 0x80:
            break
    return v, i


def weaken_binds_for_ordinal(data, off, size, want_ord):
    """OR WEAK_IMPORT into every symbol opcode bound against `want_ord`.

    The ordinal is STATEFUL in the bind stream, so it has to be tracked while
    walking -- a symbol opcode carries no dylib of its own. Returns the symbol
    names patched."""
    patched = []
    cur = None
    i, end = off, off + size
    while i < end:
        opi = i
        op = data[i] & BIND_OPCODE_MASK
        imm = data[i] & BIND_IMM_MASK
        i += 1
        if op == OP_DONE:
            # A lazy-bind stream is a sequence of per-symbol programs each ending
            # in DONE; the ordinal does not survive across them.
            cur = None
        elif op == OP_SET_DYLIB_ORDINAL_IMM:
            cur = imm
        elif op == OP_SET_DYLIB_ORDINAL_ULEB:
            cur, i = _uleb(data, i)
        elif op == OP_SET_DYLIB_SPECIAL_IMM:
            cur = -1 if imm else 0          # self/flat/main-exec: never our dep
        elif op == OP_SET_SYMBOL_TRAILING_FLAGS_IMM:
            j = i
            while data[j] != 0:
                j += 1
            sym = data[i:j].decode()
            i = j + 1
            if cur == want_ord and not (imm & BIND_SYMBOL_FLAGS_WEAK_IMPORT):
                data[opi] |= BIND_SYMBOL_FLAGS_WEAK_IMPORT
                patched.append(sym)
        elif op in (OP_SET_ADDEND_SLEB, OP_SET_SEGMENT_AND_OFFSET_ULEB,
                    OP_ADD_ADDR_ULEB, OP_DO_BIND_ADD_ADDR_ULEB):
            _, i = _uleb(data, i)
        elif op == OP_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
            _, i = _uleb(data, i)
            _, i = _uleb(data, i)
        elif op in (OP_SET_TYPE_IMM, OP_DO_BIND, OP_DO_BIND_ADD_ADDR_IMM_SCALED):
            pass
        else:
            raise SystemExit("unknown bind opcode %#02x at %#x" % (data[opi], opi))
    return patched

NEVER_WEAKEN = ("libabiconv", "libinterpose", "libSystem")


def leaf(dep_path):
    """Two-level from-name: basename up to the first '.' (libcurl.4.dylib ->
    libcurl, QuickTime -> QuickTime). Matches nm -m's '(from X)' spelling."""
    base = dep_path.rsplit("/", 1)[-1]
    return base.split(".", 1)[0]


def bound_leaves(path):
    out = subprocess.run(["nm", "-m", path], capture_output=True, text=True).stdout
    return set(re.findall(r"\(from ([^)]+)\)", out))


def absent_leaves():
    v = os.environ.get("M64_ABSENT_DEPS", "")
    return {x.strip() for x in v.split(",") if x.strip()}


def weaken_absent(path, absent):
    """Weaken the load command AND the binds for each deliberately-absent dep."""
    data = bytearray(open(path, "rb").read())
    magic, _cpu, _sub, _ft, ncmds, sizeofcmds, flags, _res = struct.unpack_from(
        "<IiiIIIII", data, 0)
    if magic != MH_MAGIC_64:
        return 0
    # Ordinals count EVERY dylib load command, in order, 1-based.
    off, ordinal, targets, info = 32, 0, [], None
    for _ in range(ncmds):
        cmd, csz = struct.unpack_from("<II", data, off)
        if cmd in DYLIB_CMDS:
            ordinal += 1
            noff = struct.unpack_from("<I", data, off + 8)[0]
            name = data[off + noff: off + csz].split(b"\0")[0].decode()
            if leaf(name) in absent:
                targets.append((off, ordinal, name, cmd))
        elif cmd in (LC_DYLD_INFO, LC_DYLD_INFO_ONLY):
            info = struct.unpack_from("<10I", data, off + 8)
        off += csz
    if not targets:
        return 0
    n = 0
    for cmd_off, ordn, name, cmd in targets:
        if cmd == LC_LOAD_DYLIB:
            struct.pack_into("<I", data, cmd_off, LC_LOAD_WEAK_DYLIB)
        syms = []
        if info is not None:
            # info = rebase_off,size, bind_off,size, weak_off,size, lazy_off,size,
            #        export_off,size
            for o, sz in ((info[2], info[3]), (info[4], info[5]), (info[6], info[7])):
                if sz:
                    syms += weaken_binds_for_ordinal(data, o, sz, ordn)
        print("weaken-dangling-deps: %s DELIBERATELY ABSENT -> "
              "LC_LOAD_WEAK_DYLIB + %d weak-import bind(s)%s"
              % (name, len(syms), (": " + ", ".join(syms[:6])) if syms else ""))
        n += 1
    open(path, "wb").write(data)
    return n


def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <macho>", file=sys.stderr)
        return 2
    path = sys.argv[1]
    absent = absent_leaves()
    if absent:
        weaken_absent(path, absent)
    with open(path, "r+b") as f:
        hdr = f.read(32)
        magic, _cpu, _sub, _ft, ncmds, sizeofcmds, flags, _res = struct.unpack(
            "<IiiIIIII", hdr
        )
        if magic != MH_MAGIC_64:
            print(f"weaken-dangling-deps: not a thin 64-bit Mach-O ({magic:#x}), skipping")
            return 0
        if not (flags & MH_TWOLEVEL):
            print("weaken-dangling-deps: flat-namespace image, skipping")
            return 0
        cmds = f.read(sizeofcmds)

        # Collect LC_LOAD_DYLIB deps first (name -> cmd file offset).
        deps = []  # (file_off_of_cmd, dep_path)
        off = 0
        for _ in range(ncmds):
            cmd, csz = struct.unpack("<II", cmds[off : off + 8])
            if cmd == LC_LOAD_DYLIB:
                noff = struct.unpack("<I", cmds[off + 8 : off + 12])[0]
                name = cmds[off + noff : off + csz].split(b"\0")[0].decode()
                deps.append((32 + off, name))
            off += csz

        candidates = [
            (o, n)
            for (o, n) in deps
            if n.startswith("/") and leaf(n) not in NEVER_WEAKEN
        ]
        if not candidates:
            return 0

        # Leaf-name collisions: conservatively keep both strong.
        leaves = [leaf(n) for (_o, n) in deps]
        used = bound_leaves(path)
        flipped = 0
        for o, n in candidates:
            lf = leaf(n)
            if lf in used or leaves.count(lf) > 1:
                continue
            f.seek(o)
            f.write(struct.pack("<I", LC_LOAD_WEAK_DYLIB))
            flipped += 1
            print(f"weaken-dangling-deps: {n} (0 binds) -> LC_LOAD_WEAK_DYLIB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
