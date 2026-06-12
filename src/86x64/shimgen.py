#!/usr/bin/env python3
"""shimgen — automatic missing-symbol shim generator (the "shimdb" stage).

For each given Mach-O binary, statically enumerate its dyld binds, group them
by source dylib, and check every symbol against what that dylib actually
provides on the RUNNING system:

  * system frameworks (no file on disk — they live in the dyld shared cache)
    are probed via dlopen()+dlsym(): safe, they are self-consistent;
  * on-disk deps are probed via `nm -gU` static export parsing: never dlopen
    a bundled binary (a broken one aborts the prober).

Missing symbols are looked up in the shimdb (curated implementations); any
unknown symbol gets an auto-stub (ObjC class -> empty NSObject/NSView
subclass; function -> log-and-return-0; data -> zeroed pointer slot) and is
recorded in shimdb/observed.json for later curation.

One shim dylib is generated per deficient framework: <FW>ShimAuto.dylib,
which defines the missing symbols and RE-EXPORTS the real framework (or the
existing handwritten <FW>Shim.dylib, which itself re-exports the framework),
so a single dependency swap covers both missing and present symbols. The
dependent binary's load command is then rewritten to @rpath/<FW>ShimAuto.dylib
and both files are flat-signed.

usage: shimgen.py <App.app> <binary-relpath> [<binary-relpath> ...]
       shimgen.py --scan-only <App.app> <binary-relpath> ...
"""
import ctypes, json, os, platform, re, shutil, subprocess, sys, tempfile

# The dlopen/dlsym probes must inspect the X86_64 dyld namespace — the arch
# the translated app runs under Rosetta — not the arm64 host cache, whose
# exports differ (no objc_msgSend_stret/fpret/_fixup, no $INODE64/$UNIX2003
# variants, frameworks pruned differently). Probing the wrong arch was the
# root of the over-stub bug (shadowing real symbols with auto-stubs).
if platform.machine() == "arm64" and not os.environ.get("SHIMGEN_X86_64"):
    os.environ["SHIMGEN_X86_64"] = "1"
    os.execvp("arch", ["arch", "-x86_64", "/usr/bin/python3",
                       os.path.abspath(__file__)] + sys.argv[1:])

PROJ = os.path.dirname(os.path.abspath(__file__))
SHIMDB_DIR = os.path.join(PROJ, "shimdb")
INDEX_PATH = os.path.join(SHIMDB_DIR, "index.json")
OBSERVED_PATH = os.path.join(SHIMDB_DIR, "observed.json")

def run(args, **kw):
    return subprocess.run(args, capture_output=True, text=True,
                          errors="replace", **kw)

def find_int_tool():
    for c in ("/usr/local/opt/llvm/bin/llvm-install-name-tool",
              "/opt/homebrew/opt/llvm/bin/llvm-install-name-tool"):
        if os.access(c, os.X_OK):
            return c
    return shutil.which("llvm-install-name-tool") or "install_name_tool"
INT = find_int_tool()

def flat_sign(path):
    """Sign a copy outside any bundle so codesign doesn't walk a manifest."""
    with tempfile.TemporaryDirectory() as d:
        t = os.path.join(d, os.path.basename(path))
        shutil.copy2(path, t)
        run(["codesign", "--remove-signature", t])
        run(["codesign", "--force", "--sign", "-",
             "--identifier", os.path.basename(path), t])
        shutil.copy2(t, path)

def deps_of(binary):
    """[(dep_path, version_suffix)] from LC_LOAD_DYLIB (LC_ID excluded)."""
    out = run(["otool", "-L", binary]).stdout.splitlines()[1:]
    own_id = run(["otool", "-D", binary]).stdout.splitlines()
    own_id = own_id[1].strip() if len(own_id) > 1 else None
    deps = []
    for line in out:
        m = re.match(r"\s+(\S.*?) \((compatibility.*)\)$", line)
        if not m:
            continue
        path = m.group(1)
        if path == own_id:
            continue
        deps.append(path)
    return deps

