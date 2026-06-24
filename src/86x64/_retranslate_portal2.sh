#!/bin/bash
# Retranslate + deploy Portal 2 (Source engine, i386) into bin/osx64.
#
# Thin wrapper over `m64 tree`, which now captures this whole workflow
# generically (out-of-place osx32->osx64 translation, runtime-lib copy,
# equal-length bin/osx32->bin/osx64 byte-patch, ad-hoc sign). The bin/osx32
# originals are never touched.
#
# Usage:  _retranslate_portal2.sh [gameroot]
#   gameroot defaults to the Steam install path below.
# Run after build:  cd "<gameroot>"; DYLD_LIBRARY_PATH="$PWD/bin/osx64" ./bin/osx64/portal2_osx
set -u
GR="${1:-$HOME/Library/Application Support/Steam/steamapps/common/Portal 2}"

# libcef = deferred data-in-code rabbit hole; skip it for now.
exec m64 tree "$GR" \
    --src bin/osx32 --out bin/osx64 \
    --exec portal2_osx \
    --skip libcef.dylib \
    --patch
