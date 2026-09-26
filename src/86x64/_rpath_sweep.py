#!/usr/bin/env python3
"""Sweep all Mach-O binaries in an app bundle, rewriting absolute LC_LOAD_DYLIB
references to a framework/dylib that is ACTUALLY bundled in the app over to
@rpath/... so dyld finds the in-bundle copy.

Generic: there is no hard-coded list of "known" frameworks. A dependency is
rewritten iff the matching <Name>.framework directory (or loose <name>.dylib)
is physically present in Contents/Frameworks. That single test is also the
safety guard the old allow-list provided — e.g. a stale reference to a
framework that only survives as a <Name>ShimAuto.dylib (no real .framework
dir) is left untouched, and a ShimAuto's own re-export of the real system
framework is never swept.

Versioned (Foo.framework/Versions/A/Foo) AND flat (Foo.framework/Foo)
frameworks are handled, as are loose dylibs dropped straight into Frameworks/.
Missing dependencies that are NOT bundled are reported (use `m64 vendor` to
pull them in) rather than silently skipped."""
import os, re, subprocess, sys, shutil, tempfile
from pathlib import Path

# Optional app-path argv (defaults to iPhoto.app so existing usage is unchanged).
APP = Path(sys.argv[1]) if len(sys.argv) > 1 else \
      Path.home() / "Downloads/iLife11/Applications/iPhoto.app"
BFW = APP / "Contents/Frameworks"

# Prefer LLVM install-name-tool (the cctools one rejects translated dylibs
# whose __LINKEDIT has slack after signature stripping).
def _find_int():
    for c in ("/usr/local/opt/llvm/bin/llvm-install-name-tool",
              "/opt/homebrew/opt/llvm/bin/llvm-install-name-tool"):
        if os.access(c, os.X_OK):
            return c
    return shutil.which("llvm-install-name-tool") or "install_name_tool"
INT = _find_int()

# Our own load-command path rewriter, used as the LAST-RESORT fallback when both
# install_name_tool variants refuse a binary. cctools install_name_tool rejects
# translated CLASSIC dylibs ("local relocation entries out of place" — their
# __LINKEDIT lays the symbol table before the relocation/indirect tables, the
# reverse of cctools' expected canonical order) and llvm-install-name-tool punts
# on shared libraries entirely ("shared library not yet supported"). macho-tool
# change-deps re-emits the binary through its own model (rebuilding __LINKEDIT
# from scratch, so it never trips cctools' reloc-placement checks) while
# preserving the original header-region size so no code/data vmaddr moves.
MACHO_TOOL = Path(__file__).resolve().parent.parent.parent / "build/src/macho-tool/macho-tool"

def _change_deps(path, changes):
    """Rewrite dependency paths with macho-tool change-deps (classic-dylib-safe).
    `changes` is a list of ('-change', old, new) tuples (the same form passed to
    install_name_tool). Patches `path` in place. Returns the CompletedProcess so
    the caller can inspect returncode/stderr like the install_name_tool runs."""
    if not MACHO_TOOL.exists():
        return subprocess.CompletedProcess([], 1, "", f"macho-tool not built at {MACHO_TOOL}")
    args = [str(MACHO_TOOL), "change-deps", "-q"]
    for _flag, old, new in changes:
        args += ["--change", f"{old}={new}"]
    tmp = str(path) + ".cd"
    args += [str(path), tmp]
    r = subprocess.run(args, capture_output=True, text=True, errors="replace")
    if r.returncode == 0 and os.path.exists(tmp):
        shutil.copymode(str(path), tmp)
        os.replace(tmp, str(path))
    elif os.path.exists(tmp):
        os.remove(tmp)
    return r

def _thin_x86_64(path):
    """Reduce a fat binary carrying dead ppc/i386 slices to its x86_64 slice
    in place. Old iWork frameworks (e.g. MobileMe) ship ppc/i386/x86_64 fat
    binaries; cctools install_name_tool aborts byte-swapping the PPC slice
    ('malformed load command') and llvm rejects classic shared libraries. We
    only ever run x86_64, so the other slices are dead weight. Returns True if
    the file was changed."""
    # No returncode check: lipo exits 1 ("not a mach-o") on fat files whose
    # ppc7400 slice it cannot parse, yet still lists the archs and thins fine.
    r = subprocess.run(["lipo", "-archs", str(path)], capture_output=True, text=True)
    archs = r.stdout.split()
    if "x86_64" not in archs or archs == ["x86_64"]:
        return False
    tmp = str(path) + ".x86_64"
    if subprocess.run(["lipo", str(path), "-thin", "x86_64", "-output", tmp],
                      capture_output=True).returncode != 0:
        return False
    shutil.copymode(str(path), tmp)
    os.replace(tmp, str(path))
    return True

