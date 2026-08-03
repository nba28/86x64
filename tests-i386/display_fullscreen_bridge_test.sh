#!/bin/bash
#
# display_fullscreen_bridge_test.sh — A/B guard proving the classic FULLSCREEN
# DISPLAY-MODE bridge (src/abiconv/cg_display_fullscreen_shim.c +
# src/abiconv/cg_display_tramp.asm) is what changes the behaviour.
#
# Every 32-bit-era fullscreen Mac game does
#
#     CGCaptureAllDisplays();
#     mode = CGDisplayBestModeForParameters(dpy, 32, w, h, &exact);
#     CGDisplaySwitchToMode(dpy, mode);
#
# and PERFORMING that on modern macOS is fatal: CoreGraphics posts a display
# reconfiguration, AppKit's observer setFrame:s every window in the process, and
# -[NSCGSWindow _createContext] cannot be created for an NSCarbonWindow -> SIGILL
# in NSCGSPanic.  The bridge never reconfigures a display; it records a VIRTUAL
# mode, answers every mode query from it, tracks the capture family without
# performing it, and MOVES (never resizes) the app's GL window to the display
# origin.
#
# The main-suite fixture 99_display_fullscreen_bridge is the ON side: it runs the
# whole idiom and ends by asserting the PHYSICAL display is exactly where it
# started.  This script adds the OFF side: re-run the SAME translated binary with
# the kill switch M64_NO_DISPLAY_FULLSCREEN_BRIDGE=1 and assert the native answer
# comes back — a "best mode" that is NOT the requested resolution, with
# exactMatch=0.
#
# ⚠The OFF arm is deliberately run in the fixture's "query" mode, which stops
# before the capture and the switch.  With the bridge disarmed those two calls
# would really capture and really reconfigure the user's display — which is both
# destructive and the very crash under test.  The mode-report difference is the
# discriminator, and it is unambiguous: no modern Mac offers a 640x480 mode.
#
# Needs the i386 sysroot (it reuses the main suite's translated binary); SKIPs
# when the binary has not been built.
set -u
cd "$(dirname "$0")"

BIN=build/99_display_fullscreen_bridge.x86_64
if [ ! -x "$BIN" ]; then
  echo "display-fullscreen-bridge: SKIP (build/99_display_fullscreen_bridge.x86_64 missing;"
  echo "                                 run \`make 99_display_fullscreen_bridge\` first)"
  exit 0
fi

fail=0
has() { printf '%s\n' "$1" | grep -q "^$2\$"; }

# --- ON: the bridge answers with exactly the requested geometry ---------------
on=$("$BIN" query 2>/dev/null)
if has "$on" 'best_exact=1' && has "$on" 'exact=1'; then
  echo "  ON  (bridge armed):  BestModeForParameters(640x480) -> 640x480, exactMatch=1  OK"
else
  echo "  ON  (bridge armed):  expected the requested geometry back with exactMatch=1, got:"
  printf '%s\n' "$on" | sed 's/^/      /'
  fail=1
fi

# --- OFF: kill switch -> the native answer, which is a different resolution ---
off=$(M64_NO_DISPLAY_FULLSCREEN_BRIDGE=1 "$BIN" query 2>/dev/null)
if has "$off" 'best_exact=0' && has "$off" 'exact=0'; then
  echo "  OFF (kill switch):   native BestModeForParameters -> NOT 640x480, exactMatch=0 OK"
else
  echo "  OFF (kill switch):   expected the native, non-exact answer (the unfixed"
  echo "                       behaviour: a modern Mac has no 640x480 mode, so it"
  echo "                       returns some other resolution); the guard is NOT"
  echo "                       exercising the bridge. Got:"
  printf '%s\n' "$off" | sed 's/^/      /'
  fail=1
fi

if [ "$fail" = 0 ]; then
  echo "display-fullscreen-bridge: PASS"
else
  echo "display-fullscreen-bridge: FAIL"
fi
exit $fail
