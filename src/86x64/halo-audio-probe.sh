#!/bin/bash
#
# halo-audio-probe.sh — capture Halo's classic Sound Manager traffic across a
# launch, including the WHOLE-BUFFER DirectSound probe.
#
# WHY A SCRIPT AND NOT A DRIVER-RUN: Halo shows its graphics-settings dialog on
# every launch and will not reach the main menu (and so never plays menu music)
# until a human clicks Play. Synthetic input is off the table here — it goes to
# whatever app is frontmost, and Halo is an any-observer heisenbug target. So
# the run has to be started by the person who is going to click, with the trace
# environment already in place. Run this, click Play, let the menu sit for ~20s,
# then quit Halo or press Ctrl-C.
#
# WHAT IT ANSWERS. Every one of 4798 buffers Halo enqueued measured peak=0.
# That is consistent with two very different worlds:
#   * the producer never wrote anywhere  -> the defect is upstream of
#     DirectSound and our audio path is exonerated;
#   * it wrote elsewhere and our cursor arithmetic reads a silent region
#     -> the defect is ours.
# The dsprobe line peaks the ENTIRE backing buffer and says which.
#
# The probe refuses to report unless base + play == samplePtr and the declared
# size covers the window — that identity IS the layout proof, so a wrong guess
# reports "layout NOT confirmed" instead of a misleading number.
#
# NEVER run Halo under lldb (any-observer heisenbug; `sample` on a hang is
# fine). Never clear com.macsoft.halo prefs — they hold the hand-entered
# GameSpy key and EULA acceptance.

set -u

APP="${HALO_APP:-$HOME/projects/translations/Apps64/Halo.app}"
OUT="${1:-/tmp/halo-audio-probe.log}"

if pgrep -x Halo >/dev/null; then
  echo "Halo is already running — quit it first (pkill -9 -x Halo)." >&2
  exit 1
fi

ASSETS="${OUT%.log}.assets.log"
HERE="$(cd "$(dirname "$0")" && pwd)"

echo "Logging to: $OUT"
echo "Asset snapshot: $ASSETS"
echo "Click Play in the settings dialog, let the main menu sit ~20s, then quit Halo."
echo

# Auto-snapshot the asset probe once the engine is past the settings dialog.
# MEASURED: no .map file is open at the dialog stage — the asset cache is only
# opened after Play — so "a .map appeared" is a reliable signal that the engine
# initialised, and its ABSENCE after the timeout is itself the answer (the open
# failed and 0xc3ee4 silently disabled the cache). Pure lsof polling: it never
# signals or stops Halo, so it is safe against the any-observer heisenbug.
(
  for _ in $(seq 1 90); do
    pid=$(pgrep -x Halo | head -1) || true
    [ -z "${pid:-}" ] && { sleep 1; continue; }
    if lsof -p "$pid" 2>/dev/null | grep -qi '\.map'; then break; fi
    sleep 1
  done
  sleep 3
  bash "$HERE/halo-asset-probe.sh" >"$ASSETS" 2>&1
) &
WATCHER=$!

ABICONV_SND_TRACE=1 ABICONV_SND_DSPROBE=1 ABICONV_SND_VOICEPROBE=1 \
  "$APP/Contents/MacOS/Halo" >"$OUT" 2>&1

wait "$WATCHER" 2>/dev/null || true

echo
echo "===== summary ====="
printf 'SndNewChannel        %s\n' "$(grep -c 'SndNewChannel'  "$OUT")"
printf 'buffers enqueued     %s\n' "$(grep -c '^\[snd\] enqueue ' "$OUT")"
printf 'silent windows       %s\n' "$(grep -c 'SILENT BUFFER'  "$OUT")"
echo
echo "--- whole-buffer probe ---"
if ! grep -q 'dsprobe' "$OUT"; then
  echo "no dsprobe lines — Halo never reached the music (did the menu come up?)"
else
  grep 'dsprobe' "$OUT" | sort -u | head -8
fi
echo
echo "--- silence split by SAMPLE RATE (22050 = SFX, 44100 = music) ---"
for r in 22050 44100; do
  t=$(grep -c "rate=$r" "$OUT"); s=$(grep "rate=$r" "$OUT" | grep -c SILENT)
  printf '  %-6s Hz : %5s buffers, %5s silent, %5s audible\n' "$r" "$t" "$s" "$((t-s))"
done
echo
echo "--- per-VOICE record (264600 = the CONTROL, 529200 = the music) ---"
# Report each ring class SEPARATELY. A plain `sort -u | head` sorts 264600
# before 529200 and silently truncates the music away — which is exactly the
# class the run exists to measure. Never let one class hide the other.
if ! grep -q 'voiceprobe' "$OUT"; then
  echo "  no voiceprobe lines — Halo never reached the music"
else
  for r in 264600 529200; do
    n=$(grep -c "voiceprobe: ring=$r" "$OUT")
    printf '  ring=%-7s %5s lines; distinct source=%s ; distinct wr_end=%s\n' \
      "$r" "$n" \
      "$(grep "voiceprobe: ring=$r" "$OUT" | grep -o 'source=0x[0-9a-f]*' | sort -u | tr '\n' ' ')" \
      "$(grep "voiceprobe: ring=$r" "$OUT" | grep -o 'wr_end=-\?[0-9]*' | sort -u | tr '\n' ' ')"
  done
  echo "  (wr_end=-1 means the voice is IDLE)"
  grep -c "no voice record owns" "$OUT" | sed 's/^/  declined (no matching record): /'
fi
echo
echo "--- asset probe: was sounds.map ever opened and READ? ---"
if [ -s "$ASSETS" ]; then
  sed -n '/== .map assets/,/^$/p' "$ASSETS"
else
  echo "no asset snapshot (watcher never saw the engine open a .map)"
fi
