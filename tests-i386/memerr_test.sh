#!/bin/bash
# memerr_test.sh — guard: MemError() reports the result of OUR Memory Manager
# shims (carbon_memory.c). A failed SetPtrSize must read back as an error, a
# successful one as noErr; before the fix MemError always said noErr.
set -u
cd "$(dirname "$0")"
BIN=build/99_memerr.x86_64
[ -x "$BIN" ] || { echo "memerr: FAIL ($BIN missing)"; exit 1; }
out=$("$BIN" 2>/dev/null); rc=$?
if [ $rc = 0 ] && [ "$out" = "newptr_ok=1 grow_reports_error=1 shrink_ok=1" ]; then
  echo "  MemError follows NewPtr / failed SetPtrSize grow / successful shrink  OK"; echo "memerr: PASS"; exit 0
fi
echo "  rc=$rc got: $out"; echo "memerr: FAIL"; exit 1
