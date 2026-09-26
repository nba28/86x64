#!/usr/bin/env python3
"""shimgen — automatic missing-symbol shim generator (the "shimdb" stage).

For each given Mach-O binary, statically enumerate its dyld binds, group them
by source dylib, and check every symbol against what that dylib actually
provides on the RUNNING system:

  * system frameworks (no file on disk — they live in the dyld shared cache)
    are probed via dlopen()+dlsym(): safe, they are self-consistent;
  * on-disk deps are probed via `nm -gU` static export parsing: never dlopen
    a bundled binary (a broken one aborts the prober).

The real implementations of removed symbols are written ONCE, by hand, as
ordinary compilable source under shimdb/impl/ (e.g. memory.c -> BlockMoveData,
appkit.m -> NSFlippableView). shimgen compiles that pool, then for each app
links only the objects that provide a symbol the app actually binds and
-dead_strips the rest, so each shim carries solely the required impls. A
removed symbol with no curated impl YET gets a LOUD last-resort stub (function
-> log-once + optional SHIMGEN_STUB_ABORT trap, never a silent return-0 that
corrupts; class -> empty NSObject subclass; data -> zeroed slot) and is
recorded in shimdb/observed.json as the curation queue.

One shim dylib is generated per deficient framework: <FW>ShimAuto.dylib, which
defines the app's missing symbols and RE-EXPORTS the real framework (or the
existing handwritten <FW>Shim.dylib, which itself re-exports the framework), so
a single dependency swap covers both missing and present symbols. The dependent
binary's load command is rewritten to @rpath/<FW>ShimAuto.dylib and flat-signed.

usage: shimgen.py <App.app> <binary-relpath> [<binary-relpath> ...]
       shimgen.py <binary>            # a loose Mach-O: shims land beside it
       shimgen.py --scan-only <App.app|binary> [<binary-relpath> ...]
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

_sdk_cache = None
def sdk_path():
    global _sdk_cache
    if _sdk_cache is None:
        r = run(["xcrun", "--show-sdk-path"])
        _sdk_cache = r.stdout.strip() if r.returncode == 0 else ""
    return _sdk_cache

def strip_allowable_clients(src_tbd, dst_tbd):
    """Copy a .tbd, dropping every `allowable-clients:` block. A restricted
    sub-framework (HIToolbox/CarbonSound/NavigationServices under Carbon) lists
    only Apple frameworks as allowable clients, so ld refuses to let a generated
    shim -reexport_framework it directly ("not an allowed client of it"). The
    allowable-clients constraint is advisory link-time metadata; a copy with it
    removed lets ld emit the re-export (the LC_REEXPORT_DYLIB still names the
    real framework, so dyld loads the genuine one at runtime)."""
    out, skip_indent = [], None
    for line in open(src_tbd):
        stripped = line.lstrip(" ")
        indent = len(line) - len(stripped)
        if skip_indent is not None:
            # inside the block: skip while more-indented than the key line
            if stripped.strip() and indent <= skip_indent:
                skip_indent = None            # block ended; fall through
            else:
                continue
        if stripped.startswith("allowable-clients:"):
            skip_indent = indent
            continue
        out.append(line)
    with open(dst_tbd, "w") as f:
        f.write("".join(out))

def reexport_flags_stripped_tbd(dep_path, fw, gen_dir):
    """Fallback for a restricted sub-framework: reexport against a copy of its
    SDK .tbd with allowable-clients stripped. Returns clang flags, or [] if no
    tbd is found (then the caller keeps the stubs-only fallback)."""
    sdk = sdk_path()
    if not sdk or not dep_path.startswith("/"):
        return []
    tbd = sdk + dep_path + ".tbd"
    if not os.path.exists(tbd):
        return []
    rex_dir = os.path.join(gen_dir, "reexport_tbd")
    fw_dir_stub = os.path.join(rex_dir, f"{fw}.framework")
    os.makedirs(fw_dir_stub, exist_ok=True)
    strip_allowable_clients(tbd, os.path.join(fw_dir_stub, f"{fw}.tbd"))
    return ["-F", rex_dir, "-Wl,-reexport_framework," + fw]

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

def is_translated_consumer(binary):
    """True if `binary` is a macho-tool-translated image (links libabiconv). Such
    a consumer must NOT be redirected to a native <FW>ShimAuto: its calls use the
    i386 4-byte-return cdecl convention, and a native stub's 8-byte `ret`
    over-pops that frame -> fused/garbage PC (the s28 ABI family; verified on Civ
    IV / Halo translated QuickTime -> _InitHLTB). Removed symbols for a translated
    consumer are covered by libabiconv's ___X i386-frame marshalling shims
    (wired via static-interpose at translate time), not by native ShimAuto."""
    for d in deps_of(binary):
        leaf = d.rsplit("/", 1)[-1]
        if "libabiconv" in leaf:
            return True
    return False

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
_libsys.objc_getClass.restype = ctypes.c_void_p
_libsys.objc_getClass.argtypes = [ctypes.c_char_p]

_objc_fw_loaded = set()

def _ensure_fw_loaded(fw_name):
    """dlopen a framework into this prober so its PRIVATE ObjC classes register
    in the runtime — a private class's _OBJC_CLASS_$_ symbol is absent from the
    export trie, so it is only visible through objc_getClass once loaded."""
    if not fw_name or fw_name in _objc_fw_loaded:
        return
    _objc_fw_loaded.add(fw_name)
    for cand in (f"/System/Library/Frameworks/{fw_name}.framework/{fw_name}",
                 f"/System/Library/PrivateFrameworks/{fw_name}.framework/{fw_name}"):
        if _libsys.dlopen(cand.encode(), 0x1):  # RTLD_LAZY
            return

def objc_class_live(name, fw_name=None):
    """True if a class named <name> is ALREADY registered in the host ObjC
    runtime. A framework's PRIVATE classes (NSFontEffectsBox, NSColorSwatchCell,
    NSTextViewSharedData, NSFlippableView ...) are live at runtime but their
    _OBJC_CLASS_$_ symbol is NOT in the export trie, so the dlsym/nm symbol probe
    wrongly reports them missing. Re-implementing a live class with a duplicate
    @implementation SHADOWS it and corrupts the runtime (the "Class X is
    implemented in both ... may cause mysterious crashes" warning, e.g. iPhoto's
    garbage text/colour metrics). Detect liveness here so the stub can ALIAS the
    real class instead of registering a duplicate. The prober runs x86_64 (see
    the re-exec at top), matching the arch the translated app runs under."""
    for fw in ("AppKit", "Foundation", fw_name):
        _ensure_fw_loaded(fw)
    return bool(_libsys.objc_getClass(name.encode()))

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

def shim_stem(dep):
    """Base name for a <stem>ShimAuto.dylib / <stem>Shim.dylib shim, from a
    dependency leaf. A dep that already ends in .dylib (a bundled library like
    eOkaoPt.dylib) would otherwise yield the ugly 'eOkaoPt.dylibShimAuto.dylib';
    strip the extension so it reads 'eOkaoPtShimAuto.dylib'. Framework leaf
    names (e.g. CoreServices) carry no extension and pass through unchanged."""
    base = os.path.basename(dep)
    return base[:-len(".dylib")] if base.endswith(".dylib") else base

# function stubs whose name looks like a memory/data primitive: a silent
# no-op there is never "missing API tolerated" -- it is silent corruption
# (BlockMoveData = memmove). These MUST get a curated body, not a 0-stub.
DANGEROUS_STUB_RE = re.compile(r"(?i)(block(move|zero)|mem(cpy|move|set)|"
                               r"\bbcopy\b|copybits)")

IMPL_DIR = os.path.join(SHIMDB_DIR, "impl")

def build_impl_index(fw_dir):
    """Compile every curated implementation under shimdb/impl/ (real .c/.m
    source a human writes ONCE for a removed symbol) to an object file and map
    each symbol it defines -> that object. A per-app shim then links ONLY the
    objects that provide a symbol that app actually binds, and -dead_strips the
    rest, so each shim carries solely the required implementations. This is the
    model: hand-written impls live here; shimgen just selects + compiles them.
    Returns {symbol: obj_path}; an impl that fails to compile is skipped loudly.
    """
    provided = {}
    if not os.path.isdir(IMPL_DIR):
        return provided
    obj_dir = os.path.join(SHIMDB_DIR, "generated", "impl_obj")
    os.makedirs(obj_dir, exist_ok=True)
    for fn in sorted(os.listdir(IMPL_DIR)):
        if not fn.endswith((".c", ".m")):
            continue
        src = os.path.join(IMPL_DIR, fn)
        obj = os.path.join(obj_dir, fn + ".o")
        if (not os.path.exists(obj) or
                os.path.getmtime(obj) < os.path.getmtime(src)):
            r = run(["clang", "-c", "-arch", "x86_64", "-O1", "-fno-common",
                     "-fvisibility=default", "-F", fw_dir, "-o", obj, src])
            if r.returncode != 0:
                tail = (r.stderr.strip().splitlines()[-1]
                        if r.stderr.strip() else "")
                print(f"WARN impl/{fn}: compile failed, skipping ({tail})")
                continue
        for line in run(["nm", "-gU", obj]).stdout.splitlines():
            parts = line.split()
            if len(parts) >= 3:
                provided.setdefault(parts[2], obj)
    return provided


def gen_stub_source(fw_name, uncovered, index, observed):
    """Loud LAST-RESORT stubs for symbols an app needs that are NOT in the
    curated impl pool -- a removed symbol nobody has written an implementation
    for yet. Each is recorded in observed.json (the curation queue). Function
    stubs log once on first call and trap under SHIMGEN_STUB_ABORT (never a
    silent 0 -- that is what broke frameworks); a removed class becomes an
    empty NSObject subclass; data becomes a zeroed slot. Symbols are de-duped
    by emitted C identifier so two binds that collapse to one name can't
    double-define and fail the compile, and a non-identifier data symbol
    (".objc_*"/"$") keeps its exact export via __asm."""
    classes, funcs, data = [], [], []
    for sym in sorted(uncovered):
        kind, _ = classify(sym)
        ekind = index.get(sym, {}).get("kind")
        if kind in ("objc_class", "objc_metaclass"):
            classes.append(classify(sym)[1])
        elif ekind == "data" or kind == "data":
            data.append(sym)
        else:
            funcs.append(sym)
        observed.setdefault(sym, {"first_seen_fw": fw_name,
                                  "stub": kind if kind != "unknown"
                                          else "function"})
    src = ["#import <Cocoa/Cocoa.h>", "#include <stdio.h>",
           "#include <stdlib.h>", "#include <pthread.h>", ""]
    src.append(
        'static void shim_note(const char *sym) {\n'
        '  static const char *seen[2048]; static int n;\n'
        '  static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;\n'
        '  pthread_mutex_lock(&mtx);\n'
        '  for (int i = 0; i < n; i++)\n'
        '    if (seen[i] == sym) { pthread_mutex_unlock(&mtx); return; }\n'
        '  if (n < 2048) seen[n++] = sym;\n'
        '  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no '
        'curated impl -- returning 0 (write one in shimdb/impl/ if its '
        'behavior matters)\\n", "' + fw_name + '", sym);\n'
        '  if (getenv("SHIMGEN_STUB_ABORT")) abort();\n'
        '  pthread_mutex_unlock(&mtx);\n'
        '}')
    emitted = set()
    for name in sorted(set(classes)):
        if name in emitted:
            continue
        emitted.add(name)
        if objc_class_live(name, fw_name):
            # LIVE in the host runtime but its _OBJC_CLASS_$_ symbol is private
            # (not exported), so the symbol probe marked it missing. A plain
            # @implementation would register a DUPLICATE class and corrupt the
            # runtime ("implemented in both"). Define a UNIQUELY-named real class
            # and ALIAS the _OBJC_CLASS_$_/_OBJC_METACLASS_$_ symbols to it: the
            # dyld bind still resolves, but the objc runtime registers our
            # distinct name, so the live host class is never shadowed.
            fwd = f"{name}_86x64fwd"
            src += ["", f"@interface {fwd} : NSObject", "@end",
                    f"@implementation {fwd}", "@end",
                    "__asm__(",
                    f'  ".globl _OBJC_CLASS_$_{name}\\n"',
                    f'  ".set _OBJC_CLASS_$_{name}, _OBJC_CLASS_$_{fwd}\\n"',
                    f'  ".globl _OBJC_METACLASS_$_{name}\\n"',
                    f'  ".set _OBJC_METACLASS_$_{name}, _OBJC_METACLASS_$_{fwd}\\n"',
                    ");"]
        else:
            src += ["", f"@interface {name} : NSObject", "@end",
                    f"@implementation {name}", "@end"]
    for sym in sorted(funcs):
        # Force the EXACT Mach-O symbol via an __asm label (mirrors the data
        # path below) instead of relying on the C-name -> _C-name convention.
        # `sym.lstrip("_")` as a bare C function name is WRONG for any symbol
        # whose SOURCE C name itself begins with an underscore (Mach-O "__X" =>
        # C "_X", e.g. QuickTime's `__InitHLTB` from HIToolbox): the naive stub
        # defined `_InitHLTB` while the export list demanded `__InitHLTB`, so the
        # whole <FW>ShimAuto.dylib failed to link and NONE of that framework's
        # missing symbols got stubbed (a single mishandled symbol sank the
        # entire shim). It also mishandles non-identifier symbols (e.g. `$`
        # variants). The __asm label emits `sym` literally, exactly like data.
        base = re.sub(r"[^A-Za-z0-9_]", "_", sym.lstrip("_")) or "fnsym"
        if base[0].isdigit():
            base = "_" + base
        cname, i = base, 1
        while cname in emitted:          # collision-safe: exports stay exact
            cname, i = f"{base}_{i}", i + 1
        emitted.add(cname)
        if DANGEROUS_STUB_RE.search(cname):
            print(f"WARN {fw_name}: stubbing memory primitive {sym} as a "
                  f"NO-OP -- write a real impl in shimdb/impl/ (e.g. memmove).")
        src += ["", f'long {cname}(long a, long b, long c_, long d, long e, '
                    f'long f) __asm("{sym}");',
                f'long {cname}(long a, long b, long c_, long d, long e, '
                f'long f) {{ shim_note("{sym}"); return 0; }}']
    for sym in sorted(data):
        base = re.sub(r"[^A-Za-z0-9_]", "_", sym.lstrip("_")) or "datasym"
        if base[0].isdigit():
            base = "_" + base
        cname, i = base, 1
        while cname in emitted:          # collision-safe: exports stay exact
            cname, i = f"{base}_{i}", i + 1
        emitted.add(cname)
        src += ["", f'__attribute__((visibility("default"))) '
                    f'long {cname} __asm("{sym}") = 0;']
    return "\n".join(src) + "\n"

def main():
    args = sys.argv[1:]
    scan_only = "--scan-only" in args
    args = [a for a in args if a != "--scan-only"]
    if not args:
        sys.exit(__doc__)
    root = os.path.abspath(args[0])
    # Accept a BUNDLE, a loose DIRECTORY, or a single Mach-O BINARY. A bundle
    # keeps its shims in Contents/Frameworks referenced via @rpath; a loose
    # binary/dir keeps them BESIDE the binary referenced via @loader_path (a
    # loose binary has no Frameworks rpath for @rpath to resolve against).
    if os.path.isfile(root):
        app, targets = os.path.dirname(root), [os.path.basename(root)]
        fw_dir, ref_prefix = app, "@loader_path/"
    else:
        app, targets = root, args[1:]
        cf = os.path.join(app, "Contents", "Frameworks")
        fw_dir, ref_prefix = ((cf, "@rpath/") if os.path.isdir(cf)
                              else (app, "@loader_path/"))
        if not targets:
            sys.exit(__doc__)
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
                fw = shim_stem(os.path.basename(dep)[:-len("ShimAuto.dylib")])
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
                                     f"{ref_prefix}{os.path.basename(hand)}")
                    continue
            if not missing:
                continue
            fw = shim_stem(dep)
            print(f"{rel}: {len(missing)} missing from {fw}: "
                  f"{sorted(missing)[:6]}{'...' if len(missing) > 6 else ''}")
            ent = plan.setdefault(fw, {"dep_path": dep, "missing": set(),
                                       "dependents": []})
            ent["missing"] |= missing
            ent["dependents"].append((binary, dep))

    # Build the curated-impl pool (shimdb/impl/*) once: symbol -> object file.
    impl_provided = build_impl_index(fw_dir)
    # Coverage report: each per-app shim is assembled from that pool for exactly
    # the symbols THIS app needs that are absent on THIS macOS; anything with no
    # curated impl yet is a loud stub queued into observed.json. A growing
    # 'impl' column means fewer silent behavioural holes.
    if plan:
        tot = {"impl": 0, "class": 0, "data": 0, "stub": 0}
        print("shim coverage (impl <- shimdb/impl pool; stub <- needs curation):")
        for fw, ent in sorted(plan.items()):
            c = {"impl": 0, "class": 0, "data": 0, "stub": 0}
            for s in ent["missing"]:
                if s in impl_provided:
                    c["impl"] += 1
                    continue
                k, _ = classify(s)
                if k in ("objc_class", "objc_metaclass"):
                    c["class"] += 1
                elif k == "data" or index.get(s, {}).get("kind") == "data":
                    c["data"] += 1
                else:
                    c["stub"] += 1
            for kk in tot:
                tot[kk] += c[kk]
            print(f"  {fw:30} {c['impl']:3} impl  {c['class']:3} class  "
                  f"{c['data']:3} data  {c['stub']:3} stub")
        print(f"  {'TOTAL':30} {tot['impl']:3} impl  {tot['class']:3} class  "
              f"{tot['data']:3} data  {tot['stub']:3} stub")

    if scan_only or not plan:
        print("scan complete; nothing to generate" if not plan else
              "scan-only mode; no shims written")
        return

    os.makedirs(SHIMDB_DIR, exist_ok=True)
    gen_dir = os.path.join(SHIMDB_DIR, "generated")
    os.makedirs(gen_dir, exist_ok=True)
    for fw, ent in plan.items():
        shim_name = f"{fw}ShimAuto.dylib"
        shim_path = os.path.join(fw_dir, shim_name)
        # regeneration REPLACES the dylib: keep symbols a previous run provided
        # for OTHER dependents (their load commands still point here) or they
        # vanish and dyld aborts at their next launch. nm's section type marks
        # data slots (classify() can't tell from the bare name).
        if os.path.exists(shim_path):
            for line in run(["nm", "-gU", shim_path]).stdout.splitlines():
                parts = line.split()
                if len(parts) < 3:
                    continue
                ent["missing"].add(parts[2])
                if parts[1] in ("D", "S", "B", "C"):
                    index.setdefault(parts[2], {"kind": "data"})
        M = ent["missing"]
        covered = sorted(s for s in M if s in impl_provided)
        uncovered = M - set(covered)
        impl_objs = sorted({impl_provided[s] for s in covered})
        # generated source provides ONLY the uncovered remainder (loud stubs)
        srcf = os.path.join(gen_dir, f"{fw}ShimAuto.m")
        open(srcf, "w").write(gen_stub_source(fw, uncovered, index, observed))
        # export EXACTLY this app's missing set: a curated .o shared with other
        # apps may define more symbols, but only M is exported and the rest is
        # dead-stripped, so the shim carries solely the required symbols.
        expf = os.path.join(gen_dir, f"{fw}ShimAuto.exp")
        open(expf, "w").write("".join(s + "\n" for s in sorted(M)))
        cmd = ["clang", "-dynamiclib", "-arch", "x86_64",
               "-o", shim_path, srcf, *impl_objs,
               "-framework", "Cocoa",
               "-framework", "Accelerate",   # shimdb/impl/veclib.c
               "-Wl,-dead_strip_dylibs",
               "-F", fw_dir,  # bundled frameworks (iLifeSlideshow etc.)
               "-install_name", f"{ref_prefix}{shim_name}",
               "-Wl,-dead_strip",
               "-Wl,-exported_symbols_list," + expf]
        # Re-export the handwritten shim if one exists (it re-exports the real
        # framework); else re-export the real framework when it still exists.
        # NOTE: pass ONLY the reexport flag — also listing the library as a
        # plain input makes ld link it as LC_LOAD_DYLIB and drop the re-export.
        hand = os.path.join(fw_dir, f"{fw}Shim.dylib")
        if os.path.exists(hand):
            cmd += ["-Wl,-reexport_library," + hand]
        else:
            probe = provider_symbols(ent["dep_path"], fw_dir)
            if probe[0] != "missing":
                cmd += ["-Wl,-reexport_framework," + fw]
        r = run(cmd)
        if r.returncode != 0 and any(a.startswith("-Wl,-reexport") for a in cmd):
            # A restricted sub-framework (HIToolbox/CarbonSound/NavigationServices
            # under Carbon) refuses direct -reexport_framework linkage ("not an
            # allowed client of it"). Retry against a copy of its SDK .tbd with
            # allowable-clients stripped BEFORE giving up: this keeps the present
            # symbols reachable (the LC_REEXPORT_DYLIB still names the real
            # framework, so dyld loads the genuine one). Without this the shim is
            # stubs-only and the framework's PRESENT symbols regress to NULL
            # (Civ IV s32: QuickTime's 114 present HIToolbox symbols would go
            # NULL if HIToolboxShimAuto only stubbed the 115 removed ones).
            tbd_flags = reexport_flags_stripped_tbd(ent["dep_path"], fw, gen_dir)
            r2 = None
            if tbd_flags:
                base = [a for a in cmd if not a.startswith("-Wl,-reexport")]
                r2 = run(base + tbd_flags)
            if r2 is not None and r2.returncode == 0:
                r = r2
            else:
                # SDK lacks the tbd (framework truly gone) or the strip did not
                # help: fall back to stubs-only, loudly — symbols reachable only
                # via the re-export become missing.
                print(f"WARN {shim_name}: re-export link failed, retrying "
                      f"stubs-only:\n{r.stderr.strip().splitlines()[-1] if r.stderr.strip() else ''}")
                cmd = [a for a in cmd if not a.startswith("-Wl,-reexport")]
                r = run(cmd)
        if r.returncode != 0:
            print(f"FAIL compile {shim_name}:\n{r.stderr[-2000:]}")
            continue
        flat_sign(shim_path)
        print(f"generated {shim_name}: {len(covered)} impl + "
              f"{len(uncovered)} stub = {len(M)} symbols")
        for binary, dep in ent["dependents"]:
            # A TRANSLATED consumer (links libabiconv) must never be redirected to
            # a native ShimAuto: its i386 4-byte-return calls over-pop the stub's
            # 8-byte `ret` (s28 ABI family). Leave its dep as-is; libabiconv's
            # ___X marshalling shims (static-interpose) cover the removed symbols.
            # The shim is still generated + signed for any NATIVE dependents.
            if is_translated_consumer(binary):
                print(f"  skip redirect (translated consumer, defer to "
                      f"libabiconv): {os.path.basename(binary)} -> {shim_name}")
                continue
            redirect_dep(app, binary, dep, f"{ref_prefix}{shim_name}")

    with open(OBSERVED_PATH, "w") as f:
        json.dump(observed, f, indent=1, sort_keys=True)

if __name__ == "__main__":
    main()
