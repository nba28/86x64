#!/bin/bash
#
# bink-diff.sh — differential test of a translated libbinkmachox86 (Portal 2)
# against ffmpeg's own Bink decoder. Headless, ~1s per decode.
#
# probes/binkdec.c (i386, translated by our pipeline) dlopens a translated
# libbink, decodes N frames via BinkCopyToBuffer, and every frame is scored as
# PSNR vs `ffmpeg -pix_fmt bgra`. A correct decode is ~50+ dB everywhere; the
# broken intro video drifts to ~14 dB after the first few (black) frames.
#
#   bink-diff.sh [--frames N] [--bik F] [--yuv] LIB.dylib   score one libbink
#     --yuv: decode into app-registered I420 planes (Portal 2's CBIKMaterial
#            path, no Bink colour conversion) and score Y and chroma vs
#            ffmpeg yuv420p, which is the decoder alone, bit for bit.
#   bink-diff.sh [--frames N] [--bik F] --sweep [KNOB...]
#                    retranslate libbink once per translate-time knob (default:
#                    every M64_* getenv in src/core) set to 1, score each, and
#                    print the knobs that CHANGE the score vs the baseline.
. "$(dirname "$0")/../paths.sh"   # M64_* local paths
set -uo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
P2="${P2:-$HOME/Library/Application Support/Steam/steamapps/common/Portal 2}"
BIK="$P2/portal2/media/valve.bik"
N=60
W="${TMPDIR:-/tmp}/86x64-bink"
SWEEP=0
YUV=0
while [ $# -gt 0 ]; do
  case "$1" in
    --frames) N="$2"; shift 2;;
    --bik) BIK="$2"; shift 2;;
    --sweep) SWEEP=1; shift;;
    --yuv) YUV=1; shift;;
    *) break;;
  esac
done
mkdir -p "$W"
B="$REPO/build/src"

# harness: i386 build + translate (start from crt1: it reads env, not argv)
if [ ! -x "$W/binkdec" ] || [ "$REPO/src/86x64/probes/binkdec.c" -nt "$W/binkdec" ] ||
   [ "$B/macho-tool/macho-tool" -nt "$W/binkdec" ]; then
  clang -arch i386 -isysroot /tmp/i386-sysroot -mmacosx-version-min=10.6 \
        -c "$REPO/src/86x64/probes/binkdec.c" -o "$W/binkdec.o" || exit 1
  "$M64_I386_LD" -arch i386 -macos_version_min 10.6 \
        -no_pie -syslibroot /tmp/i386-sysroot -lSystem "$W/binkdec.o" \
        "$M64_SDK106/usr/lib/crt1.10.6.o" -o "$W/binkdec.i386" || exit 1
  bash "$REPO/src/86x64/86x64.sh" -m "$B/macho-tool/macho-tool" -l "$B/abiconv/libabiconv.dylib" \
       -w "$B/86x64/libwrapper.a" -i "$B/86x64/libinterpose.dylib" \
       -o "$W/binkdec" "$W/binkdec.i386" >"$W/binkdec.translate.log" 2>&1 || { echo "harness translate failed"; exit 1; }
fi
cp "$B/abiconv/libabiconv.dylib" "$W/"

FMT=bgra; [ "$YUV" -eq 1 ] && FMT=yuv420p
REF="$W/ref.$(basename "$BIK").$N.$FMT.raw"
[ -s "$REF" ] || ffmpeg -v error -y -i "$BIK" -frames:v "$N" -f rawvideo -pix_fmt "$FMT" "$REF" || exit 1

# score LIB TAG -> prints "TAG first_bad_frame mean_psnr min_psnr"
score() {
  local lib="$1" tag="$2" out="$W/out.$2.raw"
  cp "$lib" "$W/lib.$tag.dylib"          # beside libabiconv (@loader_path)
  rm -f "$out"
  ( [ "$YUV" -eq 1 ] && export BK_YUV=1
    BK_LIB="$W/lib.$tag.dylib" BK_FILE="$BIK" BK_N="$N" BK_OUT="$out" \
      M64_FAULT_REPORT=1 exec "$W/binkdec" ) >"$W/run.$tag.log" 2>&1
  python3 - "$out" "$REF" "$tag" "$FMT" <<'PY'
import sys, numpy as np
out, ref, tag, fmt = sys.argv[1:]
W, H = 1280, 720
fs = W*H*4 if fmt == "bgra" else W*H*3//2
def frames(path):
    try: a = np.fromfile(path, np.uint8)
    except OSError: a = np.zeros(0, np.uint8)
    return a[:a.size//fs*fs].reshape(-1, fs)
t, r = frames(out), frames(ref)
n = min(len(t), len(r))
if n == 0: print(f"{tag:<34} CRASH (no frames)"); sys.exit()
def psnr(a, b):
    m = ((a.astype(np.float32)-b)**2).mean(); return 99.0 if m == 0 else 10*np.log10(255**2/m)
if fmt == "bgra":
    p = [psnr(t[i].reshape(H, W, 4)[..., :3], r[i].reshape(H, W, 4)[..., :3]) for i in range(n)]
    extra = ""
else:
    p = [psnr(t[i][:W*H], r[i][:W*H]) for i in range(n)]
    c = [psnr(t[i][W*H:], r[i][W*H:]) for i in range(n)]
    extra = f" chroma_mean={np.mean(c):5.1f}dB"
bad = next((i for i, v in enumerate(p) if v < 40), -1)
print(f"{tag:<34} frames={n:<3} first_bad={bad:<3} mean={np.mean(p):5.1f}dB min={min(p):5.1f}dB{extra}")
PY
}

if [ "$SWEEP" -eq 0 ]; then
  [ $# -eq 1 ] || { sed -n 3,15p "$0"; exit 2; }
  score "$1" one
  exit 0
fi

# sweep: baseline + one retranslate per knob
SRC="$P2/bin/osx32/libbinkmachox86.dylib"
knobs=("$@")
[ ${#knobs[@]} -gt 0 ] || knobs=($(grep -rhoE 'getenv\("M64_[A-Z0-9_]+"\)' "$REPO/src/core" | sort -u | sed 's/getenv("//;s/")//'))
xl() {   # xl TAG [ENV=VAL]
  local tag="$1"; shift
  env "$@" "$REPO/src/86x64/m64" translate "$SRC" -o "$W/x.$tag.dylib" >"$W/x.$tag.log" 2>&1 &&
    file -b "$W/x.$tag.dylib" | grep -q x86_64 && score "$W/x.$tag.dylib" "$tag" ||
    printf '%-34s TRANSLATE FAILED (%s)\n' "$tag" "$W/x.$tag.log"
}
xl baseline
for k in "${knobs[@]}"; do xl "$k" "$k=1"; done
