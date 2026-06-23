#!/bin/bash
# Retranslate + deploy the Portal 2 (Source engine, i386) set into bin/osx64.
# Non-destructive: bin/osx32 originals are untouched; outputs go to bin/osx64.
# Mirrors the per-app _retranslate_iphoto.sh / _retranslate_iweb.sh convention.
#
# Run-without-swapping-osx32 model: dyld loads everything from bin/osx64 via
# DYLD_LIBRARY_PATH (applied to leafnames) + an equal-length osx32->osx64
# byte-patch in the binaries that build absolute module paths at runtime.
#
# Usage:  _retranslate_portal2.sh [gameroot]
#   gameroot defaults to the Steam install path below.
# Run after build:  cd "<gameroot>"; DYLD_LIBRARY_PATH="$PWD/bin/osx64" ./bin/osx64/portal2_osx
set -u
GR="${1:-$HOME/Library/Application Support/Steam/steamapps/common/Portal 2}"
SRC="$GR/bin/osx32"
DST="$GR/bin/osx64"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"   # repo root
LOG=/tmp/p2_retr.log
: > "$LOG"
mkdir -p "$DST"

# 1) translate every i386 dylib (skip libcef = deferred data-in-code rabbit hole)
ok=0; fail=0
for f in "$SRC"/*.dylib; do
  name=$(basename "$f")
  [ "$name" = libcef.dylib ] && { echo "SKIP $name"; continue; }
  if m64 translate "$f" -o "$DST/$name" >>"$LOG" 2>&1; then
    ok=$((ok+1)); echo "OK   $name"
  else fail=$((fail+1)); echo "FAIL $name"; fi
done
# main exec (non-PIE EXECUTE) -> exec + portal2_osx.dylib sibling
if m64 translate "$GR/portal2_osx" -o "$DST/portal2_osx" >>"$LOG" 2>&1; then
  echo "OK   portal2_osx (exec)"; else echo "FAIL portal2_osx"; fail=$((fail+1)); fi
echo "=== translate: ok=$ok fail=$fail (see $LOG) ==="

# 2) refresh our runtime libs from the build tree
cp -f "$ROOT/build/src/abiconv/libabiconv.dylib" "$DST/libabiconv.dylib"
[ -f "$ROOT/build/src/abiconv/libinterpose.dylib" ] && \
  cp -f "$ROOT/build/src/abiconv/libinterpose.dylib" "$DST/libinterpose.dylib"

# 3) byte-patch osx32->osx64 (equal length) where a binary embeds the literal
#    path (launcher/engine/vrad/vvis build absolute module paths at runtime)
for f in "$DST"/*.dylib "$DST"/portal2_osx; do
  [ -f "$f" ] || continue
  LC_ALL=C grep -q "bin/osx32" "$f" 2>/dev/null && \
    LC_ALL=C perl -0777 -pi -e 's{bin/osx32}{bin/osx64}g' "$f" && echo "patched $(basename "$f")"
done

# 4) ad-hoc re-sign (MACHO_HEADERPAD reserves room so this can't clobber __text)
for f in "$DST"/*.dylib "$DST"/portal2_osx; do
  [ -f "$f" ] && codesign -f -s - "$f" >/dev/null 2>&1 || echo "sign FAIL $f"
done
echo "=== deploy done -> $DST ==="
