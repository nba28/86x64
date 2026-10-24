#!/bin/bash
#
# rebase_interior_test.sh — A/B guard: a rebased pointer into the middle of a
# read-only data word keeps its exact byte (rebase_info.cc containing-blob
# resolution; Python 2.6 grammar bitsets were moved up to 3 bytes).
#
# ON  = g_ptrs[0..2] read 0x28 0x30 0x47 (g_tab[1..3]).
# OFF = M64_NO_REBASE_INTERIOR=1 at translate time: at least one is moved.
set -u
cd "$(dirname "$0")"
ROOT="$(cd .. && pwd)"
IN=build/99_rebase_interior.dylib
[ -f "$IN" ] || { echo "rebase-interior: FAIL (no $IN)"; exit 1; }
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
"$ROOT/src/86x64/m64" translate "$IN" -o "$TMP/on.dylib" >"$TMP/on.log" 2>&1 & p1=$!
M64_NO_REBASE_INTERIOR=1 "$ROOT/src/86x64/m64" translate "$IN" -o "$TMP/off.dylib" >"$TMP/off.log" 2>&1 & p2=$!
wait $p1 || { echo "rebase-interior: FAIL (on translate)"; tail -3 "$TMP/on.log"; exit 1; }
wait $p2 || { echo "rebase-interior: FAIL (off translate)"; tail -3 "$TMP/off.log"; exit 1; }
words() {   # words <dylib> -> "r0 r1 r2 name_ok"
  python3 - "$1" <<'PY'
import subprocess, sys
p = sys.argv[1]
sym = {l.split()[2]: int(l.split()[0], 16) for l in subprocess.run(['nm', p], capture_output=True, text=True).stdout.splitlines() if len(l.split()) == 3}
o = subprocess.run(['otool', '-l', p], capture_output=True, text=True).stdout.split('\n')
secs = [(int(o[i+2].split()[1], 16), int(o[i+3].split()[1], 16), int(o[i+4].split()[1])) for i, l in enumerate(o) if l.strip().startswith('sectname')]
d = open(p, 'rb').read()
def rd(va, n):
    for a, sz, off in secs:
        if a <= va < a + sz: return d[off + va - a: off + va - a + n]
v = [int.from_bytes(rd(sym['_g_ptrs'] + 4 * k, 4), 'little') for k in range(3)]   # translated data keeps 4-byte pointers
print(*(rd(x, 1)[0] if rd(x, 1) else -1 for x in v))
PY
}
on="$(words "$TMP/on.dylib")"; off="$(words "$TMP/off.dylib")"
echo "  ON : $on"; echo "  OFF: $off"
[ "$on" = "40 48 71" ] || { echo "rebase-interior: FAIL (ON pointers moved)"; exit 1; }
[ "$off" != "40 48 71" ] || { echo "rebase-interior: FAIL (OFF did not reproduce the shift: inert guard)"; exit 1; }
echo "rebase-interior: PASS"
