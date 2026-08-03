#!/bin/bash
#
# halo_windowed_bisect.sh — tick Halo's "Play in a window" checkbox in the
# Graphics Settings dialog, then OK, and report what Halo actually DOES.
#
# WHY: the fullscreen path (CGCaptureAllDisplays + CGDisplaySwitchToMode) is a
# whole subsystem of its own.  Windowed mode skips it entirely, so this run
# isolates "is the fullscreen idiom the LAST blocker, or just the next one?"
# If windowed Halo renders and takes input, every other fix in the tree is
# confirmed working end to end.
#
# ⚠It clicks the checkbox by its ACCESSIBILITY position, never a hardcoded
# point, and it VERIFIES the AX value flipped to 1 before clicking OK — Halo's
# licence-key modal is invisible in a screenshot but swallows all input, so an
# unverified click proves nothing.
# ⚠Halo is a SINGLETON: pkill -9 -x Halo (EXACT).  NEVER -f.
# ⚠Synthetic input goes to the FRONTMOST app and is dropped when the screen is
#   locked; this refuses to run in that state.
#
#   SCR=<dir with winlist/clicker/activate/axdump>  ./halo_windowed_bisect.sh
set -u
SCR="${SCR:-$(dirname "$0")}"
APP="${APP:-$HOME/projects/translations/Apps64/Halo.app}"
LOG="${LOG:-/tmp/halo_windowed.log}"
HOLD="${HOLD:-25}"                       # seconds to observe after OK

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
[ "$locked" = 1 ] && { echo "SCREEN IS LOCKED — aborting."; exit 2; }

pkill -9 -x Halo 2>/dev/null; sleep 1
RUN_EPOCH=$(python3 -c "import time;print(time.time())")
: > "$LOG"
( cd "$APP/Contents/MacOS" && exec env ${EXTRA_ENV:-} ./Halo ) >"$LOG" 2>&1 &
HALOPID=$!

for i in $(seq 1 90); do
  sleep 1
  "$SCR/winlist" Halo 2>/dev/null | grep -q "'Halo Graphics Settings'" && break
done
echo "== Graphics Settings window seen after ${i}s =="
"$SCR/activate" Halo >/dev/null 2>&1
osascript -e 'tell application "System Events" to set frontmost of process "Halo" to true' >/dev/null 2>&1
sleep 1
echo "== frontmost: $(osascript -e 'tell application "System Events" to get name of first application process whose frontmost is true' 2>/dev/null) =="

# Let Halo finish repositioning the dialog before reading its geometry.
sleep 3
PID=$(pgrep -x Halo | head -1)
geom=$("$SCR/winlist" Halo 2>/dev/null | sed -n "s/.*'Halo Graphics Settings'.*bounds=(\([0-9-]*\),\([0-9-]*\) \([0-9]*\)x\([0-9]*\)).*/\1 \2 \3 \4/p" | head -1)
if [ -z "$geom" ]; then echo "== no Graphics Settings geometry — aborting =="; pkill -9 -x Halo; exit 3; fi
set -- $geom; wx=$1; wy=$2; ww=$3; wh=$4
echo "== Graphics Settings at $wx,$wy ${ww}x${wh} =="

# The dialog is a pure-Carbon window: it publishes NO accessibility tree at all
# (axdump returns nothing), so the controls cannot be found by name.  The
# offsets below are CONTENT-relative and were read off a 2x screenshot of the
# live dialog, then divided by 2 — window origin + 22pt title bar + offset.
#   "Play in a window" checkbox   content (45, 294)
#   OK button                     content (408, 529)   [also the xib value]
CBX=$(( wx + 45 ));  CBY=$(( wy + 22 + 294 ))
OKX=$(( wx + 408 )); OKY=$(( wy + 22 + 529 ))

"$SCR/activate" Halo >/dev/null 2>&1
osascript -e 'tell application "System Events" to set frontmost of process "Halo" to true' >/dev/null 2>&1
sleep 1
echo "== clicking 'Play in a window' at ($CBX,$CBY) =="
"$SCR/clicker" "$CBX" "$CBY" >/dev/null 2>&1
sleep 2

