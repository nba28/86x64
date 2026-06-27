#!/bin/bash
# C++ EXCEPTION validation set (f01-f05). Builds each i386 fixture, translates it
# through the full 86x64 pipeline with the in-tree macho-tool + libabiconv, runs
# it, and diffs stdout against expected/ (the FINAL post-unwind output).
#
# Status by increment:
#   - WALL 1 (entry-point shims, landed): every fixture is now CALLABLE — no more
#     ABI-mismatch SIGSEGV. Without the core PC map a throw cleanly terminates
#     (std::terminate semantics) with an "[eh] terminate:" diagnostic, so these
#     report FAIL-(terminate), NOT a crash.
#   - WALL 2 (core __86x64_pcmap + __86x64_ehlsda emitted, libabiconv unwinder):
#     f01/f02/f04/f05 go green. f03 additionally needs the RTTI inheritance walk
#     (base-ref catch) shared with the __dynamic_cast work.
#
# Kept OUT of `make cpp` on purpose so the green cpp suite is unaffected until the
# PC map lands. Run from anywhere: ./run_eh.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$(cd "$HERE/../../.." && pwd)"          # worktree / repo root
MT="$PROJ/build/src/macho-tool/macho-tool"
LA="$PROJ/build/src/abiconv/libabiconv.dylib"
LW="$PROJ/build/src/86x64/libwrapper.a"
LI="$PROJ/build/src/86x64/libinterpose.dylib"
PIPE="$PROJ/src/86x64/86x64.sh"
SDK="$(xcrun --show-sdk-path)"
SYS="${I386_SYSROOT:-/tmp/i386-sysroot}"
OUT="$HERE/build"; mkdir -p "$OUT"
cp "$LA" "$OUT/libabiconv.dylib"; cp "$LI" "$OUT/libinterpose.dylib"
codesign -f -s - "$OUT/libabiconv.dylib" "$OUT/libinterpose.dylib" 2>/dev/null

pass=0; fail=0
for src in "$HERE"/f0*.cc; do
  t="$(basename "${src%.cc}")"
  clang++ -arch i386 -isysroot "$SDK" -mmacosx-version-min=10.6 -c "$src" -o "$OUT/$t.o" 2>/dev/null \
    || { echo "FAIL $t (compile)"; fail=$((fail+1)); continue; }
  ld -arch i386 -syslibroot "$SYS" -lstdc++ -lSystem -e _main -o "$OUT/$t.i386" "$OUT/$t.o" 2>/dev/null \
    || { echo "FAIL $t (link — stage /tmp/i386-sysroot libstdc++)"; fail=$((fail+1)); continue; }
  bash "$PIPE" -m "$MT" -l "$LA" -w "$LW" -i "$LI" -o "$OUT/$t.x86_64" "$OUT/$t.i386" >/dev/null 2>&1 \
    || { echo "FAIL $t (translate)"; fail=$((fail+1)); continue; }
  chmod +x "$OUT/$t.x86_64"
  codesign -f -s - "$OUT/$t.x86_64" "$OUT/$t.x86_64.dylib" 2>/dev/null
  "$OUT/$t.x86_64" > "$OUT/$t.actual" 2>"$OUT/$t.err"; ec=$?
  if diff -q "$HERE/expected/$t.txt" "$OUT/$t.actual" >/dev/null 2>&1; then
    echo "PASS $t"; pass=$((pass+1))
  else
    note=""; grep -q "no translator PC map" "$OUT/$t.err" 2>/dev/null && note=" (terminate: awaiting core PC map)"
    echo "FAIL $t (exit $ec)$note"; fail=$((fail+1))
  fi
done
echo ""; echo "===== eh: $pass passed, $fail failed ====="
