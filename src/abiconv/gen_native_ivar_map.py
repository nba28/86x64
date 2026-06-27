#!/usr/bin/env python3
"""gen_native_ivar_map.py — emit the i386 ivar-offset map for native ObjC
superclasses, used by objc_shim.c to SYNC a legacy instance's inherited
native-superclass ivars between the real x86_64 object and its i386-layout
shadow (BOTH directions: real->shadow on reverse-bridge entry so a legacy IMP's
direct reads see native values, shadow->real on exit so native code sees a
legacy IMP's direct writes).

WHY: iPhoto's legacy ObjC-1.0 classes (EtchedText:NSTextField, AlbumView:
NSOutlineView, ...) subclass native AppKit classes and read/write INHERITED
protected ivars directly at hardcoded i386 (fragile-ABI, ABSOLUTE) offsets —
e.g. NSControl._cell @84. The translator keeps a legacy instance's ivars in a
zeroed i386-layout shadow buffer (get_or_create_shadow); the native
superclass's ivars live only in the real modern object, so those inherited
reads return 0 -> nil cell -> garbage geometry (collapsed sidebar, 1x1 card
text). The fix maps each inherited ivar's i386 OFFSET + i386 WIDTH; the x64
offset/width/signedness are resolved at runtime BY NAME
(class_getInstanceVariable + ivar_getTypeEncoding) so the converter is robust
to the modern runtime's layout (e.g. NSInteger widened 4->8, CGFloat float->
double).

The i386 offsets cannot be computed from the modern runtime (modern NSTextField
has ~8x the ivars of 10.6's), so they are extracted from the actual 10.6 i386
framework binaries' __OBJC metadata.

Each emitted ivar records (name, i386_off, kind, i386_sz):
  kind 'P' = object/class pointer (i386 4-byte handle <-> x64 8-byte ptr)
  kind 'F' = i386 'f' float member (may be a CGFloat the modern runtime widened
             to double — decided at runtime from the modern encoding)
  kind 'I' = integer/char/short/bool, i386_sz in {1,2,4} (zero/sign-extended to
             the modern width at runtime; signedness from the modern encoding)
Skipped: C pointers (^ — meaningless across address spaces without a bridge),
SEL (:), structs ({), bitfields (b), 8-byte i386 scalars (q/Q/d — no clean
4-byte i386 shadow slot and rare in inherited protected ivars).

DEPENDENCY: the SnowLeopard install DVD must be mounted (it carries real i386
AppKit/Foundation/QuartzCore with __OBJC ivar metadata; the 10.6 SDK is a stub):
  hdiutil attach -nobrowse -readonly "~/projects/oses/SnowLeopardInstall/snow leopard install.iso"
  -> /Volumes/Mac OS X Install DVD

This header is a COMMITTED generated artifact (the DVD is not present on every
build host); regenerate it by hand when the framework set changes:
  gen_native_ivar_map.py > native_ivar_map.gen.h
"""
import re, subprocess, sys, os

DVD = "/Volumes/Mac OS X Install DVD/System/Library/Frameworks"
FRAMEWORKS = ["AppKit", "Foundation", "QuartzCore"]

def classify(enc):
    """Map an i386 ivar type encoding to (kind, i386_size) or None to skip.
    P=object/class pointer (4B), F=float (4B), I=integer (size per encoding)."""
    if not enc:
        return None
    c = enc[0]
    if c in "@#":
        return ('P', 4)
    if c == 'f':
        return ('F', 4)
    if c in "cC":          # char / BOOL : 1 byte
        return ('I', 1)
    if c in "sS":          # short : 2 bytes
        return ('I', 2)
    if c in "iIlL":        # int / long : 4 bytes on i386 (long is 4-byte LP32)
        return ('I', 4)
    return None            # ^, :, {, b, q, Q, d, *, [ ...] -> skip

def parse(path):
    """class_name -> [(ivar_name, i386_abs_offset, kind, i386_sz)]."""
    out = subprocess.run(["otool", "-arch", "i386", "-ov", path],
                         capture_output=True, text=True, errors="replace").stdout
    classes = {}
    cur = None
    pend_name = pend_type = None
    for line in out.splitlines():
        m = re.match(r'\s*name 0x[0-9a-f]+ (\S+)\s*$', line)
        if m:
            cur = m.group(1)
            classes.setdefault(cur, [])
            pend_name = pend_type = None
            continue
        m = re.match(r'\s*ivar_name 0x[0-9a-f]+ (\S+)\s*$', line)
        if m:
            pend_name = m.group(1); pend_type = None; continue
        m = re.match(r'\s*ivar_type 0x[0-9a-f]+ (.+?)\s*$', line)
        if m:
            pend_type = m.group(1); continue
        m = re.match(r'\s*ivar_offset 0x([0-9a-f]+)\s*$', line)
        if m and cur and pend_name is not None:
            off = int(m.group(1), 16)
            k = classify(pend_type)
            if k is not None:
                classes[cur].append((pend_name, off, k[0], k[1]))
            pend_name = pend_type = None
    return {c: ivs for c, ivs in classes.items() if ivs}

def main():
    all_cls = {}
    for fw in FRAMEWORKS:
        p = None
        for v in ("Versions/C", "Versions/A", ""):
            cand = os.path.join(DVD, f"{fw}.framework", v, fw) if v else \
                   os.path.join(DVD, f"{fw}.framework", fw)
            if os.path.isfile(cand):
                p = cand; break
        if not p:
            sys.stderr.write(f"WARN: {fw} not found on DVD (is it mounted?)\n")
            continue
        for c, ivs in parse(p).items():
            all_cls.setdefault(c, ivs)   # first non-empty wins on dup
    print("/* GENERATED by gen_native_ivar_map.py from the 10.6 i386 frameworks")
    print(" * (SnowLeopard install DVD). Do not edit by hand. See the script for")
    print(" * the DVD-mount dependency and rationale. */")
    print("struct nivar_ent { const char *name; uint32_t i386_off; "
          "uint8_t kind; uint8_t i386_sz; };")
    print("struct nivar_cls { const char *cls; const struct nivar_ent *iv; "
          "uint32_t n; };")
    names = sorted(all_cls)
    for c in names:
        sym = re.sub(r'[^A-Za-z0-9_]', '_', c)
        print(f"static const struct nivar_ent _niv_{sym}[] = {{")
        for n, off, k, sz in all_cls[c]:
            print(f'  {{"{n}", {off}, \'{k}\', {sz}}},')
        print("};")
    print("static const struct nivar_cls g_native_ivar_map[] = {")
    for c in names:
        sym = re.sub(r'[^A-Za-z0-9_]', '_', c)
        print(f'  {{"{c}", _niv_{sym}, {len(all_cls[c])}}},')
    print("};")
    print(f"enum {{ G_NATIVE_IVAR_MAP_N = {len(names)} }};")
    sys.stderr.write(f"emitted {len(names)} classes, "
                     f"{sum(len(v) for v in all_cls.values())} syncable ivars\n")

if __name__ == "__main__":
    main()