def binds_of(binary):
    """{dylib_short_name: set(symbols)} from bind + lazy-bind tables."""
    res = {}
    for flag in ("--bind", "--lazy-bind"):
        out = run(["objdump", "--macho", flag, binary]).stdout
        for line in out.splitlines():
            parts = line.split()
            if len(parts) < 4 or parts[0] in ("Bind", "Lazy", "segment", "Dyld"):
                continue
            if not parts[0].startswith("__"):
                continue
            sym, dylib = parts[-1], parts[-2]
            # bind table rows may carry a trailing "(weak import)" etc.
            if sym.endswith(")"):
                continue
            res.setdefault(dylib, set()).add(sym)
    return res

_dlopen_cache = {}
_libsys = ctypes.CDLL(None)
_libsys.dlsym.restype = ctypes.c_void_p
_libsys.dlsym.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
_libsys.dlopen.restype = ctypes.c_void_p
_libsys.dlopen.argtypes = [ctypes.c_char_p, ctypes.c_int]

def nm_exports(path):
    out = run(["nm", "-gU", path]).stdout
    syms = set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            syms.add(parts[2])
    return syms

def reexports_of(path):
    """LC_REEXPORT_DYLIB names of an on-disk Mach-O (host-arch-independent,
    unlike dlopen of an x86_64-only binary on arm64)."""
    out = run(["otool", "-l", path]).stdout
    rex, take = [], False
    for line in out.splitlines():
        if "LC_REEXPORT_DYLIB" in line:
            take = True
        elif take and " name " in line:
            rex.append(line.split()[1])
            take = False
    return rex

def provider_symbols(dep_path, app_frameworks, _depth=4, _seen=None,
                     loader_dir=None):
    """Return ('disk', set) | ('dl', handle) | ('multi', [providers]) |
    ('missing', None).

    An on-disk provider is probed as its full re-export CLOSURE: nm of a
    SL-era umbrella shell (vecLib) or of one of our ShimAuto dylibs shows
    only direct exports, while the symbols a dependent actually binds live
    behind LC_REEXPORT_DYLIB links (libBLAS, the real framework, a hand
    shim). nm alone mis-reports those as missing — the over-stub bug that
    shadowed working cblas/LAPACK."""
    if _seen is None:
        _seen = set()
    candidates = []
    if dep_path.startswith("/"):
        candidates.append(dep_path)
    for tag in ("@rpath/", "@executable_path/../Frameworks/",
                "@loader_path/../Frameworks/", "@loader_path/",
                "@executable_path/"):
        if dep_path.startswith(tag):
            candidates.append(os.path.join(app_frameworks,
                                           dep_path[len(tag):]))
    # @loader_path is relative to the DEPENDENT's own directory (e.g.
    # MacOS/libabiconv.dylib, eOkao*.dylib beside iLifeFaceRecognition) —
    # the Frameworks-dir guess above misses those
    if loader_dir:
        for tag in ("@loader_path/", "@executable_path/", "@rpath/"):
            if dep_path.startswith(tag):
                candidates.append(os.path.normpath(
                    os.path.join(loader_dir, dep_path[len(tag):])))
    # bundled framework referenced by an absolute path that doesn't exist
    # on the system (e.g. /Library/Frameworks/Geode.framework/...): try the
    # app's Frameworks dir by the .framework-relative subpath.
    m = re.search(r"([^/]+\.framework/.*)$", dep_path)
    if m:
        candidates.append(os.path.join(app_frameworks, m.group(1)))
    for resolved in candidates:
        if os.path.exists(resolved):
            real = os.path.realpath(resolved)
            if real in _seen:
                return ("missing", None)
            _seen.add(real)
            provs = [("disk", nm_exports(resolved))]
            if _depth > 0:
                for rex in reexports_of(resolved):
                    p = provider_symbols(rex, app_frameworks,
                                         _depth - 1, _seen)
                    if p[0] != "missing":
                        provs.append(p)
            return ("multi", provs) if len(provs) > 1 else provs[0]
    if dep_path.startswith("/"):
        h = _libsys.dlopen(dep_path.encode(), 0x1)  # RTLD_LAZY
        if h:
            return ("dl", h)
    # @rpath/<FW>.framework/... deps that aren't bundled usually resolve at
    # runtime to the system framework (the rpath sweep rewrites /System
    # paths). Probe it by name in the dyld cache before declaring missing.
    m = re.search(r"([^/]+)\.framework/", dep_path)
    if m:
        fw = m.group(1)
        for cand in (f"/System/Library/Frameworks/{fw}.framework/Versions/A/{fw}",
                     f"/System/Library/PrivateFrameworks/{fw}.framework/Versions/A/{fw}"):
            h = _libsys.dlopen(cand.encode(), 0x1)
            if h:
                return ("dl", h)
    return ("missing", None)

