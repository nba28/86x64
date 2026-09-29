#!/bin/bash
#
# agl_fullscreen_present_test.sh — A/B guard: the WINDOWED-context fullscreen
# idiom (capture + CGDisplaySwitchToMode while an AGL context draws into the
# app's Carbon window) is presented on the fullscreen surface
# (cgl_fullscreen_shim.m via cg_display_fullscreen_shim.c + agl_drawable_shim.c).
#
# Before: the bridge could only MOVE the Carbon window to the display origin
# (a resize is fatal), so Halo's menu sat at the screen's top-left corner at its
# own resolution, under the menu bar, never fullscreen.
#
# Reuses 99_agl_window_drawable in its AGL_PRESENT mode and reads the
# WindowServer list for the fixture's pid (works on a locked screen too):
#   ON : the surface window (titled with the process name) is on screen, the Carbon window
#        (400x300 content) is ordered out;
#   OFF: M64_NO_CGL_FULLSCREEN_BRIDGE=1 -> no surface, the Carbon window stays.
set -u
cd "$(dirname "$0")"
BIN=build/99_agl_window_drawable.x86_64
if [ ! -x "$BIN" ]; then
  echo "agl-fullscreen-present: SKIP ($BIN missing; run \`make build-only\` first)"; exit 0
fi

# prints "<surface on-screen> <carbon on-screen>" as 0/1
arm() {
  env AGL_PRESENT=1 "$@" perl -e 'alarm 15; exec @ARGV' "$BIN" >/dev/null 2>&1 &
  local runner=$! pid=""
  for _ in 1 2 3 4 5 6 7 8 9 10; do
    sleep 0.5; pid=$(pgrep -f "$BIN" | head -1); [ -n "$pid" ] && break
  done
  sleep 2
  python3 - "$pid" <<'PY'
import sys, Quartz
pid = int(sys.argv[1] or 0); surf = carb = 0
for w in Quartz.CGWindowListCopyWindowInfo(Quartz.kCGWindowListOptionAll, 0):
    if w.get('kCGWindowOwnerPID') != pid or not w.get('kCGWindowIsOnscreen'): continue
    # the surface is titled with the process name (it is scaled to the visible
    # screen, so its size is not an identity); the Carbon window is untitled 400 wide
    if w.get('kCGWindowName', '').startswith('99_agl_window_drawable'): surf = 1
    elif int(w['kCGWindowBounds']['Width']) == 400: carb = 1
print(surf, carb)
PY
  wait $runner 2>/dev/null
}

fail=0
read -r s c <<<"$(arm)"
if [ "$s" = 1 ] && [ "$c" = 0 ]; then echo "  ON : surface up, Carbon window ordered out  OK"
else echo "  ON : expected surface=1 carbon=0, got surface=$s carbon=$c"; fail=1; fi
read -r s c <<<"$(arm M64_NO_CGL_FULLSCREEN_BRIDGE=1)"
if [ "$s" = 0 ] && [ "$c" = 1 ]; then echo "  OFF: no surface, Carbon window stays  OK"
else echo "  OFF: expected surface=0 carbon=1, got surface=$s carbon=$c"; fail=1; fi
[ $fail = 0 ] && echo "agl-fullscreen-present: PASS" || echo "agl-fullscreen-present: FAIL"
exit $fail
