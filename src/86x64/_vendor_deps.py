#!/usr/bin/env python3
"""Vendor missing framework/dylib dependencies into an app bundle.

Many i386 apps link shared frameworks that lived OUTSIDE the .app — e.g. the
iWork '09 apps reference /Library/Application Support/iWork '09/Frameworks/
SFTabular.framework etc. Those paths don't exist on a modern machine and the
frameworks aren't inside the bundle, so dyld hard-fails at load and
`m64 rpath-fix` has nothing in-bundle to point at.

This walks the full LC_LOAD_DYLIB closure of every Mach-O already in the
bundle. Any dependency that is neither

  (a) already inside Contents/Frameworks, nor
  (b) a host-resolvable system framework (/System, /usr, or an absolute path
      that still exists on disk),

is "missing". For each missing framework/dylib we search a set of SOURCE ROOTS
for the matching <Name>.framework / <name>.dylib, copy it into
Contents/Frameworks, rewrite its install id to @rpath, and recurse into ITS
own dependencies (transitive closure).

The copied-in originals are still i386 — `m64 translate` then picks them up
like any other bundled binary, and `m64 rpath-fix` rewrites every consumer's
absolute reference to @rpath. So the intended order is:

    m64 vendor <app>      # (auto-run at the start of `m64 translate`)
    m64 translate <app>
    m64 rpath-fix <app>   # or `m64 deploy`

usage: _vendor_deps.py <app> [--source DIR]... [--dry-run]
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

# Default places to look for the original i386 frameworks, most-specific first.
# Repeatable --source args are searched ahead of these.
DEFAULT_SOURCES = [
    # Curated i386 originals after the 2026-07-05 reorg (~/Downloads/iLife11 is
    # GONE). ~/projects/Library/Frameworks holds the reusable framework library
    # (incl. the iLife11/ subdir with iLifeFaceRecognition/Helium/…); each root is
    # rglob-walked, so the iLife11/ nesting resolves automatically.
    Path.home() / "projects/Library/Frameworks",
    Path.home() / "projects/Library/Application Support",
    Path("/Library/Application Support"),
    Path("/Library/Frameworks"),
]

LINE_RE = re.compile(r'^\s+(?P<path>.*?)\s+\(compatibility version')
FW_RE = re.compile(r'/(?P<fw>[^/]+)\.framework/(?:Versions/[^/]+/)?(?P=fw)$')

# Frameworks that must run NATIVE (x86_64 under Rosetta) rather than be
# translated, because they export ObjC classes the rest of the system consumes
# (a translated, libabiconv-linked copy can't). These are never vendored even
# when their original absolute path is dead and no longer in /System; supply
# them natively (e.g. extracted from a Snow Leopard system framework). Override
# / extend with --native NAME. Seeded with the one known iLife/iWork case.
DEFAULT_NATIVE = {"MobileMe"}


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
                 b'\xfa\xed\xfe\xcf', b'\xcf\xed\xfa\xfe',
                 b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca')


def deps_of(binary):
    """Yield the install paths of every LC_LOAD_DYLIB of `binary` (space-safe)."""
    out = subprocess.run(["otool", "-L", str(binary)], capture_output=True,
                         text=True, errors="replace").stdout
    for line in out.splitlines()[1:]:
        m = LINE_RE.match(line)
        if m:
            yield m.group('path')


def fw_name(path):
    """('framework', 'SFTabular') | ('dylib', 'libfoo.dylib') | None for a dep path."""
    m = FW_RE.search(path)
    if m:
        return ("framework", m.group('fw'))
    base = os.path.basename(path)
    if base.endswith(".dylib"):
        return ("dylib", base)
    return None


# System frameworks whose on-disk .framework is a HOLLOW SHELL on modern macOS:
# the directory + Info.plist still exist under /System/Library/Frameworks, but the
# x86_64 binary was REMOVED from the dyld shared cache, so dlopen fails ("Library
# not loaded: .../QuickTime … not in dyld cache"). A bare directory-exists check
# wrongly treats these as host-resolvable and skips them → the translated app then
# hard-fails at LOAD. These must be VENDORED (from a curated x86_64 copy in
# ~/projects/Library/Frameworks, e.g. QuickTime.framework/{86x64,iLife11,legacy}).
# QuickTime is the documented one for our i386 targets (Halo/iMovie/iLife); extend
# as other removed frameworks surface. (General follow-up: replace this curated set
# with a live `arch -x86_64` dlopen loadability probe — see todo_gaps.)
HOLLOW_SHELL_SYSTEM_FRAMEWORKS = {"QuickTime"}


def system_fw_exists(kind, name):
    """A modern-macOS native home for this dependency? If so we must NOT vendor
    and translate the i386 original — it has to run native so it can keep
    exporting its ObjC classes to the rest of the system. A hollow-shell framework
    (dir present but x86_64 binary stripped from the dyld cache) is NOT a real
    native home → return False so it gets vendored."""
    if kind == "framework":
        if name in HOLLOW_SHELL_SYSTEM_FRAMEWORKS:
            return False
        return any(Path(p, f"{name}.framework").is_dir() for p in (
            "/System/Library/Frameworks", "/System/Library/PrivateFrameworks"))
    return any(Path(p, name).exists() for p in (
        "/usr/lib", "/usr/local/lib", "/System/Library/Frameworks"))


def search_sources(kind, name, sources):
    """Find the original framework dir / dylib file under the source roots."""
    want = f"{name}.framework" if kind == "framework" else name
    for root in sources:
        if not root.is_dir():
            continue
        # shallow hit first (the common /Library/Frameworks/<Name>.framework)
        direct = root / want
        if (kind == "framework" and direct.is_dir()) or \
           (kind == "dylib" and direct.is_file()):
            return direct
        # otherwise walk (bounded — frameworks nest only a couple levels)
        for hit in root.rglob(want):
            if kind == "framework" and hit.is_dir():
                return hit
            if kind == "dylib" and hit.is_file():
                return hit
    return None


def framework_binary(fwdir):
    """The current-version Mach-O inside a copied <Name>.framework dir."""
    name = fwdir.name[:-len(".framework")]
    for cand in (fwdir / name,                              # flat or symlink
                 fwdir / "Versions" / "Current" / name):
        if cand.exists():
            return cand
    vers = fwdir / "Versions"
    if vers.is_dir():
        for v in sorted(vers.iterdir()):
            b = v / name
            if b.is_file():
                return b
    return fwdir / name


def vendor(app, sources, dry, native=DEFAULT_NATIVE):
    bfw = app / "Contents/Frameworks"
    bfw.mkdir(parents=True, exist_ok=True)

    def is_bundled(kind, name):
        return (bfw / f"{name}.framework").is_dir() if kind == "framework" \
            else (bfw / name).is_file()

    # Every basename present anywhere in the bundle, to resolve @rpath /
    # @loader_path / @executable_path deps (our own runtime libs + the
    # translation's split .dylibs live in MacOS/, not Frameworks/).
    present = {p.name for p in app.rglob("*") if p.is_file()}

    # Seed the queue with everything already in the bundle.
    queue = []
    macos = app / "Contents/MacOS"
    if macos.is_dir():
        queue += [p for p in macos.glob("*") if p.is_file() and is_macho(p)]
    queue += [p for p in bfw.rglob("*")
              if p.is_file() and is_macho(p) and "_CodeSignature" not in p.parts]

    visited_bins = set()
    handled = set()          # (kind, name) already vendored / decided
    vendored, unresolved, dangling = [], {}, {}

    while queue:
        b = queue.pop()
        key = str(b.resolve())
        if key in visited_bins:
            continue
        visited_bins.add(key)
        for dep in deps_of(b):
            fn = fw_name(dep)
            if fn is None:
                continue
            kind, name = fn
            if is_bundled(kind, name):
                continue                              # already in the bundle
            if dep.startswith('@'):
                # bundle-relative: fine if it resolves anywhere in the bundle,
                # otherwise just flag it — we never vendor (and translate) over
                # a @-relative ref, which is reserved for native frameworks the
                # bundle is expected to supply itself.
                if os.path.basename(dep) not in present:
                    dangling.setdefault((kind, name), set()).add(b.name)
                continue
            if dep.startswith('/System/') or dep.startswith('/usr/'):
                # OS-owned location → normally native and left alone. EXCEPTION:
                # a hollow-shell framework (dir present but x86_64 binary stripped
                # from the dyld cache, e.g. QuickTime) is NOT loadable, so fall
                # through to vendoring instead of skipping — otherwise the app
                # dyld-fails at load ("Library not loaded: .../QuickTime").
                if not (kind == "framework"
                        and name in HOLLOW_SHELL_SYSTEM_FRAMEWORKS):
                    continue
            elif os.path.exists(dep):
                continue          # resolves at its absolute path → host-native
            if system_fw_exists(kind, name):
                continue          # has a modern native home → leave native
            # `native`-marked (e.g. MobileMe): it must RUN native (its own x86_64
            # slice), never be translated. If it already resolves at a host /
            # native home the checks above `continue`d. Reaching here means the
            # dep is an app-private path that no longer exists (iWork '09's
            # MobileMe lives at "/Library/Application Support/iWork '09/…" — gone
            # on modern macOS) AND it is not yet bundled → it must be VENDORED
            # from a curated source that carries an x86_64 slice, then left
            # native (m64 translate classifies an x86_64-having binary as
            # "native" and leaves it alone; rpath-fix repoints the consumer at
            # the in-bundle copy). Only if no source is found do we flag it
            # dangling. Without this a `native` framework with no host home was
            # dropped entirely -> dyld "Library not loaded" at launch
            # (Numbers/iWork SFApplication -> MobileMe). Falls through to the
            # shared vendoring block below.
            # An app-private absolute path (not under /System or /usr) that no
            # longer exists, with no native home and not bundled: vendor it in.
            if (kind, name) in handled:
                continue
            handled.add((kind, name))
            src = search_sources(kind, name, sources)
            if src is None:
                unresolved[(kind, name)] = {b.name}
                continue
            # Copy into the bundle (preserving the Versions/Current symlinks).
            dst = bfw / src.name
            info(f"vendor {src.name}  <- {src}")
            if dry:
                print(f"    dry-run: cp -R {src} {dst}")
            else:
                if dst.exists():
                    shutil.rmtree(dst) if dst.is_dir() else dst.unlink()
                if kind == "framework":
                    shutil.copytree(src, dst, symlinks=True)
                else:
                    shutil.copy2(src, dst)
            vendored.append(src.name)
            # Recurse into the copied-in binary so its own app-private deps
            # (e.g. SF* cross-references) get vendored too. Its install id is
            # left as shipped — `translate-bundle` preserves it and consumers
            # are rewritten to @rpath by `m64 rpath-fix`, so dyld resolves the
            # in-bundle copy regardless of the loaded dylib's LC_ID.
            vb = framework_binary(dst) if kind == "framework" else dst
            if not dry and vb.exists() and is_macho(vb):
                queue.append(vb)

    info(f"vendored {len(vendored)} framework(s)/dylib(s) into {app.name}")
    for v in vendored:
        good(v)
    if unresolved:
        warn(f"{len(unresolved)} app-private dependency(ies) not found in any "
             f"source root {[str(s) for s in sources]} — pass --source:")
        for (kind, name), who in sorted(unresolved.items()):
            print(f"      • {name}{'.framework' if kind=='framework' else ''}"
                  f"  <- {', '.join(sorted(who))}", file=sys.stderr)
    if dangling:
        warn(f"{len(dangling)} bundle-relative (@rpath) dependency(ies) resolve "
             f"to nothing in the bundle — supply these natively if needed:")
        for (kind, name), who in sorted(dangling.items()):
            print(f"      • {name}{'.framework' if kind=='framework' else ''}"
                  f"  <- {', '.join(sorted(who))}", file=sys.stderr)
    return len(vendored), len(unresolved)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("app")
    ap.add_argument("--source", action="append", default=[], metavar="DIR",
                    help="extra directory to search for original frameworks "
                         "(repeatable, searched before the defaults)")
    ap.add_argument("--native", action="append", default=[], metavar="NAME",
                    help="framework that must run native, never vendor/translate "
                         "(repeatable; adds to the built-in set)")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args(argv)
    app = Path(a.app).resolve()
    if not (app / "Contents").is_dir():
        print(f"_vendor_deps: not an app bundle: {app}", file=sys.stderr)
        return 2
    sources = [Path(s) for s in a.source] + DEFAULT_SOURCES
    native = DEFAULT_NATIVE | set(a.native)
    n, miss = vendor(app, sources, a.dry_run, native)
    return 0


if __name__ == "__main__":
    sys.exit(main())
