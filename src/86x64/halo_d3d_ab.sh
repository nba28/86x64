#!/bin/bash
#
# halo_d3d_ab.sh — live A/B for the classic GPU-enumeration fix on the REAL Halo
# binary (agl_renderer_shim.c + the DM* half in carbon_ui_shim.c).
#
# Halo CE reaches its own "An unrecoverable error has occurred, and Halo cannot
# continue" alert DETERMINISTICALLY, from IDirect3D_Mac::IDirect3D_Mac: on
# 64-bit, aglQueryRendererInfo(GDHandle) returns NULL for every input, so the
# accelerated-renderer count is 0 and the guard at i386 0x2b477f raises
# FatalErrorNotCaught 'renderer count changed'.
#
# ONE variable differs between the arms: the kill switches. Same bytes, same
# bundle, same click.
#
#   ./halo_d3d_ab.sh on    # fix armed
#   ./halo_d3d_ab.sh off   # M64_NO_AGL_RENDERERINFO=1 M64_NO_DM_DISPLAYID=1
#
# The verdict is the presence of Halo's own alert window, which is what the
# failure actually looks like (the OS crash reporter never fires: Halo catches
# it itself). winlist reports every WindowServer window owned by Halo.
#
# ⚠ Halo is a SINGLETON: pkill -9 -x Halo (EXACT name). NEVER -f.
# ⚠ Synthetic input goes to the FRONTMOST app and is silently dropped when the
#   screen is locked — this script refuses to run in that state.
set -u
ARM="${1:-on}"
SCR="${SCR:-$(dirname "$0")}"          # where winlist / clicker / activate live
APP="${APP:-$HOME/projects/translations/Apps64/Halo.app}"
LOG="${LOG:-/tmp/halo_d3d_$ARM.log}"

