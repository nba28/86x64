#!/bin/bash
# inplace_symlink_guard_test.sh — regression guard for the m64 in-place-symlink
# DATA-LOSS guard (src/86x64/m64: _inplace_symlink_escape / cmd_translate).
#
# The bug it prevents: `Apps32/Civilization IV (Steam).app` and `Apps32/Portal 2`
# are SYMLINKS into the live Steam install. `m64 translate <symlink>` WITHOUT
# `-o` translated the binaries IN PLACE, writing THROUGH the link and DESTROYING
# the read-only i386 source (the tester had to Steam "Verify integrity of game files"
# to recover — and even then only the main exec came back). The protocol is to
# ALWAYS translate to a copy with `-o Apps64`; this guard makes the tool ENFORCE
# it: an in-place translate that would escape through a symlink to a different
# tree is refused, with a message pointing at `-o`.
#
# Asserts, WITHOUT needing the i386 sysroot:
#   1. UNIT (the pure detector, imported from the real m64 module):
#      a. a symlink that jumps to a DIFFERENT tree (Apps32/<app> -> Steam) is
#         flagged as an escape;
#      b. a real directory with no symlink in its path is NOT flagged;
#      c. an INTRA-bundle link (framework Versions/Current -> A, target stays
#         inside its own parent) is NOT flagged  [no false positive];
#      d. a macOS firmlink ancestor (/tmp, /var, /etc) is NOT flagged.
#   2. END-TO-END (if the built artifacts are present): `m64 translate` of an
#      escaping-symlink bundle WITHOUT -o exits nonzero with the refusal, while
#      the SAME target WITH -o is NOT refused by the symlink guard.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ_ROOT="$(cd "$HERE/.." && pwd)"
M64="$PROJ_ROOT/src/86x64/m64"

[ -f "$M64" ] || { echo "m64 not found — SKIP"; exit 0; }

fail=0

# ---- 1: unit-test the pure detector against the real m64 module. Imports m64
# as a module (its CLI is guarded behind __main__), so we call the exact
# production function cmd_translate uses — no reimplementation to drift.
python3 - "$M64" <<'PY' || fail=1
import importlib.util, os, sys, tempfile
from importlib.machinery import SourceFileLoader
m64_path = sys.argv[1]
loader = SourceFileLoader("m64drv", m64_path)     # extension-less: explicit loader
spec = importlib.util.spec_from_file_location("m64drv", m64_path, loader=loader)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
f = m._inplace_symlink_escape

ok = True
d = tempfile.mkdtemp()

# (a) ESCAPE: a symlink whose real target lands in a different subtree.
os.makedirs(f"{d}/realtree/App.app")
os.makedirs(f"{d}/linkdir")
os.symlink(f"{d}/realtree/App.app", f"{d}/linkdir/App.app")
r = f(f"{d}/linkdir/App.app")
if not r:
    print("FAIL(a): escaping symlink App.app -> ../realtree not flagged"); ok = False

# (b) NO symlink in path -> None (the common Apps64 real-dir case).
os.makedirs(f"{d}/plain/Real.app")
if f(f"{d}/plain/Real.app") is not None:
    print("FAIL(b): a plain real directory was flagged"); ok = False

# (c) INTRA-bundle link: framework Versions/Current -> A (target stays inside
# its own parent) must NOT be flagged — else in-place work on a framework's
# Current alias would be wrongly refused.
os.makedirs(f"{d}/Foo.framework/Versions/A")
os.symlink("A", f"{d}/Foo.framework/Versions/Current")
open(f"{d}/Foo.framework/Versions/A/Foo", "w").close()
if f(f"{d}/Foo.framework/Versions/Current/Foo") is not None:
    print("FAIL(c): Versions/Current -> A falsely flagged (intra-bundle link)"); ok = False

# (d) macOS firmlink ancestor is not an escape.
if f("/tmp") is not None:
    print("FAIL(d): /tmp firmlink flagged as an escape"); ok = False

print("detector unit checks:", "PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
PY

# ---- 2: end-to-end. Requires the artifacts cmd_translate's require() checks.
# Skip gracefully if the tree isn't built (unit leg already proved the logic).
MT="$PROJ_ROOT/build/src/macho-tool/macho-tool"
LIBAB="$PROJ_ROOT/build/src/abiconv/libabiconv.dylib"
TB="$PROJ_ROOT/src/86x64/translate-bundle"
if [ -x "$MT" ] && [ -f "$LIBAB" ] && [ -f "$TB" ]; then
  tmp="$(mktemp -d)"
  mkdir -p "$tmp/realtree/Escaper.app/Contents/MacOS"
  # a non-empty file so the .app looks like a bundle; the guard fires before any
  # Mach-O is ever read, so its contents don't matter.
  : > "$tmp/realtree/Escaper.app/Contents/MacOS/Escaper"
  mkdir -p "$tmp/linkdir"
  ln -s "$tmp/realtree/Escaper.app" "$tmp/linkdir/Escaper.app"

  out="$(python3 "$M64" translate "$tmp/linkdir/Escaper.app" -n 2>&1)"; rc=$?
  if [ "$rc" -ne 0 ] && printf '%s' "$out" | grep -q "refusing to translate in place"; then
    echo "e2e: in-place through symlink REFUSED (exit $rc) — PASS"
  else
    echo "FAIL: in-place translate of an escaping symlink was NOT refused"
    echo "      (exit=$rc); output was:"; printf '%s\n' "$out" | sed 's/^/        /'
    fail=1
  fi

  # With -o, the symlink guard must NOT fire (the copy path is safe). It may
  # still fail later on the fake bundle — we only assert it is not the refusal.
  out2="$(python3 "$M64" translate "$tmp/linkdir/Escaper.app" -o "$tmp/o" -n 2>&1)"
  if printf '%s' "$out2" | grep -q "refusing to translate in place"; then
    echo "FAIL: -o output mode was wrongly refused by the symlink guard"; fail=1
  else
    echo "e2e: -o output mode NOT refused by the symlink guard — PASS"
  fi
  rm -rf "$tmp"
else
  echo "artifacts not built — skipping end-to-end leg (unit leg covers the logic)"
fi

if [ "$fail" = 0 ]; then echo "inplace_symlink_guard_test: PASS"; exit 0; fi
echo "inplace_symlink_guard_test: FAIL"; exit 1
