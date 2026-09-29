#!/bin/bash
# heap_guard_test.sh — guard for malloc_shim.c's M64_HEAP_GUARD=1 diagnostic
# mode: an overrun and a use-after-free FAULT at the write (ON); without the
# mode both writes land silently (OFF) — i.e. the mode is what catches them.
set -u
cd "$(dirname "$0")"
BIN=build/99_heap_guard.x86_64
[ -x "$BIN" ] || { echo "heap-guard: FAIL ($BIN missing)"; exit 1; }
fail=0
for mode in over uaf; do
  on=$(M64_HEAP_GUARD=1 GUARD_MODE=$mode "$BIN" 2>/dev/null); onrc=$?
  off=$(GUARD_MODE=$mode "$BIN" 2>/dev/null); offrc=$?
  if [ $onrc != 0 ] && ! grep -q survived=1 <<<"$on" && grep -q allocated=1 <<<"$on"; then
    printf '  ON  %-4s: M64_HEAP_GUARD=1 faults at the bad write (rc=%s)          OK\n' $mode $onrc
  else echo "  ON  $mode: expected a fault, rc=$onrc out=$on"; fail=1; fi
  if [ $offrc = 0 ] && grep -q survived=1 <<<"$off"; then
    printf '  OFF %-4s: without the mode the write lands silently                   OK\n' $mode
  else echo "  OFF $mode: rc=$offrc out=$off"; fail=1; fi
done
out=$(M64_HEAP_GUARD=1 "$BIN" 2>/dev/null); rc=$?
if [ $rc = 0 ] && grep -q survived=1 <<<"$out"; then
  echo "  ON  none: a well-behaved program runs normally under the mode           OK"
else echo "  ON  none: rc=$rc"; fail=1; fi
[ $fail = 0 ] && echo "heap-guard: PASS" || echo "heap-guard: FAIL"
exit $fail
