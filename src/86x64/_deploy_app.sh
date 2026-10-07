#!/bin/bash
# Post-retranslate deploy for a translated app bundle:
#   sweep rpaths -> shimgen -> resync+sign libabiconv copies ->
#   fix nested signing -> outer codesign -> clear savedState.
# usage: _deploy_app.sh <App.app> <identifier> <shimgen_targets_file> <savedstate-id>
set -u
APP="${1:?app path}"
IDENT="${2:?codesign identifier}"
TARGETS="${3:?shimgen targets file}"
SAVEDSTATE="${4:-}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"   # repo root (this script lives in src/86x64)
L="$ROOT/build/src/abiconv/libabiconv.dylib"

echo "== rpath sweep =="
python3 "$ROOT/src/86x64/_rpath_sweep.py" "$APP" | tail -2 || exit 1

echo "== shimgen =="
python3 "$ROOT/src/86x64/shimgen.py" "$APP" $(cat "$TARGETS") | tail -4 || exit 1

echo "== resync + flat-sign libabiconv copies =="
n=0
for f in $(find "$APP" -name libabiconv.dylib); do
    cp "$L" "$f" && codesign -f -s - "$f" 2>/dev/null || echo "  RESYNC/SIGN FAIL: $f"
    n=$((n+1))
done
echo "  $n copies"

echo "== flat-sign loose Frameworks dylibs =="
for f in "$APP"/Contents/Frameworks/*.dylib; do
    [ -f "$f" ] && { codesign -f -s - "$f" 2>/dev/null || echo "  SIGN FAIL: $f"; }
done

echo "== fix nested signing =="
python3 "$ROOT/src/86x64/_fix_bundle_signing.py" "$APP" | tail -1

echo "== outer codesign =="
codesign -f -s - --identifier "$IDENT" "$APP" 2>&1 | tail -1

if [ -n "$SAVEDSTATE" ]; then
    rm -rf "$HOME/Library/Saved Application State/$SAVEDSTATE.savedState"
    echo "== cleared $SAVEDSTATE savedState =="
fi
echo "== deploy done =="
