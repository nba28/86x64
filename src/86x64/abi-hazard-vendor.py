#!/usr/bin/env python3
"""abi-hazard-vendor.py — redirect a TRANSLATED consumer's bind on a system
C++-runtime dylib to our own translated (i386-ABI) replacement.

THE PROBLEM (Portal 2 2026-09-29, ~50 libstdc++ symbols in
unbridged-native-calls.py's output — basic_ios/ostream/istream/filebuf/
sentry/ios_base, operator<<, ...): a translated i386 caller reaches these
through the SAME 4-byte-return-address convention as every other unbridged
native call (the unbridged native-call bug) -- `/usr/lib/libstdc++.6.dylib` on
modern macOS is a native x86_64 build with SysV `ret`/register-arg
conventions, so an i386 cdecl call into it is a raw cross-ABI call (fused PC
on return, garbage args). Hand-marshalling this surface one symbol at a time
(cow_string_shim.c / iostream_shim.c) is the Tier-2 track the C ABI bridging notes
flags as hard: libstdc++ objects carry an i386 MEMORY LAYOUT (basic_string's
COW rep, locale facets, ios_base's bitmask + streambuf pointers), not just an
ABI convention, so a marshalling shim would have to reimplement the class,
not just convert arguments.

THE FIX (decision, 2026-09-29): translate a genuine i386 libstdc++.6.dylib
with the ordinary pipeline (the i386 original comes from the Snow Leopard
install image, as for the test sysroot) -- so libstdc++ becomes a SIBLING
TRANSLATED module, exactly like every other dylib in the tree. Sibling
translated modules already use the same 4-byte-return convention as each
other (that's why the 42-module Portal 2 tree works at all), and the object
layout matches by construction (it's the SAME i386 code, just re-emitted for
x86_64 instructions). This closes the whole surface at once instead of hand-
shimming ~50 more entry points, and it generalizes to any translated i386
target that links libstdc++ -- not just Portal 2.

THE MECHANISM (reused, not invented): the same "copy the golden binary next
to the consumer, then `install_name_tool -change` the consumer's bind onto
the co-located copy" pattern already used for libabiconv itself
(86x64.sh's `--insert load-dylib,name=@loader_path/libabiconv.dylib`),
shimgen's <FW>ShimAuto.dylib swap, and _vendor_deps.py's framework vendoring.
This script applies exactly that pattern to one more binary, gated on a
small explicit registry (a system dylib basename -> our translated
replacement), not on any app name -- universal per the universal-fixes rule.

Only a TRANSLATED consumer (one that already links libabiconv) gets
redirected: a native x86_64 sibling in the same tree must keep the real
system libstdc++, which is what its own SysV calls actually expect.

usage: abi-hazard-vendor.py <root>...    (a bundle .app or a flat tree; walked
                                           recursively for Mach-O files)
       abi-hazard-vendor.py --list <root>...   (report only, no writes)
"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

import m64_paths

# system dylib basename -> our translated (i386-ABI, libabiconv-linked) replacement.
# Built by: m64 translate <i386-original> -o <path>. Extend this registry, never
# special-case an app -- any translated tree binding the same system dylib benefits.
GOLDEN = {
    "libstdc++.6.dylib":
        m64_paths.FRAMEWORKS / "libstdc++-x86_64/libstdc++.6.dylib",
}


def info(m): print(f"\033[1;36m==>\033[0m {m}")
def good(m): print(f"\033[1;32m  ok\033[0m {m}")
def warn(m): print(f"\033[1;33m  !!\033[0m {m}", file=sys.stderr)


def find_int():
    for c in ("/usr/local/opt/llvm/bin/llvm-install-name-tool",
              "/opt/homebrew/opt/llvm/bin/llvm-install-name-tool"):
        if os.access(c, os.X_OK):
            return c
    return shutil.which("llvm-install-name-tool") or "install_name_tool"
INT = find_int()


def is_macho(p):
    try:
        with open(p, "rb") as f:
            m = f.read(4)
    except OSError:
        return False
    return m in (b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf',
                 b'\xce\xfa\xed\xfe', b'\xfe\xed\xfa\xce',
                 b'\xfa\xed\xfe\xcf', b'\xcf\xed\xfa\xfe',
                 b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca')


def deps_of(binary):
    out = subprocess.run(["otool", "-L", str(binary)], capture_output=True,
                         text=True, errors="replace").stdout
    for line in out.splitlines()[1:]:
        line = line.strip()
        if "(compatibility version" in line:
            yield line.split(" (compatibility version")[0]


def links_our_runtime(path):
    """True iff this Mach-O is one of OUR translated artifacts (loads libabiconv),
    i.e. it uses the i386 4-byte-return convention and needs the redirect. A
    native x86_64 sibling must keep binding the real system libstdc++."""
    try:
        out = subprocess.run(["otool", "-L", str(path)],
                             capture_output=True, text=True).stdout
    except Exception:
        return False
    return "libabiconv" in out


def iter_machos(root):
    root = Path(root)
    if root.is_file():
        if is_macho(root):
            yield root
        return
    for p in root.rglob("*"):
        if p.is_file() and not p.is_symlink() and is_macho(p):
            yield p


def local_leaf(base):
    """The co-located copy's file name: NOT the system leaf. dyld resolves an
    absolute load (/usr/lib/<base>) by leaf name through DYLD_LIBRARY_PATH first
    (Portal 2's launcher and run-portal2.sh set it to the tree), so a copy named
    <base> there would replace the REAL system dylib for every native loader too
    (libabiconv's own binds, cxx_shim.c's dlopen) -- a native call into 4-byte-ret
    translated code."""
    stem = base[:-len(".dylib")] if base.endswith(".dylib") else base
    return f"{stem}.i386.dylib"


def redirect(consumer, base, golden_src, dry):
    """Co-locate `golden_src` beside `consumer` under local_leaf(base) and
    repoint the ONE dep on the system copy (or an older @loader_path/<base>
    redirect) at it."""
    leaf = local_leaf(base)
    dst = consumer.parent / leaf
    new_dep = f"@loader_path/{leaf}"
    if dry:
        print(f"    dry-run: cp {golden_src} {dst}")
        print(f"    dry-run: install_name_tool -change ... {new_dep} {consumer}")
        return
    # Its own install name too: the golden asset's LC_ID_DYLIB is still the
    # system path, and dyld matches loaded images by install name.
    shutil.copy2(golden_src, dst)
    subprocess.run([INT, "-id", new_dep, str(dst)], capture_output=True, text=True)
    # The copy sits beside its consumer, so it must reach the runtime the way the
    # consumer does: a bundle keeps libabiconv in Contents/Frameworks
    # (@loader_path/../Frameworks/...), not beside MacOS/ like Portal 2's flat tree
    # (Call of Duty 4: "Library not loaded: @loader_path/libabiconv.dylib").
    theirs = {os.path.basename(d): d for d in deps_of(consumer)}
    for d in deps_of(dst):
        want = theirs.get(os.path.basename(d))
        if d.startswith("@loader_path/") and want and want != d:
            subprocess.run([INT, "-change", d, want, str(dst)], capture_output=True, text=True)
    subprocess.run(["codesign", "--force", "--sign", "-", str(dst)],
                   capture_output=True)
    stale = consumer.parent / base    # an older redirect's system-named copy
    if stale.is_file() and links_our_runtime(stale):
        stale.unlink()
    old_dep = next((d for d in deps_of(consumer) if os.path.basename(d) == base
                    and (not d.startswith("@") or d == f"@loader_path/{base}")), None)
    if old_dep is None or old_dep == new_dep:
        return
    subprocess.run([INT, "-change", old_dep, new_dep, str(consumer)],
                   capture_output=True, text=True)
    subprocess.run(["codesign", "--force", "--sign", "-", str(consumer)],
                   capture_output=True)


def main(argv):
    list_only = "--list" in argv
    roots = [a for a in argv if a != "--list"]
    if not roots:
        print(__doc__, file=sys.stderr)
        return 2
    for g in GOLDEN.values():
        if not g.is_file():
            warn(f"no golden replacement built at {g} -- "
                 f"`m64 translate <i386-original> -o {g}` first")
    n_redirected = 0
    n_consumers = 0
    ours = {local_leaf(b): b for b in GOLDEN}
    for root in roots:
        for m in iter_machos(root):
            if not links_our_runtime(m):
                continue          # native sibling: leave bound to the real system lib
            if m.name in ours:
                continue          # a vendored copy: otool lists its own id as a dep
            for dep in deps_of(m):
                base = os.path.basename(dep)
                base = ours.get(base, base)   # our own earlier redirect: redo it
                if dep.startswith("@") and dep not in (f"@loader_path/{base}",
                                                       f"@loader_path/{local_leaf(base)}"):
                    continue      # bundle-relative and not an old redirect
                golden = GOLDEN.get(base)
                if golden is None or not golden.is_file():
                    continue
                if list_only and dep == f"@loader_path/{local_leaf(base)}":
                    continue      # already redirected
                n_consumers += 1
                if list_only:
                    print(f"  {m}  binds {dep}")
                    continue
                info(f"redirect {m.name}: {dep} -> @loader_path/{local_leaf(base)}")
                redirect(m, base, golden, dry=False)
                n_redirected += 1
    if list_only:
        info(f"{n_consumers} translated consumer(s) bind an ABI-hazard system dylib")
    else:
        good(f"{n_redirected} consumer(s) redirected to the translated replacement")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
