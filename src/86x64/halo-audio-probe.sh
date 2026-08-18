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

# --force-stream arms the ONE diagnostic that MUTATES Halo: it sets R+0x15 = 1
# on the state-2 (streaming) voice, which is the gate on 0x24b934's entire
# mixing payload. Opt-in and separate from the read-only probes, because a
# perturbing run is not a measuring run — never leave it on for a baseline.
FORCE=""
if [ "${1:-}" = "--force-stream" ]; then
  FORCE="ABICONV_SND_FORCE_STREAM_MODE=1"; shift
  echo "⚠ --force-stream: this run MUTATES Halo's voice state (diagnostic)."
fi
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

env ABICONV_SND_TRACE=1 ABICONV_SND_DSPROBE=1 ABICONV_SND_VOICEPROBE=1 \
    ABICONV_SND_CURSORPROBE=1 $FORCE \
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
# ★THE #46 QUESTION (2026-08-17). The mix gate at 0x24c333 reduces to
# `word[R+0x0c] != 0`: the `cmpb $0x1, R+0x15` fallback beneath it can never
# fire, because R+0x15 has four writers in the whole sound subsystem and all
# four store $0x0 (verified on the i386 ORIGINAL). So the state field, sampled
# OVER TIME and for EVERY voice — not just the one owning the ring we were
# handed — is what should separate the audible 22050 class from the silent
# 44100 one. Print the per-voice trajectory, not just a count: a state that
# reaches 0 and stays there is the whole hypothesis.
echo "--- voice STATE trajectories (word[R+0x0c]; 0 = mixer refuses the voice) ---"
if ! grep -q 'voicestate:' "$OUT"; then
  echo "  no voicestate lines — probe not deployed, or Halo never enqueued"
  echo "  (deploy with: m64 resync ~/projects/translations/Apps64/Halo.app)"
else
  for v in $(grep -o 'voicestate: voice=[0-9]*' "$OUT" | grep -o '[0-9]*$' | sort -un); do
    printf '  voice=%-3s %s\n' "$v" \
      "$(grep "voicestate: voice=$v " "$OUT" \
         | grep -oE 'state\(R\+0x0c\)[= ][0-9]*(->[0-9]+)?' \
         | sed 's/.*[= ]//' | tr '\n' ' ')"
    printf '        f15(R+0x15)=%s ; wr_end(R+0x88)=%s ; rings=%s\n' \
      "$(grep "voicestate: voice=$v " "$OUT" | grep -oE 'f15[= ][0-9]+(->[0-9]+)?' | sed 's/^f15[= ]//' | sort -u | tr '\n' ' ')" \
      "$(grep "voicestate: voice=$v " "$OUT" | grep -o 'wr_end(R+0x88)=-\?[0-9]*' | sed 's/.*=//' | sort -u | tr '\n' ' ')" \
      "$(grep "voicestate: voice=$v " "$OUT" | grep -o 'ring=[0-9]*' | sed 's/ring=//' | sort -u | tr '\n' ' ')"
  done
  echo "  ⚠if f15 is 0 for EVERY voice above, that re-confirms R+0x15 is inert"
  echo "   and the answer must lie in the state column."
fi
# Report the experiment whenever it was REQUESTED, not only when it fired — a
# run that crashed before the music voice reached state 2 is INCONCLUSIVE, and
# silently omitting the section made that look like a null result.
if [ -n "$FORCE" ] || grep -q 'voiceforce:' "$OUT"; then
  echo
  echo "--- ★FORCED-STREAM experiment (this run MUTATED Halo) ---"
  if ! grep -q 'voiceforce:' "$OUT"; then
    echo "  ⚠DID NOT FIRE — no voice ever reached state 2, so the write was never"
    echo "   executed. Almost always an early crash (check the exit status above)."
    echo "   INCONCLUSIVE: this says nothing about 0x24b934. Re-run."
    s=$(grep -c 'voicestate: voice=30 ' "$OUT")
    echo "   voice 30 state samples this run: $s (a healthy run reaches 1->2)"
  else
    grep 'voiceforce:' "$OUT" | sed 's/^/  /'
    a=$(grep 'rate=44100' "$OUT" | grep -vc SILENT)
    n=$(grep -c 'rate=44100' "$OUT")
    echo "  => 44100 Hz buffers this run: $n, of which AUDIBLE: $a"
    if [ "$n" -lt 100 ]; then
      echo "  ⚠only $n music buffers — the run was too short to conclude either way."
    elif [ "$a" -gt 0 ]; then
      echo "  ★MUSIC PLAYED. 0x24b934 IS the stream feeder; the open question becomes"
      echo "   who was supposed to arm R+0x15."
    else
      echo "  no change: 0x24b934 is NOT the feeder (or not the only blocker)."
      echo "   The feeder is somewhere we have not looked."
    fi
  fi
fi
echo
echo "--- asset probe: was sounds.map ever opened and READ? ---"
if [ -s "$ASSETS" ]; then
  sed -n '/== .map assets/,/^$/p' "$ASSETS"
else
  echo "no asset snapshot (watcher never saw the engine open a .map)"
fi
