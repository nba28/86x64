#!/bin/bash
#
# agl_renderer_info_test.sh — A/B guard proving the classic GPU-ENUMERATION
# substrate (src/abiconv/agl_renderer_shim.c + the Display Manager half in
# src/abiconv/carbon_ui_shim.c, bridged by qd_gworld.c's GDevice registry) is
# what lets a translated 32-bit-era 3D app see the machine's GPU.
#
# On 64-bit macOS the classic sequence
#
#     gd = DMGetFirstScreenDevice(true);
#     DMGetDisplayIDByGDevice(gd, &displayID, true);
#     ri = aglQueryRendererInfo(&gd, 1);
#
# was dead twice over: aglQueryRendererInfo takes a QuickDraw GDHandle and
# returns NULL for EVERY input (measured — it does not even set an AGL error),
# and DMGetDisplayIDByGDevice was shimmed to paramErr.  An app therefore
# concludes the machine has no accelerated GPU.  Halo CE's
# IDirect3D_Mac::IDirect3D_Mac raises its own "An unrecoverable error has
# occurred" alert on either one (i386 0x2b477f / 0x2b47a5).
#
# The main-suite fixture 99_agl_renderer_info runs the ON side (a real
# accelerated renderer with a real renderer id and video-memory figure).  This
# script adds the OFF side: re-run the SAME translated binary with both kill
# switches and assert the pre-fix failure comes back.  Without that half, a
# passing fixture would not prove these shims are what fixed it.
#
# Needs the i386 sysroot (it reuses the main suite's translated binary); SKIPs
# when the binary has not been built.
set -u
cd "$(dirname "$0")"

BIN=build/99_agl_renderer_info.x86_64
if [ ! -x "$BIN" ]; then
  echo "agl-renderer-info: SKIP (build/99_agl_renderer_info.x86_64 missing;"
  echo "                         run \`make 99_agl_renderer_info\` first)"
  exit 0
fi

fail=0
has() { printf '%s\n' "$1" | grep -q "^$2\$"; }

# --- ON: GDevice -> CGDirectDisplayID -> a REAL accelerated renderer ---------
on=$("$BIN" 2>/dev/null)
if has "$on" 'outdef=1'  && has "$on" 'gdevice=1' && has "$on" 'displayid=1' &&
   has "$on" 'info=1'    && has "$on" 'accel=1'   && has "$on" 'props=1'; then
  echo "  ON  (shims armed):   DMGetDisplayIDByGDevice -> real display, aglQueryRendererInfo"
  echo "                       -> accelerated renderer with id/VRAM/texture memory        OK"
else
  echo "  ON  (shims armed):   expected a real display id and an accelerated renderer, got:"
  printf '%s\n' "$on" | sed 's/^/      /'
  fail=1
fi

# --- OFF: kill switches -> the pre-fix "this machine has no GPU" ------------
off=$(M64_NO_AGL_RENDERERINFO=1 M64_NO_DM_DISPLAYID=1 "$BIN" 2>/dev/null)
# outdef is an invariant of the shim itself (rule A: a failing shim still leaves
# the caller's out-param defined), so it must hold in BOTH arms -- it is
# deliberately NOT part of what the kill switch changes.
if has "$off" 'outdef=1'  && has "$off" 'gdevice=1' && has "$off" 'displayid=0' &&
   has "$off" 'info=0'    && has "$off" 'accel=0'   && has "$off" 'props=0'; then
  echo "  OFF (kill switches): DMGetDisplayIDByGDevice -> paramErr, aglQueryRendererInfo"
  echo "                       -> NULL, zero renderers (the unfixed behaviour)           OK"
else
  echo "  OFF (kill switches): expected paramErr and a NULL renderer list (the unfixed"
  echo "                       behaviour); the guard is NOT exercising the enumeration"
  echo "                       substrate. Got:"
  printf '%s\n' "$off" | sed 's/^/      /'
  fail=1
fi

if [ "$fail" = 0 ]; then
  echo "agl-renderer-info: PASS"
else
  echo "agl-renderer-info: FAIL"
fi
exit $fail
