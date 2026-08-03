#!/bin/bash
# civ_cfstr_run.sh — reproduce Civ IV's uncaught NSRangeException WITH the
# cfstrrange probe loaded, and capture the exception reason + the offending range.
#
# WHY THIS SCRIPT EXISTS. The crash report for the retranslated Civ says only
# "abort() called": the exception REASON is discarded, and the unwinder cannot
# walk below our abigen `.l1` shim into the i386 caller. probes/cfstrrange.m
# recovers both, but only if it is actually loaded into the process — and that is
# the whole difficulty here:
#
#   * a bare `exec` of Contents/MacOS/<binary> never shows a window (no
#     LaunchServices activation), which manufactures a fictitious bug — see
#     gotcha 1 in this directory's README. So we must launch via `open`.
#   * `open` does not inherit the caller's environment, so DYLD_INSERT_LIBRARIES
#     set in the shell is NOT passed through, and LaunchServices sends the app's
#     stderr to the system log rather than our terminal.
#
# Both are solved by `open --env` + `open --stderr`, which is why this is a
# script and not a one-liner.
#
# The click reproduces exactly what the tester did by hand ("Launch in Window"). The
# button centre is computed from the panel's LIVE bounds rather than hardcoded,
# because the panel is positioned by the window manager, not by us.
#
# ⚠HANDS OFF while this runs: synthetic CGEvent input goes to the FRONTMOST app.
# ⚠Civ is a SINGLETON: pkill -9 -x (never -f, which would kill a concurrent
#   translate whose argv contains the app name).
#
# usage: civ_cfstr_run.sh [runs] [--all]
#        --all  set CFSPY_ALL=1 (log every CFStringGetCharacters, not just the
#               out-of-bounds one) — use when the bad call never shows up.
set -u

SCR="${M64_SCRATCH:?set M64_SCRATCH to the dir holding winlist/clicker/activate/cfstrrange.dylib}"
STEAM="$HOME/Library/Application Support/Steam/steamapps/common/Sid Meier's Civilization IV 34440"
APP="${CIVAPP:-$STEAM/Civilization IV (Steam).app}"
DYLIB="$SCR/cfstrrange.dylib"
N="${1:-1}"
EXTRA=""
[ "${2:-}" = "--all" ] && EXTRA="--env CFSPY_ALL=1"

# gotcha 0: a locked screen silently discards every synthetic event and makes a
# healthy app look inert. Refuse to produce meaningless measurements.
LOCKED=$(python3 -c "import Quartz; d=Quartz.CGSessionCopyCurrentDictionary() or {}; print(d.get('CGSSessionScreenIsLocked'))" 2>/dev/null)
if [ "$LOCKED" = "True" ]; then
   echo "ABORT: screen is LOCKED — no synthetic input is deliverable, results would be fiction."
   exit 2
fi
[ -f "$DYLIB" ] || { echo "ABORT: probe not built at $DYLIB"; exit 2; }

for i in $(seq 1 "$N"); do
   LOG="$SCR/civ_cfstr_$i.log"
   rm -f "$LOG"
   pkill -9 -x "Civilization IV" 2>/dev/null
   sleep 1

   # Show the launcher panel so we can choose "Launch in Window", as the tester did.
   defaults write com.aspyr.civ4 DoNotShowGameGuide -bool NO

   BEFORE=$(ls -1 ~/Library/Logs/DiagnosticReports/ 2>/dev/null | grep -c "^Civilization")

   # shellcheck disable=SC2086
   open --env "DYLD_INSERT_LIBRARIES=$DYLIB" $EXTRA --stderr "$LOG" "$APP"

   # Wait for the 800-wide launcher panel to materialise.
   LINE=""
   for _ in $(seq 1 40); do
      sleep 1
      LINE=$("$SCR/winlist" Civilization 2>/dev/null | grep "onscreen=1" | grep " 800x" | head -1)
      [ -n "$LINE" ] && break
   done
   if [ -z "$LINE" ]; then
      echo "run $i: NO PANEL (app never showed the launcher)"
      echo "  probe said: $(grep -c cfspy "$LOG" 2>/dev/null || echo 0) lines"
      grep -m5 "cfspy" "$LOG" 2>/dev/null | sed 's/^/    /'
      continue
   fi

   # Input only lands if the app is genuinely frontmost.
   "$SCR/activate" "Civilization IV" >/dev/null 2>&1
   sleep 1

   # "Launch in Window" centre, MEASURED from a screencapture of the live panel
   # (scratchpad/panel.png) rather than derived from the nib. The old nib math
   # (an 800x577 content view, button centre (331,169) from the bottom-left)
   # computes a point in the empty grey area ~170pt BELOW the buttons: this
   # window is 800x609 and its content is not laid out as that nib assumed.
   # The click silently did nothing, the panel stayed up, and three runs looked
   # like "the game launched and survived" when the game had never launched.
   # ★If this ever stops working, re-measure with screencapture -R before
   #  trusting any run that reports the app ALIVE.
   GEO=$(echo "$LINE" | sed -n 's/.*bounds=(\([0-9.-]*\),\([0-9.-]*\) \([0-9.]*\)x\([0-9.]*\)).*/\1 \2 \3 \4/p')
   # shellcheck disable=SC2086
   set -- $GEO
   CX=$(python3 -c "print($1 + 400)")
   CY=$(python3 -c "print($2 + 268)")
   "$SCR/clicker" "$CX" "$CY" >/dev/null 2>&1

   sleep 25
   PID=$(pgrep -x "Civilization IV" | head -1)
   AFTER=$(ls -1 ~/Library/Logs/DiagnosticReports/ 2>/dev/null | grep -c "^Civilization")

   if [ -n "$PID" ]; then
      echo "run $i: ALIVE pid=$PID  crashlogs $BEFORE->$AFTER"
      # "alive" is not "working": both these apps relaunch themselves, and the
      # launcher panel alone looks identical to a running game from the outside.
      echo "  windows now:"
      "$SCR/winlist" Civilization 2>/dev/null | grep "onscreen=1" | sed 's/^/    /'
   elif [ "$AFTER" -gt "$BEFORE" ]; then
      echo "run $i: CRASHED (new crashlog)  crashlogs $BEFORE->$AFTER"
   else
      echo "run $i: exited, NO new crashlog  ($BEFORE->$AFTER)"
   fi

   echo "  --- probe output ($LOG) ---"
   if [ -s "$LOG" ]; then
      grep "cfspy" "$LOG" | tail -40 | sed 's/^/    /'
   else
      echo "    (EMPTY — probe never loaded. dyld may have stripped DYLD_INSERT_LIBRARIES;"
      echo "     check the app is not hardened-runtime signed.)"
   fi
   pkill -9 -x "Civilization IV" 2>/dev/null
done
