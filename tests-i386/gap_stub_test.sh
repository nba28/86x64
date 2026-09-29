#!/bin/bash
# gap_stub_test.sh — guard for src/abiconv/gap.c: a constant-return STUB shim
# reports its FIRST hit (one stderr line + one reach-ledger line), a shim listed
# in coverage-audit.ok stays quiet, and M64_GAP=0 / M64_GAP=abort do what they say.
set -u
cd "$(dirname "$0")"
BIN=build/99_gap_stub.x86_64
[ -x "$BIN" ] || { echo "gap-stub: FAIL (build/99_gap_stub.x86_64 missing)"; exit 1; }
fail=0
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
mkdir -p "$T/on" "$T/off"

err=$(TMPDIR="$T/on" "$BIN" 2>&1 >/dev/null); rc=$?
n=$(grep -c 'GAP (silent no-op reached) stub TextWidth 99_gap_stub.x86_64[.a-z]*+0x' <<<"$err")
q=$(grep -c 'EqualPt' <<<"$err")
led=$(cat "$T"/on/86x64-reach/*.txt 2>/dev/null)
if [ "$rc" = 0 ] && [ "$n" = 1 ] && [ "$q" = 0 ] && [ "$(grep -c '^stub TextWidth ' <<<"$led")" = 1 ] \
   && [ "$(wc -l <<<"$led" | tr -d ' ')" = 1 ]; then
  echo "  ON  : two TextWidth calls -> ONE line + ONE ledger entry; EqualPt (.ok) quiet   OK"
else
  echo "  ON  : rc=$rc lines=$n equalpt=$q"; printf '%s\n---\n%s\n' "$err" "$led" | sed 's/^/      /'; fail=1
fi

err=$(M64_GAP=0 TMPDIR="$T/off" "$BIN" 2>&1 >/dev/null); rc=$?
if [ "$rc" = 0 ] && ! grep -q GAP <<<"$err" && [ ! -e "$T/off/86x64-reach" ]; then
  echo "  OFF : M64_GAP=0 -> silent, no ledger                                          OK"
else
  echo "  OFF : rc=$rc"; printf '%s\n' "$err" | sed 's/^/      /'; fail=1
fi

M64_GAP=abort TMPDIR="$T/off" "$BIN" >/dev/null 2>&1; rc=$?
if [ "$rc" != 0 ]; then
  echo "  ABRT: M64_GAP=abort -> dies on the first hit (rc=$rc)                          OK"
else
  echo "  ABRT: expected a non-zero exit"; fail=1
fi
[ "$fail" = 0 ] && echo "gap-stub: PASS" || echo "gap-stub: FAIL"
exit $fail