def _has_x86_64(path):
    """True iff the Mach-O at `path` carries an x86_64 slice (thin or fat).
    A bundled framework that is still a thin i386 binary (never translated /
    shimmed) cannot be loaded into the x86_64 translated process — rewriting a
    dependency to @rpath then points at an unloadable image, which dyld reports
    as a missing library. We surface that distinctly so the deploy knows to
    translate or shim it."""
    try:
        r = subprocess.run(["lipo", "-archs", str(path)],
                           capture_output=True, text=True)
        if "x86_64" in r.stdout.split():   # listed even when lipo exits 1
            return True
    except Exception:
        pass
    # lipo can choke on translated dylibs with __LINKEDIT slack; fall back to
    # the Mach-O cputype in the header (x86_64 = 0x01000007, little-endian).
    try:
        with open(path, "rb") as f:
            magic = f.read(4); cpu = f.read(4)
        if magic in (b'\xcf\xfa\xed\xfe',):           # 64-bit LE Mach-O
            return cpu == b'\x07\x00\x00\x01'
    except Exception:
        pass
    return False

def flat_sign(path):
    """Ad-hoc sign FLAT (not as a bundle): sign a copy outside any
    .framework/.app so codesign doesn't walk the bundle manifest, then move
    it back. Matches translate-bundle's signing."""
    with tempfile.TemporaryDirectory() as d:
        t = os.path.join(d, os.path.basename(path))
        shutil.copy2(path, t)
        subprocess.run(["codesign", "--remove-signature", t], capture_output=True)
        subprocess.run(["codesign", "--force", "--sign", "-",
                        "--identifier", os.path.basename(path), t], capture_output=True)
        shutil.copy2(t, path)

# A dep line from `otool -L` is:   \t<install path> (compatibility version ...)
# The path itself can contain spaces and quotes (e.g. iWork '09), so split on
# the reliable " (compatibility version" marker rather than on whitespace.
LINE_RE = re.compile(r'^\s+(?P<path>.*?)\s+\(compatibility version')
# A framework leaf:  .../<Fw>.framework[/Versions/<v>]/<Fw>
FW_RE = re.compile(r'/(?P<fw>[^/]+)\.framework/(?:Versions/[^/]+/)?(?P=fw)$')

def bundled_target(path):
    """If `path` names a framework/dylib that is physically present in the
    bundle's Frameworks dir, return the @rpath-relative replacement, else None."""
    m = FW_RE.search(path)
    if m:
        fw = m.group('fw')
        if not (BFW / f"{fw}.framework").is_dir():
            return None
        # keep everything from "<Fw>.framework/" onward (versioned or flat)
        rel = path[path.index(f"/{fw}.framework/") + 1:]
        return f"@rpath/{rel}"
    base = os.path.basename(path)
    if base.endswith(".dylib") and (BFW / base).is_file():
        return f"@rpath/{base}"
    return None

# An LC_RPATH path line from `otool -l` is:   <indent>path <P> (offset <N>)
RPATH_RE = re.compile(r'^\s+path (?P<path>.*) \(offset \d+\)\s*$')

def leaked_rpaths(bin_path):
    """LC_RPATH entries that are ABSOLUTE build/staging-machine paths pointing
    OUTSIDE the bundle. The wrapper-link step (86x64.sh / translate-bundle) bakes
    its `-o <out>` output dir and the libwrapper/libinterpose build dir into the
    wrapper's LC_RPATHs (e.g. `/tmp`, `/var/folders/.../translate-bundle.XXXX`,
    `.../build/src/86x64`). Those are dev-machine leakage: at best dead weight,
    at worst they SHADOW the in-bundle `@loader_path` rpath, so `@rpath/<lib>`
    resolves to a stale staging copy of <lib> (whose own deps may still be dead
    /System paths) instead of the bundled one — dyld then fails the load before
    anything of ours runs. Generic + safe: we only drop ABSOLUTE rpaths that do
    not resolve inside the bundle; `@loader_path`/`@executable_path`/`@rpath`
    relative entries and any absolute rpath that does point into the bundle are
    kept untouched."""
    out = subprocess.run(["otool", "-l", str(bin_path)],
                         capture_output=True, text=True, errors="replace").stdout
    app_root = str(APP.resolve())
    bad, in_rpath = [], False
    for line in out.splitlines():
        if "cmd LC_RPATH" in line:
            in_rpath = True
            continue
        if not in_rpath:
            continue
        m = RPATH_RE.match(line)
        if not m:
            continue
        in_rpath = False
        rp = m.group('path')
        if not rp.startswith('/'):          # @loader_path / @rpath / relative
            continue
        try:
            if os.path.realpath(rp).startswith(app_root):
                continue                    # absolute, but inside the bundle
        except Exception:
            pass
        bad.append(rp)
    return bad

