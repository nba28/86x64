#!/usr/bin/env python3
"""coverage-audit.py — every system call our targets make, and how we serve it.

WHY THIS EXISTS. Blocker after blocker turned out to be the same shape: a call
the app makes reaches a runtime path that silently does nothing. Found one at a
time by running the app and debugging the symptom. All of it is visible
STATICALLY, before anything runs:

  DEAD      abigen bridge whose native target is gone (libabiconv.nulljump):
            the bridge jumps to NULL.
  DEADNAME  hand shim that resolves a native BY NAME (dlsym / UIDL / R() /
            AGL_NATIVE) that this OS no longer exports: the shim silently
            no-ops. (Get/SetControl32BitValue -> every checkbox read 0.)
  STUB      hand shim whose body only returns a constant: "success", nothing
            done. Some are correct (a no-op IS the modern semantic); those get
            an entry in coverage-audit.ok with the reason, so the list converges.
  RAW       no bridge, native exists: a raw cross-ABI call (4-byte push vs
            8-byte ret, stack args vs registers).
  MISSING   no bridge and no native: the bind is weakened to NULL.
  OK        bridged to a live native, or a hand shim with real work.

Input: the i386 ORIGINALS (Apps32 bundles, non-bundle game trees). An import a
sibling module of the same target defines is not a system call and is skipped.

REACH. The static list is an upper bound (IMPORTED, not called). Every STUB
and failed by-name lookup reports its first hit per process at run time
(src/abiconv/gap.c) and appends it to $TMPDIR/86x64-reach/<prog>.<pid>.txt;
so does every RAW call (a one-shot stub on each bound call slot whose target is
native) and every by-name lookup served by the generic int-only thunk. `--reach
DIR` ranks what real runs hit: that is the work queue. DEAD bridges abort in
dyld on first call (naming the symbol), so they are loud without the ledger.

usage: coverage-audit.py [--lib libabiconv.dylib] [--nulljump file] [--top N]
                         [--target NAME]... [root]...
       coverage-audit.py --reach [DIR]     (default $TMPDIR/86x64-reach)
       roots default to every Apps32 bundle + the Portal 2 tree.
"""
import argparse, collections, glob, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from find_null_jump_bridges import resolves_on_x86_64, system_deps  # noqa: E402
import importlib.util  # noqa: E402
_spec = importlib.util.spec_from_file_location("unbridged", os.path.join(HERE, "unbridged-native-calls.py"))
unbridged = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(unbridged)

ABICONV = os.path.join(REPO, "src/abiconv")
DEF_ROOTS = sorted(glob.glob(os.path.expanduser("~/projects/translations/Apps64/*.app"))) + \
    [os.path.expanduser("~/Library/Application Support/Steam/steamapps/common/Portal 2/bin/osx64")]


def sh(args):
    return subprocess.run(args, capture_output=True, text=True).stdout


def binds(root):
    """Every bind a TRANSLATED image of `root` makes that leaves the tree:
    {("abiconv", X)} for libabiconv bridges ___X, {("native", X)} for native
    CALLS (__la_symbol_ptr / __jt_ptrs). Siblings and data are skipped."""
    files, translated = unbridged.collect([root])
    out = set()
    for p in files:
        tag = "v" if is_vendored(root, p) else "a"
        if unbridged.text_vmaddr(p) != unbridged.TRANSLATED_TEXT_BASE:
            continue
        for line in unbridged.run(["dyld_info", "-fixups", p]).splitlines():
            m = unbridged.ROW.match(line)
            if not m:
                continue
            _seg, sect, _a, dylib, sym = m.groups()
            if dylib == "libabiconv":
                if sym.startswith("___"):
                    out.add(("abiconv", sym[3:], tag))
                continue
            if dylib.startswith("<") or sect not in ("__la_symbol_ptr", "__jt_ptrs"):
                continue
            base = dylib if dylib.endswith(".dylib") else dylib + ".dylib"
            if base.lower() in translated or dylib.lower() in translated:
                continue
            out.add(("native", sym[1:] if sym.startswith("_") else sym, tag))
    return out


def is_vendored(root, path):
    """A framework/plugin shipped inside the bundle (golden QuickTime, Sparkle,
    ...) rather than the app's own code: its calls are often unreachable."""
    rel = os.path.relpath(path, root)
    return ".framework/" in rel or ".bundle/" in rel or ".plugin/" in rel


