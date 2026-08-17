#!/bin/bash
#
# halo-gl-texprobe.sh — one command, one human click: measure what Halo actually
# passes to the texture-upload entry points, and what pixel-store state is in
# force when it does.
#
# THE QUESTION. Halo's ANIMATED menu background textures render as a DIAGONAL
# SHEAR while pre-rendered stills, the HALO logo and all menu text are correct.
# A shear is the signature of a ROW-PITCH mismatch. Three candidates:
#   (a) wrong width/height/format reaching glTex(Sub)Image2D
#   (b) a GL_UNPACK_* setting that is wrong or never applied
#   (c) correct GL parameters, but data laid out wrong UPSTREAM by a
#       mistranslated decode/copy loop — in which case (a) and (b) look perfect
# This run separates (a)/(b) from (c). That is the point: (c) is a completely
# different hunt, and knowing which one we are in is worth a launch.
#
# ⚠This does NOT touch libabiconv. It inserts a standalone diagnostic dylib that
# interposes the real GL entry points — which works because the abigen bridge
# reaches OpenGL through an ordinary symbol stub. Delete the two files and it is
# gone. See halo-gl-texprobe.c.
#
# ⚠OpenGL is NOT missing on this OS — it is deprecated but present, and we call
# the real thing. The shim layer exists for the i386->x86_64 ABI boundary and
# for CGLMacro (Halo dispatches GL through a table inside the context object and
# never binds a GL symbol), not to reimplement OpenGL.
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
APP="${HALO_APP:-$HOME/projects/translations/Apps64/Halo.app}"
# --no-client-storage: force GL_UNPACK_CLIENT_STORAGE_APPLE off. This RUN IS A
# VISUAL TEST — the answer is on the screen, not in the log.
NOCS=""
if [ "${1:-}" = "--no-client-storage" ]; then
  NOCS="HALO_TEXPROBE_NO_CLIENT_STORAGE=1"; shift
  echo "★ experiment: forcing GL_UNPACK_CLIENT_STORAGE_APPLE=0 — WATCH THE BACKGROUND."
