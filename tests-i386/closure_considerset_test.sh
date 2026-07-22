#!/bin/bash
# closure_considerset_test.sh — regression guard for the CO-TRANSLATED-CLOSURE
# import discovery (src/86x64/_i386_closure.py + abigen_modern_manifest.py).
#
# The bug it prevents: the abigen consider-set discovery read ONLY each target's
# MAIN EXECUTABLE imports. Any app whose code lives in companion binaries
# contributed none of their import surface — iWork '09's thin main exes link 15+
# SF*.framework classic dylibs installed at a DEAD absolute path
# ("/Library/Application Support/iWork '09/Frameworks/…", vendored from the
# source pool at translate time); SFTabular alone imports 31 AddressBook kAB*
# NSString-const data symbols the main exe never mentions. Unseen symbols got no
# shim/data-shadow, the bind stayed on the native >4GB framework, and the
# translated i386 4-byte load truncated the address -> SFTabular static-init
# crash (Numbers). The fix expands every --target to its co-translated
# dependency closure (bundle-embedded + source-pool + @loader_path siblings),
# resolved the same way _vendor_deps.py vendors.
#
# Hermetic (no i386 sysroot, no app bundles): x86_64 fixtures + the production
# modules. Asserts:
#   1. _vendor_deps.is_macho recognizes THIN 32-bit Mach-O magics (CE FA ED FE /
#      FE ED FA CE) — previously missing, so thin-i386 companions (Source-engine
#      @loader_path siblings, iLife-embedded thin frameworks) were invisible to
#      both vendoring and closure discovery.
#   2. _i386_closure.expand_target resolves a DEAD-absolute-install-name
#      framework dep through a --source pool (the SFTabular shape).
#   3. abigen_modern_manifest.py end-to-end: the consider set contains a symbol
#      imported ONLY by the pool-resolved companion framework (RED with the old
#      main-exe-only discovery).
#   4. the AddressBook umbrella mapping exists (kAB* header decls reach abigen).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ_ROOT="$(cd "$HERE/.." && pwd)"
SRC86="$PROJ_ROOT/src/86x64"

[ -f "$SRC86/_i386_closure.py" ] || { echo "FAIL closure-considerset: _i386_closure.py missing"; exit 1; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# ---- fixtures: a "main exe" linking a framework by DEAD absolute install name;
# the framework's original lives only in a scratch source POOL and imports a
# marker symbol (regcomp) the exe itself never references.
DEADNAME="/Library/Application Support/FakeWork86x64/Frameworks/ClosureFakeDep.framework/Versions/A/ClosureFakeDep"
POOL="$TMP/pool"
FWBIN="$POOL/ClosureFakeDep.framework/Versions/A/ClosureFakeDep"
mkdir -p "$(dirname "$FWBIN")"

cat > "$TMP/dep.c" <<'EOF'
#include <regex.h>
int closure_fake_entry(void) { regex_t re; return regcomp(&re, "x", 0); }
EOF
cat > "$TMP/main.c" <<'EOF'
extern int closure_fake_entry(void);
int main(void) { return closure_fake_entry(); }
EOF

clang -arch x86_64 -dynamiclib -install_name "$DEADNAME" \
      -o "$FWBIN" "$TMP/dep.c" 2> "$TMP/cc1.err" \
   || { echo "SKIP closure-considerset (cannot build x86_64 fixture dylib)"; exit 0; }
clang -arch x86_64 -o "$TMP/main" "$TMP/main.c" "$FWBIN" 2> "$TMP/cc2.err" \
   || { echo "SKIP closure-considerset (cannot build x86_64 fixture exe)"; exit 0; }

fail=0
note() { echo "FAIL closure-considerset: $1"; fail=1; }

# ---- 1+2+4: python unit assertions against the production modules
python3 - "$SRC86" "$TMP/main" "$POOL" <<'PY' || fail=1
import sys, os, struct, tempfile
src86, exe, pool = sys.argv[1], sys.argv[2], sys.argv[3]
sys.path.insert(0, src86)
import _vendor_deps, _i386_closure, importlib
# abigen_modern_manifest: underscore-free, plain import works
sys.path.insert(0, src86)
amm = importlib.import_module("abigen_modern_manifest")

ok = True

# (1) thin 32-bit Mach-O magics
with tempfile.TemporaryDirectory() as d:
    for name, magic, want in (
            ("thin32_le", b"\xce\xfa\xed\xfe", True),
            ("thin32_be", b"\xfe\xed\xfa\xce", True),
            ("thin64_le", b"\xcf\xfa\xed\xfe", True),
            ("fat_be",    b"\xca\xfe\xba\xbe", True),
            ("not_macho", b"#!/b",             False)):
        p = os.path.join(d, name)
        with open(p, "wb") as f:
            f.write(magic + b"\0" * 12)
        got = _vendor_deps.is_macho(p)
        if got != want:
            print("FAIL closure-considerset: is_macho(%s)=%s, want %s"
                  % (name, got, want))
            ok = False

# (2) closure resolves the dead-absolute dep through the --source pool
bins, skipped = _i386_closure.expand_target(exe, arch="x86_64", sources=[pool])
names = [os.path.basename(str(b)) for b in bins]
if "ClosureFakeDep" not in names:
    print("FAIL closure-considerset: pool companion not in closure: %s (skipped=%s)"
          % (names, skipped))
    ok = False

# (4) AddressBook umbrella mapping (kAB* decls must reach abigen)
if "AddressBook" not in amm.UMBRELLA:
    print("FAIL closure-considerset: no AddressBook umbrella mapping")
    ok = False

sys.exit(0 if ok else 1)
PY

# ---- 3: end-to-end manifest run — marker symbol from the companion only
python3 "$SRC86/abigen_modern_manifest.py" \
        --target "$TMP/main" --source "$POOL" --arch x86_64 \
        --out-syms "$TMP/out.syms" --out-includes "$TMP/out.h" \
        > "$TMP/mm.out" 2> "$TMP/mm.err" \
   || { note "abigen_modern_manifest.py exited nonzero"; cat "$TMP/mm.err"; }
if [ -f "$TMP/out.syms" ]; then
   grep -q '^_regcomp$' "$TMP/out.syms" \
      || note "companion-only import _regcomp missing from consider set (main-exe-only discovery regression)"
fi

[ $fail = 0 ] && echo "PASS closure-considerset"
exit $fail
