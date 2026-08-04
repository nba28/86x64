#!/bin/bash
#
# nulljump_probe_arch_test.sh — A/B guard on the INVARIANT that defines the
# NULL-jump exclusion list: no symbol on it may actually resolve in an x86_64
# process.
#
# find_null_jump_bridges.py decides "is this bridge dead?" by dlsym. The answer
# is architecture-dependent, and the list is consumed by static-interpose to
# decide whether an app's bind gets marshalled or left alone — so an answer from
# the wrong arch is not a cosmetic error, it silently changes what every
# translated binary binds to.
#
# ARMS:
#   ON  : default. The probe is compiled -arch x86_64 and run under Rosetta.
#         Every listed symbol must be genuinely absent from an x86_64 process.
#   OFF : M64_NULLJUMP_PROBE_ARCH=native reinstates the in-process ctypes answer
#         from an arm64 python. The invariant must BREAK — otherwise this guard
#         is not exercising anything.
#
# Measured 2026-08-04, the defect this pins down, in both directions:
#   * 33 x86_64-only symbols called dead: objc_msgSend_stret / _fpret /
#     Super_stret and the whole $INODE64 stat/readdir/scandir family. Excluding
#     them from interposition binds them to native x86_64 code called with i386
#     4-byte argument slots.
#   * 63 Python C-API bridges called healthy, because the probing process WAS
#     python and RTLD_DEFAULT found Py_Initialize in the interpreter itself.
#     Civilization IV runs its entire game logic through that API.
set -u
cd "$(dirname "$0")"

LIB=../build/src/abiconv/libabiconv.dylib
FINDER=../src/86x64/find_null_jump_bridges.py
PROBE=../src/86x64/nulljump_probe.c
WORK="${TMPDIR:-/tmp}/nulljump_probe_arch.$$"

if [ ! -f "$LIB" ]; then
  echo "nulljump-probe-arch: SKIP (no $LIB — build libabiconv first)"; exit 0
fi

mkdir -p "$WORK"
trap 'rm -rf "$WORK"' EXIT

# The referee is the same x86_64 probe, built independently of the finder so a
# broken finder cannot also break the check on itself.
if ! clang -arch x86_64 -O0 -o "$WORK/ref" "$PROBE" 2>"$WORK/cc.err"; then
  echo "nulljump-probe-arch: SKIP (cannot build an x86_64 probe)"; sed -n 1,3p "$WORK/cc.err"; exit 0
fi
FW=(/System/Library/Frameworks/{CoreFoundation.framework/CoreFoundation,CoreServices.framework/CoreServices,ApplicationServices.framework/ApplicationServices,CoreGraphics.framework/CoreGraphics,Foundation.framework/Foundation,AppKit.framework/AppKit,Carbon.framework/Carbon,OpenGL.framework/OpenGL,AGL.framework/AGL,ImageIO.framework/ImageIO,AudioToolbox.framework/AudioToolbox,IOKit.framework/IOKit} /usr/lib/libSystem.B.dylib /usr/lib/libobjc.A.dylib)

# live_in_list <list> -> symbols that are on the list yet DO resolve on x86_64
live_in_list() { arch -x86_64 "$WORK/ref" "${FW[@]}" < "$1"; }

fail=0

python3 "$FINDER" --emit "$LIB" > "$WORK/on.txt" 2>"$WORK/on.err"
if [ ! -s "$WORK/on.txt" ]; then
  echo "  ON : the finder produced no list at all"; sed -n 1,5p "$WORK/on.err"; fail=1
else
  live_in_list "$WORK/on.txt" > "$WORK/on.live"
  if [ -s "$WORK/on.live" ]; then
    echo "  ON  (x86_64 probe): $(wc -l < "$WORK/on.live" | tr -d ' ') listed symbols DO resolve on x86_64 —"
    echo "                      they would be excluded from interposition and then called"
    echo "                      with i386 argument slots. e.g. $(head -3 "$WORK/on.live" | tr '\n' ' ')"
    fail=1
  else
    echo "  ON  (x86_64 probe): none of the $(wc -l < "$WORK/on.txt" | tr -d ' ') listed symbols resolve on x86_64  OK"
  fi
fi

M64_NULLJUMP_PROBE_ARCH=native python3 "$FINDER" --emit "$LIB" > "$WORK/off.txt" 2>/dev/null
live_in_list "$WORK/off.txt" > "$WORK/off.live"
if [ -s "$WORK/off.live" ]; then
  echo "  OFF (in-process arm64): $(wc -l < "$WORK/off.live" | tr -d ' ') listed symbols resolve on x86_64 anyway —"
  echo "                      the contaminated answer reproduces                      OK"
else
  echo "  OFF (in-process arm64): expected the arm64 answer to list live x86_64 symbols."
  echo "                      It did not, so this guard proves nothing on this host."
  fail=1
fi

# The other direction: the arm64 answer must also MISS real traps (the Py* set).
missed=$(comm -13 <(sort "$WORK/off.txt") <(sort "$WORK/on.txt") | wc -l | tr -d ' ')
if [ "$missed" -gt 0 ]; then
  echo "  OFF (in-process arm64): and MISSES $missed real traps the x86_64 probe finds  OK"
else
  echo "  OFF (in-process arm64): expected it to miss traps too (the self-contamination"
  echo "                      half of the defect); it missed none"
  fail=1
fi

if [ "$fail" = 0 ]; then echo "nulljump-probe-arch: PASS"; else echo "nulljump-probe-arch: FAIL"; fi
exit $fail