fi
DUMPDIR="${HALO_TEXPROBE_DUMP:-/tmp/halo-texdump}"
mkdir -p "$DUMPDIR"
rm -f "$DUMPDIR"/*.dxt
LOG="${1:-/tmp/halo-texprobe.log}"
DYLIB="${TMPDIR:-/tmp}/halo-texprobe.dylib"

if pgrep -x Halo >/dev/null; then
  echo "Halo is already running — quit it first (pkill -9 -x Halo)." >&2
  exit 1
fi

echo "building probe -> $DYLIB"
clang -arch x86_64 -dynamiclib -O1 -Wall -DGL_SILENCE_DEPRECATION \
      -framework OpenGL -o "$DYLIB" "$HERE/halo-gl-texprobe.c" || {
  echo "probe build FAILED" >&2; exit 1; }

echo "Logging to: $LOG"
echo "Click Play, let the MAIN MENU sit ~15s so the animated background uploads,"
echo "then quit Halo."

env DYLD_INSERT_LIBRARIES="$DYLIB" HALO_TEXPROBE_LOG="$LOG" \
    HALO_TEXPROBE_DUMP="$DUMPDIR" $NOCS \
  "$APP/Contents/MacOS/Halo" >/dev/null 2>&1

echo
echo "===== summary ====="
if [ ! -s "$LOG" ]; then
  echo "NO PROBE OUTPUT AT ALL — the dylib never even loaded."
  echo "  DYLD_INSERT_LIBRARIES was ignored (library validation / hardened runtime),"
  echo "  so this says NOTHING about the texture path. Re-sign or re-run; do not"
  echo "  read it as a result."
  exit 0
fi
if ! grep -q 'TexImage2D\|TexSubImage2D\|PixelStorei' "$LOG"; then
  echo "probe LOADED but saw ZERO texture calls."
  echo "  THAT IS A RESULT: the uploads do not go through these symbols at all, so"
  echo "  Halo must reach GL only via the CGLMacro dispatch table — instrument the"
  echo "  shim's own bridge (gli_tramp -> ___glTexSubImage2D) instead."
  exit 0
fi

printf 'TexImage2D calls            %s\n' "$(grep -c 'TexImage2D ' "$LOG")"
printf 'TexSubImage2D calls         %s\n' "$(grep -c 'TexSubImage2D' "$LOG")"
printf 'CompressedTexImage2D calls  %s\n' "$(grep -c 'CompressedTexImage2D' "$LOG")"
printf 'PixelStorei calls           %s\n' "$(grep -c 'PixelStorei' "$LOG")"

echo
echo "--- distinct pixel-store settings (the prime suspect) ---"
grep 'PixelStorei' "$LOG" | sed 's/.*pname/pname/' | sort | uniq -c | sort -rn | head -10
echo "  (GL_UNPACK_ROW_LENGTH=0xcf2 GL_UNPACK_ALIGNMENT=0xcf5 GL_UNPACK_SKIP_PIXELS=0xcf4"
echo "   GL_UNPACK_SKIP_ROWS=0xcf3; a nonzero row_length that does not match the"
echo "   upload width is exactly the shear)"

echo
echo "--- distinct unpack state AT UPLOAD TIME ---"
grep -oE 'unpack\{[^}]*\}' "$LOG" | sort | uniq -c | sort -rn | head -8

echo
echo "--- ★VERTEX ARRAY SETUPS (the current suspect) ---"
if ! grep -q '^\[geo\]' "$LOG"; then
  echo "  none seen. If the menu still drew, the 3D path does not use client"
  echo "  vertex arrays — look at VBOs (glBufferData) or immediate mode instead."
else
  grep '^\[geo\]' "$LOG" | sed 's/^/  /'
  echo
  echo "  READ THE STRIDES. For an interleaved vertex the stride must equal the"
  echo "  struct size, and every array sharing that struct must report the SAME"
  echo "  stride. A stride that disagrees with its neighbours, or is 0 where the"
  echo "  data is interleaved, walks the buffer at the wrong step and drags each"
  echo "  successive vertex further off — which is exactly the smearing seen."
fi

echo
echo "--- distinct upload geometries (size/format) ---"
grep -oE 'TexSubImage2D lvl=[0-9-]+ at\([0-9-]+,[0-9-]+\) [0-9]+x[0-9]+ fmt=[^ ]+' "$LOG" \
  | sed 's/at([0-9-]*,[0-9-]*)/at(..)/' | sort | uniq -c | sort -rn | head -10
grep -oE 'TexImage2D    lvl=[0-9-]+ ifmt=[^ ]+ [0-9]+x[0-9]+' "$LOG" \
  | sort | uniq -c | sort -rn | head -10

echo
if grep -q 'FORCED 0' "$LOG"; then
  echo
  echo "--- ★client-storage experiment ---"
  printf '  forced off on %s call(s). THE VERDICT IS VISUAL:\n' \
    "$(grep -c 'FORCED 0' "$LOG")"
  echo "    background now CORRECT  -> APPLE_client_storage is the cause"
  echo "    background still SHEARED-> extension exonerated; the bytes are wrong"
fi

if ls "$DUMPDIR"/*.dxt >/dev/null 2>&1; then
  echo
  echo "--- captured texture blobs (decode them offline, no eyeballs needed) ---"
  ls -la "$DUMPDIR"/*.dxt | awk '{print "  "$5" bytes  "$NF}'
  echo "  decode: python3 src/86x64/dxt-decode.py $DUMPDIR/*.dxt"
fi

echo "--- VERDICT HINT ---"
bad=$(grep -oE 'unpack\{row_len=[0-9-]+' "$LOG" | grep -vc 'row_len=0')
if [ "$bad" -gt 0 ]; then
  echo "  $bad uploads ran with a NONZERO GL_UNPACK_ROW_LENGTH — inspect those against"
  echo "  their upload width; a mismatch IS the shear (candidate b)."
else
  echo "  every upload ran with GL_UNPACK_ROW_LENGTH=0, so the row length is the"
  echo "  upload width by definition and cannot shear the image."
  echo "  => leans (c): the GL parameters are right and the PIXEL DATA is already"
  echo "     sheared when Halo hands it over — i.e. the defect is upstream, in"
  echo "     translated Halo code that decodes/copies the texture."
fi
