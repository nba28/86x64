#!/usr/bin/env python3
"""Sweep all Mach-O binaries in iPhoto.app, rewriting LC_LOAD_DYLIB
references to bundled frameworks from /System/... or /Library/... paths
to @rpath/... so dyld finds them in iPhoto.app/Contents/Frameworks/."""
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
BUNDLED = {
    "QuickTime", "QTKit", "Python", "ProKit", "IMCore", "InstantMessage",
    "Message", "vecLib", "CoreMediaAuthoring", "CrashReporterSupport",
    "DiscRecording", "DiscRecordingUI", "iLifeFaceRecognition", "iLifeSQLAccess",
    "iLifeKit", "iLifePageLayout", "iLifeSlideshow", "iLifeMediaBrowser",
    "Tessera", "Tellus", "AccountConfigurationPlugin", "UpgradeChecker",
    "Geode", "MediaSync", "MobileMe", "ProUtils", "ProXTCore", "RedRock",
    "NavigationServices", "CarbonSound", "NyxAudioAnalysis",
    # nested sub-frameworks
    "IMFoundation", "IMUtils", "IMDaemonCore", "IMSecurityUtils", "XMPPCore",
    "iLifeSlideshowCore", "iLifeSlideshowExporter", "iLifeSlideshowProducer",
    "iLifeSlideshowRenderer",
    # iWeb / iWork-shared SF* framework family + FTPKit (the .is_dir() guard
    # below keeps these inert for bundles that don't ship them, e.g. iPhoto).
    "SFUtility", "SFArchiving", "SFLicense", "SFApplication", "SFRendering",
    "SFDrawables", "SFStyles", "SFControls", "SFInspectors", "SFAnimation",
    "SFProofReader", "SFWordProcessing", "FTPKit",
}

# pattern: /path/to/<fwname>.framework/Versions/<vers>/<fwname>
DEP_RE = re.compile(r'^\s*(?P<full>(?P<prefix>/[^@\s].*?)/(?P<fw>\w+)\.framework/Versions/(?P<vers>[^/]+)/\3)\s')

def all_binaries():
    # Main executables AND the co-located translated dylibs in MacOS/ (the
    # wrapper-exec split means the bundled-framework LC_LOAD_DYLIBs live in
    # MacOS/<name>.dylib, not the wrapper). Earlier this only yielded the
    # wrapper, so iPhoto.dylib's deps were never rewritten.
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
for bin_path in all_binaries():
    if not is_macho(bin_path): continue
    out = subprocess.run(["otool", "-L", str(bin_path)], capture_output=True, text=True, errors="replace").stdout
    changes = []
    for line in out.splitlines()[1:]:  # skip first line (file path)
        m = DEP_RE.match(line)
        if not m: continue
        fw = m.group('fw')
        if fw not in BUNDLED: continue
        # the framework must ACTUALLY be bundled: a stale BUNDLED entry whose
        # framework only exists as a <FW>ShimAuto.dylib (DiscRecording,
        # InstantMessage, Message...) would break the dep — and sweeping a
        # ShimAuto's own re-export of the real system framework bricks dyld
        if not (BFW / f"{fw}.framework").is_dir(): continue
        old = m.group('full')
        # don't double-patch
        if old.startswith('@'): continue
        new = f"@rpath/{fw}.framework/Versions/{m.group('vers')}/{fw}"
        changes.append(('-change', old, new))
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
        if r.returncode != 0:
            print(f"  FAIL {bin_path.name}: {r.stderr.splitlines()[0] if r.stderr else 'no stderr'}")
            continue
        flat_sign(str(bin_path))
        print(f"  patched {bin_path.name}: {len(changes)} changes")
        patched_count += 1

print(f"TOTAL: {patched_count} binaries patched")
