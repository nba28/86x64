#!/bin/bash
#
# carbon_window_compositing_test.sh — A/B guard proving the 64-bit Window Manager
# COMPOSITING injection (src/abiconv/carbon_window_shim.c) is what makes a legacy
# programmatic Carbon window creation succeed.
#
# 64-bit HIToolbox deleted the non-compositing window model:
#   HIToolbox`NewWindowCommon+23: btl $0x13,%r14d / jae -> errUnsupportedWindow-
#   AttributesForClass (-5601)
# so any CreateNewWindow whose attributes lack kWindowCompositingAttribute (bit
# 19) fails for every class. Practically all 32-bit-era Carbon code omits that
# bit (it did not exist before 10.2 and stayed opt-in through 10.6) — Halo CE's
# fullscreen (kPlainWindowClass + kWindowNoConstrainAttribute) and windowed
# (kDocumentWindowClass + 0x82000009) render windows both came back NULL, and the
# app took its error path and showed an alert instead of a window.
#
# The main-suite fixture 99_carbon_window_compositing runs the ON side (real
# window, bounds round-trip). This script adds the OFF side: re-run the SAME
# translated binary with the kill-switch M64_NO_CARBON_COMPOSITING=1 and assert
# it FAILS with -5601 and a NULL window. Without that half, a passing fixture
# would not prove the injection is what fixed it.
#
# Needs the i386 sysroot (it reuses the main suite's translated binary); SKIPs
# when the binary has not been built.
set -u
cd "$(dirname "$0")"

BIN=build/99_carbon_window_compositing.x86_64
if [ ! -x "$BIN" ]; then
  echo "carbon-window-compositing: SKIP (build/99_carbon_window_compositing.x86_64 missing;"
  echo "                                 run \`make 99_carbon_window_compositing\` first)"
  exit 0
fi

fail=0

# --- ON: the shim injects the compositing bit -> real windows -------------------
on=$("$BIN" 2>/dev/null)
if printf '%s\n' "$on" | grep -q '^fullscreen_status=0$' &&
   printf '%s\n' "$on" | grep -q '^fullscreen_window=1$' &&
   printf '%s\n' "$on" | grep -q '^windowed_status=0$' &&
   printf '%s\n' "$on" | grep -q '^windowed_window=1$'; then
  echo "  ON  (shim armed):    CreateNewWindow -> noErr, real WindowRef      OK"
else
  echo "  ON  (shim armed):    expected noErr + non-NULL window, got:"
  printf '%s\n' "$on" | sed 's/^/      /'
  fail=1
fi

# --- OFF: kill-switch -> raw native behaviour, -5601 and a NULL window ----------
off=$(M64_NO_CARBON_COMPOSITING=1 "$BIN" 2>/dev/null)
if printf '%s\n' "$off" | grep -q '^fullscreen_status=-5601$' &&
   printf '%s\n' "$off" | grep -q '^fullscreen_window=0$' &&
   printf '%s\n' "$off" | grep -q '^windowed_status=-5601$' &&
   printf '%s\n' "$off" | grep -q '^windowed_window=0$'; then
  echo "  OFF (kill-switch):   CreateNewWindow -> -5601, NULL WindowRef      OK"
else
  echo "  OFF (kill-switch):   expected -5601 + NULL window (the unfixed"
  echo "                       behaviour); the guard is NOT exercising the"
  echo "                       injection. Got:"
  printf '%s\n' "$off" | sed 's/^/      /'
  fail=1
fi

if [ "$fail" = 0 ]; then
  echo "carbon-window-compositing: PASS"
else
  echo "carbon-window-compositing: FAIL"
fi
exit $fail
