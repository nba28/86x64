#!/bin/bash
#
# halo-input-probe.sh — why do the main-menu items HIGHLIGHT but not respond to
# a click?
#
# THE READING BEING TESTED. classic_input_coords.c already documents the classic
# click idiom:
#
#     if (Button()) { while (StillDown()) { track } }   // ... then act
#
# and it fixed the StillDown half — but only that half. Button() and its Carbon
# successor GetCurrentEventButtonState() were never hand-shimmed; they were
# plain abigen ABI bridges straight to HIToolbox, the ONLY classic input entry
# points left that way (GetMouse, GetGlobalMouse, GetKeys, GlobalToLocal,
# LocalToGlobal, StillDown and WaitMouseUp are all shimmed). If HIToolbox
# answers 0 in this context, the guard never opens, the tracking loop never
# runs, and nothing is ever clicked — while the pointer still moves and items
# still HIGHLIGHT, because highlighting is driven by GetMouse polling, which
# works. That is exactly the reported symptom.
#
# This run MEASURES AND CURES AT THE SAME TIME. The shims ask HIToolbox FIRST
# and OR in the physical button state, and the trace prints both answers, so the
# log says which one actually carried the click rather than leaving it to
# inference:
#     native=0 cg=1   -> HIToolbox was the problem and the fix supplied the click
#     native=1 cg=1   -> HIToolbox was fine; the click was never the issue
#     no lines at all -> Halo does not poll Button() and the menu is driven by
#                        Carbon events instead; a completely different path
#
# NEVER run Halo under lldb (any-observer heisenbug). Never clear
# com.macsoft.halo prefs (hand-entered GameSpy key + EULA).
. "$(dirname "$0")/../../paths.sh"   # M64_* local paths
set -u

APP="${HALO_APP:-$M64_APPS64/Halo.app}"
OUT="${1:-/tmp/halo-input-probe.log}"

if pgrep -x Halo >/dev/null; then
  echo "Halo is already running — quit it first (pkill -9 -x Halo)." >&2
  exit 1
fi

OFF=""
if [ "${HALO_INPUT_OFF:-0}" = "1" ]; then
  OFF="M64_NO_CLASSIC_INPUT_FIX=1"
  echo "⚠ kill switch ON: the classic input fix is DISABLED for this run"
  echo "  (control arm — the menu is expected to stay unclickable)"
fi

echo "Logging to: $OUT"
echo
echo "Click Play, then at the main menu:"
echo "  1. move the pointer over a few items (confirms highlighting still works)"
echo "  2. CLICK one — 'Campaign' or 'Settings'"
echo "  3. quit Halo"
echo

env ABICONV_INPUT_TRACE=1 $OFF "$APP/Contents/MacOS/Halo" >"$OUT" 2>&1

echo
echo "===== result ====="
if ! grep -q '^\[input\]' "$OUT"; then
  echo "NO [input] LINES AT ALL."
  echo "  Halo never called Button() or GetCurrentEventButtonState(). That is a"
  echo "  RESULT, not a failure: the menu is not driven by classic button"
  echo "  polling, so this whole reading is wrong and the click must arrive"
  echo "  through the Carbon event handlers Halo installs (it imports"
  echo "  AddEventTypesToHandler / GetEventParameter / ConvertEventRefToEventRecord)."
  echo "  Next step would be to trace THAT path instead."
  exit 0
fi

printf 'Button calls traced                  %s\n' "$(grep -c 'Button  ' "$OUT")"
printf 'GetCurrentEventButtonState traced    %s\n' "$(grep -c 'GetCurrentEventButtonState' "$OUT")"
echo
echo "--- every traced line (first 5 calls, then state CHANGES only) ---"
grep '^\[input\]' "$OUT" | sed 's/^\[input\] /  /' | head -40

echo
echo "--- verdict ---"
if grep -q 'PHYSICAL BUTTON DOWN, HIToolbox did NOT report it' "$OUT"; then
  echo "  ★HIToolbox reported the button as UP while it was physically DOWN."
  echo "   That is the defect, measured directly: Button()/GetCurrentEventButtonState()"
  echo "   as raw bridges could never open the click guard. The shim supplied the"
  echo "   real state, so IF THE MENU RESPONDED, this was the root cause."
elif grep -qE 'native=0x1|native=1' "$OUT"; then
  echo "  HIToolbox DID report the press (native was 1 at some point)."
  echo "  ⇒ Button() was never the blocker; the click is being lost somewhere"
  echo "   after it. Do not credit this fix for any change you see."
else
  echo "  The button was polled but never observed DOWN by either source."
  echo "  ⇒ either the click did not land while Halo was frontmost, or the"
  echo "   press is being consumed before it reaches any of these paths."
fi
echo
echo "  ⚠THE REAL VERDICT IS WHAT YOU SAW: did clicking a menu item DO anything?"
echo "   The log explains the mechanism; only the screen decides the outcome."
echo "   Control arm: HALO_INPUT_OFF=1 bash src/86x64/probes/halo/halo-input-probe.sh"
