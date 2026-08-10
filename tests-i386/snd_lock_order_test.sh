#!/bin/bash
#
# snd_lock_order_test.sh — A/B guard proving sndmgr_shim.c's lock-ordering rule
# is what keeps a classic Sound Manager caller from deadlocking against its own
# AudioQueue completion callback.
#
# AudioQueueReset / AudioQueueStop(...,true) / AudioQueueDispose(...,true) block
# in AQ::API::Queue::AwaitAllPendingCallbacks. aq_output_cb takes c->lk on
# entry. Making any of those calls WHILE HOLDING c->lk parks the caller forever.
#
# MEASURED on Halo CE (2026-08-10): Return on a main-menu item issues flushCmd;
# 3364/3364 sample frames had the main thread in
#   shim_SndDoImmediate -> do_command -> AudioQueueReset
#   -> AwaitAllPendingCallbacks -> pthread_cond_wait
#
# ON  = snapshot the queue under the lock, release it, then make the blocking
#       call. The fixture runs to done=1.
# OFF = M64_NO_SND_LOCK_FIX=1: the original order. The fixture MUST hang at the
#       first flushCmd and get killed by the watchdog. A clean OFF arm means the
#       fixture never got a live queue with pending callbacks, i.e. this guard is
#       not exercising the fix — that is a FAIL, not a pass.
#
# The fixture is inaudible by construction (constant-0x80 PCM + ampCmd 0), but
# it does open a real output device: with no device the queue never starts, the
# deadlock cannot exist, and the guard reports that it did not exercise rather
# than passing silently.
#
# Needs the i386 sysroot; SKIPs when the binary has not been built.
set -u
cd "$(dirname "$0")"

BIN=build/99_snd_lock_order.x86_64
if [ ! -x "$BIN" ]; then
  echo "snd-lock-order: SKIP (build/99_snd_lock_order.x86_64 missing;"
  echo "                      run \`make 99_snd_lock_order\` first)"
  exit 0
fi

ON_TIMEOUT=25     # the fixture itself sleeps 250ms; 25s is pure slack
OFF_TIMEOUT=15    # the OFF arm is expected to never return

# run <timeout> <env...> — runs the fixture under a watchdog and prints its
# stdout plus a trailing "__rc=N" line (137 == the watchdog SIGKILLed it).
# The caller splits the two: `run` executes in a command-substitution subshell,
# so it cannot hand back a variable.
run() {
  local t=$1; shift
  "$@" "$BIN" 2>/dev/null &
  local pid=$!
  ( sleep "$t"; kill -9 $pid 2>/dev/null ) >/dev/null 2>&1 &
  local wd=$!
  wait $pid; local rc=$?
  kill $wd 2>/dev/null
  echo "__rc=$rc"
}
rc_of()  { printf '%s\n' "$1" | sed -n 's/^__rc=//p'; }
out_of() { printf '%s\n' "$1" | grep -v '^__rc='; }

fail=0
has() { printf '%s\n' "$1" | grep -q "^$2\$"; }

# --- ON: lock released before the blocking call -> the fixture completes -----
on_raw=$(run "$ON_TIMEOUT" env); on=$(out_of "$on_raw"); on_rc=$(rc_of "$on_raw")
want='chan_ok=1 enqueued=1 busy=1 flush_ok=1 quiet_ok=1 dispose_ok=1 done=1'
missing=""
for k in $want; do has "$on" "$k" || missing="$missing $k"; done

if has "$on" 'busy=0'; then
  echo "  ON  (lock released):  the channel never became busy — AudioQueue could"
  echo "                        not open an output device, so no callback was ever"
  echo "                        pending and the deadlock cannot be reproduced."
  echo "                        This guard did NOT exercise sndmgr_shim.c."
  printf '%s\n' "$on" | sed 's/^/      /'
  fail=1
elif [ "$on_rc" = 0 ] && [ -z "$missing" ]; then
  echo "  ON  (lock released):  six 1s buffers queued and playing, then flushCmd"
  echo "                        (AudioQueueReset), quietCmd (Stop true) and"
  echo "                        SndDisposeChannel (Stop+Dispose true) all returned OK"
else
  echo "  ON  (lock released):  rc=$on_rc, missing/failed:$missing"
  printf '%s\n' "$on" | sed 's/^/      /'
  fail=1
fi

# --- OFF: kill switch -> blocking call under the lock -> hang ---------------
# Expect the fixture to reach the flush and never come back: no flush_ok, and a
# watchdog SIGKILL (rc 137). Any completed OFF run means no pending callback.
off_raw=$(run "$OFF_TIMEOUT" env M64_NO_SND_LOCK_FIX=1); off=$(out_of "$off_raw"); off_rc=$(rc_of "$off_raw")
if has "$off" 'busy=1' && ! has "$off" 'flush_ok=1' && [ "$off_rc" != 0 ]; then
  echo "  OFF (kill switch):    reached the queue (busy=1) and then WEDGED inside"
  echo "                        flushCmd -> AudioQueueReset, holding c->lk that"
  echo "                        aq_output_cb needs; killed after ${OFF_TIMEOUT}s (rc=$off_rc)  OK"
elif has "$off" 'flush_ok=1'; then
  echo "  OFF (kill switch):    expected the original lock order to deadlock at"
  echo "                        flushCmd. It returned (rc=$off_rc), so no callback"
  echo "                        was pending and this guard is NOT exercising"
  echo "                        sndmgr_shim.c's lock-ordering fix."
  printf '%s\n' "$off" | sed 's/^/      /'
  fail=1
else
  echo "  OFF (kill switch):    did not reach a live queue (no busy=1), rc=$off_rc —"
  echo "                        the arms are not comparable."
  printf '%s\n' "$off" | sed 's/^/      /'
  fail=1
fi

if [ "$fail" = 0 ]; then
  echo "snd-lock-order: PASS"
else
  echo "snd-lock-order: FAIL"
fi
exit $fail