def all_binaries():
    # Main executables AND the co-located translated dylibs in MacOS/ (the
    # wrapper-exec split means the bundled-framework LC_LOAD_DYLIBs live in
    # MacOS/<name>.dylib, not the wrapper).
    for p in (APP / "Contents/MacOS").glob("*"):
        if p.is_file():
            yield p
    for p in BFW.rglob("*"):
        if not p.is_file(): continue
        if p.name.endswith(".bak"): continue
        if "_CodeSignature" in p.parts: continue
        if p.suffix in (".plist", ".strings", ".nib", ".rsrc"): continue
        yield p

def is_macho(p):
    try:
        with open(p, "rb") as f:
            m = f.read(4)
        # 64/32-bit Mach-O LE/BE, plus FAT (cafebabe / bebafeca)
        return m in (b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf',
                     b'\xfa\xed\xfe\xcf', b'\xcf\xed\xfa\xfe',
                     b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca')
    except: return False

patched_count = 0
missing = {}      # framework/dylib name -> set of consumers (not bundled)
wrong_arch = {}   # bundled name -> set of x86_64 consumers (target is i386-only)
for bin_path in all_binaries():
    if not is_macho(bin_path): continue
    # Only the x86_64 slice ever runs. Thin native fat binaries (iLife/iWork
    # ship ppc/i386/x86_64) up front: otool -L on a fat file lists every
    # slice under "(architecture X):" headers, and the dead slices break
    # install_name_tool — which left MobileMe/iLifeMediaBrowser un-swept.
    _thin_x86_64(bin_path)
    out = subprocess.run(["otool", "-L", str(bin_path)], capture_output=True, text=True, errors="replace").stdout
    consumer_x64 = _has_x86_64(bin_path)
    changes = []
    bad_rpaths = leaked_rpaths(bin_path)
    for line in out.splitlines()[1:]:  # skip first line (file path)
        lm = LINE_RE.match(line)
        if not lm: continue
        old = lm.group('path')
        if old.startswith('@'):          # already bundle-relative
            # Already @rpath/... — still validate the bundled target's arch,
            # since a prior sweep may have pointed an x86_64 consumer at a
            # framework that was never translated past i386.
            if old.startswith('@rpath/'):
                tgt = BFW / old[len('@rpath/'):]
                if consumer_x64 and tgt.is_file() and not _has_x86_64(tgt):
                    fm = FW_RE.search(old)
                    nm = (fm.group('fw') + ".framework") if fm else os.path.basename(old)
                    wrong_arch.setdefault(nm, set()).add(bin_path.name)
            continue
        # The single test that decides everything: is this framework/dylib
        # PHYSICALLY bundled? If so, redirect to the in-bundle copy regardless
        # of whether the stale install path is /System/, /Library/ or elsewhere
        # (legacy frameworks like QuickTime/QTKit are recorded with a /System/
        # path but ship a bundled shim — they must be swept too).
        new = bundled_target(old)
        if new is not None:
            changes.append(('-change', old, new))
            # The path now resolves, but a bundled framework that is still a
            # thin i386 binary (never translated/shimmed) cannot load into the
            # x86_64 process — dyld reports it as "missing". Flag it loudly.
            tgt = BFW / new[len('@rpath/'):]
            if consumer_x64 and tgt.is_file() and not _has_x86_64(tgt):
                fm = FW_RE.search(new)
                nm = (fm.group('fw') + ".framework") if fm else os.path.basename(new)
                wrong_arch.setdefault(nm, set()).add(bin_path.name)
            continue
        # not bundled: a genuine native host framework — leave it alone.
        if old.startswith('/System/') or old.startswith('/usr/lib'):
            continue
        # an absolute, non-system dep that isn't in the bundle: surface it
        fm = FW_RE.search(old)
        name = (fm.group('fw') + ".framework") if fm else os.path.basename(old)
        missing.setdefault(name, set()).add(bin_path.name)
    if changes or bad_rpaths:
        subprocess.run(["codesign", "--remove-signature", str(bin_path)],
                       capture_output=True)
        flat = [a for c in changes for a in c]
        for rp in bad_rpaths:
            flat += ["-delete_rpath", rp]
        flat += [str(bin_path)]
        # PRIMARY PATH — stock cctools install_name_tool. Since route C
        # (`convert --synthesize-dyld-info`, archive.cc) gives every translated
        # CLASSIC dylib a canonical LC_DYLD_INFO_ONLY + emptied classic reloc
        # tables, cctools no longer chokes on it ("local relocation entries out
        # of place" is gone — proven across the Portal 2 C++ dylib spread and the
        # 57MB Civ IV dylib; see tests-i386/canonical_matrix.sh). So for a
        # route-C bundle this first call succeeds and NOTHING below it runs.
        #
        # The remaining fallbacks are pure safety net for the residual non-route-C
        # shapes still found in mixed bundles: cctools handles .so/MH_BUNDLE but
        # llvm rejects MH_BUNDLE; llvm tolerates odd __LINKEDIT slack that cctools
        # may not; dead ppc/i386 slices can trip both until thinned. Try in turn —
        # whichever succeeds wins.
        r = subprocess.run(["install_name_tool", *flat], capture_output=True, text=True, errors="replace")
        if r.returncode != 0:
            r = subprocess.run([INT, *flat], capture_output=True, text=True, errors="replace")
        if r.returncode != 0 and _thin_x86_64(bin_path):
            # dead ppc/i386 slices were breaking the tools; retry on the thin x86_64
            r = subprocess.run(["install_name_tool", *flat], capture_output=True, text=True, errors="replace")
            if r.returncode != 0:
                r = subprocess.run([INT, *flat], capture_output=True, text=True, errors="replace")
        used_change_deps = False
        if r.returncode != 0 and changes:
            # LAST RESORT — our own re-emitter (macho-tool change-deps, 4d02bd6).
            # NOTE: change-deps only rewrites LC_LOAD_DYLIB deps, not LC_RPATH —
            # but with route C active install_name_tool never refuses a translated
            # classic dylib, and the wrapper/native binaries that carry leaked
            # rpaths are modern Mach-O that install_name_tool always accepts, so
            # this fallback never has to strip rpaths in practice.
            # With route C active this should NEVER fire for a translated classic
            # dylib; if it does, the input is NON-canonical (a classic dylib that
            # missed --synthesize-dyld-info, or a shape route C doesn't cover yet)
            # — a canonical gap worth root-causing in synthesize_dyld_info rather
            # than papering over here. The warning below flags it loudly.
            r = _change_deps(bin_path, changes)
            used_change_deps = (r.returncode == 0)
            if used_change_deps:
                print(f"  NOTE {bin_path.name}: stock install_name_tool refused; used "
                      f"macho-tool change-deps fallback — this dylib is not fully "
                      f"canonical (route C should have prevented this).")
        if r.returncode != 0:
            print(f"  FAIL {bin_path.name}: {r.stderr.splitlines()[0] if r.stderr else 'no stderr'}")
            continue
        flat_sign(str(bin_path))
        how = " (via macho-tool change-deps)" if used_change_deps else ""
        rps = f" + {len(bad_rpaths)} leaked rpath(s) dropped" if bad_rpaths else ""
        print(f"  patched {bin_path.name}: {len(changes)} changes{rps}{how}")
        patched_count += 1

print(f"TOTAL: {patched_count} binaries patched")
if missing:
    print(f"\n  {len(missing)} dependency(ies) referenced by absolute path are NOT bundled")
    print("  (run `m64 vendor <app>` to pull them in, then re-translate):")
    for name in sorted(missing):
        print(f"      • {name}  <- {', '.join(sorted(missing[name]))}")
if wrong_arch:
    print(f"\n  WARNING: {len(wrong_arch)} bundled framework(s) are i386-only — they")
    print("  resolve via @rpath but CANNOT load into the x86_64 process (dyld will")
    print("  report them as missing). Translate or shim them (m64 translate / shimgen):")
    for name in sorted(wrong_arch):
        print(f"      • {name}  <- {', '.join(sorted(wrong_arch[name]))}")
