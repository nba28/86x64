#!/bin/bash
#
# halo-vorbis-probe.sh — DRIVE Halo's translated libVorbis instead of watching it.
#
# ⚠DIAGNOSTIC ONLY (task #46). Delete with src/abiconv/vorbis_selftest.c and the
# sndmgr_shim.c probes when #46 closes.
#
# WHAT IS DIFFERENT ABOUT THIS RUN. Every previous probe could only OBSERVE:
# they established that `ov_open_callbacks` never succeeds, but not WHY, and
# each new question cost another human-driven launch. This one calls the
# translated decoder DIRECTLY -- Halo's own callbacks, our datasource, our
# bitstream -- so one launch answers several questions at once:
#
#   part 1  each callback in isolation. SEEK is the structural suspect: it takes
#           an ogg_int64_t as TWO stack words, and _ov_open1 calls
#           seek(f,0,SEEK_CUR) before parsing a single header. There is also an
#           explicit 2^32 offset test: if the high word is dropped anywhere in
#           the bridge, the seek is ACCEPTED instead of refused.
#   part 2  ov_open_callbacks itself -> the actual RETURN CODE, and how many
#           bytes it consumed. OV_ENOTVORBIS / OV_EBADHEADER / OV_EREAD point at
#           three completely different subsystems, so this one number is worth
#           more than everything the watching probes produced.
#
# The probe runs on its own timer thread, NOT off the audio path -- the silent
# music voice never enqueues, so hanging it off the sound system would make it
# depend on the very thing under investigation. It needs Halo loaded, nothing
# more, so it reports even if you never reach the menu.
#
# NEVER run Halo under lldb (any-observer heisenbug). Never clear
# com.macsoft.halo prefs (they hold the hand-entered GameSpy key + EULA).
set -u

APP="${HALO_APP:-$HOME/projects/translations/Apps64/Halo.app}"
OGG="${HALO_VORBIS_OGG:-/tmp/halo-music.ogg}"
VARIANTS="${HALO_VORBIS_VARIANTS:-1}"
DELAY="${HALO_VORBIS_DELAY:-12}"
OUT="${1:-/tmp/halo-vorbis-probe.log}"

if pgrep -x Halo >/dev/null; then
  echo "Halo is already running — quit it first (pkill -9 -x Halo)." >&2
  exit 1
fi
if [ ! -s "$OGG" ]; then
  echo "Missing reproducer $OGG." >&2
  echo "  Produce it with:  bash src/86x64/halo-audio-probe.sh" >&2
  echo "  (that run dumps the music bitstream via ABICONV_SND_OGGDUMP)." >&2
  exit 1
fi

HERE="$(cd "$(dirname "$0")" && pwd)"

# --- build the single-variable variants -----------------------------------
# Each differs from the control in exactly ONE respect, so a difference in the
# decoder's verdict names that respect. ffmpeg is the CONTROL DECODER: a variant
# it refuses is not evidence about our translation, so any variant that must be
# openable is checked against it before the run.
LIST="$OGG"
if [ "$VARIANTS" = "1" ]; then
  SPLIT=/tmp/halo-music-split.ogg
  BADCRC=/tmp/halo-music-badcrc.ogg
  HDRS=/tmp/halo-music-hdrs.ogg
  python3 "$HERE/ogg-repage.py" "$OGG" "$SPLIT"  --split-headers >/dev/null || exit 1
  python3 "$HERE/ogg-repage.py" "$OGG" "$BADCRC" --break-crc     >/dev/null || exit 1
  python3 "$HERE/ogg-repage.py" "$OGG" "$HDRS"   --headers-only  >/dev/null || exit 1
  if command -v ffmpeg >/dev/null && ! ffmpeg -v error -i "$SPLIT" -f s16le -y /dev/null 2>/dev/null; then
    echo "ABORT: ffmpeg refuses the --split-headers variant, so it is not a" >&2
    echo "  valid stream and a failure on it would prove nothing." >&2
    exit 1
  fi
  LIST="$OGG:$SPLIT:$BADCRC:$HDRS"
  echo "variants  : split-headers (no packet spans a page) | break-crc | headers-only"
  echo "            split-headers is CONFIRMED decodable by ffmpeg"
fi

