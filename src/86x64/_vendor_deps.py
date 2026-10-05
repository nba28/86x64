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
import ctypes
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
    return m in (b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf',   # MH_MAGIC_64 LE/BE
                 b'\xce\xfa\xed\xfe', b'\xfe\xed\xfa\xce',   # MH_MAGIC 32-bit LE/BE
                                                             # (thin i386 dylibs —
                                                             # Source engine siblings)
                 b'\xfa\xed\xfe\xcf', b'\xcf\xed\xfa\xfe',
                 b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca')   # FAT_MAGIC/CIGAM


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


# dyld's own "is this path served by the shared cache?" oracle. On modern macOS
# almost every /usr/lib/*.dylib and /System framework has NO file on disk — it
# lives only in the dyld shared cache — so a bare os.path.exists() is False even
# for perfectly loadable system libraries (libSystem.B.dylib, libobjc.A.dylib,
# Foundation, …). _dyld_shared_cache_contains_path() answers the real question:
# True  = dyld can load it (leave native, never vendor);
# False = neither on disk nor in the cache = REMOVED (must vendor a translated
#         copy). This cleanly separates "gone" (Python 2.6, QuickTime) from
#         "cache-only" (libSystem) without any per-name allow/deny list.
def _make_cache_probe():
    try:
        fn = ctypes.CDLL(None)._dyld_shared_cache_contains_path
        fn.restype = ctypes.c_bool
        fn.argtypes = [ctypes.c_char_p]
        return lambda p: bool(fn(p.encode()))
    except (AttributeError, OSError):
        return None
_CACHE_CONTAINS = _make_cache_probe()


def system_dep_loadable(dep):
    """True iff the OS-owned dependency at absolute path `dep` can actually be
    loaded on this host — either it exists as a file on disk, or dyld serves it
    from the shared cache. False means the framework/dylib was REMOVED from the
    OS (e.g. Python 2.6, or a hollow-shell framework) and must be vendored, else
    the translated app dyld-fails at load. If the probe API is unavailable we
    conservatively fall back to os.path.exists (never over-vendors, but may miss
    a removed framework on that host)."""
    if os.path.exists(dep):
        return True
    if _CACHE_CONTAINS is not None:
        return _CACHE_CONTAINS(dep)
    return False


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


def iter_sources(kind, name, sources):
    """Every original framework dir / dylib file under the source roots, in
    search order (per root: the shallow hit first, then a walk)."""
    want = f"{name}.framework" if kind == "framework" else name
    ok = (lambda h: h.is_dir()) if kind == "framework" else (lambda h: h.is_file())
    for root in sources:
        if not root.is_dir():
            continue
        direct = root / want
        if ok(direct):
            yield direct
        for hit in root.rglob(want):
            if hit != direct and ok(hit):
                yield hit


def search_sources(kind, name, sources):
    """The first original framework dir / dylib file under the source roots."""
    return next(iter_sources(kind, name, sources), None)


def _archs_of(path):
    """Architectures in a Mach-O, or () if it is not one."""
    try:
        out = subprocess.run(["lipo", "-info", str(path)],
                             capture_output=True, text=True).stdout
    except Exception:
        return ()
    if "architecture:" in out:                       # "Non-fat file: X is architecture: i386"
        return (out.rsplit("architecture:", 1)[1].strip(),)
    if "are:" in out:
        return tuple(out.rsplit("are:", 1)[1].split())
    return ()


def links_our_runtime(path):
    """True iff this Mach-O loads libabiconv, i.e. it is one of OUR translated
    artifacts rather than a stock native binary."""
    try:
        out = subprocess.run(["otool", "-L", str(path)],
                             capture_output=True, text=True).stdout
    except Exception:
        return False
    return "libabiconv" in out


def bundle_is_translated(app):
    """True iff the bundle already contains translated (libabiconv-linked)
    binaries — i.e. the consumer lives in our i386-derived ABI world. Checked
    against the bundle's OWN code, not against any vendored dependency."""
    for sub in ("Contents/MacOS", "Contents/Frameworks", "Contents/PlugIns"):
        d = Path(app) / sub
        if not d.is_dir():
            continue
        for p in d.rglob("*"):
            if not p.is_file() or p.is_symlink():
                continue
            if p.name.startswith("libabiconv"):
                continue                              # the runtime itself proves nothing
            try:
                if p.stat().st_size < 4096:
                    continue
            except OSError:
                continue
            if links_our_runtime(p):
                return True
    return False


def _donor_binary(kind, src):
    """The Mach-O to judge a donor by: the framework's current-version binary,
    or the dylib itself."""
    if kind != "framework":
        return src
    name = src.name[:-len(".framework")]
    for cand in (src / name,
                 src / "Versions" / "Current" / name):
        if cand.is_file():
            return cand
    vers = src / "Versions"
    if vers.is_dir():
        for v in sorted(vers.iterdir(), reverse=True):
            cand = v / name
            if cand.is_file():
                return cand
    return None


def donor_arch_mismatch(kind, src, app_is_translated):
    """★A DONOR MUST MATCH THE CONSUMER'S ABI WORLD.

    MEASURED 2026-08-05 (Civ IV): Civ links /System/Library/Frameworks/
    Python.framework/Versions/2.6/Python by ABSOLUTE path. Apple removed Python 2
    in macOS 12.3, so the dependency became unresolvable and this function's
    caller happily satisfied it from ~/projects/Library/Frameworks/iLife11/
    Python.framework — which is **x86_64-ONLY**. That donated a NATIVE
    interpreter into a TRANSLATED i386-world app: the translated extension
    modules (wx/_core_.so links libabiconv, 122 undefined _Py* symbols) bound
    into a CPython whose PyObject layout differs from the one their INLINED
    Py_INCREF/Py_TYPE macros assume. Civ died at "Init Python" and the cause was
    invisible for a long time because the vendor step said nothing at all.

    The rule, stated structurally: when the consuming bundle contains
    translated i386-derived binaries, a donor is acceptable only if it either
    (a) still carries an i386 slice, so it CAN be translated, or
    (b) is already one of our translated artifacts (it links libabiconv).
    A native-x86_64-only donor is correct only for a native consumer.

    Returns a reason string when the donor is unusable, else None."""
    if not app_is_translated:
        return None                                   # native consumer: anything goes
    binp = _donor_binary(kind, src)
    if binp is None:
        return None                                   # nothing to judge; leave as-is
    archs = _archs_of(binp)
    if not archs:
        return None
    if "i386" in archs:
        return None                                   # translatable
    if links_our_runtime(binp):
        return None                                   # already ours
    return (f"donor is {'/'.join(archs)} with no i386 slice and does not link "
            f"libabiconv, but this bundle contains TRANSLATED i386 binaries")


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
    if not dry:
        bfw.mkdir(parents=True, exist_ok=True)

    # Which ABI world is the CONSUMER in? Decides whether a native-only donor is
    # acceptable. Probed once from the bundle's own binaries — see
    # donor_arch_mismatch for why a silent mismatch is so expensive.
    app_translated = bundle_is_translated(app)
    mismatches = []

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

    def try_vendor(kind, name, consumer, removed_system=False):
        """Vendor (kind, name) from the first source root that has it, copying the
        framework/dylib into the bundle (preserving Versions/Current symlinks),
        queueing the copied-in binary for its own dependency recursion. Returns
        True if vendored (or already handled), False if no source has it. Shared
        by the app-private-absolute-path path and the unresolved-@rpath-sibling
        path so both vendor identically (native — the curated x86_64 copy — and
        left for m64 translate to classify native + rpath-fix to repoint)."""
        if (kind, name) in handled:
            return True                    # already decided this round
        cands = list(iter_sources(kind, name, sources))
        if not cands:
            return False
        # Prefer a donor in the consumer's ABI world (i386-bearing or already
        # translated) wherever one exists — see donor_arch_mismatch.
        src = next((c for c in cands
                    if not donor_arch_mismatch(kind, c, app_translated)), cands[0])
        handled.add((kind, name))
        # ★ABI-WORLD CHECK. Donating a native-x86_64-only framework into a
        # bundle of TRANSLATED i386 binaries is never correct and used to fail
        # SILENTLY — see donor_arch_mismatch for the Civ IV / Python 2.6 case it
        # cost us. Refuse loudly and name the donor, so the fix (supply a donor
        # with an i386 slice, or pre-translate it) is obvious. Override with
        # M64_ALLOW_ARCH_MISMATCH=1.
        bad = donor_arch_mismatch(kind, src, app_translated)
        if bad and removed_system:
            # A framework Apple REMOVED from /System is supplied the way every
            # other system framework is: native, reached through the bridge
            # (e.g. iLife's /System/Library/Frameworks/IMCore). Only when no
            # same-ABI donor exists — Python's translated copy still wins above.
            warn(f"{src.name}: removed system framework, no i386 donor — "
                 f"vendoring the native copy")
            bad = None
        if bad and not os.environ.get("M64_ALLOW_ARCH_MISMATCH"):
            warn(f"REFUSING to vendor {src.name}: {bad}")
            warn(f"    donor: {src}")
            warn(f"    supply a donor carrying an i386 slice (it will be "
                 f"translated), or pre-translate it; set "
                 f"M64_ALLOW_ARCH_MISMATCH=1 to override")
            mismatches.append(src.name)
            return False
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
        present.add(src.name)              # now resolvable for later @rpath deps
        # Recurse into the copied-in binary so its own app-private / @rpath-sibling
        # deps get vendored too. Its install id is left as shipped — consumers are
        # rewritten to @rpath by `m64 rpath-fix`, so dyld resolves the in-bundle
        # copy regardless of the loaded dylib's LC_ID.
        vb = framework_binary(dst) if kind == "framework" else dst
        if not dry and vb.exists() and is_macho(vb):
            queue.append(vb)
        return True

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
                # bundle-relative: fine if it resolves anywhere in the bundle.
                if os.path.basename(dep) in present:
                    continue
                # Unresolved @rpath/@loader_path/@executable_path dep. This is a
                # SIBLING a vendored framework was built to load from the bundle
                # but that was itself never vendored — e.g. our curated QuickTime
                # references @rpath/NavigationServices + @rpath/CarbonSound, both
                # x86_64 curated frameworks that live in the source pool. Try to
                # vendor it from a source root (NATIVE — an x86_64-carrying curated
                # copy; m64 translate leaves x86_64 binaries native and rpath-fix
                # points the consumer at the in-bundle copy). Only flag dangling
                # if no source has it. Without this a vendored framework's own
                # @rpath sibling deps are dropped -> cascading dyld "Library not
                # loaded" at launch (Numbers: QuickTime -> NavigationServices ->
                # CarbonSound). Generic: triggers on the structural "unbundled
                # @-relative dep that a source pool can satisfy", not an app name.
                if not try_vendor(kind, name, b.name):
                    dangling.setdefault((kind, name), set()).add(b.name)
                continue
            if dep.startswith('/System/') or dep.startswith('/usr/'):
                # OS-owned location → normally native and left alone. Two EXCEPTIONS
                # fall through to vendoring instead of skipping, else the app
                # dyld-fails at load ("Library not loaded: <dep>"):
                #   1. a hollow-shell framework (the .framework dir + Info.plist
                #      still exist but the x86_64 binary was stripped from the dyld
                #      cache, e.g. QuickTime) — flagged by the curated set; and
                #   2. a DEAD absolute system path — the framework/dylib is GONE
                #      from disk entirely (Apple removed it), e.g. i386-era apps
                #      linking /System/Library/Frameworks/Python.framework/
                #      Versions/2.6/Python (Civ IV's embedded Python, iPhoto's, …).
                #      Modern macOS ships no Python 2.x; the whole framework dir is
                #      absent. This is universal — any i386 app linking a removed
                #      system framework hits it — and needs no per-name list: the
                #      signal is "dyld cannot load the exact install path (neither
                #      on disk nor in the shared cache) AND there is no modern
                #      native home for it". system_dep_loadable() handles the
                #      cache-only case so real system libs (libSystem, libobjc,
                #      Foundation — files absent but cache-served) still
                #      short-circuit and are never vendored.
                hollow = (kind == "framework"
                          and name in HOLLOW_SHELL_SYSTEM_FRAMEWORKS)
                # Dead = dyld cannot load this exact path. A same-named
                # framework elsewhere (IMCore moved to PrivateFrameworks) is
                # not a home: dyld never looks there for this install path.
                dead = not system_dep_loadable(dep)
                # A dead absolute /System path whose leaf is ALREADY somewhere in
                # the bundle (e.g. Python's own Extras hold
                # libsvn_swig_py-1.0.dylib, recorded with its dead /System install
                # path but shipped inside Python.framework/…/Extras/lib) is not
                # actually missing — rpath-fix will repoint the consumer at the
                # in-bundle copy. Don't re-vendor it. Mirrors the @-relative
                # present-in-bundle short-circuit above.
                if dead and os.path.basename(dep) in present:
                    continue
                if not (hollow or dead):
                    continue
                if (kind, name) not in handled and \
                        not try_vendor(kind, name, b.name, removed_system=True):
                    unresolved[(kind, name)] = {b.name}
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
            if not try_vendor(kind, name, b.name):
                unresolved[(kind, name)] = {b.name}

    info(f"vendored {len(vendored)} framework(s)/dylib(s) into {app.name}")
    for v in vendored:
        good(v)
    if mismatches:
        # Loud on purpose, and repeated at the END where it cannot scroll past:
        # the dependency is now MISSING, which is a better failure than a
        # wrong-ABI donor that loads and then corrupts everything downstream.
        warn(f"{len(mismatches)} donor(s) REFUSED on an ABI-world mismatch: "
             f"{', '.join(mismatches)}")
        warn(f"    this bundle contains TRANSLATED i386 binaries, so a "
             f"native-x86_64-only donor cannot satisfy it")
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
