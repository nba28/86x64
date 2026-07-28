#!/bin/bash
# THE decisive test: park the app in the modal loop, then deliver a real event and see
# whether -[NSApplication runModalForWindow:] returns.
# Signal: once the modal returns, Civ proceeds into the windowed-launch path and SIGBUSes.
# So "process gone + fresh SIGBUS crashlog" == the modal RETURNED.
SCR=${TMPDIR:-/tmp}/86x64-scratch
APP="$HOME/Library/Application Support/Steam/steamapps/common/Sid Meier's Civilization IV 34440/Civilization IV (test).app"
MODE="${1:-move}"    # move | none

pkill -9 -x "Civilization IV" 2>/dev/null
pkill -9 lldb 2>/dev/null
sleep 1
BEFORE=$(ls -1 ~/Library/Logs/DiagnosticReports/ 2>/dev/null | grep -c "^Civilization")
defaults write com.aspyr.civ4 DoNotShowGameGuide -bool NO
open "$APP"
for i in $(seq 1 40); do
  sleep 1
  LINE=$("$SCR/winlist" Civilization | grep "onscreen=1" | grep " 800x" | head -1)
  [ -n "$LINE" ] && break
done
PID=$(pgrep -x "Civilization IV" | head -1)
echo "pid=$PID panel=$LINE"
[ -z "$LINE" ] && { echo "NO PANEL"; exit 1; }
GEO=$(echo "$LINE" | sed -n 's/.*bounds=(\([0-9.-]*\),\([0-9.-]*\) \([0-9.]*\)x\([0-9.]*\)).*/\1 \2 \3 \4/p')
set -- $GEO
WX=$1; WY=$2; WW=$3; WH=$4
CX=$(python3 -c "print($WX + 331)")
CY=$(python3 -c "print($WY + ($WH - 577) + (577 - 169))")
"$SCR/clicker" "$CX" "$CY" >/dev/null
echo "clicked btn-inwindow at ($CX,$CY)"
sleep 12

PID2=$(pgrep -x "Civilization IV" | head -1)
if [ -z "$PID2" ]; then echo "RESULT: process already gone BEFORE any wake event (no hang this run)"; exit 0; fi
echo "--- PARKED CHECK (CPU time frozen?) ---"
ps -o pid,%cpu,time,etime -p "$PID2" | tail -1
sleep 3
ps -o pid,%cpu,time,etime -p "$PID2" | tail -1
sample "$PID2" 1 -mayDie -file "$SCR/sample_park.txt" >/dev/null 2>&1
echo "parked in: $(grep -m1 -o 'runModalForWindow:' "$SCR/sample_park.txt" || echo 'NOT in runModalForWindow')"

CPU_BEFORE=$(ps -o time= -p "$PID2" | tr -d ' ')
echo "cpu-time BEFORE events: $CPU_BEFORE"
if [ "$MODE" = "move" ]; then
  echo "--- DELIVERING WAKE EVENTS: 20 mouse-moves across the window ---"
  for k in $(seq 1 20); do
    MX=$(python3 -c "print($WX + 100 + $k*20)")
    MY=$(python3 -c "print($WY + 200 + ($k%5)*20)")
    "$SCR/mover" "$MX" "$MY" >/dev/null 2>&1
    sleep 0.2
  done
elif [ "$MODE" = "click" ]; then
  # A CLICK is delivered to the app unconditionally; a mouse-MOVE is only delivered if the
  # window has acceptsMouseMovedEvents set, so `move` alone cannot prove an event arrived.
  # Click the panel's TITLE BAR: always delivered, activates the window, triggers no control.
  TX=$(python3 -c "print($WX + 400)")
  TY=$(python3 -c "print($WY + 16)")
  echo "--- DELIVERING WAKE EVENTS: 3 title-bar clicks at ($TX,$TY) ---"
  for k in 1 2 3; do "$SCR/clicker" "$TX" "$TY" >/dev/null 2>&1; sleep 1; done
else
  echo "--- CONTROL: delivering NOTHING, just waiting the same wall time ---"
  sleep 5
fi

sleep 6
CPU_AFTER=$(ps -o time= -p "$PID2" 2>/dev/null | tr -d ' ')
echo "cpu-time AFTER  events: ${CPU_AFTER:-<process gone>}   (BEFORE was $CPU_BEFORE)"
if [ -n "$CPU_AFTER" ] && [ "$CPU_AFTER" != "$CPU_BEFORE" ]; then
  echo "DELIVERY PROVEN: cpu time advanced -> the process WOKE and ran code"
elif [ -n "$CPU_AFTER" ]; then
  echo "DELIVERY NOT PROVEN: cpu time did NOT advance -> nothing reached the process"
fi
PID3=$(pgrep -x "Civilization IV" | head -1)
AFTER=$(ls -1 ~/Library/Logs/DiagnosticReports/ 2>/dev/null | grep -c "^Civilization")
echo "=== RESULT (mode=$MODE) ==="
if [ -z "$PID3" ]; then
  echo "PROCESS GONE  (crashlogs before=$BEFORE after=$AFTER)"
  [ "$AFTER" -gt "$BEFORE" ] && echo ">>> MODAL RETURNED: proceeded past the guide and hit the windowed SIGBUS"
  find ~/Library/Logs/DiagnosticReports -name "Civilization*" -mmin -3 2>/dev/null | tail -1
else
  ps -o pid,%cpu,time,etime -p "$PID3" | tail -1
  sample "$PID3" 1 -mayDie -file "$SCR/sample_after.txt" >/dev/null 2>&1
  if grep -q "runModalForWindow:" "$SCR/sample_after.txt"; then
    echo ">>> STILL PARKED in runModalForWindow: despite the events"
  else
    echo ">>> MODAL RETURNED (no longer in runModalForWindow:)"
    sed -n '/^Call graph:/,+12p' "$SCR/sample_after.txt"
  fi
  "$SCR/winlist" Civilization
fi
pkill -9 -x "Civilization IV" 2>/dev/null
