#!/bin/bash
#
# snd_cb_reentrancy_test.sh — A/B guard for the classic Sound Manager's DELIVERY
# CONTRACT: a queued callback must never run on the caller's stack.
#
# SndDoCommand QUEUES a command; the Sound Manager runs it later on its own
# context. So a callBackCmd can never re-enter the app from inside the app's own
# SndDoCommand call. Firing it inline invents a re-entrancy the real API cannot
# produce, and any app holding a non-recursive lock across SndDoCommand — which
# every double-buffering app does — then deadlocks against itself.
#
# MEASURED on Halo CE (2026-08-19): its SndCallBackProc (i386 0x2d80fa) holds a
# voice semaphore across SndDoCommand(bufferCmd) + SndDoCommand(callBackCmd).
# 0x2d8127 is the ONLY 500 ms MPWaitOnSemaphore in the entire binary, and the
# inline fire made it wait on a semaphore its own outer frame already held: the
# nested wait burned the full deadline, returned kMPTimeoutErr, and took the
# early-out that SKIPS THE REFILL.
#
# ON  = deliver on a per-channel serial context. maxdepth stays 1.
# OFF = M64_NO_SND_CB_DEFER=1: inline delivery. The nested SndDoCommand re-enters
#       the callback on the same stack -> maxdepth 2, reentered=1.
#
# A clean OFF arm is a FAIL, not a pass: it would mean the fixture never reached
# the inline path, so the guard is not exercising the fix.
set -u
cd "$(dirname "$0")"

BIN=build/99_snd_cb_reentrancy.x86_64
if [ ! -x "$BIN" ]; then
  echo "snd-cb-reentrancy: SKIP (build/99_snd_cb_reentrancy.x86_64 missing;"
  echo "                         run \`make 99_snd_cb_reentrancy\` first)"
  exit 0
fi


echo "--- ON arm (default: deferred delivery) ---"
ON=$(env "$BIN" 2>&1); ON_RC=$?
echo "$ON" | sed 's/^/  /'

if [ "$ON_RC" = "2" ] || echo "$ON" | grep -q '^SKIP'; then
  echo "snd-cb-reentrancy: SKIP (no audio output device — the contract cannot be"
  echo "                         exercised, and passing here would be vacuous)"
  exit 0
fi

echo "--- OFF arm (M64_NO_SND_CB_DEFER=1: inline delivery) ---"
OFF=$(env M64_NO_SND_CB_DEFER=1 "$BIN" 2>&1); OFF_RC=$?
echo "$OFF" | sed 's/^/  /'

fail=0
if [ "$ON_RC" != "0" ]; then
  echo "FAIL: ON arm did not pass (rc=$ON_RC)"; fail=1
fi
if ! echo "$ON" | grep -q 'maxdepth=1'; then
  echo "FAIL: ON arm did not keep maxdepth at 1"; fail=1
fi
# The OFF arm must REPRODUCE the defect. If it comes back clean the fixture
# never took the inline path and this guard proves nothing.
if ! echo "$OFF" | grep -q 'reentered=1'; then
  echo "FAIL: OFF arm did NOT reproduce the re-entrancy — the guard is inert,"
  echo "      not passing (the fixture never reached the inline delivery path)."
  fail=1
fi

if [ "$fail" = "0" ]; then
  echo "snd-cb-reentrancy: PASS (ON keeps depth 1; OFF reproduces the re-entry)"
  exit 0
fi
echo "snd-cb-reentrancy: FAIL"
exit 1
