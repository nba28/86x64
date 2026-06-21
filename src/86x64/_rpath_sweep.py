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

def _thin_x86_64(path):
    """Reduce a fat binary carrying dead ppc/i386 slices to its x86_64 slice
    in place. Old iWork frameworks (e.g. MobileMe) ship ppc/i386/x86_64 fat
    binaries; cctools install_name_tool aborts byte-swapping the PPC slice
    ('malformed load command') and llvm rejects classic shared libraries. We
    only ever run x86_64, so the other slices are dead weight. Returns True if
    the file was changed."""
    r = subprocess.run(["lipo", "-archs", str(path)], capture_output=True, text=True)
    if r.returncode != 0:
        return False
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
for bin_path in all_binaries():
    if not is_macho(bin_path): continue
    out = subprocess.run(["otool", "-L", str(bin_path)], capture_output=True, text=True, errors="replace").stdout
    changes = []
    for line in out.splitlines()[1:]:  # skip first line (file path)
        lm = LINE_RE.match(line)
        if not lm: continue
        old = lm.group('path')
        if old.startswith('@'):          # already bundle-relative
            continue
        # The single test that decides everything: is this framework/dylib
        # PHYSICALLY bundled? If so, redirect to the in-bundle copy regardless
        # of whether the stale install path is /System/, /Library/ or elsewhere
        # (legacy frameworks like QuickTime/QTKit are recorded with a /System/
        # path but ship a bundled shim — they must be swept too).
        new = bundled_target(old)
        if new is not None:
            changes.append(('-change', old, new))
            continue
        # not bundled: a genuine native host framework — leave it alone.
        if old.startswith('/System/') or old.startswith('/usr/lib'):
            continue
        # an absolute, non-system dep that isn't in the bundle: surface it
        fm = FW_RE.search(old)
        name = (fm.group('fw') + ".framework") if fm else os.path.basename(old)
        missing.setdefault(name, set()).add(bin_path.name)
    if changes:
        subprocess.run(["codesign", "--remove-signature", str(bin_path)],
                       capture_output=True)
        flat = [a for c in changes for a in c] + [str(bin_path)]
        # cctools install_name_tool handles .so/MH_BUNDLE but chokes on the
        # __LINKEDIT slack of translated dylibs; llvm handles the slack but
        # rejects MH_BUNDLE. Try both — whichever succeeds wins.
        r = subprocess.run(["install_name_tool", *flat], capture_output=True, text=True, errors="replace")
        if r.returncode != 0:
            r = subprocess.run([INT, *flat], capture_output=True, text=True, errors="replace")
        if r.returncode != 0 and _thin_x86_64(bin_path):
            # dead ppc/i386 slices were breaking the tools; retry on the thin x86_64
            r = subprocess.run(["install_name_tool", *flat], capture_output=True, text=True, errors="replace")
            if r.returncode != 0:
                r = subprocess.run([INT, *flat], capture_output=True, text=True, errors="replace")
        if r.returncode != 0:
            print(f"  FAIL {bin_path.name}: {r.stderr.splitlines()[0] if r.stderr else 'no stderr'}")
            continue
        flat_sign(str(bin_path))
        print(f"  patched {bin_path.name}: {len(changes)} changes")
        patched_count += 1

print(f"TOTAL: {patched_count} binaries patched")
if missing:
    print(f"\n  {len(missing)} dependency(ies) referenced by absolute path are NOT bundled")
    print("  (run `m64 vendor <app>` to pull them in, then re-translate):")
    for name in sorted(missing):
        print(f"      • {name}  <- {', '.join(sorted(missing[name]))}")
