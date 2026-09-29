#!/bin/bash
# display_point_modes_test.sh — guard: every display-mode list (classic Display
# Manager, CGDisplayAvailableModes, CGDisplayCopyAllDisplayModes) describes the
# desktop in POINTS and only lists sizes that fit (the pre-Mojave Retina
# contract, cgdisp_point_modes). Headless: lists only, no window, no switch.
#   ON : all three non-empty, every mode fits, DM record tree well-formed, 800x600 offered
#   OFF: M64_NO_DISPLAY_FULLSCREEN_BRIDGE=1 -> the old lists (DM empty; the
#        modern list only has modes larger than a Retina desktop)
set -u
cd "$(dirname "$0")"
BIN=build/99_display_point_modes.x86_64
[ -x "$BIN" ] || { echo "display-point-modes: FAIL ($BIN missing)"; exit 1; }
fail=0
has() { printf '%s\n' "$1" | tr ' ' '\n' | grep -qx "$2"; }
on=$("$BIN" 2>/dev/null); rc=$?
ok=1; for k in dm_count=1 dm_fit=1 dm_tree=1 dm_800=1 avail_count=1 avail_fit=1 copyall_count=1 copyall_fit=1; do has "$on" $k || ok=0; done
if [ $rc = 0 ] && [ $ok = 1 ]; then
  echo "  ON : DM / AvailableModes / CopyAllDisplayModes all list point modes that fit  OK"
  printf '%s\n' "$on" | grep desktop= | sed 's/^/      /'
else echo "  ON : rc=$rc"; printf '%s\n' "$on" | sed 's/^/      /'; fail=1; fi
off=$(M64_NO_DISPLAY_FULLSCREEN_BRIDGE=1 "$BIN" 2>/dev/null); rc=$?
if [ $rc = 0 ] && has "$off" dm_count=0; then
  echo "  OFF: kill switch -> the old empty Display Manager list                         OK"
  printf '%s\n' "$off" | grep desktop= | sed 's/^/      /'
else echo "  OFF: expected dm_count=0, rc=$rc"; printf '%s\n' "$off" | sed 's/^/      /'; fail=1; fi
[ $fail = 0 ] && echo "display-point-modes: PASS" || echo "display-point-modes: FAIL"
exit $fail