locked=$(python3 -c "
import ctypes,ctypes.util
cg=ctypes.CDLL(ctypes.util.find_library('CoreGraphics'))
cf=ctypes.CDLL(ctypes.util.find_library('CoreFoundation'))
cg.CGSessionCopyCurrentDictionary.restype=ctypes.c_void_p
cf.CFStringCreateWithCString.restype=ctypes.c_void_p
cf.CFDictionaryGetValue.restype=ctypes.c_void_p
cf.CFBooleanGetValue.restype=ctypes.c_ubyte
d=cg.CGSessionCopyCurrentDictionary()
k=cf.CFStringCreateWithCString(None,b'CGSSessionScreenIsLocked',0x08000100)
v=cf.CFDictionaryGetValue(ctypes.c_void_p(d),ctypes.c_void_p(k)) if d else None
print(1 if (v and cf.CFBooleanGetValue(ctypes.c_void_p(v))) else 0)")
if [ "$locked" = 1 ]; then
  echo "SCREEN IS LOCKED — synthetic input cannot reach any app. Aborting."; exit 2
fi

pkill -9 -x Halo 2>/dev/null; sleep 1

ENVARGS=(--env M64_AB_ARM="$ARM")     # harmless marker; also keeps the array non-empty
if [ "$ARM" = off ]; then
  ENVARGS+=(--env M64_NO_AGL_RENDERERINFO=1 --env M64_NO_DM_DISPLAYID=1)
fi
[ "${TRACE:-0}" = 1 ] && ENVARGS+=(--env ABICONV_AGL_TRACE=1)

# EXEC=1 launches Contents/MacOS/Halo DIRECTLY instead of through `open`, so the
# shell owns the process and `wait` yields the real WAIT STATUS.  That is the only
# way to tell an exit(N) from a fatal signal for a process that leaves no crash
# report.  Everything else about the run is identical.  EXTRA_ENV="A=1 B=2" adds
# env vars in both launch modes.
RUN_EPOCH=$(python3 -c "import time;print(time.time())")
: > "$LOG"
if [ "${EXEC:-0}" = 1 ]; then
  # `exec` replaces the subshell so $! IS Halo's own pid (otherwise we would wait
  # on the subshell and learn nothing about how Halo died).
  ( cd "$APP/Contents/MacOS" && exec env ${EXTRA_ENV:-} M64_AB_ARM="$ARM" \
      $( [ "$ARM" = off ] && echo M64_NO_AGL_RENDERERINFO=1 M64_NO_DM_DISPLAYID=1 ) \
      $( [ "${TRACE:-0}" = 1 ] && echo ABICONV_AGL_TRACE=1 ) \
      ./Halo ) >"$LOG" 2>&1 &
  HALOPID=$!
else
  for kv in ${EXTRA_ENV:-}; do ENVARGS+=(--env "$kv"); done
  open -a "$APP" --stderr "$LOG" --stdout "$LOG" "${ENVARGS[@]}"
  HALOPID=
fi
# Poll for the Graphics Settings window instead of a fixed sleep: startup time
# varies a lot between `open` and a direct exec, and clicking before the window
# exists silently does nothing (that produced a false "still running" once).
for i in $(seq 1 90); do
  sleep 1
  "$SCR/winlist" Halo 2>/dev/null | grep -q "'Halo Graphics Settings'" && break
done
echo "== Graphics Settings window seen after ${i}s =="

# ⚠ A DIRECTLY-EXEC'd binary has no NSRunningApplication (the `activate` helper
# reports "pid 0"), so it cannot be fronted that way — and a synthetic click that
# lands while another app is frontmost is silently dropped.  System Events works
# for both launch modes, so front it with BOTH and verify.
"$SCR/activate" Halo >/dev/null 2>&1
osascript -e 'tell application "System Events" to set frontmost of process "Halo" to true' >/dev/null 2>&1
sleep 1
echo "== frontmost now: $(osascript -e 'tell application "System Events" to get name of first application process whose frontmost is true' 2>/dev/null) =="

# Compute OK's CG-global centre from the LIVE Graphics Settings window bounds
# rather than a hardcoded point. From the xib: Graphics window id=236, content
# 468x559, OK button bounds {top,left,bottom,right} = {519,368,539,448} -> its
# centre is (408,529) in CONTENT coords. CGWindow bounds include the 22pt title
# bar (live: 468x581 = 559+22), so content origin = (wx, wy+22).
# ⚠ Read the geometry until it STOPS MOVING. Halo repositions this window shortly
# after showing it, and a click computed from the pre-move bounds lands on the
# desktop and is silently lost (observed: read 1087,140 / clicked / window was
# already at 1016,143, and the run looked like "no effect" rather than "missed").
readgeom() { "$SCR/winlist" Halo 2>/dev/null | sed -n "s/.*'Halo Graphics Settings'.*bounds=(\([0-9-]*\),\([0-9-]*\) \([0-9]*\)x\([0-9]*\)).*/\1 \2 \3 \4/p" | head -1; }
geom=$(readgeom)
for i in 1 2 3 4 5 6 7 8; do
  sleep 1; g2=$(readgeom)
  [ -n "$g2" ] && [ "$g2" = "$geom" ] && break
  geom="$g2"
done
if [ -n "$geom" ]; then
  set -- $geom; wx=$1; wy=$2; ww=$3; wh=$4
  cx=$(( wx + 408 ))
  cy=$(( wy + 22 + 529 ))
else
  wx=?; wy=?; ww=?; wh=?
  cx=567; cy=721                      # recorded fallback (halo_target.md)
  echo "== WARNING: no Graphics Settings geometry; winlist said: =="
  "$SCR/winlist" Halo 2>&1 | head -10
fi
echo "== arm=$ARM  clicking OK at ($cx,$cy)  [graphics window at ${wx:-?},${wy:-?} ${ww:-?}x${wh:-?}]"
"$SCR/clicker" "$cx" "$cy" >/dev/null 2>&1
sleep 8

echo "== windows after the click =="
"$SCR/winlist" Halo 2>/dev/null
echo "== alive? =="
pgrep -x Halo >/dev/null && echo "Halo STILL RUNNING (pid $(pgrep -x Halo))" || echo "Halo EXITED"
if [ -n "${HALOPID:-}" ]; then
  if kill -0 "$HALOPID" 2>/dev/null; then
    echo "== wait status: still running (pid $HALOPID) =="
  else
    wait "$HALOPID"; st=$?
    if [ "$st" -gt 128 ]; then
      echo "== WAIT STATUS: KILLED BY SIGNAL $(( st - 128 )) ($(kill -l $(( st - 128 )) 2>/dev/null)) =="
    else
      echo "== WAIT STATUS: exit($st) =="
    fi
  fi
fi
if [ -s "$LOG" ]; then echo "== stderr (tail) =="; tail -40 "$LOG"; fi

# ---- CRASH-SIGNATURE VERDICT ------------------------------------------------
# ★On a Halo GUI A/B the verdict is the crash-report SIGNATURE, never window
# presence or "still running": Halo's license-key modal is invisible in a
# screenshot but grabs all input, so a click can be swallowed and the run merely
# beeps.  ⚠A SANDBOXED `ls ~/Library/Logs/DiagnosticReports/` returns EMPTY WITH
# NO ERROR — that once faked a "no crash report" conclusion — so this uses find.
echo "== crash-report verdict =="
RUN_START_FILE=${RUN_START_FILE:-}
python3 - "$RUN_EPOCH" <<'PY'
import glob, json, os, sys, time
start = float(sys.argv[1])
reps = [p for p in glob.glob(os.path.expanduser(
            "~/Library/Logs/DiagnosticReports/Halo-*.ips")) if os.path.getmtime(p) >= start]
if not reps:
    print("  no Halo crash report written during this run")
    raise SystemExit
for p in sorted(reps, key=os.path.getmtime):
    raw = open(p).read(); i = raw.index("\n")
    body = json.loads(raw[i:])
    imgs = body.get("usedImages", [])
    frames = []
    for t in body.get("threads", []):
        if t.get("triggered"):
            for f in t["frames"]:
                nm = imgs[f["imageIndex"]].get("name") or "?"
                frames.append("%s %s" % (nm, f.get("symbol") or ""))
    blob = "\n".join(frames)
    sig = "UNCLASSIFIED"
    if "NSCGSPanic" in blob and "CGDisplaySwitchToMode" in blob:
        sig = "THE DISPLAY-SWITCH WALL (NSCGSPanic <- __CGDisplaySwitchToMode)"
    elif "NSCGSPanic" in blob and ("setSize:" in blob or "SetBounds" in blob):
        sig = "THE NSCarbonWindow GEOMETRY WALL (NSCGSPanic <- setSize:, no display API)"
    elif "NSCGSPanic" in blob:
        sig = "NSCGSPanic, some OTHER path"
    elif "fr_handler" in blob:
        sig = "an M64_FAULT_REPORT=1 armed run, not a distinct crash"
    elif frames and frames[0].startswith("?"):
        sig = "the translated-code fault family (rip in translated text)"
    print("  %s\n    signature: %s" % (os.path.basename(p), sig))
    for f in frames[:8]:
        print("      " + f)
PY
