#!/usr/bin/env python3
"""Normalize framework layouts in iPhoto.app so codesign accepts them, then
sign every nested bundle bottom-up. Modern codesign framework rules:
root may contain only the Versions dir plus symlinks to DIRECT children of
Versions/Current; version roots may contain only the binary, Resources,
_CodeSignature, Frameworks, Helpers, XPCServices, etc. (no stray Mach-Os)."""
import os, subprocess, sys, stat

# Optional app-path argv (defaults to iPhoto.app so existing usage is unchanged).
APP = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else \
      os.path.expanduser("~/Downloads/iLife11/Applications/iPhoto.app")
SIDE = os.path.expanduser("~/Downloads/iLife11/iPhoto_baks")

def run(*cmd):
    return subprocess.run(cmd, capture_output=True, text=True)

MIN_PLIST = """<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
\t<key>CFBundleDevelopmentRegion</key><string>en</string>
\t<key>CFBundleExecutable</key><string>{name}</string>
\t<key>CFBundleIdentifier</key><string>com.apple.{name}</string>
\t<key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
\t<key>CFBundleName</key><string>{name}</string>
\t<key>CFBundlePackageType</key><string>FMWK</string>
\t<key>CFBundleShortVersionString</key><string>1.0</string>
\t<key>CFBundleVersion</key><string>1.0</string>
</dict>
</plist>
"""

def ensure_min_plist(target, name):
    """codesign rejects a versioned framework whose version dir has no
    Resources/Info.plist ('bundle format unrecognized'). Synthesize a minimal
    one for the compat/shim frameworks (VideoToolbox, CoreMedia, CarbonSound,
    ...) that ship without it. Idempotent."""
    res = os.path.join(target, "Resources")
    ip = os.path.join(res, "Info.plist")
    if os.path.exists(ip):
        return False
    os.makedirs(res, exist_ok=True)
    with open(ip, "w") as f:
        f.write(MIN_PLIST.format(name=name))
    return True

