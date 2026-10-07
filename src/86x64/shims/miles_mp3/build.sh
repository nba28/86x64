#!/usr/bin/env bash
# build.sh <i386 libMilesX86.dylib> <out.dylib> — build the i386 Miles MP3
# provider (miles_mp3.c) against the game's own Miles, for translation.
#
# -O0 -fno-jump-tables: clang's -O1+ PIC code adds displacements to a register
# that already holds anchor+index (`add $disp,%reg`), and its -O0 switch tables
# dispatch through spill slots; the translator handles neither yet (known gap).
# Built this way the translation decodes within ±1 LSB of a native decode
# (float rounding), >25x real time.
. "$(dirname "$0")/../../paths.sh"   # M64_* local paths
set -euo pipefail
[ $# -eq 2 ] || { echo "usage: $0 <i386 libMilesX86.dylib> <out.dylib>" >&2; exit 2; }
HERE="$(cd "$(dirname "$0")" && pwd)"
SYSROOT="${I386_SYSROOT:-/tmp/i386-sysroot}"
LD="$M64_I386_LD"
[ -x "$LD" ] || LD=ld
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
clang -arch i386 -isysroot "$SYSROOT" -mmacosx-version-min=10.6 -O0 -fno-jump-tables -Wall \
   -c "$HERE/miles_mp3.c" -o "$TMP/miles_mp3.o"
"$LD" -arch i386 -dylib -macos_version_min 10.6 -syslibroot "$SYSROOT" -lSystem \
   -install_name @loader_path/libmiles_mp3.dylib -o "$2" "$TMP/miles_mp3.o" "$1"
