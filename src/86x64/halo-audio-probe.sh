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

echo "Logging to: $OUT"
echo "Click Play in the settings dialog, let the main menu sit ~20s, then quit Halo."
echo

ABICONV_SND_TRACE=1 ABICONV_SND_DSPROBE=1 \
  "$APP/Contents/MacOS/Halo" >"$OUT" 2>&1

echo
echo "===== summary ====="
printf 'SndNewChannel        %s\n' "$(grep -c 'SndNewChannel'  "$OUT")"
printf 'buffers enqueued     %s\n' "$(grep -c '^\[snd\] enqueue ' "$OUT")"
printf 'silent windows       %s\n' "$(grep -c 'SILENT BUFFER'  "$OUT")"
echo
echo "--- whole-buffer probe (the answer) ---"
if ! grep -q 'dsprobe' "$OUT"; then
  echo "no dsprobe lines — Halo never reached the music (did the menu come up?)"
else
  grep 'dsprobe' "$OUT" | sort -u | head -20
fi