def is_macho(p):
    with open(p, "rb") as f:
        return f.read(4) in (b'\xcf\xfa\xed\xfe', b'\xce\xfa\xed\xfe',
                             b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca')

def ensure_top_symlinks(fw, target):
    """A framework root must symlink each DIRECT child of Versions/Current.
    Compat shims sometimes ship the binary only under Versions/A with no
    top-level <name> / Resources symlink, which codesign also rejects."""
    made = []
    for entry in os.listdir(target):
        # _CodeSignature is signed per-version (lives in Versions/A), never
        # symlinked at the root; skip junk/backup files too.
        if entry == "_CodeSignature" or entry.startswith(".") \
           or entry.endswith(".bak"):
            continue
        top = os.path.join(fw, entry)
        if os.path.lexists(top):
            continue
        os.symlink(os.path.join("Versions", "Current", entry), top)
        made.append(("mk-topsym", top))
    return made

frameworks = []
apps = []
for dirpath, dirnames, filenames in os.walk(APP):
    for d in dirnames:
        p = os.path.join(dirpath, d)
        if d.endswith(".framework"):
            frameworks.append(p)
        elif d.endswith(".app") and p != APP:
            apps.append(p)

fixed = []
for fw in frameworks:
    vers = os.path.join(fw, "Versions")
    if not os.path.isdir(vers):
        continue  # flat bundle; codesign treats differently
    # pick the real version dir Current points to (fallback: first entry)
    cur = os.path.join(vers, "Current")
    target = os.path.realpath(cur) if os.path.lexists(cur) else None
    if not target or not os.path.isdir(target):
        cands = [v for v in os.listdir(vers) if v != "Current"]
        if not cands:
            continue
        target = os.path.join(vers, sorted(cands)[0])
        if not os.path.lexists(cur):
            os.symlink(os.path.basename(target), cur)
            fixed.append(("mk-Current", fw))
    fwname = os.path.basename(fw)[:-len(".framework")]
    if ensure_min_plist(target, fwname):
        fixed.append(("mk-Info.plist", fw))
    # codesign wants the main binary to be a regular file. Inventor ships
    # Versions/C/Inventor -> Libraries/libCoin.dylib: swap them so the real
    # file is the main binary and the library path is the symlink.
    main = os.path.join(target, fwname)
    if os.path.islink(main):
        real = os.path.realpath(main)
        if os.path.isfile(real) and real.startswith(os.path.realpath(target) + os.sep):
            os.remove(main)
            os.rename(real, main)
            os.symlink(os.path.relpath(main, os.path.dirname(real)), real)
            fixed.append(("swap-main", main))
    # Loose non-binary files in the version root are unsealable subcomponents
    # (ProKitVersion.plist). They belong under Resources/.
    res = os.path.join(target, "Resources")
    for entry in os.listdir(target):
        p = os.path.join(target, entry)
        if entry == fwname or os.path.islink(p) or not os.path.isfile(p) \
           or is_macho(p):
            continue
        os.makedirs(res, exist_ok=True)
        if not os.path.exists(os.path.join(res, entry)):
            os.rename(p, os.path.join(res, entry))
            fixed.append(("to-Resources", p))
    for entry in os.listdir(fw):
        if entry == "Versions":
            continue
        p = os.path.join(fw, entry)
        if os.path.islink(p):
            dest = os.readlink(p)
            # must be Versions/Current/<entry>, a direct child
            want = os.path.join("Versions", "Current", entry)
            tgt_abs = os.path.join(fw, dest) if not os.path.isabs(dest) else dest
            norm = os.path.normpath(dest)
            parts = norm.split(os.sep)
            direct = (len(parts) == 3 and parts[0] == "Versions"
                      and parts[1] == "Current")
            exists_after = os.path.exists(os.path.join(target, entry))
            if direct and (os.path.exists(tgt_abs) or entry == "CodeResources"
                           or exists_after):
                continue
            if not direct and exists_after:
                os.remove(p)
                os.symlink(want, p)
                fixed.append(("relink", p))
            elif not os.path.exists(tgt_abs) and entry != "CodeResources":
                os.remove(p)
                fixed.append(("rm-broken", p))
        else:
            # regular file or dir in framework root: move into version dir
            dest = os.path.join(target, entry)
            if os.path.exists(dest):
                # duplicate: archive the root copy
                sd = os.path.join(SIDE, os.path.relpath(p, APP))
                os.makedirs(os.path.dirname(sd), exist_ok=True)
                os.rename(p, sd)
                fixed.append(("archive-dup", p))
            elif os.path.isfile(p) and not is_macho(p):
                # a loose file: into Resources/ (see the version-root pass)
                os.makedirs(os.path.join(target, "Resources"), exist_ok=True)
                os.rename(p, os.path.join(target, "Resources", entry))
                fixed.append(("to-Resources", p))
                continue
            else:
                os.rename(p, dest)
                fixed.append(("move-in", p))
            os.symlink(os.path.join("Versions", "Current", entry), p)
    fixed.extend(ensure_top_symlinks(fw, target))

for f in fixed:
    print("FIX", *f)

# sign bottom-up: deepest paths first; frameworks sign per-version
items = []
for fw in frameworks:
    vers = os.path.join(fw, "Versions")
    if os.path.isdir(vers):
        for v in os.listdir(vers):
            vp = os.path.join(vers, v)
            # skip the Current symlink and stray non-dir junk (.DS_Store)
            if v == "Current" or not os.path.isdir(vp) or os.path.islink(vp):
                continue
            items.append(vp)
    else:
        items.append(fw)
items += apps
items.sort(key=lambda p: -p.count(os.sep))

bad = 0
for it in items:
    r = run("codesign", "-f", "-s", "-", it)
    if r.returncode != 0:
        print("SIGNFAIL", it)
        print("   ", r.stderr.strip().replace("\n", "\n    "))
        bad += 1
print(f"signed {len(items)-bad}/{len(items)} nested bundles")
sys.exit(1 if bad else 0)
