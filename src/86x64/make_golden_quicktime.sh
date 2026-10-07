#!/bin/bash
#
# make_golden_quicktime.sh — build the canonical ("golden") TRANSLATED x86_64
# QuickTime.framework from the pristine i386 one, for deposit in
# $M64_FRAMEWORKS/86x64/ and reuse by every QuickTime app
# (Halo CE, Civ IV, iPhoto, Numbers, Pages, iWeb).
#
# The whole recipe is the CURRENT standard pipeline — nothing app-specific:
#   1. m64 translate (in place: translate + static-interpose + embedded
#      @loader_path libabiconv; removed-symbol binds are emitted WEAK_IMPORT
#      so dyld NULLs them at load and import_repair covers any that are
#      actually reached).
#   2. _weaken_removed_binds.py — no-op safety net on current-toolchain
#      output; cures artifacts translated before the weak-import emission.
#   3. @rpath id + @rpath rewrites of the two deps that are dead on modern
#      macOS (NavigationServices, CarbonSound — bundle the x86_64 natives
#      from $M64_FRAMEWORKS/iLife11/ alongside).
#   4. ad-hoc sign.
#
# Usage: make_golden_quicktime.sh [pristine-framework] [dest-dir]
. "$(dirname "$0")/paths.sh"   # M64_* local paths
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
M64="$SCRIPT_DIR/m64"
LIBABICONV="$PROJ_ROOT/build/src/abiconv/libabiconv.dylib"

SRC_FW="${1:-$HOME/Downloads/iLife11/Library/Frameworks/QuickTime.framework}"
DEST="${2:-$M64_FRAMEWORKS/86x64}"

[ -f "$SRC_FW/Versions/A/QuickTime" ] || { echo "no pristine QuickTime at $SRC_FW" >&2; exit 1; }
file -b "$SRC_FW/Versions/A/QuickTime" | grep -q i386 || { echo "$SRC_FW is not i386 (already translated?)" >&2; exit 1; }
[ -f "$LIBABICONV" ] || { echo "build libabiconv first (m64 build)" >&2; exit 1; }

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
cp -R "$SRC_FW" "$STAGE/"
xattr -cr "$STAGE/QuickTime.framework"
QT="$STAGE/QuickTime.framework/Versions/A/QuickTime"

"$M64" translate "$QT"
python3 "$SCRIPT_DIR/_weaken_removed_binds.py" "$QT" "$LIBABICONV"

install_name_tool \
  -id "@rpath/QuickTime.framework/Versions/A/QuickTime" \
  -change /System/Library/Frameworks/Carbon.framework/Versions/A/Frameworks/NavigationServices.framework/Versions/A/NavigationServices \
          @rpath/NavigationServices.framework/Versions/A/NavigationServices \
  -change /System/Library/Frameworks/Carbon.framework/Versions/A/Frameworks/CarbonSound.framework/Versions/A/CarbonSound \
          @rpath/CarbonSound.framework/Versions/A/CarbonSound \
  "$QT"
codesign -f -s - "$QT"
codesign -f -s - "$STAGE/QuickTime.framework"

mkdir -p "$DEST"
rm -rf "$DEST/QuickTime.framework"
cp -R "$STAGE/QuickTime.framework" "$DEST/"
echo "golden QuickTime.framework -> $DEST/QuickTime.framework"
echo "deploy: replace <App>.app/Contents/Frameworks/QuickTime.framework, keep"
echo "NavigationServices+CarbonSound bundled, then m64 resync <App>.app"
