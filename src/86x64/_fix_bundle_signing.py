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
            else:
                os.rename(p, dest)
                fixed.append(("move-in", p))
            os.symlink(os.path.join("Versions", "Current", entry), p)

for f in fixed:
    print("FIX", *f)

# sign bottom-up: deepest paths first; frameworks sign per-version
items = []
for fw in frameworks:
    vers = os.path.join(fw, "Versions")
    if os.path.isdir(vers):
        for v in os.listdir(vers):
            if v == "Current":
                continue
            items.append(os.path.join(vers, v))
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
