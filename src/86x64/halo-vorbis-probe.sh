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

env ABICONV_VORBIS_SELFTEST="$OGG" \
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
rc=$(grep -o 'ov_open_callbacks -> -\?[0-9]*' "$OUT" | head -1 | awk '{print $3}')
case "${rc:-}" in
  0)    echo "  ★ov_open SUCCEEDED here but fails inside Halo ⇒ the bitstream and the"
        echo "   decoder are both fine, and the defect is in HOW HALO CALLS IT —"
        echo "   its datasource contents, its timing, or its state. Compare the"
        echo "   ds line above with the cursorprobe's src= fields." ;;
  -128) echo "  OV_EREAD ⇒ a read callback failed. Part 1 says whether read works"
        echo "   in isolation; if it does, the failure is state-dependent." ;;
  -132) echo "  OV_ENOTVORBIS ⇒ the sync layer never found a vorbis ident. With a"
        echo "   BOS page at offset 0 and 0 CRC failures, that points at libogg's"
        echo "   PAGE SYNC — ogg_sync_pageseek / the CRC recompute — not at the"
        echo "   codec. Note how many bytes were consumed: a multiple of 8500"
        echo "   (CHUNKSIZE) means it kept refilling and never matched a page." ;;
  -133) echo "  OV_EBADHEADER ⇒ pages WERE found and unpacked; the failure is in"
        echo "   header parsing (oggpack / vorbis_synthesis_headerin), i.e. past"
        echo "   the sync layer. That exonerates libogg." ;;
  -134) echo "  OV_EVERSION ⇒ the ident packet parsed but its version field read"
        echo "   wrong — a bit-unpacking (oggpack) defect, very narrow." ;;
  "")   echo "  ov_open never reported — read the raw log; the probe may have"
        echo "   crashed partway (each line is flushed, so what printed is real)." ;;
  *)    echo "  See the decoded name above; any negative code localises the"
        echo "   failure to one subsystem." ;;
esac
echo
echo "  ⚠Part 1 first: if READ or SEEK is already wrong in isolation, that is"
echo "   the bug and part 2's code is just its consequence."
