#!/bin/bash
# Civ IV memdisp-code-alias merge discriminator, per-binary arm.
# Translates ONLY the main exec, from a NEUTRALLY-NAMED copy of the pristine i386, so that a
# parallel run's `pkill -9 -f "Civilization IV"` cannot match this pipeline's argv (that pkill
# SIGKILLed 4 earlier full-bundle runs, exit 137).
# Exercises the full per-binary pipeline incl. static-interpose (the stage that failed at exit 144).
# usage: run_civ_bin.sh on|off
set -u
export PYTHONUNBUFFERED=1

SP="${M64_WORK:-$HOME/.86x64-work}"
W=$HOME/projects/86x64-worktree
ARM="${1:-on}"

if [ "$ARM" = "off" ]; then
    export M64_NO_MEMDISP_CODE_ALIAS_GATE=1
fi
OUT="$SP/civsrc_gate$ARM.out"
LOG="$SP/civbin_gate$ARM.log"
STATUS="$SP/civbin_status.txt"

rm -f "$OUT" "$OUT.dylib"

echo "=== START arm=$ARM $(date '+%F %T') gate_killswitch=${M64_NO_MEMDISP_CODE_ALIAS_GATE:-unset}" >> "$STATUS"

"$W/src/86x64/86x64.sh" \
    -m "$W/build/src/macho-tool/macho-tool" \
    -l "$W/build/src/abiconv/libabiconv.dylib" \
    -w "$W/build/src/86x64/libwrapper.a" \
    -i "$W/build/src/86x64/libinterpose.dylib" \
    -o "$OUT" "$SP/civsrc.i386" > "$LOG" 2>&1
rc=$?

{
  echo "=== END   arm=$ARM $(date '+%F %T') EXIT=$rc"
  echo "    out=$(ls -la "$OUT" 2>/dev/null | awk '{print $5}') dylib=$(ls -la "$OUT.dylib" 2>/dev/null | awk '{print $5}')"
  echo "    log_tail: $(tail -2 "$LOG" 2>/dev/null | tr '\n' ' | ')"
} >> "$STATUS"
exit $rc