def symbol_present(provider, sym):
    kind, val = provider
    if kind == "disk":
        return sym in val
    if kind == "multi":
        return any(symbol_present(p, sym) for p in val)
    if kind == "dl":
        # dlsym takes the name without its single leading underscore
        return bool(_libsys.dlsym(val, sym[1:].encode()
                                  if sym.startswith("_") else sym.encode()))
    return False

def redirect_dep(app, binary, old_dep, new_dep):
    run(["codesign", "--remove-signature", binary])
    r = run(["install_name_tool", "-change", old_dep, new_dep, binary])
    if r.returncode != 0:
        r = run([INT, "-change", old_dep, new_dep, binary])
    if r.returncode != 0:
        print(f"FAIL redirect {os.path.basename(binary)}: {r.stderr}")
        return False
    flat_sign(binary)
    print(f"  redirected {os.path.relpath(binary, app)}: "
          f"{old_dep} -> {new_dep}")
    return True

def load_index():
    if os.path.exists(INDEX_PATH):
        with open(INDEX_PATH) as f:
            return json.load(f)
    return {}

def classify(sym):
    if sym.startswith("_OBJC_CLASS_$_"):
        return ("objc_class", sym[len("_OBJC_CLASS_$_"):])
    if sym.startswith("_OBJC_METACLASS_$_"):
        return ("objc_metaclass", sym[len("_OBJC_METACLASS_$_"):])
    if sym.startswith("_OBJC_IVAR_$_") or sym.startswith(".objc_"):
        return ("data", sym)
    return ("unknown", sym)

def gen_source(fw_name, missing, index, observed):
    """Build the shim .m source for one framework's missing symbols."""
    classes, funcs, data = {}, [], []
    for sym in sorted(missing):
        entry = index.get(sym, {})
        kind, name = classify(sym)
        if kind == "objc_metaclass":
            kind, sym_cls = "objc_class", name
            classes.setdefault(name, entry)   # class def covers metaclass
            continue
        if kind == "objc_class":
            classes.setdefault(name, entry)
            continue
        ekind = entry.get("kind")
        if ekind == "objc_class":
            classes.setdefault(name, entry)
        elif ekind == "data" or kind == "data":
            data.append(sym)
        else:
            funcs.append(sym)        # default: function stub
        if sym not in index:
            observed.setdefault(sym, {"first_seen_fw": fw_name,
                                      "stub": kind if kind != "unknown"
                                              else "function"})
    src = ["#import <Cocoa/Cocoa.h>", "#include <stdio.h>", ""]
    src.append('static void shim_note(const char *s) '
               '{ fprintf(stderr, "[shimauto:%s] %s\\n", "' + fw_name +
               '", s); }')
    for cls, entry in sorted(classes.items()):
        sup = entry.get("superclass", "NSObject")
        methods = entry.get("methods", "")
        src += ["", f"@interface {cls} : {sup}", "@end",
                f"@implementation {cls}", methods, "@end"]
    for sym in funcs:
        c = sym.lstrip("_")
        src += ["", f"long {c}(long a, long b, long c_, long d, long e, "
                    f"long f) {{ shim_note(\"{c} called (auto-stub)\"); "
                    f"return 0; }}"]
    for sym in data:
        c = sym.lstrip("_")
        # exported zeroed pointer-sized slot; asm label keeps exact name
        src += ["", f'__attribute__((visibility("default"))) '
                    f'long {c.replace("$", "_DOLLAR_")} '
                    f'__asm("{sym}") = 0;']
    return "\n".join(src) + "\n"

