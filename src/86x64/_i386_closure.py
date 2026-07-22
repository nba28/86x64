#!/usr/bin/env python3
"""_i386_closure.py — expand a translate TARGET executable into its full
co-translated i386 binary CLOSURE (the main executable PLUS every dependency
that `m64 translate` vendors + translates alongside it: bundle-embedded
frameworks/dylibs, source-pool originals for dead absolute install paths,
@loader_path/@rpath siblings).

WHY: the abigen consider-set discovery (abigen_modern_manifest.py, modern pass;
shimdb_legacy_considerset.py, legacy pass) is IMPORT-DRIVEN — it reads each
target binary's undefined externals and shims exactly that surface. It used to
read ONLY the main executable. That is structurally wrong for any app whose
code lives in companion binaries:

  * iWork '09: Numbers/Pages/Keynote main exes are thin; the app code is in
    15+ SF*.framework classic dylibs installed OUTSIDE the bundle
    ("/Library/Application Support/iWork '09/Frameworks/…", dead on modern
    macOS, vendored from the source pool at translate time). SFTabular alone
    imports 31 AddressBook kAB* data constants the main exe never mentions —
    unseen symbols got no shim/shadow, so the translated 4-byte load truncated
    the native 64-bit address and crashed SFTabular's static init.
  * Source-engine games: engine.dylib pulls @loader_path sibling dylibs.

The import surface that needs shims is the union over EVERY binary that ends up
translated, i.e. exactly this closure. Universal: triggered by dependency
structure (what would be vendored+translated), never by app identity.

Reuses _vendor_deps.py's primitives (DEFAULT_SOURCES pools, dep listing,
framework-name parsing, pool search, native-home checks) so the discovery
closure resolves deps the same way the vendoring step will. The walk is
ADDITIVE and fail-soft: an unresolvable or arch-missing dep is skipped (and
reported) — that import surface simply stays as uncovered as it was before.

Library use:
    from _i386_closure import expand_target
    bins, skipped = expand_target(exe, arch="i386", sources=[...extra pools...])

CLI (debugging):
    _i386_closure.py <exe> [--arch i386] [--source POOL]...
"""
import argparse
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _vendor_deps import (DEFAULT_SOURCES, DEFAULT_NATIVE, deps_of, fw_name,
                          search_sources, framework_binary, is_macho,
                          system_fw_exists)
try:
    # dyld-cache-aware "is this /System//usr dep actually loadable natively"
    # oracle (a cache-served lib has no file on disk). Older revisions may not
    # export it; fall back to a plain existence check.
    from _vendor_deps import system_dep_loadable
except ImportError:      # pragma: no cover
    def system_dep_loadable(p):
        return os.path.exists(p)

_ARCH_CACHE = {}


def has_arch(path, arch):
    """True iff the Mach-O at `path` carries an `arch` slice (thin or fat)."""
    key = (str(path), arch)
    if key not in _ARCH_CACHE:
        out = subprocess.run(["lipo", "-archs", str(path)],
                             capture_output=True, text=True).stdout
        _ARCH_CACHE[key] = arch in out.split()
    return _ARCH_CACHE[key]


def bundle_root(binary):
    """The enclosing .app dir of `binary`, or None (bare dylib targets)."""
    p = Path(binary).resolve()
    for parent in p.parents:
        if parent.name.endswith(".app"):
            return parent
    return None


def _bundle_index(broot):
    """basename -> Path for every Mach-O file in the bundle (resolves @rpath /
    @executable_path / dead-abs deps whose leaf is already embedded)."""
    idx = {}
    if broot is None:
        return idx
    for p in broot.rglob("*"):
        if p.is_file() and "_CodeSignature" not in p.parts and is_macho(p):
            idx.setdefault(p.name, p)
    return idx


