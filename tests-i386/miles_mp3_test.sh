#!/bin/bash
#
# miles_mp3_test.sh — A/B guard for the Miles MP3 provider (src/86x64/shims/
# miles_mp3): Miles decodes MP3 only through mssmp3.asi, a PE32 DLL a translated
# process cannot run, so Portal 2's voice lines (MP3) were mute.
#
# Builds the provider and 99_miles_mp3_provider against the game's own i386
# libMilesX86 (MILES_DYLIB overrides; SKIP if absent), translates all three, and
# decodes a lame-made tone the way vaudio_miles does.
# ON  = exit 42 and PCM within ±1 LSB of a native minimp3 decode (float rounding).
# OFF = M64_NO_MILES_MP3=1 at run time: no provider registers -> exit 1.
set -u
cd "$(dirname "$0")"
ROOT="$(cd .. && pwd)"
MILES="${MILES_DYLIB:-$HOME/Library/Application Support/Steam/steamapps/common/Portal 2/bin/osx32/libmilesx86.dylib}"
[ -f "$MILES" ] || { echo "miles-mp3: SKIP (no i386 Miles at $MILES)"; exit 0; }
command -v lame >/dev/null || { echo "miles-mp3: SKIP (no lame to make the test MP3)"; exit 0; }
SR="${I386_SYSROOT:-/tmp/i386-sysroot}"
LD="$HOME/projects/Library/Toolchains/sl-ld64/ld-i386"; [ -x "$LD" ] || LD=ld
SHIM="$ROOT/src/86x64/shims/miles_mp3"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/i386" "$TMP/x64"
fail() { echo "miles-mp3: FAIL ($1)"; exit 1; }

cp "$MILES" "$TMP/i386/libMilesX86.dylib"
bash "$SHIM/build.sh" "$TMP/i386/libMilesX86.dylib" "$TMP/i386/libmiles_mp3.dylib" || fail "provider build"
clang -arch i386 -isysroot "$SR" -mmacosx-version-min=10.6 -O1 -c src/99_miles_mp3_provider.c -o "$TMP/h.o" &&
"$LD" -arch i386 -macos_version_min 10.6 -no_pie -syslibroot "$SR" -lSystem -e _main -o "$TMP/i386/harness" \
   "$TMP/h.o" "$TMP/i386/libMilesX86.dylib" "$TMP/i386/libmiles_mp3.dylib" || fail "harness build"

for n in libMilesX86.dylib libmiles_mp3.dylib harness; do
  "$ROOT/src/86x64/m64" translate "$TMP/i386/$n" -o "$TMP/x64/$n" >"$TMP/$n.log" 2>&1 &
done
wait
for n in libMilesX86.dylib libmiles_mp3.dylib harness; do [ -f "$TMP/x64/$n" ] || fail "translate $n"; done
cp "$ROOT/build/src/abiconv/libabiconv.dylib" "$ROOT/build/src/86x64/libinterpose.dylib" "$TMP/x64/"

python3 - "$TMP/tone.wav" <<'PY'
import math, struct, sys, wave
w = wave.open(sys.argv[1], 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(44100)
w.writeframes(b''.join(struct.pack('<h', int(12000 * math.sin(2 * math.pi * 440 * i / 44100))) for i in range(44100 // 2)))
PY
lame --quiet -b 80 -m m -t "$TMP/tone.wav" "$TMP/tone.mp3" || fail "lame"
cat > "$TMP/ref.c" <<'C'
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#include "minimp3.h"
#include <stdio.h>
int main(int c, char **v) {
   static unsigned char b[1 << 20]; FILE *f = fopen(v[1], "rb"); int n = (int)fread(b, 1, sizeof b, f); FILE *o = fopen(v[2], "wb");
   mp3dec_t d; mp3dec_init(&d); mp3dec_frame_info_t i; short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME]; int off = 0;
   while (off < n) { int s = mp3dec_decode_frame(&d, b + off, n - off, pcm, &i); if (!i.frame_bytes) break; off += i.frame_bytes; fwrite(pcm, 2, s * i.channels, o); }
   return 0; }
C
clang -O1 -I"$SHIM" "$TMP/ref.c" -o "$TMP/ref" && "$TMP/ref" "$TMP/tone.mp3" "$TMP/ref.pcm" || fail "native reference"

run() { (cd "$TMP/x64" && env ABICONV_RUN_INITS=1 MP3_IN="$TMP/tone.mp3" PCM_OUT="$TMP/$1.pcm" "${@:2}" ./harness >"$TMP/$1.log" 2>&1); echo $?; }
r_on=$(run on); r_off=$(run off M64_NO_MILES_MP3=1)
cmp=$(python3 - "$TMP/ref.pcm" "$TMP/on.pcm" <<'PY'
import struct, sys
a = open(sys.argv[1], 'rb').read(); b = open(sys.argv[2], 'rb').read() if len(sys.argv) > 2 else b''
if len(a) != len(b) or not a: print('len %d vs %d' % (len(a), len(b))); sys.exit()
n = len(a) // 2
print('maxdiff %d' % max(abs(x - y) for x, y in zip(struct.unpack('<%dh' % n, a), struct.unpack('<%dh' % n, b))))
PY
)
echo "  ON : rc=$r_on $(grep -h rate= "$TMP/on.log") ($cmp vs native)"; echo "  OFF: rc=$r_off $(grep -h provider "$TMP/off.log")"
[ "$r_on" = 42 ] || fail "ON exit $r_on"
case "$cmp" in "maxdiff 0"|"maxdiff 1") ;; *) fail "ON PCM: $cmp";; esac
[ "$r_off" = 1 ] || fail "OFF exit $r_off: inert guard"
echo "miles-mp3: PASS"