# ---- libabiconv side -------------------------------------------------------
def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    return re.sub(r"//[^\n]*", " ", src)


def c_functions(src):
    """name -> body (braces included) for every top-level shim_* definition."""
    out = {}
    for m in re.finditer(r"\b(shim_\w+)\s*\([^;{)]*\)\s*\{", src):
        i, depth = m.end() - 1, 0
        for j in range(i, len(src)):
            depth += (src[j] == "{") - (src[j] == "}")
            if depth == 0:
                out[m.group(1)] = src[i:j + 1]
                break
    return out


STUB_BODY = re.compile(r"^\{\s*((\(void\)\s*[\w\[\]]+|GAP_STUB\(\w+\))\s*;\s*)*(return\s+[^;()]*;)?\s*\}$")


def shim_facts():
    """(mtshim: ___X -> shim fn, stub fns, fn -> by-name natives it relies on)"""
    mt = {}
    for asm in glob.glob(os.path.join(ABICONV, "*.asm")):
        for m in re.finditer(r"^\s*MTSHIM\s+___(\w+)\s*,\s*_(\w+)", open(asm).read(), re.M):
            mt[m.group(1)] = m.group(2)
    stubs, byname = set(), {}
    for f in glob.glob(os.path.join(ABICONV, "*.c")) + glob.glob(os.path.join(ABICONV, "*.m")):
        src = strip_comments(open(f, errors="replace").read())
        # every native this file resolves by name, and the identifier that holds it
        names = {}
        for m in re.finditer(r"(\w+)\s*=\s*\([^;]*?dlsym\s*\([^,]+,\s*\"(\w+)\"", src):
            names[m.group(1)] = m.group(2)
        src_nodef = re.sub(r"^\s*#\s*define[^\n]*(\\\n[^\n]*)*", " ", src, flags=re.M)
        for m in re.finditer(r"\bdlsym\s*\([^,]+,\s*\"(\w+)\"", src_nodef):
            names.setdefault(m.group(1), m.group(1))
        for m in re.finditer(r"\bUIDL\(\s*(\w+)", src_nodef):
            names[m.group(1)] = m.group(1)
        for m in re.finditer(r"\bAGL_NATIVE\(\s*(\w+)\s*,\s*\w+\s*,\s*\"(\w+)\"", src):
            names[m.group(1)] = m.group(2)
        for m in re.finditer(r"\bR\(\s*(\w+)\s*\)", src):
            names["n_" + m.group(1)] = m.group(1)
        for fn, body in c_functions(src).items():
            if STUB_BODY.match(re.sub(r"\s+", " ", body)):
                stubs.add(fn)
            used = {nat for var, nat in names.items() if re.search(r"\b%s\b" % re.escape(var), body)}
            if used:
                byname[fn] = used
    return mt, stubs, byname


def load_ok():
    p = os.path.join(HERE, "coverage-audit.ok")
    ok = {}
    if os.path.exists(p):
        for l in open(p):
            l = l.split("#", 1)
            sym = l[0].strip()
            if sym:
                ok[sym] = l[1].strip() if len(l) > 1 else ""
    return ok


def reach(d):
    """Aggregate the gap.c ledgers: one row per (kind, symbol), ranked by how
    many processes hit it, with the apps and first callers seen."""
    procs, apps, callers = collections.Counter(), collections.defaultdict(set), collections.defaultdict(set)
    files = glob.glob(os.path.join(d, "*.txt"))
    for p in files:
        app, seen = os.path.basename(p).rsplit(".", 2)[0], set()
        for l in open(p, errors="replace"):
            f = l.split()
            if len(f) < 2:
                continue
            k = (f[0], f[1])
            procs[k] += k not in seen
            seen.add(k)
            apps[k].add(app)
            if len(f) > 2:
                callers[k].add(f[2])
    print(f"== reached gaps: {len(procs)} from {len(files)} process ledgers in {d}")
    for (kind, sym), n in sorted(procs.items(), key=lambda kv: (-len(apps[kv[0]]), -kv[1], kv[0])):
        k = (kind, sym)
        print(f"  {kind:5s} {sym:40s} {n:3d}  {','.join(sorted(apps[k]))[:40]:40s} {' '.join(sorted(callers[k])[:3])}")
    return 0


