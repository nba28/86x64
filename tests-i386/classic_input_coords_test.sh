#!/bin/bash
#
# classic_input_coords_test.sh — A/B guard proving classic_input_coords.c is what
# restores the classic GLOBAL-vs-LOCAL coordinate contract.
#
# GetMouse is defined against the CURRENT PORT and GetGlobalMouse against the
# SCREEN; GlobalToLocal/LocalToGlobal convert between them. Our old
# implementations answered GLOBAL to both mouse calls and made both conversions
# identity no-ops — correct only for an origin-(0,0) offscreen port, wrong for
# any real on-screen window. An app that hit-tests its own menu with GetMouse
# then compares a screen point against window-local item rects and finds the
# pointer inside nothing: the cursor moves and nothing is clickable.
#
# Both arms are asserted POSITIVELY, so neither can pass by accident:
#   ON  : l2g_global=1  l2g_identity=0  roundtrip=1  mouse_local=1  mouse_identity=0  port_wins=1
#   OFF : l2g_global=0  l2g_identity=1  roundtrip=1  mouse_local=0  mouse_identity=1
# (roundtrip holds in both arms — identity is trivially its own inverse. It is
# asserted to catch a one-sided conversion, not to separate the arms.)
#
# The fixture briefly shows a 400x300 Carbon window. It takes no input, but do
# not run the suite while a GUI target is under live test.
#
# Needs the i386 sysroot; SKIPs when the binary has not been built.
set -u
cd "$(dirname "$0")"

BIN=build/99_classic_input_coords.x86_64
if [ ! -x "$BIN" ]; then
  echo "classic-input-coords: SKIP (build/99_classic_input_coords.x86_64 missing;"
  echo "                            run \`make classic-input-coords\` first)"
  exit 0
fi

fail=0
has() { printf '%s\n' "$1" | grep -q "^$2\$"; }

check() {   # check <label> <output> <rc> <expected k=v ...>
  local label=$1 out=$2 rc=$3; shift 3
  local missing=""
  local k
  for k in "$@"; do has "$out" "$k" || missing="$missing $k"; done
  if [ "$rc" = 0 ] && [ -z "$missing" ]; then
    return 0
  fi
  echo "  $label rc=$rc, missing/failed:$missing"
  printf '%s\n' "$out" | sed 's/^/      /'
  return 1
}

on=$("$BIN" 2>/dev/null); on_rc=$?

# A window that landed at the screen origin makes identity and correctness the
# same answer — refuse to report a verdict rather than pass on an ambiguity.
if ! has "$on" 'window=1'; then
  echo "  ON  (port-aware):     could not create/measure a Carbon window, so the"
  echo "                        coordinate contract cannot be exercised at all."
  printf '%s\n' "$on" | sed 's/^/      /'
  fail=1
elif has "$on" 'origin_nonzero=0'; then
  echo "  ON  (port-aware):     the window landed at content origin (0,0), where"
  echo "                        identity and correct conversion are the same"
  echo "                        answer. This guard did NOT exercise the fix."
  printf '%s\n' "$on" | sed 's/^/      /'
  fail=1
elif has "$on" 'mouse_stable=0'; then
  echo "  ON  (port-aware):     the cursor moved through all 8 sample attempts, so"
  echo "                        the global/local mouse pair is not comparable."
  printf '%s\n' "$on" | sed 's/^/      /'
  fail=1
elif check "ON  (port-aware):" "$on" "$on_rc" \
        'l2g_global=1' 'l2g_identity=0' 'roundtrip=1' \
        'mouse_local=1' 'mouse_identity=0' 'port_wins=1' 'done=1'; then
  org=$(printf '%s\n' "$on" | sed -n 's/^origin_h=//p'),$(printf '%s\n' "$on" | sed -n 's/^origin_v=//p')
  echo "  ON  (port-aware):     content origin ($org): LocalToGlobal((0,0)) lands on"
  echo "                        it, GlobalToLocal inverts it exactly, and"
  echo "                        GetGlobalMouse - GetMouse equals it; with ANOTHER"
  echo "                        window active, the current port still decides     OK"
else
  fail=1
fi

off=$(M64_NO_CLASSIC_INPUT_FIX=1 "$BIN" 2>/dev/null); off_rc=$?
if check "OFF (kill switch):" "$off" "$off_rc" \
        'l2g_global=0' 'l2g_identity=1' 'roundtrip=1' \
        'mouse_local=0' 'mouse_identity=1' 'port_wins=0' 'done=1'; then
  echo "  OFF (kill switch):    the old stubs: LocalToGlobal leaves (0,0) alone and"
  echo "                        GetMouse returns the SCREEN position — a menu"
  echo "                        hit-test would find the pointer inside nothing    OK"
else
  echo "                        (expected the identity behaviour to survive the"
  echo "                         kill switch; if it did not, this guard is not"
  echo "                         exercising classic_input_coords.c)"
  fail=1
fi

if [ "$fail" = 0 ]; then
  echo "classic-input-coords: PASS"
else
  echo "classic-input-coords: FAIL"
fi
exit $fail
