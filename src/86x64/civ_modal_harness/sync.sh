#!/bin/bash
# Sync freshly built libabiconv into EVERY co-located copy in the test bundle, then re-sign.
set -e
WT=$HOME/projects/86x64-worktree
APP="$HOME/Library/Application Support/Steam/steamapps/common/Sid Meier's Civilization IV 34440/Civilization IV (test).app"
SRC="$WT/build/src/abiconv/libabiconv.dylib"
n=0
while IFS= read -r f; do
  cp "$SRC" "$f"
  n=$((n+1))
done < <(find "$APP" -name libabiconv.dylib)
echo "synced $n copies"
codesign -f -s - --deep "$APP" >/dev/null 2>&1 || codesign -f -s - "$APP"
echo "signed rc=$?"
codesign -v "$APP" && echo "verify OK"