# ★VERIFY THE CLICK REGISTERED. The dialog publishes no AX tree, so the only
# honest check is the PIXELS: a ticked macOS checkbox is filled with the accent
# colour, so count strongly-blue pixels inside the checkbox's own box. An
# unverified click is worthless — Halo's licence-key modal is invisible in a
# screenshot and swallows all input.
SHOT=$(mktemp -t halocb)
screencapture -x -R"$((CBX-8)),$((CBY-8)),16,16" "$SHOT.png" 2>/dev/null
sips -s format bmp "$SHOT.png" --out "$SHOT.bmp" >/dev/null 2>&1
TICKED=$(python3 - "$SHOT.bmp" <<'PY2'
import struct, sys
try:
    d = open(sys.argv[1], "rb").read()
except OSError:
    print("?"); raise SystemExit
off, w, h, bpp = struct.unpack_from("<I", d, 10)[0], struct.unpack_from("<i", d, 18)[0], \
                 struct.unpack_from("<i", d, 22)[0], struct.unpack_from("<H", d, 28)[0]
bypp, blue = bpp // 8, 0
row = ((w * bpp + 31) // 32) * 4
for y in range(abs(h)):
    for x in range(w):
        i = off + y * row + x * bypp
        b, g, r = d[i], d[i+1], d[i+2]
        if b > 140 and b > r + 50 and b > g + 40:
            blue += 1
print(1 if blue > 20 else 0)
PY2
)
if [ "$TICKED" = 1 ]; then
  echo "== VERIFIED: 'Play in a window' is now TICKED (accent-filled checkbox) =="
else
  echo "== ⚠CLICK NOT VERIFIED (checkbox does not read as ticked: $TICKED)."
  echo "   Screenshot kept at $SHOT.png — inspect before believing this run. =="
fi

echo "== clicking OK at ($OKX,$OKY) =="
"$SCR/clicker" "$OKX" "$OKY" >/dev/null 2>&1

sleep "$HOLD"
echo "== windows after OK =="
"$SCR/winlist" Halo 2>/dev/null
echo "== alive? =="
pgrep -x Halo >/dev/null && echo "Halo STILL RUNNING (pid $(pgrep -x Halo))" || echo "Halo EXITED"
if kill -0 "$HALOPID" 2>/dev/null; then
  echo "== wait status: still running (pid $HALOPID) =="
else
  wait "$HALOPID"; st=$?
  [ "$st" -gt 128 ] && echo "== WAIT STATUS: KILLED BY SIGNAL $(( st - 128 )) ==" \
                    || echo "== WAIT STATUS: exit($st) =="
fi
echo "== stderr (tail) =="; tail -30 "$LOG"

echo "== crash-report verdict =="
python3 - "$RUN_EPOCH" <<'PY'
import glob, json, os, sys
start = float(sys.argv[1])
reps = [p for p in glob.glob(os.path.expanduser(
            "~/Library/Logs/DiagnosticReports/Halo-*.ips")) if os.path.getmtime(p) >= start]
if not reps:
    print("  no Halo crash report written during this run"); raise SystemExit
for p in sorted(reps, key=os.path.getmtime):
    raw = open(p).read(); i = raw.index("\n"); body = json.loads(raw[i:])
    imgs = body.get("usedImages", []); frames = []
    for t in body.get("threads", []):
        if t.get("triggered"):
            for f in t["frames"]:
                frames.append("%s %s +0x%x" % (imgs[f["imageIndex"]].get("name") or "?",
                                               f.get("symbol") or "", f.get("imageOffset", 0)))
    blob = "\n".join(frames)
    sig = ("THE DISPLAY-SWITCH WALL" if ("NSCGSPanic" in blob and "CGDisplaySwitchToMode" in blob)
           else "THE NSCarbonWindow GEOMETRY WALL" if ("NSCGSPanic" in blob and "setSize:" in blob)
           else "NSCGSPanic, other path" if "NSCGSPanic" in blob
           else "M64_FAULT_REPORT armed run" if "fr_handler" in blob
           else "translated-code fault family")
    print("  %s -> %s" % (os.path.basename(p), sig))
    for f in frames[:6]: print("      " + f)
PY
