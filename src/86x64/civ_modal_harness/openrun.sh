#!/bin/bash
# Launch via LaunchServices (`open`) so the app gets a real activation context.
# Capture windows, CPU%, and a full sample BEFORE killing anything.
SCR=${TMPDIR:-/tmp}/86x64-scratch
APP="$HOME/Library/Application Support/Steam/steamapps/common/Sid Meier's Civilization IV 34440/Civilization IV (test).app"
TAG="${1:-o1}"
WAIT="${2:-25}"

pkill -9 -x "Civilization IV" 2>/dev/null
sleep 1
if pgrep -x "Civilization IV" >/dev/null; then echo "STILL RUNNING - abort"; exit 1; fi

echo "=== prefs before ==="
defaults read com.aspyr.civ4 2>&1 | grep -E "GameDisplayMode|DoNotShowGameGuide"

open "$APP"
sleep "$WAIT"
PID=$(pgrep -x "Civilization IV" | head -1)
echo "=== $TAG: after ${WAIT}s pid=$PID ==="
if [ -z "$PID" ]; then
  echo "PROCESS GONE"
  find ~/Library/Logs/DiagnosticReports -name "Civilization*" -mmin -4 2>/dev/null | tail -3
  exit 0
fi
echo "--- CPU sample #1 (ps, 3 reads 2s apart) ---"
for i in 1 2 3; do ps -o pid,stat,%cpu,rss,time,etime -p "$PID" | tail -1; sleep 2; done
echo "--- windows (CGWindowListCopyWindowInfo) ---"
"$SCR/winlist" Civilization
echo "--- sample 5s ---"
sample "$PID" 5 -mayDie -file "$SCR/sample_$TAG.txt" >/dev/null 2>&1
echo "sample rc=$? -> sample_$TAG.txt"
grep -n "^Call graph:" -A 40 "$SCR/sample_$TAG.txt" | head -50
echo "--- still alive? ---"
ps -o pid,stat,%cpu,time,etime -p "$PID" | tail -1
