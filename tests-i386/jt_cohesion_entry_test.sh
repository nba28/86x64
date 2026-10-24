#!/bin/bash
# jt_cohesion_entry_test.sh — A/B guard for the entry-evidence arm of the
# jump-table cohesion gate (ParseEnv::code_alias_run_contradicted). The gate
# runs at TRANSLATE time, so each arm translates the fixture itself.
#   on   shipped config               -> table intact, fn-ptr table still callable (rc 42)
#   off  M64_NO_JT_COHESION_ENTRY=1   -> table word rebased (rc 1)
set -u
cd "$(dirname "$0")"
ROOT=$(cd .. && pwd)
I386=build/99_jt_cohesion_entry.i386
[ -f "$I386" ] || { echo "jt-cohesion-entry: FAIL ($I386 missing)"; exit 1; }

translate() { local a=$1; shift
  rm -f "build/99_jce.$a.x86_64" "build/99_jce.$a.x86_64.dylib"
  env "$@" bash "$ROOT/src/86x64/86x64.sh" -m "$ROOT/build/src/macho-tool/macho-tool" \
      -l "$ROOT/build/src/abiconv/libabiconv.dylib" -w "$ROOT/build/src/86x64/libwrapper.a" \
      -i "$ROOT/build/src/86x64/libinterpose.dylib" -o "build/99_jce.$a.x86_64" "$I386" >/dev/null 2>&1
  chmod +x "build/99_jce.$a.x86_64" 2>/dev/null
  ./build/99_jce.$a.x86_64 >/dev/null 2>&1
}
translate on;  on=$?
translate off M64_NO_JT_COHESION_ENTRY=1; off=$?
if [ $on = 42 ] && [ $off = 1 ]; then
  echo "jt-cohesion-entry: PASS (on=$on off=$off)"
else
  echo "jt-cohesion-entry: FAIL (on=$on off=$off; want 42 / 1)"; exit 1
fi
