#!/bin/bash
#
# snd_cb_fifo_test.sh — A/B guard: every callBackCmd fires, in order, once the
# buffers queued before it have played (src/99_snd_cb_fifo.c has the story:
# Portal 2's Bink intro audio crackled because the shim kept one pending
# callback and fired it only at drain).
#
# ON  = callbacks=123.
# OFF = M64_NO_SND_CB_FIFO=1: the old single slot -> callbacks=3.
# A clean OFF arm is a FAIL: the fixture never reached the overwrite.
set -u
cd "$(dirname "$0")"
BIN=build/99_snd_cb_fifo.x86_64
[ -x "$BIN" ] || { echo "snd-cb-fifo: SKIP ($BIN missing)"; exit 0; }

ON=$("$BIN" 2>&1); ON_RC=$?
echo "  ON : $(echo "$ON" | tr '\n' ' ')"
if [ "$ON_RC" = "2" ]; then
  echo "snd-cb-fifo: SKIP (no audio output device — passing would be vacuous)"; exit 0
fi
OFF=$(M64_NO_SND_CB_FIFO=1 "$BIN" 2>&1)
echo "  OFF: $(echo "$OFF" | tr '\n' ' ')"

fail=0
echo "$ON"  | grep -q '^callbacks=123$' || { echo "FAIL: ON arm did not deliver 1,2,3 in order"; fail=1; }
echo "$OFF" | grep -q '^callbacks=3$'   || { echo "FAIL: OFF arm did not reproduce the lost callbacks (guard inert)"; fail=1; }
[ "$fail" = 0 ] && { echo "snd-cb-fifo: PASS (ON 1,2,3; OFF loses 1,2)"; exit 0; }
echo "snd-cb-fifo: FAIL"; exit 1
