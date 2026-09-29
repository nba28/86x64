#!/bin/bash
# gap_raw_test.sh — guard for gap.c's RAW class: a translated call to a native
# function with no libabiconv bridge reports ONE line on its first hit;
# M64_GAP=0
# is silent and arms nothing. The call itself must behave exactly as before.
set -u
cd "$(dirname "$0")"
BIN=build/99_gap_raw.x86_64; b=99_gap_raw
fail=0
[ -x "$BIN" ] || { echo "gap-raw: FAIL (build/99_gap_raw.x86_64 missing)"; exit 1; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
# dyld4 binds even the lazy slot at load; gap.c's one-shot stub sees the call.
for arm in bound; do
  mkdir -p "$T/$arm"
  out=$(TMPDIR="$T/$arm" "$BIN" 2>"$T/err"); rc=$?
  n=$(grep -c "GAP (silent no-op reached) raw _\{0,1\}NSGetArgc $b.x86_64[.a-z]*+0x" "$T/err")
  if [ "$rc" = 0 ] && [ "$out" = survived=1 ] && [ "$n" = 1 ] && grep -q '^raw _*NSGetArgc ' "$T/$arm"/86x64-reach/*.txt 2>/dev/null; then
    printf '  ON  %-4s: raw _NSGetArgc -> ONE line naming the caller + ledger     OK\n' $arm
  else
    echo "  ON  $arm: rc=$rc out=$out lines=$n"; sed 's/^/      /' "$T/err"; fail=1
  fi
  out=$(M64_GAP=0 TMPDIR="$T/off" "$BIN" 2>"$T/err"); rc=$?
  if [ "$rc" = 0 ] && [ "$out" = survived=1 ] && ! grep -q GAP "$T/err"; then
    printf '  OFF %-4s: M64_GAP=0 -> silent, call unchanged                    OK\n' $arm
  else
    echo "  OFF $arm: rc=$rc out=$out"; sed 's/^/      /' "$T/err"; fail=1
  fi
done
[ "$fail" = 0 ] && echo "gap-raw: PASS" || echo "gap-raw: FAIL"
exit $fail
