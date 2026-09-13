#!/usr/bin/env python3
"""Find translated images that can hijack a NATIVE image's C++ coalesced binds.

THE DEFECT THIS FINDS (Portal 2 2026-09-13, root-caused)
--------------------------------------------------------
MH_WEAK_DEFINES (0x8000) in a Mach-O header advertises "my weak definitions are
candidates for process-wide C++ coalescing".  dyld resolves another image's
<weak-def-coalesce> bind by searching the images carrying that bit, in load
order, and taking the first definition it finds.

In an all-i386 process every candidate has the same ABI, so that is harmless.  A
TRANSLATED image, however, shares its process with NATIVE frameworks -- so the
bit lets i386 code satisfy an x86_64 caller's bind.  Measured consequence:

    AGXMetalG16X (native GPU driver) calls operator delete through a
    <weak-def-coalesce> bind.  dyld bound the DRIVER's slot to launcher.dylib's
    TRANSLATED __ZdlPv.  The i386 epilogue's 4-byte `pop %ebp` zeroes the high
    half of rbp, so the driver's next frame-pointer store went to unmapped low
    memory:  movq %rax,-0x9d0(%rbp)  at AGXMetalG16X+0x3f7e59.

It presented as "the app dies in renderer init" for a long time.  The victim was
the driver; the culprit was the translated image.  Every native framework in the
process is exposed this way, not just GPU drivers -- operator new/delete,
template instantiations, inline functions and typeinfos are all coalesced.

Cleared at translate time in src/core/transform.cc (kill switch
M64_KEEP_WEAK_DEFINES=1).  This tool is the artifact-level check that the fix
actually reached the shipped bytes, and the detector for any tree translated by
an older macho-tool.

⚠ READ THE DIRECTIONALITY BEFORE "FIXING" A HIT BY HIDING SYMBOLS.  A translated
image that IMPORTS operator new/delete binds `libabiconv/____ZdlPv` -- an abigen
bridge -- as an ordinary TWO-LEVEL bind, and its calls to its own definition are
direct rather than binds.  Making the definitions private-extern therefore
BREAKS the importers (Portal 2's libtier0/libsteam_api/engine have no local
definition and must keep reaching the i386 bridge).  Clearing the header bit is
what has the right directionality: translated importers keep translated
definitions, native importers get native ones.

⚠ A CLEAN REPORT IS NOT AN EXONERATION OF THE PROCESS.  This only inspects the
images you point it at.  The authoritative runtime question is where a NATIVE
image's own got slot actually points -- read that slot, because
dlsym(RTLD_DEFAULT, "_ZdlPv") answers the flat search order instead and reports
libc++abi even in a failing run.  See gl-call-probe.c's agx_delete_slot().

usage:
    weakdef-coalesce-scan.py <file-or-dir>...        # report offenders
    weakdef-coalesce-scan.py --all <file-or-dir>...  # report every image
exit: 0 = no offenders, 1 = at least one, 2 = usage/read error
"""
import os
import struct
import sys

MH_MAGIC_64 = 0xFEEDFACF
MH_CIGAM_64 = 0xCFFAEDFE
MH_MAGIC_32 = 0xFEEDFACE
MH_CIGAM_32 = 0xCEFAEDFE
FAT_MAGIC = 0xCAFEBABE
FAT_CIGAM = 0xBEBAFECA

# ⚠ FROM mach-o/loader.h, VERIFIED against the header -- do not eyeball these.
# 0x1000 is MH_ALLMODSBOUND, and using it here made this tool report EVERY tree
# clean, including one otool proved carried the bit. A detector whose constant is
# wrong is worse than no detector: it manufactures exonerations.
MH_WEAK_DEFINES = 0x8000
MH_BINDS_TO_WEAK = 0x10000
CPU_TYPE_X86_64 = 0x01000007


def slices(data):
    """Yield (offset, magic) for each Mach-O slice, FAT or thin."""
    if len(data) < 8:
        return
    magic = struct.unpack(">I", data[:4])[0]
    if magic in (FAT_MAGIC, FAT_CIGAM):
        # ⚠ A FAT binary's offsets are SLICE-relative; never read a thin header's
        # fields against the whole file (see the fat-slice-offsets gotcha).
        nfat = struct.unpack(">I", data[4:8])[0]
        for i in range(nfat):
            off = 8 + i * 20
            if off + 20 > len(data):
                return
            _, _, soff, _, _ = struct.unpack(">5I", data[off:off + 20])
            if soff + 4 <= len(data):
                yield soff, struct.unpack("<I", data[soff:soff + 4])[0]
        return
    yield 0, struct.unpack("<I", data[:4])[0]