# ---- main ------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("roots", nargs="*")
    ap.add_argument("--lib", default=os.path.join(REPO, "build/src/abiconv/libabiconv.dylib"))
    ap.add_argument("--nulljump", default=os.path.join(REPO, "build/src/abiconv/libabiconv.nulljump"))
    ap.add_argument("--top", type=int, default=60)
    ap.add_argument("--all", action="store_true", help="list OK/RAW-data too")
    ap.add_argument("--reach", nargs="?", const=os.path.join(os.environ.get("TMPDIR", "/tmp"), "86x64-reach"),
                    help="rank the gaps real runs reached (gap.c ledger dir)")
    a = ap.parse_args()
    if a.reach:
        return reach(a.reach)
    roots = a.roots or [r for r in DEF_ROOTS if os.path.exists(r)]

    exported = {s[3:] for s in sh(["nm", "-gUj", a.lib]).split() if s.startswith("___")}
    nulljump = {l.strip().lstrip("_") for l in open(a.nulljump)} if os.path.exists(a.nulljump) else set()
    mt, stubs, byname = shim_facts()
    ok = load_ok()

    # target -> binds leaving the tree
    imports, natives, appcode = {}, set(), set()
    for root in roots:
        name = "Portal 2" if root.rstrip("/").endswith("osx64") else os.path.basename(root.rstrip("/"))
        bs = binds(root)
        imports[name] = {x for _k, x, _t in bs}
        natives |= {x for k, x, _t in bs if k == "native"}
        appcode |= {x for _k, x, t in bs if t == "a"}
        print(f"  scanned {name}: {len(imports[name])} outbound calls", file=sys.stderr)

    allsyms = set().union(*imports.values())
    shimnames = set().union(*byname.values()) if byname else set()
    # the probe takes nm-style names (it strips ONE leading underscore itself)
    asked = {"_" + n: n for n in natives | shimnames}
    got = resolves_on_x86_64(set(asked), system_deps(a.lib))
    live = {asked[g] for g in got if g in asked}

    def classify(s):
        if s in ok:
            return "OK", "reviewed: " + ok[s]
        if s in natives and s not in exported:
            return ("RAW", "") if s in live else ("MISSING", "")
        if s in nulljump:
            return "DEAD", ""
        fn = mt.get(s)
        if fn:
            # our own internal names (_86x64_*, x64_*) are not OS exports
            dead = sorted(n for n in byname.get(fn, ()) if n not in live and not n.startswith(("_86x64_", "x64_")))
            if dead:
                return "DEADNAME", fn + " -> " + ",".join(dead)
            if fn in stubs:
                return "STUB", fn
        return "OK", ""

    users = collections.defaultdict(list)
    for t, syms in imports.items():
        for s in syms:
            users[s].append(t)
    rows = [(s, *classify(s), sorted(users[s])) for s in allsyms]
    app = lambda s: s in appcode

    order = ["DEADNAME", "DEAD", "STUB", "MISSING", "RAW", "OK"]
    counts = collections.Counter(r[1] for r in rows)
    print("\n== totals: " + "  ".join(f"{k} {counts[k]}" for k in order))
    per = collections.defaultdict(collections.Counter)
    for s, cls, _d, ts in rows:
        for t in ts:
            per[t][cls] += 1
    print("\n== per target")
    for t in sorted(per):
        print(f"  {t:34s} " + "  ".join(f"{k} {per[t][k]:4d}" for k in order[:-1]))
    for cls in order[:-1]:
        sel = sorted((r for r in rows if r[1] == cls), key=lambda r: (not app(r[0]), -len(r[3]), r[0]))
        if not sel:
            continue
        napp = sum(1 for r in sel if app(r[0]))
        print(f"\n== {cls} ({len(sel)}; {napp} from app code, rest only from vendored frameworks)")
        for s, _c, det, ts in sel[:a.top]:
            who = ",".join(t.replace(".app", "") for t in ts[:4]) + ("…" if len(ts) > 4 else "")
            print(f"  {'A' if app(s) else 'v'} {s:44s} {len(ts):2d}  {who:40s} {det}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
