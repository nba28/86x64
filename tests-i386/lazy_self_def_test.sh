#!/bin/bash
#
# lazy_self_def_test.sh — A/B guard: in a CLASSIC i386 image (no LC_DYLD_INFO),
# a LAZY symbol pointer to a symbol the image DEFINES ITSELF must hold that
# definition after translation, not its stub_helper.
#
# Classic dyld bound such a slot on first call through the image's own
# `dyld_stub_binding_helper` + __dyld section; modern dyld never fills __dyld,
# so the first call jumped to 0. Portal 2 client.dylib died in its static init
# this way (std::allocator<std::string>'s copy ctor, a weak template instance,
# 534 such slots in client and in server). Route C synthesis now points the slot
# at the definition (archive.cc synthesize_dyld_info).
#
# ld64-95 cannot emit this layout (__symbol_stub + __la_symbol_ptr +
# __stub_helper without LC_DYLD_INFO), so like synth-dyld-info this checks a
# REAL classic dylib BY CONTENT: Portal 2's libsteam.dylib by default
# (LAZY_SELF_DEF_DYLIB overrides), skipped if absent.
#
# ON  = every self-defined lazy slot == the symbol's translated address.
# OFF = M64_NO_LAZY_SELF_DEF=1 at translate time: they point into __stub_helper.
set -u
cd "$(dirname "$0")"
IN="${LAZY_SELF_DEF_DYLIB:-$HOME/Library/Application Support/Steam/steamapps/common/Portal 2/bin/osx32/libsteam.dylib}"
[ -f "$IN" ] || { echo "lazy-self-def: SKIP (no classic input at $IN)"; exit 0; }
ROOT="$(cd .. && pwd)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT

check() {   # check <translated> -> prints "ok=N bad=M"
  python3 - "$IN" "$1" <<'EOF'
import subprocess, struct, sys
orig, trans = sys.argv[1], sys.argv[2]
run = lambda *a: subprocess.run(a, capture_output=True, text=True).stdout
defined = {l.split()[2] for l in run('nm', '-U', orig).splitlines() if len(l.split()) == 3}
names, on = [], False
for l in run('otool', '-Iv', orig).splitlines():
    if l.startswith('Indirect symbols for'):
        on = '__la_symbol_ptr' in l; continue
    p = l.split()
    if on and len(p) == 3 and p[0].startswith('0x'): names.append(p[2])
taddr = {l.split()[2]: int(l.split()[0], 16) for l in run('nm', trans).splitlines() if len(l.split()) == 3}
data = open(trans, 'rb').read()
sect = run('otool', '-l', trans).split()
i = sect.index('__la_symbol_ptr'); fo = int(sect[sect.index('offset', i) + 1])
ok = bad = 0
for k, n in enumerate(names):
    if n not in defined or n not in taddr: continue
    v = struct.unpack_from('<Q', data, fo + 8 * k)[0]
    if v == taddr[n]: ok += 1
    else: bad += 1
print(f'ok={ok} bad={bad}')
EOF
}

# both arms translate in parallel (each is a full dylib translate)
"$ROOT/src/86x64/m64" translate "$IN" -o "$TMP/on.dylib" >"$TMP/on.log" 2>&1 & p_on=$!
M64_NO_LAZY_SELF_DEF=1 "$ROOT/src/86x64/m64" translate "$IN" -o "$TMP/off.dylib" >"$TMP/off.log" 2>&1 & p_off=$!
wait $p_on  || { echo "lazy-self-def: FAIL (on translate)";  tail -5 "$TMP/on.log";  exit 1; }
wait $p_off || { echo "lazy-self-def: FAIL (off translate)"; tail -5 "$TMP/off.log"; exit 1; }
r_on="$(check "$TMP/on.dylib")"; r_off="$(check "$TMP/off.dylib")"
echo "  ON : $r_on"; echo "  OFF: $r_off"
case "$r_on $r_off" in
  "ok="[1-9]*" bad=0 ok=0 bad="[1-9]*) echo "lazy-self-def: PASS"; exit 0;;
esac
echo "lazy-self-def: FAIL (ON must be all ok, OFF all bad)"; exit 1