LC_SEGMENT_64 = 0x19
TRANSLATED_TEXT_BASE = 0x10000000   # IR_TRANSLATED_TEXT_BASE (import_repair.c)


def header(data, off, magic):
    """Return (cputype, flags, text_vmaddr) or None if not a Mach-O we read.

    text_vmaddr is the PREFERRED __TEXT vmaddr, which is how we tell OUR
    translated output from a natively-compiled image: macho-tool bases a
    translated image's __TEXT at 0x10000000, while anything clang built (this
    runtime's own libabiconv, the wrapper exec, a probe) is 0x0 or a PIE base.
    Same discriminator the runtime gates on, deliberately.
    """
    if magic in (MH_MAGIC_64, MH_MAGIC_32):
        end = "<"
    elif magic in (MH_CIGAM_64, MH_CIGAM_32):
        end = ">"
    else:
        return None
    is64 = magic in (MH_MAGIC_64, MH_CIGAM_64)
    if off + 32 > len(data):
        return None
    cputype = struct.unpack(end + "i", data[off + 4:off + 8])[0]
    ncmds = struct.unpack(end + "I", data[off + 16:off + 20])[0]
    flags = struct.unpack(end + "I", data[off + 24:off + 28])[0]

    text_vmaddr = None
    p = off + (32 if is64 else 28)
    for _ in range(min(ncmds, 512)):
        if p + 8 > len(data):
            break
        cmd, cmdsize = struct.unpack(end + "II", data[p:p + 8])
        if cmdsize < 8:
            break
        if cmd == LC_SEGMENT_64 and p + 32 + 8 <= len(data):
            name = data[p + 8:p + 24].rstrip(b"\x00")
            if name == b"__TEXT":
                text_vmaddr = struct.unpack(end + "Q", data[p + 24:p + 32])[0]
                break
        p += cmdsize
    return cputype, flags, text_vmaddr


def scan(path):
    """Return a list of (path, arch, flags, offending) for each slice."""
    try:
        with open(path, "rb") as fh:
            data = fh.read()
    except (IOError, OSError):
        return []
    out = []
    for off, magic in slices(data):
        h = header(data, off, magic)
        if h is None:
            continue
        cputype, flags, text_vmaddr = h
        # A finding requires ALL THREE: the bit, an x86_64 slice, and OUR
        # translated __TEXT base.
        #   - an i386/arm64 slice is somebody else's untranslated binary;
        #   - a NATIVE x86_64 image with weak defs (our own libabiconv, the
        #     wrapper exec, a probe dylib) is ABI-CORRECT and harmless -- its
        #     coalesced definitions are real x86_64 code.
        # Flagging either would train the reader to ignore this tool, and
        # libabiconv trips exactly that case in every deployed tree.
        translated = (text_vmaddr == TRANSLATED_TEXT_BASE)
        offending = (bool(flags & MH_WEAK_DEFINES)
                     and cputype == CPU_TYPE_X86_64 and translated)
        arch = {CPU_TYPE_X86_64: "x86_64"}.get(cputype, "cpu%d" % cputype)
        kind = "translated" if translated else "native"
        out.append((path, "%s/%s" % (arch, kind), flags, offending))
    return out


def walk(args):
    for a in args:
        if os.path.isdir(a):
            for root, _, files in os.walk(a):
                for f in sorted(files):
                    yield os.path.join(root, f)
        else:
            yield a


def main(argv):
    show_all = False
    args = []
    for a in argv:
        if a == "--all":
            show_all = True
        elif a.startswith("-"):
            sys.stderr.write(__doc__)
            return 2
        else:
            args.append(a)
    if not args:
        sys.stderr.write(__doc__)
        return 2

    nbad = nseen = 0
    for path in walk(args):
        for p, arch, flags, offending in scan(path):
            nseen += 1
            if offending:
                nbad += 1
                print("WEAK_DEFINES  %-18s flags=0x%06x  %s" % (arch, flags, p))
            elif show_all:
                print("ok            %-18s flags=0x%06x  %s" % (arch, flags, p))

    print()
    print("scanned %d Mach-O slice(s); %d advertise MH_WEAK_DEFINES" % (nseen, nbad))
    if nbad:
        print("⚠ each of these can satisfy a NATIVE image's <weak-def-coalesce> bind")
        print("  with i386 code. Retranslate with a macho-tool that clears the bit")
        print("  (src/core/transform.cc); M64_KEEP_WEAK_DEFINES=1 keeps the old way.")
    return 1 if nbad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
