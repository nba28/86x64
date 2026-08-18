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

# --- SELF-TEST BEFORE SPENDING A HUMAN RUN -------------------------------
# The first version of this probe reached Carbon with dlsym(RTLD_NEXT,...),
# which returned the probe's OWN replacement: every call re-entered itself,
# 10800 log lines with ZERO completions, stack overflow, and it took Halo down.
# It also *looked* like a finding ("Halo installs 10800 handlers") until the
# missing return lines gave it away. A probe is a measuring instrument and gets
# calibrated before use: this runs it against a trivial Carbon program first and
# REFUSES to launch Halo unless one call produces exactly one entry and one
# return.
SELF="${TMPDIR:-/tmp}/halo-event-selftest"
cat > "$SELF.c" <<'CEOF'
#include <Carbon/Carbon.h>
#include <stdio.h>
static OSStatus h(EventHandlerCallRef r, EventRef e, void *u) {
   (void)r; (void)e; (void)u; return eventNotHandledErr;
}
int main(void) {
   EventTypeSpec t = { kEventClassCommand, kEventCommandProcess };
   EventHandlerRef ref = NULL;
   InstallEventHandler(GetApplicationEventTarget(), NewEventHandlerUPP(h),
                       1, &t, NULL, &ref);
   return 0;
}
CEOF
clang -arch x86_64 -Wno-deprecated-declarations -framework Carbon \
      -o "$SELF" "$SELF.c" 2>/dev/null || { echo "self-test build FAILED" >&2; exit 1; }
if ! DYLD_INSERT_LIBRARIES="$DYLIB" HALO_EVENT_LOG="$SELF.log" "$SELF" 2>/dev/null; then
  echo "SELF-TEST CRASHED — not launching Halo." >&2; exit 1
fi
ent=$(grep -c 'InstallEventHandler #1 target' "$SELF.log" 2>/dev/null); ent=${ent:-0}
ret=$(grep -c 'InstallEventHandler #1 ->'     "$SELF.log" 2>/dev/null); ret=${ret:-0}
if [ "$ent" != "1" ] || [ "$ret" != "1" ]; then
  echo "SELF-TEST FAILED: $ent entries / $ret returns for ONE call." >&2
  echo "  The call-through is not reaching Carbon (it is recursing, or the" >&2
  echo "  interpose table is inert). NOT launching Halo." >&2
  exit 1
fi
echo "self-test: 1 call -> 1 entry, 1 return; interposition live  OK"

echo "Logging to: $OUT"
echo
echo "Click Play, then at the main menu:"
echo "  1. move the pointer over a few items (highlighting should still work)"
echo "  2. CLICK one — 'Campaign' or 'Settings'  (click a couple of times)"
echo "  3. quit Halo"
echo

# ABICONV_INPUT_TRACE also on: the classic Button/GetCurrentEventButtonState
# counters land in Halo's own stderr ($OUT.app) and answer, in the SAME run,
# whether that path is polled at all and whether it ever sees a press.
env DYLD_INSERT_LIBRARIES="$DYLIB" HALO_EVENT_LOG="$OUT" ABICONV_INPUT_TRACE=1 ABICONV_KEYS_TRACE=1 \
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

grep -E '^\[ev\] (InstallEventHandler #|ReceiveNextEvent REQUEST|  requested type|>>>|<<<|Convert|WaitNextEvent ->)' "$OUT" | head -40
echo
echo "--- mouse events (first 8) ---"
grep -E 'MOUSE (DOWN|UP)' "$OUT" | head -8
echo "  DOWN total: $(grep -c 'MOUSE DOWN' "$OUT")   UP total: $(grep -c 'MOUSE UP' "$OUT")"
echo
sed -n '/===== summary/,$p' "$OUT"

echo
echo "--- ★Multiprocessing: does a worker wait STALL? (report: ~1s freeze per selection) ---"
if grep -q '^\[mp\]' "$OUT"; then
  grep -E '^\[mp\] (MPCreate|MP waits|MPCreateTask calls|⚠)' "$OUT" | head -14 | sed 's/^/  /'
  echo "  --- waits that BLOCKED ---"
  grep 'BLOCKED' "$OUT" | head -8 | sed 's/^/  /'
  [ "$(grep -c 'BLOCKED' "$OUT")" = "0" ] && echo "  (none — MP waits are not the stall)"
else
  echo "  no [mp] lines — Halo never used Multiprocessing Services this run"
fi
echo
echo "--- classic button-state path (from Halo's stderr) ---"
if grep -q '^\[input\]' "$OUT.app" 2>/dev/null; then
  grep -E '^\[input\] (=====|Button |GetCurrentEventButtonState |⚠)' "$OUT.app" | sed 's/^/  /'
else
  echo "  no [input] lines — the classic button path was never called at all"
fi
echo
echo "--- GetKeys polling (does Halo read the keyboard this way at all?) ---"
if grep -q '^\[keys\]' "$OUT.app" 2>/dev/null; then
  grep -E '^\[keys\]' "$OUT.app" | tail -8 | sed 's/^/  /'
else
  echo "  no [keys] lines — GetKeys was never called"
fi
echo
echo "--- which target are Halo's handlers on? ---"
grep -E 'target 0x[0-9a-f]* is:' "$OUT" | head -6 | sed 's/^/  /'
echo
echo "--- keyboard EVENTS (report: ENTER on a menu item does nothing either) ---"
grep -E 'KEY kind=|KEYBOARD events' "$OUT" | head -14 | sed 's/^/  /'
echo
echo "--- where the chain breaks ---"
inst=$(grep -c 'InstallEventHandler #[0-9]* target' "$OUT" 2>/dev/null); inst=${inst:-0}
down=$(sed -n 's/.*mouse DOWN received *: *//p' "$OUT" | tail -1); down=${down:-0}
hand=$(sed -n "s/.*HALO'S handler entered *: *\\([0-9]*\\).*/\\1/p" "$OUT" | tail -1); hand=${hand:-0}
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
elif [ "$hand" = "0" ]; then
  echo "  Mouse-DOWN events ARRIVED ($down) and were dispatched, but HALO'S OWN"
  echo "  HANDLER WAS NEVER ENTERED. ⇒ something else claims the event first,"
  echo "  or the dispatcher never routes it to Halo's target. Check whether the"
  echo "  classic path (ConvertEventRefToEventRecord / WaitNextEvent) is the one"
  echo "  Halo actually uses — the lines above say whether it is called at all."
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
