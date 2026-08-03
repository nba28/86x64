#!/bin/bash
#
# csmem_handle_test.sh — A/B guard proving carbon_memory.c's CSMem* coverage is
# what keeps a classic Handle intact across CarbonCore's internal entry points.
#
# A classic Handle is `Ptr*`, 4 bytes on i386. CarbonCore exports the whole
# Memory Manager twice — classic names and `CSMem*` twins — and the CSMem
# spelling is what CarbonCore and the classic QuickTime.framework call
# internally (the translated QuickTime imports 22 of them). Those bound straight
# to the native 64-bit Memory Manager, which breaks both ways: CSMemNewHandle
# returns a >4GB Handle that truncates into the i386 slot, and CSMemDisposeHandle
# reads an i386 Handle as 8 bytes. MEASURED on Civilization IV (2026-08-03):
# native CSMemDisposePtr got 0x0000_0037_9000_ff9c — a real i386 address in the
# low half, adjacent heap garbage in the high half — and libmalloc aborted.
#
# The ON side is the main-suite fixture 99_csmem_handle_family. This script adds
# the OFF side: re-run the SAME translated binary under M64_NO_CSMEM_SHIM=1,
# which forwards the allocation core to native CarbonCore, and assert the
# round-trip through the master pointer FAILS. Without that half a passing
# fixture would not prove these shims are load-bearing.
#
# Needs the i386 sysroot; SKIPs when the binary has not been built.
set -u
cd "$(dirname "$0")"

BIN=build/99_csmem_handle_family.x86_64
if [ ! -x "$BIN" ]; then
  echo "csmem-handle: SKIP (build/99_csmem_handle_family.x86_64 missing;"
  echo "                    run \`make 99_csmem_handle_family\` first)"
  exit 0
fi

fail=0
has() { printf '%s\n' "$1" | grep -q "^$2\$"; }

# --- ON: the family is ours -> every shape round-trips -----------------------
on=$("$BIN" 2>/dev/null); on_rc=$?
want='h_nonnull=1 roundtrip=1 size_exact=1 h2h_err=1 h2h_distinct=1 h2h_content=1
h2h_indep=1 hah_err=1 hah_size=1 hah_tail=1 p2h_err=1 p2h_ok=1 empty_null=1
realloc_err=1 realloc_ok=1 munger_at=1 munger_size=1 munger_content=1
ptr_nonnull=1 ptr_size=1 disposed=1 done=1'
missing=""
for k in $want; do has "$on" "$k" || missing="$missing $k"; done
if [ "$on_rc" = 0 ] && [ -z "$missing" ]; then
  echo "  ON  (CSMem shimmed):  Handle round-trip, exact size, HandToHand copy,"
  echo "                        HandAndHand, PtrToHand, Empty/Reallocate, Munger"
  echo "                        growing replace and the Ptr half all correct     OK"
else
  echo "  ON  (CSMem shimmed):  rc=$on_rc, missing/failed:$missing"
  printf '%s\n' "$on" | sed 's/^/      /'
  fail=1
fi

# --- OFF: kill switch -> native Memory Manager -> a truncated Handle ---------
# The process may die outright (a truncated master pointer is a wild deref) or
# survive with wrong data; either is acceptable evidence, but a clean, CORRECT
# run is not -- that would mean the shims are not what makes this work.
off=$(M64_NO_CSMEM_SHIM=1 "$BIN" 2>/dev/null); off_rc=$?
if [ "$off_rc" != 0 ] || ! has "$off" 'roundtrip=1'; then
  echo "  OFF (kill switch):    native CSMemNewHandle -> >4GB Handle truncated into"
  echo "                        the i386 4-byte slot; round-trip fails (rc=$off_rc) OK"
else
  echo "  OFF (kill switch):    expected the native Memory Manager to hand back a"
  echo "                        Handle that cannot survive an i386 slot. It round-"
  echo "                        tripped cleanly (rc=$off_rc), so this guard is NOT"
  echo "                        exercising the CSMem coverage."
  printf '%s\n' "$off" | sed 's/^/      /'
  fail=1
fi

if [ "$fail" = 0 ]; then
  echo "csmem-handle: PASS"
else
  echo "csmem-handle: FAIL"
fi
exit $fail