def main():
    args = sys.argv[1:]
    scan_only = "--scan-only" in args
    args = [a for a in args if a != "--scan-only"]
    if len(args) < 2:
        sys.exit(__doc__)
    app = os.path.abspath(args[0])
    targets = args[1:]
    fw_dir = os.path.join(app, "Contents", "Frameworks")
    index = load_index()
    observed = {}
    if os.path.exists(OBSERVED_PATH):
        with open(OBSERVED_PATH) as f:
            observed = json.load(f)

    # fw_name -> {"dep_path": ..., "missing": set(), "dependents": [...]}
    plan = {}
    for rel in targets:
        binary = os.path.join(app, rel)
        if not os.path.isfile(binary):
            print(f"!! no such binary: {rel}")
            continue
        # objdump's per-bind short name strips the ".dylib" suffix (and any
        # trailing version component), so key the map under those variants
        # too — otherwise every .dylib dep (our ShimAuto redirects!) fails
        # the lookup and is silently skipped
        dep_paths = {}
        for p in deps_of(binary):
            base = os.path.basename(p)
            dep_paths[base] = p
            if base.endswith(".dylib"):
                stem = base[:-len(".dylib")]
                dep_paths.setdefault(stem, p)
                dep_paths.setdefault(stem.split(".")[0], p)
        for dylib_short, syms in binds_of(binary).items():
            dep = dep_paths.get(dylib_short)
            if dep is None:
                # objdump short name may lack lib prefix variations; skip
                continue
            if any(os.path.basename(dep).startswith(d)
                   for d in ("libSystem", "libobjc", "libabiconv",
                             "libinterpose", "libc++", "libgcc")):
                # runtime-critical libs: NEVER shim or redirect — a stub
                # shadowing objc_msgSend/malloc is instant death. Genuine
                # misses here surface loudly from dyld instead.
                continue
            if "ShimAuto" in dep:
                # A dep already redirected to our shim by a previous run.
                # The shim's nm output shows only its OWN stubs (re-exported
                # symbols live in the underlying lib), so check binds against
                # the union of: the shim, the handwritten <FW>Shim.dylib, and
                # the real/bundled framework. Anything still missing goes
                # into the plan so the shim is REGENERATED with it — without
                # this, regenerating for one dependent silently drops the
                # stubs every other dependent needs (RedRock/DRFile bug).
                fw = os.path.basename(dep)[:-len("ShimAuto.dylib")]
                shim_disk = os.path.join(fw_dir, os.path.basename(dep))
                if not os.path.exists(shim_disk):
                    print(f"!! {rel}: dep {dep} but no such shim on disk")
                    continue
                # only what's reachable THROUGH the shim counts: its stubs +
                # its re-export closure (the real framework is irrelevant if
                # the shim doesn't re-export it)
                provider = provider_symbols(shim_disk, fw_dir)
                still = {s for s in syms if not symbol_present(provider, s)}
                if not still:
                    continue
                print(f"{rel}: {len(still)} missing via {os.path.basename(dep)}: "
                      f"{sorted(still)[:6]}{'...' if len(still) > 6 else ''}")
                ent = plan.setdefault(fw, {"dep_path": dep,
                                           "missing": set(),
                                           "dependents": []})
                ent["missing"] |= still
                # dependent already points at the shim; no redirect needed
                continue
            provider = provider_symbols(dep, fw_dir,
                                        loader_dir=os.path.dirname(binary))
            missing = {s for s in syms if not symbol_present(provider, s)}
            if not missing:
                continue
            # a handwritten <FW>Shim.dylib (re-exported by our generated
            # shim) may already provide some — don't redefine those; if it
            # covers EVERYTHING, just redirect the dep straight to it.
            hand = os.path.join(fw_dir, os.path.basename(dep) + "Shim.dylib")
            if os.path.exists(hand) and not dep.endswith("Shim.dylib"):
                covered = missing & nm_exports(hand)
                missing -= covered
                if covered and not missing:
                    print(f"{rel}: handwritten "
                          f"{os.path.basename(hand)} covers all "
                          f"{len(covered)} missing from "
                          f"{os.path.basename(dep)}")
                    if not scan_only:
                        redirect_dep(app, binary, dep,
                                     f"@rpath/{os.path.basename(hand)}")
                    continue
            if not missing:
                continue
            fw = os.path.basename(dep)
            print(f"{rel}: {len(missing)} missing from {fw}: "
                  f"{sorted(missing)[:6]}{'...' if len(missing) > 6 else ''}")
            ent = plan.setdefault(fw, {"dep_path": dep, "missing": set(),
                                       "dependents": []})
            ent["missing"] |= missing
            ent["dependents"].append((binary, dep))

    if scan_only or not plan:
        print("scan complete; nothing to generate" if not plan else
              "scan-only mode; no shims written")
        return

    os.makedirs(SHIMDB_DIR, exist_ok=True)
    for fw, ent in plan.items():
        shim_name = f"{fw}ShimAuto.dylib"
        shim_path = os.path.join(fw_dir, shim_name)
        # regeneration REPLACES the dylib: keep the stubs a previous run
        # generated for other dependents (their load commands still point
        # here), or they vanish and dyld aborts at their next launch. Seed
        # the index with nm's section type so data slots stay data slots
        # (classify() can't tell from the bare name).
        if os.path.exists(shim_path):
            for line in run(["nm", "-gU", shim_path]).stdout.splitlines():
                parts = line.split()
                if len(parts) < 3:
                    continue
                ent["missing"].add(parts[2])
                if parts[1] in ("D", "S", "B", "C"):
                    index.setdefault(parts[2], {"kind": "data"})
        src = gen_source(fw, ent["missing"], index, observed)
        gen_dir = os.path.join(SHIMDB_DIR, "generated")
        os.makedirs(gen_dir, exist_ok=True)
        srcf = os.path.join(gen_dir, f"{fw}ShimAuto.m")
        open(srcf, "w").write(src)
        if True:
            # translated apps run x86_64 under Rosetta — never host arch
            cmd = ["clang", "-dynamiclib", "-arch", "x86_64",
                   "-o", shim_path, srcf,
                   "-framework", "Cocoa",
                   "-F", fw_dir,  # bundled frameworks (iLifeSlideshow etc.)
                   "-install_name", f"@rpath/{shim_name}"]
            # Re-export the handwritten shim if one exists (it re-exports
            # the real framework); else re-export the real framework when
            # it still exists on this system.
            # NOTE: pass ONLY the reexport flag — also listing the library
            # as a plain input makes ld link it as a regular LC_LOAD_DYLIB
            # and silently drop the re-export.
            hand = os.path.join(fw_dir, f"{fw}Shim.dylib")
            if os.path.exists(hand):
                cmd += ["-Wl,-reexport_library," + hand]
            else:
                dp = ent["dep_path"]
                probe = provider_symbols(dp, fw_dir)
                if probe[0] != "missing":
                    cmd += ["-Wl,-reexport_framework," + fw]
            r = run(cmd)
            if r.returncode != 0 and any(a.startswith("-Wl,-reexport") for a in cmd):
                # SDK may lack the tbd (framework gone) or refuse direct
                # linkage (sub-framework): fall back to stubs-only, loudly —
                # symbols reachable only via the re-export become missing.
                print(f"WARN {shim_name}: re-export link failed, retrying "
                      f"stubs-only:\n{r.stderr.strip().splitlines()[-1] if r.stderr.strip() else ''}")
                cmd = [a for a in cmd if not a.startswith("-Wl,-reexport")]
                r = run(cmd)
            if r.returncode != 0:
                print(f"FAIL compile {shim_name}:\n{r.stderr[-2000:]}")
                continue
        flat_sign(shim_path)
        print(f"generated {shim_name} ({len(ent['missing'])} symbols)")
        for binary, dep in ent["dependents"]:
            redirect_dep(app, binary, dep, f"@rpath/{shim_name}")

    with open(OBSERVED_PATH, "w") as f:
        json.dump(observed, f, indent=1, sort_keys=True)

if __name__ == "__main__":
    main()
