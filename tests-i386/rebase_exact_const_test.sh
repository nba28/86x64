#!/bin/bash
#
# rebase_exact_const_test.sh — A/B guard: in a slidable image with an
# LC_DYLD_INFO rebase stream, a data word with no rebase is a constant however
# well its value aliases the image (section.cc DataParser; Miles MP3 provider's
# minimp3 g_hz {44100, 48000, 32000} were "rebased" and no MP3 frame synced).
#
# ON  = g_rates keeps 44100/48000/32000 and rate_name (rebased) still points at
#       its string.
# OFF = M64_NO_REBASE_EXACT=1 at translate time: at least one rate is rewritten.
set -u
cd "$(dirname "$0")"
ROOT="$(cd .. && pwd)"
IN=build/99_rebase_exact_const.dylib
[ -f "$IN" ] || { echo "rebase-exact-const: FAIL (no $IN)"; exit 1; }
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
"$ROOT/src/86x64/m64" translate "$IN" -o "$TMP/on.dylib" >"$TMP/on.log" 2>&1 & p1=$!
M64_NO_REBASE_EXACT=1 "$ROOT/src/86x64/m64" translate "$IN" -o "$TMP/off.dylib" >"$TMP/off.log" 2>&1 & p2=$!
wait $p1 || { echo "rebase-exact-const: FAIL (on translate)"; tail -3 "$TMP/on.log"; exit 1; }
wait $p2 || { echo "rebase-exact-const: FAIL (off translate)"; tail -3 "$TMP/off.log"; exit 1; }
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
r = [int.from_bytes(rd(sym['_g_rates'] + 4 * k, 4), 'little') for k in range(3)]
nv = int.from_bytes(rd(sym['_rate_name'], 4), 'little')   # translated data keeps 4-byte pointers
s = rd(nv, 5)
print(*r, int(s == b'rates'))
PY
}
on="$(words "$TMP/on.dylib")"; off="$(words "$TMP/off.dylib")"
echo "  ON : $on"; echo "  OFF: $off"
case "$on" in "44100 48000 32000 1") ;; *) echo "rebase-exact-const: FAIL (ON altered the constants or lost the pointer)"; exit 1;; esac
[ "${off% *}" != "44100 48000 32000" ] || { echo "rebase-exact-const: FAIL (OFF did not reproduce the rewrite: inert guard)"; exit 1; }
echo "rebase-exact-const: PASS"
