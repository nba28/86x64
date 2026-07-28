#!/bin/bash
# Reproduce the tester's HANG: launch via `open` with the Game Guide SHOWN, then synthetically
# click the panel's btn-inwindow, then capture CPU% + full sample BEFORE killing anything.
SCR=${TMPDIR:-/tmp}/86x64-scratch
DEFAULT_APP="$HOME/Library/Application Support/Steam/steamapps/common/Sid Meier's Civilization IV 34440/Civilization IV (test).app"
APP="${CIVAPP:-$DEFAULT_APP}"
TAG="${1:-h1}"
# nib rects (Cocoa, content-view coords, bottom-left origin), content is 800x577
#   btn-fullscreen {{20, 89},{236,160}}   btn-inwindow {{255, 89},{153,160}}
BTN="${2:-inwindow}"
if [ "$BTN" = "fullscreen" ]; then BX=138; BY=169; else BX=331; BY=169; fi

pkill -9 -x "Civilization IV" 2>/dev/null
sleep 1
defaults write com.aspyr.civ4 DoNotShowGameGuide -bool NO
open "$APP"

# wait for the panel to appear
for i in $(seq 1 40); do
  sleep 1
  LINE=$("$SCR/winlist" Civilization | grep "onscreen=1" | grep " 800x" | head -1)
  [ -n "$LINE" ] && break
done
PID=$(pgrep -x "Civilization IV" | head -1)
echo "pid=$PID panel=$LINE"
[ -z "$LINE" ] && { echo "NO PANEL"; exit 1; }
# bounds=(X,Y WxH)
GEO=$(echo "$LINE" | sed -n 's/.*bounds=(\([0-9.-]*\),\([0-9.-]*\) \([0-9.]*\)x\([0-9.]*\)).*/\1 \2 \3 \4/p')
set -- $GEO
WX=$1; WY=$2; WW=$3; WH=$4
TITLE=$(python3 -c "print($WH - 577)")
CX=$(python3 -c "print($WX + $BX)")
CY=$(python3 -c "print($WY + $TITLE + (577 - $BY))")
echo "window=($WX,$WY ${WW}x${WH}) titlebar=$TITLE -> clicking $BTN at ($CX,$CY)"
echo "--- CPU before click ---"; ps -o pid,stat,%cpu,time,etime -p "$PID" | tail -1
"$SCR/clicker" "$CX" "$CY"
sleep 12
PID2=$(pgrep -x "Civilization IV" | head -1)
echo "=== after click: pid=$PID2 ==="
if [ -z "$PID2" ]; then
  echo "PROCESS GONE (crashed or quit)"
  find ~/Library/Logs/DiagnosticReports -name "Civilization*" -mmin -3 2>/dev/null | tail -2
  exit 0
fi
echo "--- CPU% x5, 2s apart (spin vs wait) ---"
for i in 1 2 3 4 5; do ps -o pid,stat,%cpu,rss,time,etime -p "$PID2" | tail -1; sleep 2; done
echo "--- windows ---"; "$SCR/winlist" Civilization
echo "--- sample 5s ---"
sample "$PID2" 5 -mayDie -file "$SCR/sample_$TAG.txt" >/dev/null 2>&1
echo "sample rc=$?"
sed -n '/^Call graph:/,/^$/p' "$SCR/sample_$TAG.txt" | head -60
echo "--- alive? ---"; ps -o pid,stat,%cpu,time,etime -p "$PID2" | tail -1
