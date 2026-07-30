#!/bin/bash
# Guide-skipped SIGBUS repro. Was DETERMINISTIC pre-fix. usage: sigbus_ab.sh <app> <n>
SCR=${TMPDIR:-/tmp}/86x64-scratch
STEAM="$HOME/Library/Application Support/Steam/steamapps/common/Sid Meier's Civilization IV 34440"
APP="$STEAM/$1"
N="${2:-3}"
echo "########## $1  (n=$N) ##########"
for i in $(seq 1 "$N"); do
  pkill -9 -x "Civilization IV" 2>/dev/null
  sleep 1
  BEFORE=$(ls -1 ~/Library/Logs/DiagnosticReports/ 2>/dev/null | grep -c "^Civilization")
  defaults write com.aspyr.civ4 DoNotShowGameGuide -bool YES
  defaults write com.aspyr.civ4 GameDisplayMode -int 2
  open "$APP"
  sleep 22
  PID=$(pgrep -x "Civilization IV" | head -1)
  AFTER=$(ls -1 ~/Library/Logs/DiagnosticReports/ 2>/dev/null | grep -c "^Civilization")
  if [ -n "$PID" ]; then
    CPU=$(ps -o time= -p "$PID" | tr -d ' ')
    sample "$PID" 1 -mayDie -file "$SCR/sb_$i.txt" >/dev/null 2>&1
    TOP=$(grep -m1 -oE "runModalForWindow:|CGLFlushDrawable|mach_msg" "$SCR/sb_$i.txt" | head -1)
    echo "  run $i: ALIVE  cpu=$CPU  crashlogs $BEFORE->$AFTER  top=${TOP:-?}"
  elif [ "$AFTER" -gt "$BEFORE" ]; then
    echo "  run $i: CRASHED (new crashlog)  crashlogs $BEFORE->$AFTER"
    python3 "$SCR/parsecrash.py" 2>/dev/null | grep -E "^exception|KERN_|signal" | head -2
  else
    echo "  run $i: EXITED cleanly, no crashlog  ($BEFORE->$AFTER)"
  fi
done
pkill -9 -x "Civilization IV" 2>/dev/null
