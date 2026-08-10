#!/bin/bash
#
# halo-asset-probe.sh — does Halo ever OPEN and READ its sound asset?
#
# WHY THIS EXISTS. The whole-buffer audio probe proved the 529200-byte
# DirectSound streaming ring is untouched across a full run while the play
# cursor advances normally, so the defect is upstream of DirectSound: Halo's
# software mixer (i386 original @0x248cf2) either never runs or bails out. Both
# of its silent-exit paths converge on the same upstream question — was the
# sound data ever loaded?
#
#   - Lock failure:  `hr = buf->Lock(...); if (hr < 0) goto 0x249a98;`
#                    jumps past BOTH the mixing loop and the Unlock, returns 0,
#                    writes nothing, logs nothing.
#   - No source:     `eax = [R+0x94]; if (!eax) skip this voice;`
#                    every voice contributes nothing if its data never loaded.
#
# Halo opens the asset as a WIN32 path: 0xc3ee4 does
# sprintf(buf, "maps\\%s.map", name) for name = "bitmaps" and "sounds", then its
# Win32 CreateFileA emulation (0x2b2aee, OPEN_ALWAYS) resolves it through
# CFStringCreateWithCString and opens with FSOpenFork; reads go through
# FSReadFork. If the open returns -1 or the 16-byte header read comes up short,
# 0xc3ee4 SILENTLY DISABLES that whole cache (0xc3f61 / 0xc3f91).
#
# ★WHY lsof AND NOT fs_usage: fs_usage needs sudo. lsof does NOT, for a process
# owned by the same uid — and it is a pure observer (it never stops or signals
# the target), so it is safe against Halo's any-observer heisenbug in a way lldb
# and in-process fprintf are not. `-o` prints the file OFFSET, so this shows not
# just whether the file is open but whether anything was ever read from it.
#
# USAGE: launch Halo by hand, let it reach the main menu, then run this. It
# NEVER launches Halo, never sends input, and never signals the process.
set -u

PID=$(pgrep -x Halo | head -1)
if [ -z "$PID" ]; then
  echo "halo-asset-probe: Halo is not running. Launch it, reach the main menu, then re-run."
  exit 1
fi
echo "halo-asset-probe: Halo pid=$PID"
echo

echo "== .map assets currently OPEN (FD, offset, size, path) =="
# -o gives the file offset; a nonzero offset means the file has actually been read.
if ! lsof -o -p "$PID" 2>/dev/null | grep -i '\.map' ; then
  echo "  (none — Halo has NO .map file open)"
fi
echo

echo "== every regular file Halo has open under GameData =="
lsof -p "$PID" 2>/dev/null | grep -i 'GameData' || echo "  (none)"
echo

echo "== the assets on disk, for comparison =="
BUNDLE=$(lsof -p "$PID" 2>/dev/null | awk '/Halo\.app.*MacOS\/Halo$/ {print $NF; exit}')
if [ -n "${BUNDLE:-}" ]; then
  MAPS="$(dirname "$(dirname "$BUNDLE")")/Resources/GameData/Maps"
  ls -l "$MAPS"/sounds.map "$MAPS"/bitmaps.map "$MAPS"/ui.map 2>/dev/null || echo "  (could not locate $MAPS)"
fi
echo

echo "== interpretation =="
echo "  sounds.map OPEN with a nonzero offset -> the asset IS being read; the defect is"
echo "     downstream in the decode/mixer (next: the per-voice source pointer R+0x94)."
echo "  sounds.map ABSENT, or open at offset 0 -> the cache open/header read failed and"
echo "     0xc3ee4 silently disabled the sound cache; the defect is in the Win32"
echo "     CreateFileA path resolution (maps\\sounds.map) or FSOpenFork/FSReadFork."
echo "  ★Compare against bitmaps.map: the menu renders artwork, so if bitmaps.map is"
echo "     open and sounds.map is not, the two go through the SAME 0xc3ee4 code and the"
echo "     difference is data-dependent, not a broken code path."
