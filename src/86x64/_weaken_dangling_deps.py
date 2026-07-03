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
"""
import re
import struct
import subprocess
import sys

LC_LOAD_DYLIB = 0x0C
LC_LOAD_WEAK_DYLIB = 0x80000018
MH_MAGIC_64 = 0xFEEDFACF
MH_TWOLEVEL = 0x80

NEVER_WEAKEN = ("libabiconv", "libinterpose", "libSystem")


def leaf(dep_path):
    """Two-level from-name: basename up to the first '.' (libcurl.4.dylib ->
    libcurl, QuickTime -> QuickTime). Matches nm -m's '(from X)' spelling."""
    base = dep_path.rsplit("/", 1)[-1]
    return base.split(".", 1)[0]


def bound_leaves(path):
    out = subprocess.run(["nm", "-m", path], capture_output=True, text=True).stdout
    return set(re.findall(r"\(from ([^)]+)\)", out))


def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <macho>", file=sys.stderr)
        return 2
    path = sys.argv[1]
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
