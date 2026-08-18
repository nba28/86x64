#!/bin/bash
#
# halo-event-probe.sh — trace the Carbon EVENT path and find the link where a
# main-menu click is lost.
#
# ⚠DIAGNOSTIC ONLY. Touches nothing in libabiconv: DYLD_INSERT_LIBRARIES +
# __DATA,__interpose replaces the real Carbon entry points, which works because
# the abigen bridges reach Carbon through ordinary symbol stubs (the same trick
# halo-gl-texprobe.sh uses for OpenGL). Delete two files and it is gone.
#
# WHY THIS AND NOT MORE BUTTON WORK. The first reading — classic Button()
# polling stuck at 0 — was MEASURED AND FALSIFIED: Halo calls Button() exactly
# zero times. Its imports say the menu runs on the Carbon Event Manager, so
# this traces that chain rather than theorising about it again.
#
# NEVER run Halo under lldb (any-observer heisenbug). Never clear
# com.macsoft.halo prefs (hand-entered GameSpy key + EULA).
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
APP="${HALO_APP:-$HOME/projects/translations/Apps64/Halo.app}"
OUT="${1:-/tmp/halo-event-probe.log}"
DYLIB="${TMPDIR:-/tmp}/halo-event-probe.dylib"

if pgrep -x Halo >/dev/null; then
  echo "Halo is already running — quit it first (pkill -9 -x Halo)." >&2
  exit 1
fi

echo "building probe -> $DYLIB"
clang -arch x86_64 -dynamiclib -O1 -Wall -Wno-deprecated-declarations \
      -framework Carbon -o "$DYLIB" "$HERE/halo-event-probe.c" || {
  echo "probe build FAILED" >&2; exit 1; }

echo "Logging to: $OUT"
echo
echo "Click Play, then at the main menu:"
echo "  1. move the pointer over a few items (highlighting should still work)"
echo "  2. CLICK one — 'Campaign' or 'Settings'  (click a couple of times)"
echo "  3. quit Halo"
echo

env DYLD_INSERT_LIBRARIES="$DYLIB" HALO_EVENT_LOG="$OUT" \
  "$APP/Contents/MacOS/Halo" >"$OUT.app" 2>&1

echo
echo "===== result ====="
if [ ! -s "$OUT" ]; then
  echo "NO PROBE OUTPUT AT ALL — the dylib never loaded."
  echo "  DYLD_INSERT_LIBRARIES was ignored (library validation / hardened"
  echo "  runtime), so this says NOTHING about the event path. Do not read it"
  echo "  as a result."
  exit 0
fi

grep -E '^\[ev\] (InstallEventHandler|ReceiveNextEvent|SendEventToEventTarget|CallNextEventHandler)' "$OUT" | head -30
echo
sed -n '/===== summary/,$p' "$OUT"

echo
echo "--- where the chain breaks ---"
inst=$(grep -c 'InstallEventHandler  target' "$OUT" 2>/dev/null); inst=${inst:-0}
down=$(sed -n 's/.*mouse DOWN received *: *//p' "$OUT" | tail -1); down=${down:-0}
hand=$(sed -n 's/.*app handler ran on a down *: *//p' "$OUT" | tail -1); hand=${hand:-0}
if [ "$inst" = "0" ]; then
  echo "  NO handler was ever installed ⇒ the break is BEFORE the event system."
  echo "  Look at NewEventHandlerUPP wrapping, or at Halo bailing out earlier."
elif [ "$down" = "0" ]; then
  echo "  Handlers installed ($inst types) but NO mouse-DOWN ever arrived."
  echo "  ⇒ ReceiveNextEvent is not producing them. Either the app is not"
  echo "   pumping the queue, or the press is going somewhere else entirely"
  echo "   (a captured display / a different event target)."
elif [ "$hand" = "0" ]; then
  echo "  Mouse-DOWN events ARRIVED ($down) but the app's handler never ran on"
  echo "  one. ⇒ dispatch is dropping them: wrong target, or the registered"
  echo "   event types do not include this class/kind."
else
  echo "  The click REACHES translated Halo code ($hand handler runs on a"
  echo "  mouse-down). ⇒ the plumbing is fine and the click is lost INSIDE the"
  echo "  game. Next: compare the GetEventParameter(MouseLocation) coordinates"
  echo "  above against where the pointer actually was — highlight (GetMouse,"
  echo "  port-aware) and click (event coords) come from DIFFERENT sources, and"
  echo "  a hit-test cannot succeed if they disagree."
fi
echo
echo "  ⚠If no mouse lines appear at all, check you actually clicked while Halo"
echo "   was frontmost — an empty trace from a run with no click proves nothing."