def _resolve_dep(dep, loader_dir, exe_dir, bundle_index, sources, arch):
    """Resolve one LC_LOAD_DYLIB install path to an on-disk CO-TRANSLATED
    binary (returns Path), or None when the dep is native / unresolvable.
    Mirrors _vendor_deps.vendor()'s decisions, restricted to discovery."""
    kn = fw_name(dep)                       # ('framework'|'dylib', name) | None
    name = kn[1] if kn else os.path.basename(dep)

    # Frameworks that must run NATIVE are never translated -> their imports are
    # not our marshalling surface.
    if name in DEFAULT_NATIVE:
        return None

    # /System, /usr: native when actually loadable (on disk OR dyld-cache
    # served). A DEAD system path (removed Python 2.x, QuickTime) falls through
    # to the pool exactly like _vendor_deps' dead-absolute-system vendor.
    if dep.startswith("/System/") or dep.startswith("/usr/"):
        if system_dep_loadable(dep):
            return None
        # dead -> maybe pool below
    elif os.path.isfile(dep):
        # live app-private absolute path
        return Path(dep)

    # a name with a modern native system home runs native (vendor() parity)
    if kn and system_fw_exists(kn[0], kn[1]):
        return None

    # @loader_path / @executable_path relative siblings
    for pfx, base_dir in (("@loader_path", loader_dir),
                          ("@executable_path", exe_dir)):
        if dep.startswith(pfx + "/") and base_dir is not None:
            cand = Path(os.path.realpath(os.path.join(base_dir,
                                                      dep[len(pfx) + 1:])))
            if cand.is_file():
                return cand

    # @rpath / dead absolute whose leaf already lives in the bundle
    hit = bundle_index.get(os.path.basename(dep))
    if hit is not None:
        return hit

    # source pools (the i386 originals m64 vendors at translate time)
    if kn:
        src = search_sources(kn[0], kn[1], sources)
        if src is not None:
            return framework_binary(src) if kn[0] == "framework" else src
    return None


def expand_target(exe, arch="i386", sources=None):
    """BFS the co-translated closure of `exe`.

    Returns (binaries, skipped):
      binaries: [Path] — `exe` first, then every resolved dep binary that has
                an `arch` slice (each appears once; order deterministic).
      skipped:  [(dep_install_path, reason)] — deps that stayed uncovered
                ('unresolved') or resolved without the wanted slice
                ('no-<arch>-slice'), for gap visibility.
    """
    exe = Path(os.path.realpath(os.path.expanduser(str(exe))))
    if sources is None:
        sources = list(DEFAULT_SOURCES)
    else:
        sources = [Path(s) for s in sources] + list(DEFAULT_SOURCES)

    broot = bundle_root(exe)
    bidx = _bundle_index(broot)
    exe_dir = exe.parent

    out, skipped = [], []
    visited = set()
    queue = [exe]
    visited.add(str(exe))
    while queue:
        b = queue.pop(0)
        out.append(b)
        for dep in deps_of(b):
            r = _resolve_dep(dep, b.parent, exe_dir, bidx, sources, arch)
            if r is None:
                # native or genuinely unresolvable; only report the latter
                kn = fw_name(dep)
                name = kn[1] if kn else os.path.basename(dep)
                native = (name in DEFAULT_NATIVE
                          or ((dep.startswith("/System/")
                               or dep.startswith("/usr/"))
                              and system_dep_loadable(dep))
                          or (kn and system_fw_exists(kn[0], kn[1])))
                if not native:
                    skipped.append((dep, "unresolved"))
                continue
            r = Path(os.path.realpath(str(r)))
            if str(r) in visited:
                continue
            visited.add(str(r))
            if not (r.is_file() and is_macho(r)):
                skipped.append((dep, "not-macho"))
                continue
            if not has_arch(r, arch):
                skipped.append((dep, "no-%s-slice (%s)" % (arch, r)))
                continue
            queue.append(r)
    return out, skipped


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("exe")
    ap.add_argument("--arch", default="i386")
    ap.add_argument("--source", action="append", default=[], metavar="DIR",
                    help="extra source pool searched before the defaults")
    a = ap.parse_args(argv)
    bins, skipped = expand_target(a.exe, a.arch, a.source or None)
    for b in bins:
        print(b)
    for dep, why in skipped:
        print("  ## skipped: %s (%s)" % (dep, why), file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
