#!/bin/bash
# agl_oversize_window_test.sh — A/B guard: a WINDOWED Carbon+AGL window larger
# than the screen's usable area (Halo "Play in a window" at 2560x1440 on a
# 1512x982 screen: title bar off-screen, nothing reachable; its size cannot be
# changed after show) is presented on the surface as an ordinary, scaled window
# (cgl_fullscreen_shim.m cglfs_present_window via agl_drawable_shim.c).
#   ON : the surface window (titled with the process name) exists, the
#        4000-wide Carbon window is not on screen, and Halo's per-frame
#        re-centre (warp, read back) reads back exactly -- at this scale (<1)
#        the warp lands on a whole point, so it drifted (-2,-1) px per frame
#   OFF: M64_NO_CGL_FULLSCREEN_BRIDGE=1 -> no surface, the Carbon window stays
set -u
cd "$(dirname "$0")"
BIN=build/99_agl_window_drawable.x86_64
if [ ! -x "$BIN" ]; then
  echo "agl-oversize-window: SKIP ($BIN missing; run \`make build-only\` first)"; exit 0
fi
arm() {   # prints "<surface exists> <carbon on-screen> <warp round-trip>"
  OUT=$(mktemp)
  env AGL_OVERSIZE=1 "$@" perl -e 'alarm 15; exec @ARGV' "$BIN" >"$OUT" 2>&1 &
  local runner=$!
  sleep 2.5
  python3 - "$runner" <<'PY'
import sys, Quartz
pid = int(sys.argv[1]); surf = carb = 0
for w in Quartz.CGWindowListCopyWindowInfo(Quartz.kCGWindowListOptionAll, 0):
    if w.get('kCGWindowOwnerPID') != pid: continue
    if (w.get('kCGWindowName') or '').startswith('99_agl_window_drawable'): surf = 1
    elif w.get('kCGWindowIsOnscreen') and int(w['kCGWindowBounds']['Width']) == 4000: carb = 1
print(surf, carb, end=" ")
PY
  wait $runner 2>/dev/null
  w=$(sed -n 's/^warp_roundtrip=\([01]\).*/\1/p' "$OUT" | head -1); echo "${w:-x}"; rm -f "$OUT"
}
fail=0
read -r s c w <<<"$(arm)"
if [ "$s" = 1 ] && [ "$c" = 0 ] && [ "$w" = 1 ]; then echo "  ON : scaled surface up, oversized Carbon window hidden, warp round-trips  OK"
else echo "  ON : expected surface=1 carbon=0 warp=1, got surface=$s carbon=$c warp=$w"; fail=1; fi
read -r s c w <<<"$(arm M64_NO_CGL_FULLSCREEN_BRIDGE=1)"
if [ "$s" = 0 ] && [ "$c" = 1 ]; then echo "  OFF: no surface, oversized Carbon window stays  OK"
else echo "  OFF: expected surface=0 carbon=1, got surface=$s carbon=$c"; fail=1; fi
[ $fail = 0 ] && echo "agl-oversize-window: PASS" || echo "agl-oversize-window: FAIL"
exit $fail
