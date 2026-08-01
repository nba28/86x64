#!/bin/bash
#
# agl_window_drawable_test.sh — A/B guard proving the Carbon+AGL DRAWABLE
# substrate (src/abiconv/qd_gworld.c window-backed ports +
# src/abiconv/agl_drawable_shim.c) is what brings a translated Carbon+OpenGL
# app's GL context up.
#
# 64-bit macOS deleted GetWindowPort, so the universal 32-bit-era idiom
#
#     aglSetDrawable(ctx, GetWindowPort(win));
#
# attached a NULL drawable: aglSetDrawable returned 0 and no GL context existed
# at all, dropping the app into an error path that 2006 hardware never ran.
# (Halo CE: 21 aglSetDrawable call sites, zero aglSetWindowRef.)  The fix hands
# the window a REAL port that records its WindowRef and routes such a drawable
# to the surviving native aglSetWindowRef.
#
# The main-suite fixture 99_agl_window_drawable runs the ON side (live GL
# context on a real compositing window, port round-trip, and the save/detach/
# restore cycle Carbon apps run around QuickDraw UI).  This script adds the OFF
# side: re-run the SAME translated binary with the kill-switch
# M64_NO_AGL_WINDOWREF=1 and assert the NULL-drawable failure comes back.
# Without that half, a passing fixture would not prove the substrate is what
# fixed it.
#
# Needs the i386 sysroot (it reuses the main suite's translated binary); SKIPs
# when the binary has not been built.
set -u
cd "$(dirname "$0")"

BIN=build/99_agl_window_drawable.x86_64
if [ ! -x "$BIN" ]; then
  echo "agl-window-drawable: SKIP (build/99_agl_window_drawable.x86_64 missing;"
  echo "                           run \`make 99_agl_window_drawable\` first)"
  exit 0
fi

fail=0
has() { printf '%s\n' "$1" | grep -q "^$2\$"; }

# --- ON: window-backed port -> aglSetWindowRef -> a LIVE GL context ------------
on=$("$BIN" 2>/dev/null)
if has "$on" 'window=1'      && has "$on" 'port=1'        && has "$on" 'stable=1' &&
   has "$on" 'roundtrip=1'   && has "$on" 'set=1'         && has "$on" 'renderer=1' &&
   has "$on" 'getdrawable=1' && has "$on" 'restore=1'; then
  echo "  ON  (shim armed):    GetWindowPort -> real port, aglSetDrawable -> live GL   OK"
else
  echo "  ON  (shim armed):    expected a real port and a live GL renderer, got:"
  printf '%s\n' "$on" | sed 's/^/      /'
  fail=1
fi

# --- OFF: kill-switch -> the pre-fix NULL drawable ----------------------------
# The window itself still comes up (that is the compositing shim's job, guarded
# separately); everything downstream of the port must collapse.
off=$(M64_NO_AGL_WINDOWREF=1 "$BIN" 2>/dev/null)
if has "$off" 'window=1'      && has "$off" 'port=0'      && has "$off" 'stable=0' &&
   has "$off" 'roundtrip=0'   && has "$off" 'set=0'       && has "$off" 'renderer=0' &&
   has "$off" 'getdrawable=0' && has "$off" 'restore=0'; then
  echo "  OFF (kill-switch):   GetWindowPort -> NULL, aglSetDrawable -> 0, no GL        OK"
else
  echo "  OFF (kill-switch):   expected a NULL port and no GL renderer (the unfixed"
  echo "                       behaviour); the guard is NOT exercising the drawable"
  echo "                       substrate. Got:"
  printf '%s\n' "$off" | sed 's/^/      /'
  fail=1
fi

if [ "$fail" = 0 ]; then
  echo "agl-window-drawable: PASS"
else
  echo "agl-window-drawable: FAIL"
fi
exit $fail