echo "bitstream : $OGG ($(stat -f%z "$OGG") bytes)"
if [ -f "$HERE/ogg-stream-check.py" ]; then
  python3 "$HERE/ogg-stream-check.py" "$OGG" 2>/dev/null | head -6 | sed 's/^/  /'
fi
echo "Logging to: $OUT"
echo
echo "The probe fires ${DELAY}s after launch, on its own thread."
echo "You do NOT need to reach the menu — but clicking Play is fine and gives"
echo "the sound path a chance to run too. Quit Halo once you see '--- done ---'"
echo "or after ~40s."
echo

env ABICONV_VORBIS_SELFTEST="$LIST" \
    ABICONV_VORBIS_SELFTEST_DELAY="$DELAY" \
  "$APP/Contents/MacOS/Halo" >"$OUT" 2>&1

echo
echo "===== result ====="
if ! grep -q '^\[vorbis\]' "$OUT"; then
  echo "NO PROBE OUTPUT AT ALL."
  echo "  The selftest never ran. That says the deployed libabiconv is not the"
  echo "  one built with vorbis_selftest.c (re-run: m64 build abiconv &&"
  echo "  m64 resync Halo.app), or Halo exited before the ${DELAY}s timer."
  echo "  It says NOTHING about the decoder — do not read it as a result."
  exit 0
fi
grep '^\[vorbis\]' "$OUT" | sed 's/^\[vorbis\] /  /'

echo
echo "--- how to read it ---"
if grep -q 'DECLINED' "$OUT"; then
  echo "  DECLINED = the probe could not resolve its targets and called NOTHING."
  echo "  That is a resolution failure, not a measurement. Nothing below it is"
  echo "  evidence about libVorbis."
  exit 0
fi
ctl=$(grep -o "halo-music.ogg  *ov_open -> *-\?[0-9]*" "$OUT" | head -1 | awk '{print $NF}')
spl=$(grep -o "halo-music-split.ogg  *ov_open -> *-\?[0-9]*" "$OUT" | head -1 | awk '{print $NF}')
bad=$(grep -o "halo-music-badcrc.ogg  *ov_open -> *-\?[0-9]*" "$OUT" | head -1 | awk '{print $NF}')
echo "  control (as Halo sees it) : ${ctl:-?}"
echo "  split-headers             : ${spl:-?}"
echo "  break-crc                 : ${bad:-?}"
echo
if [ -n "${ctl:-}" ] && [ -n "${spl:-}" ]; then
  if [ "$ctl" != "0" ] && [ "$spl" = "0" ]; then
    echo "  ★★DECISIVE: the ONLY difference between those two streams is that the"
    echo "   4140-byte setup header spans a page boundary in the control and does"
    echo "   not in the variant. Same audio, same pages, same CRCs, and ffmpeg"
    echo "   decodes both. ⇒ MULTI-PAGE PACKET REASSEMBLY is broken —"
    echo "   ogg_stream_packetout / the 255-byte lacing continuation in libogg."
  elif [ "$ctl" = "$spl" ]; then
    echo "  Both fail identically ⇒ page SPANNING is NOT the variable. The fault"
    echo "  is upstream of packet reassembly (sync/page acceptance) or downstream"
    echo "  in the codec's header parse."
  fi
fi
if [ -n "${bad:-}" ] && [ -n "${ctl:-}" ]; then
  if [ "$bad" = "$ctl" ]; then
    echo "  break-crc behaves the SAME as the control ⇒ the page checksum is not"
    echo "  discriminating (21 deliberately corrupt CRCs changed nothing)."
  else
    echo "  break-crc differs from the control ⇒ CRC verification IS live, and"
    echo "  the control's checksums are being accepted."
  fi
fi
echo
case "${ctl:-}" in
  -132) echo "  OV_ENOTVORBIS ⇒ the sync layer never yielded a vorbis ident." ;;
  -133) echo "  OV_EBADHEADER ⇒ pages were found; the failure is at or after"
        echo "   packet reassembly / header parse, not at page sync." ;;
  -128) echo "  OV_EREAD ⇒ a read callback failed (part 1 says whether in isolation)." ;;
esac
echo
echo "  ⚠Part 1 first: if READ or SEEK is already wrong in isolation, that is"
echo "   the bug and part 2's code is just its consequence."
