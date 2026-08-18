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

# --selftest-only calibrates the instrument WITHOUT spending a human run (and
# without launching Halo, which must only ever be started by the person who is
# going to click).
SELFTEST_ONLY=0
if [ "${1:-}" = "--selftest-only" ]; then SELFTEST_ONLY=1; shift; fi

# --forward-down arms the MUTATING experiment: every mouse-DOWN the dispatcher
# declines is re-sent to the APPLICATION target, where Halo's handler lives.
# Opt-in and separate from the read-only runs, because a perturbing run is not a
# measuring run — never leave it on for a baseline.
FWD=""
if [ "${1:-}" = "--forward-down" ]; then
  FWD="HALO_EV_FORWARD_DOWN=1"; shift
  echo "⚠ --forward-down: this run MUTATES event delivery (experiment)."
fi

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
#include <unistd.h>
static OSStatus h(EventHandlerCallRef r, EventRef e, void *u) {
   (void)r; (void)e; (void)u; return eventNotHandledErr;
}
/* The MP arm of the calibration: the probe now substitutes its own entry proc
 * for the app's, so "the worker never ran" is only trustworthy if the
 * substitution demonstrably fires when a worker DOES run. */
static volatile int ran = 0;
static OSStatus worker(void *p) { (void)p; ran = 1; return 0; }
int main(void) {
   EventTypeSpec t = { kEventClassCommand, kEventCommandProcess };
   EventHandlerRef ref = NULL;
   InstallEventHandler(GetApplicationEventTarget(), NewEventHandlerUPP(h),
                       1, &t, NULL, &ref);
   MPQueueID q = NULL; MPTaskID task = NULL;
   if (MPCreateQueue(&q) == noErr &&
       MPCreateTask(worker, NULL, 0, q, NULL, NULL, 0, &task) == noErr) {
      for (int i = 0; i < 200 && !ran; i++) { usleep(5000); }
   }
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
# The MP arm. A control that creates a task and waits for it to run: if the
# worker-entry substitution cannot see a worker that provably ran here, then
# "worker never entered" in the Halo log would be the probe's artifact rather
# than a finding — the exact mistake the recursion bug taught.
tent=$(grep -c '>>> WORKER 0 ENTERED' "$SELF.log" 2>/dev/null); tent=${tent:-0}
if [ "$tent" != "1" ]; then
  echo "SELF-TEST FAILED: a worker that ran was seen $tent times." >&2
  echo "  The MPCreateTask entry substitution is inert, so a 'never ran'" >&2
  echo "  verdict from Halo would be meaningless. NOT launching Halo." >&2
  exit 1
fi
echo "self-test: 1 call -> 1 entry, 1 return; worker entry seen; live  OK"
if [ "$SELFTEST_ONLY" = "1" ]; then
  echo "--selftest-only: instrument verified, Halo NOT launched."
  exit 0
fi

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
env DYLD_INSERT_LIBRARIES="$DYLIB" HALO_EVENT_LOG="$OUT" ABICONV_INPUT_TRACE=1 ABICONV_KEYS_TRACE=1 $FWD \
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
  echo "  --- ★did the WORKERS actually run? ---"
  if grep -q '^\[mp\] worker ' "$OUT"; then
    grep '^\[mp\] worker ' "$OUT" | sed 's/^/  /'
    grep -q 'NEVER RAN' "$OUT" && \
      echo "  ⇒ tasks are created but never entered: the main thread waits on a worker that does not exist"
    grep -q 'DID NOT RETURN' "$OUT" && \
      echo "  ⇒ the worker entered TRANSLATED code and never came back — our bug, in the callback trampoline"
  else
    echo "  (no worker lines — MPCreateTask was never called)"
  fi
  grep '>>> WORKER' "$OUT" | head -8 | sed 's/^/  /'
  echo "  --- ★per-SEMAPHORE ledger (who is starved?) ---"
  if grep -q '^\[mp\] sem ' "$OUT"; then
    grep '^\[mp\] sem ' "$OUT" | sed 's/^/  /'
    grep -q 'TABLE OVERFLOWED' "$OUT" && \
      echo "  ⚠the ledger overflowed — counts above are INCOMPLETE, do not conclude from them"
    grep -A1 'TIMEOUT on sem' "$OUT" | head -14 | sed 's/^/  /'
    grep -q 'STARVED' "$OUT" && \
      echo "  ⇒ a semaphore is waited on and never signalled: find who was supposed to signal it"
    grep -q 'CONTENDED' "$OUT" && {
      echo "  ⇒ NOT starvation: signals match waits, so the lock is released properly."
      echo "    A holder is running past the 500 ms deadline — 'maxhold' is how long,"
      echo "    and the holder= thread on the TIMEOUT lines is who. Compare that thread"
      echo "    against the WORKER ENTERED thread ids above: main thread or worker?"; }
  else
    echo "  (no semaphore rows)"
  fi
  echo "  --- ★TIMELINE: do the timeouts land ON the selection changes? ---"
  # The freeze is reported per selection change, so the decisive question is not
  # how many timeouts there are but WHEN. Interleave keypresses and timeouts on
  # one process-relative clock: if each arrow/Return is followed within a
  # fraction of a second by a 500 ms timeout, that IS the freeze. If they are
  # uncorrelated, the timeouts are background noise and the freeze is elsewhere.
  grep -hE '^\[(ev|mp)\] t= *[0-9]' "$OUT" | sort -k2 -n | head -40 | sed 's/^/  /'
  echo "  (a KEY line followed closely by a TIMEOUT line = the freeze you feel)"
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
echo "--- ★★CONTROL: does a NATIVELY-installed handler get the mouse-DOWN? ---"
grep -E 'CONTROL handler|CONTROL: our own|⇒ mouse-DOWN DOES|⇒ moved reaches|★ACTIVATION|⇒ NEITHER' "$OUT" | sed 's/^/  /'
echo
if grep -q '★FORWARD' "$OUT"; then
  echo "--- ★★FORWARD experiment (this run MUTATED delivery) ---"
  grep '★FORWARD' "$OUT" | tail -6 | sed 's/^/  /'
  hd=$(sed -n "s/.*HALO'S handler entered.*(on mouse-down: *\([0-9]*\)).*/\1/p" "$OUT" | tail -1)
  echo "  ⇒ Halo's handler ran on ${hd:-0} mouse-DOWNs this run"
  echo "    (>0 with forwarding on, 0 with it off = the dispatcher was eating them)"
  echo
fi
echo "--- ★per-HANDLER breakdown: registered vs actually received ---"
if grep -q 'handler #.*registered\[' "$OUT"; then
  grep 'handler #.*registered\[' "$OUT" | sed 's/^/  /'
  grep -q 'NEVER got one' "$OUT" && {
    echo "  ⇒ a handler registered for mouse-DOWN never received one. Compare the"
    echo "    SendEventToEventTarget target above: if Halo dispatches to the"
    echo "    DISPATCHER and the window handlers all DECLINE, the event should"
    echo "    have propagated up to the APPLICATION target where that handler is."; }
else
  echo "  (no per-handler lines — probe predates this breakdown)"
fi
echo
echo "--- where the chain breaks ---"
inst=$(grep -c 'InstallEventHandler #[0-9]* target' "$OUT" 2>/dev/null); inst=${inst:-0}
down=$(sed -n 's/.*mouse DOWN received *: *//p' "$OUT" | tail -1); down=${down:-0}
hand=$(sed -n "s/.*HALO'S handler entered *: *\\([0-9]*\\).*/\\1/p" "$OUT" | tail -1); hand=${hand:-0}
# ★Use the ON-MOUSE-DOWN sub-count, not the total. The total counts mouse-MOVED
# entries too, and on every run so far it has equalled the moved count exactly —
# so testing it reported "the click reaches Halo" on runs where Halo's handler
# had NEVER been entered for a single button press. The question here is about
# clicks; only the click sub-count can answer it.
hdown=$(sed -n "s/.*HALO'S handler entered.*(on mouse-down: *\\([0-9]*\\)).*/\\1/p" "$OUT" | tail -1); hdown=${hdown:-0}
if [ "$inst" = "0" ]; then
  echo "  NO handler was ever installed ⇒ the break is BEFORE the event system."
  echo "  Look at NewEventHandlerUPP wrapping, or at Halo bailing out earlier."
elif [ "$down" = "0" ]; then
  echo "  Handlers installed ($inst types) but NO mouse-DOWN ever arrived."
  echo "  ⇒ ReceiveNextEvent is not producing them. Either the app is not"
  echo "   pumping the queue, or the press is going somewhere else entirely"
  echo "   (a captured display / a different event target)."
elif [ "$hdown" = "0" ]; then
  echo "  Mouse-DOWN events ARRIVED ($down) but HALO'S handler never ran on ONE"
  echo "  of them (it was entered $hand times TOTAL — those are mouse-MOVED)."
  echo "  ⇒ dispatch is dropping the presses: wrong target, or the registered"
  echo "   event types do not include this class/kind."
elif [ "$hdown" = "0" ]; then
  echo "  Mouse-DOWN events ARRIVED ($down) and were dispatched, but HALO'S OWN"
  echo "  HANDLER WAS NEVER ENTERED. ⇒ something else claims the event first,"
  echo "  or the dispatcher never routes it to Halo's target. Check whether the"
  echo "  classic path (ConvertEventRefToEventRecord / WaitNextEvent) is the one"
  echo "  Halo actually uses — the lines above say whether it is called at all."
else
  echo "  The click REACHES translated Halo code ($hdown handler runs on a"
  echo "  mouse-DOWN, out of $hand entries total). ⇒ the plumbing is fine and"
  echo "  the click is lost INSIDE the"
  echo "  game. Next: compare the GetEventParameter(MouseLocation) coordinates"
  echo "  above against where the pointer actually was — highlight (GetMouse,"
  echo "  port-aware) and click (event coords) come from DIFFERENT sources, and"
  echo "  a hit-test cannot succeed if they disagree."
fi
echo
echo "  ⚠If no mouse lines appear at all, check you actually clicked while Halo"
echo "   was frontmost — an empty trace from a run with no click proves nothing."
