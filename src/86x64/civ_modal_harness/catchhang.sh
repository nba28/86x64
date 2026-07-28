#!/bin/bash
# Launch, click, and RETRY until we actually catch a hang; then probe the PARKED process
# (lldb's own attach may unstick it, so we read state BEFORE continuing).
SCR=${TMPDIR:-/tmp}/86x64-scratch
DEFAULT_APP="$HOME/Library/Application Support/Steam/steamapps/common/Sid Meier's Civilization IV 34440/Civilization IV (test).app"
APP="${CIVAPP:-$DEFAULT_APP}"
MAXTRY="${1:-6}"

for try in $(seq 1 "$MAXTRY"); do
  pkill -9 -x "Civilization IV" 2>/dev/null
  pkill -9 lldb 2>/dev/null
  sleep 1
  defaults write com.aspyr.civ4 DoNotShowGameGuide -bool NO
  open "$APP"
  LINE=""
  for i in $(seq 1 40); do
    sleep 1
    LINE=$("$SCR/winlist" Civilization | grep "onscreen=1" | grep " 800x" | head -1)
    [ -n "$LINE" ] && break
  done
  [ -z "$LINE" ] && { echo "try $try: NO PANEL"; continue; }
  GEO=$(echo "$LINE" | sed -n 's/.*bounds=(\([0-9.-]*\),\([0-9.-]*\) \([0-9.]*\)x\([0-9.]*\)).*/\1 \2 \3 \4/p')
  set -- $GEO
  CX=$(python3 -c "print($1 + 331)")
  CY=$(python3 -c "print($2 + ($4 - 577) + (577 - 169))")
  "$SCR/clicker" "$CX" "$CY" >/dev/null
  sleep 14
  PID=$(pgrep -x "Civilization IV" | head -1)
  if [ -z "$PID" ]; then echo "try $try: no hang (process exited)"; continue; fi
  echo "try $try: HANG CAUGHT pid=$PID"
  ps -o pid,%cpu,time,etime -p "$PID" | tail -1
  cd "$SCR"
  lldb -b -p "$PID" -o "command script import $SCR/probe.py" -o "probe" -o "detach" -o quit 2>&1 \
    | sed -n '/PARKED-PROCESS PROBE/,/END PROBE/p'
  pkill -9 -x "Civilization IV" 2>/dev/null
  exit 0
done
echo "no hang caught in $MAXTRY tries"
pkill -9 -x "Civilization IV" 2>/dev/null
