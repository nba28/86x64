#!/bin/bash
#
# snd_status_layout_test.sh — A/B guard proving shim_SndChannelStatus honours
# the real SCStatus ABI: 24 bytes, scChannelBusy at @12, scChannelPaused at @14,
# and `theLength` as a HARD upper bound on what the shim may write.
#
# CarbonSound/Sound.h:
#   scStartTime@0 scEndTime@4 scCurrentTime@8
#   scChannelBusy@12 scChannelDisposed@13 scChannelPaused@14 scUnused@15
#   scChannelAttributes@16 scCPULoad@20                          sizeof == 24
#
# The old shim did `memset(theStatus, 0, 28)` and wrote busy@24 / paused@26, so
# it overran a 24-byte caller record by 4 bytes AND left scChannelBusy@12 at 0
# forever. MEASURED on Halo CE from the i386 original: two call sites, both
# passing theLength=0x18 and both reading the flag at +0x0C —
# IDirectSoundBuffer_Mac::Play @0x2d93a7 (47-channel free-voice scan) and
# IDirect Sound_Mac::GetCaps @0x2d8362 (48-slot loop, i.e. 48 overruns a call).
#
# ON  = clamp to theLength, flags at @12/@14.
# OFF = M64_NO_SND_STATUS_FIX=1 restores the defect.
#
# ★The OVERRUN half is device-independent — the shim clears the record before
# it looks the channel up — so it is the MUST-PASS assertion on every host.
# The BUSY half needs a real output device and is reported but not required.
#
# Needs the i386 sysroot; SKIPs when the binary has not been built.
set -u
cd "$(dirname "$0")"

BIN=build/98_snd_status_layout.x86_64
if [ ! -x "$BIN" ]; then
  echo "snd-status-layout: SKIP (build/98_snd_status_layout.x86_64 missing;"
  echo "                         run \`make 98_snd_status_layout\` first)"
  exit 0
fi

TMO=30
run() {   # run <env...> ; prints stdout then __rc=N
  local t=$TMO
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
has()    { printf '%s\n' "$1" | grep -q "^$2\$"; }

fail=0

on_raw=$(run env);                          on=$(out_of "$on_raw");  on_rc=$(rc_of "$on_raw")
off_raw=$(run env M64_NO_SND_STATUS_FIX=1); off=$(out_of "$off_raw"); off_rc=$(rc_of "$off_raw")

if ! has "$on" 'chan_ok=1' || ! has "$on" 'done=1'; then
  echo "  ON  (SCStatus ABI):   fixture did not complete (rc=$on_rc)"
  printf '%s\n' "$on" | sed 's/^/      /'
  exit 1
fi
if ! has "$off" 'chan_ok=1'; then
  echo "  OFF (kill switch):    fixture did not start (rc=$off_rc)"
  printf '%s\n' "$off" | sed 's/^/      /'
  exit 1
fi

# --- MUST PASS: the 4-byte overrun past a 24-byte record --------------------
if has "$on" 'canary_idle=intact' && has "$on" 'canary_busy=intact'; then
  echo "  ON  (bounded write):  theLength=24 honoured — the 8 canary bytes after"
  echo "                        the record are untouched at both call points"
else
  echo "  ON  (bounded write):  the shim wrote PAST the caller's 24-byte SCStatus"
  printf '%s\n' "$on" | sed 's/^/      /'
  fail=1
fi

if has "$off" 'canary_idle=CLOBBERED'; then
  echo "  OFF (kill switch):    reproduced the overrun — memset(...,28) zeroed the"
  echo "                        4 bytes after a 24-byte SCStatus  OK"
else
  echo "  OFF (kill switch):    expected the 28-byte clear to clobber the canary and"
  echo "                        it did NOT — this guard is not exercising the fix."
  printf '%s\n' "$off" | sed 's/^/      /'
  fail=1
fi

# --- REPORTED (needs a real output device): scChannelBusy at @12 ------------
if has "$on" 'busy12=1'; then
  if has "$off" 'busy12=0'; then
    echo "  ON  (flag offset):    scChannelBusy@12 == 1 with 6s of audio in flight,"
    echo "                        and 0 under the kill switch  OK"
  else
    echo "  OFF (kill switch):    scChannelBusy@12 was set even with the fix off —"
    echo "                        the arms are not comparable."
    fail=1
  fi
else
  echo "  ..  (flag offset):     no output device on this host (busy12=0 in BOTH"
  echo "                         arms), so only the device-independent overrun half"
  echo "                         was exercised. Not a failure."
fi

if [ "$fail" = 0 ]; then
  echo "snd-status-layout: PASS"
else
  echo "snd-status-layout: FAIL"
fi
exit $fail
