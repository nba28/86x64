#!/bin/bash
#
# bundled_owner_bind_test.sh — A/B guard: an import the app bound to its own
# @loader_path dylib keeps that owner even when libabiconv exports a generated
# bridge of the same name (static-interpose.sh `system_only`). Portal's
# filesystem_stdio called vstdlib's CreateThreadPool through the Carbon Thread
# Manager bridge, got paramErr (-50) back as an object pointer and crashed.
#
# ON  = the translated payload binds libbundled_pool/_CreateThreadPool, for a
#       modern (LC_DYLD_INFO) and a classic (symbol-table) input.
# OFF = BRIDGES=/dev/null at translate time: bound to libabiconv/___CreateThreadPool.
# RUN = the translated modern fixture beside its translated libbundled_pool:
#       exit 0 (the bundled 0x5150). OFF = M64_NO_IR_TRANSLATED_BOUND=1: the
#       load-time import repair rewires the bound slot to the bridge, exit 1.
set -u
cd "$(dirname "$0")"
ROOT="$(cd .. && pwd)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
owner() {   # owner <in> <tag> [env...] -> the bind of CreateThreadPool
  local in="$1" tag="$2"; shift 2
  env "$@" bash "$ROOT/src/86x64/86x64.sh" -m "$ROOT/build/src/macho-tool/macho-tool" \
      -l "$ROOT/build/src/abiconv/libabiconv.dylib" -w "$ROOT/build/src/86x64/libwrapper.a" \
      -i "$ROOT/build/src/86x64/libinterpose.dylib" -o "$TMP/$tag" "$in" >"$TMP/$tag.log" 2>&1 \
    || { echo "bundled-owner-bind: FAIL ($tag translate)"; tail -3 "$TMP/$tag.log"; exit 1; }
  local out="$TMP/$tag.dylib"
  dyld_info -fixups "$out" 2>/dev/null | grep -o '[^ ]*CreateThreadPool' | sort -u | tr '\n' ' '
}
fail=0
for in in build/99_bundled_owner_bind.i386 build/99_bundled_owner_bind.classic.i386; do
  [ -f "$in" ] || { echo "bundled-owner-bind: FAIL (no $in)"; exit 1; }
  k=$(basename "$in" .i386)
  on=$(owner "$in" "$k.on") || exit 1
  off=$(owner "$in" "$k.off" BRIDGES=/dev/null) || exit 1
  echo "  $k  ON : $on"; echo "  $k  OFF: $off"
  [ "$on" = "libbundled_pool/_CreateThreadPool " ] || { echo "bundled-owner-bind: FAIL ($k ON rerouted the bundled bind)"; fail=1; }
  [ "$off" = "libabiconv/___CreateThreadPool " ] || { echo "bundled-owner-bind: FAIL ($k OFF did not reproduce the reroute: inert guard)"; fail=1; }
done
mkdir "$TMP/lib"
bash "$ROOT/src/86x64/86x64.sh" -m "$ROOT/build/src/macho-tool/macho-tool" \
    -l "$ROOT/build/src/abiconv/libabiconv.dylib" -o "$TMP/lib/libbundled_pool" build/libbundled_pool.dylib >"$TMP/lib.log" 2>&1 \
  || { echo "bundled-owner-bind: FAIL (stub translate)"; tail -3 "$TMP/lib.log"; exit 1; }
cp "$TMP/lib/libbundled_pool.dylib" build/libabiconv.dylib "$TMP/"
"$TMP/99_bundled_owner_bind.on" >/dev/null 2>&1; run_on=$?
M64_NO_IR_TRANSLATED_BOUND=1 "$TMP/99_bundled_owner_bind.on" >/dev/null 2>&1; run_off=$?
echo "  run  ON : exit $run_on   OFF: exit $run_off"
[ "$run_on" = 0 ] || { echo "bundled-owner-bind: FAIL (RUN ON did not reach the bundled CreateThreadPool)"; fail=1; }
[ "$run_off" = 1 ] || { echo "bundled-owner-bind: FAIL (RUN OFF did not reproduce the import-repair reroute: inert guard)"; fail=1; }
[ "$fail" = 0 ] && echo "bundled-owner-bind: PASS"
exit "$fail"
