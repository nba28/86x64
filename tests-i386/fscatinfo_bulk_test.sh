#!/bin/bash
# fscatinfo_bulk_test.sh — A/B guard for the classic BULK directory enumeration.
#
# FSCatalogInfo is 144 bytes on i386 and 148 on x86_64 (FSPermissionInfo ends in
# an FSFileSecurityRef POINTER), so abigen deep-copies it through a scratch slot
# sized for ONE struct. FSGetCatalogInfoBulk fills `actualObjects` entries, so
# the native callee overruns the shim frame into the i386 caller's locals —
# measured on Civ IV as a SIGSEGV with the caller's just-allocated buffers
# reading back NULL.
#
# ON: all six files enumerated, every entry (not just entry 0) carries its own
#     file's size and a non-zero nodeID, canaries intact.
# OFF (M64_NO_FSCATINFO=1): the shim declines — arrays zeroed, actual==0, error
#     returned. Out-params still DEFINED (rule A), but nothing is enumerated.
set -u
cd "$(dirname "$0")"
BIN=build/99_fscatinfo_bulk.x86_64
if [ ! -x "$BIN" ]; then
  echo "fscatinfo-bulk: SKIP (build/99_fscatinfo_bulk.x86_64 missing)"; exit 0
fi
fail=0
has() { printf '%s\n' "$1" | grep -q "^$2\$"; }

on=$("$BIN" 2>/dev/null); rc=$?
want='sizeof_ok=1 fixture=1 pathref=1 iter_ok=1 bulk_err_ok=1 survived=1
canary_ok=1 count_ok=1 names_ok=1 all_sized=1 beyond_first=1 nodeids_ok=1 done=1'
miss=""; for k in $want; do has "$on" "$k" || miss="$miss $k"; done
if [ "$rc" = 0 ] && [ -z "$miss" ]; then
  echo "  ON  (shim armed):   all 6 entries enumerated; every entry beyond the"
  echo "                      first carries its OWN file's size; frame intact   OK"
else
  echo "  ON  (shim armed):   rc=$rc missing:$miss"; printf '%s\n' "$on" | sed 's/^/      /'; fail=1
fi

off=$(M64_NO_FSCATINFO=1 "$BIN" 2>/dev/null); orc=$?
# Must SURVIVE with defined out-params (rule A) but must NOT enumerate.
if [ "$orc" = 0 ] && has "$off" 'survived=1' && has "$off" 'canary_ok=1' \
   && has "$off" 'actual=0' && ! has "$off" 'count_ok=1' \
   && ! has "$off" 'all_sized=1' && ! has "$off" 'beyond_first=1'; then
  echo "  OFF (kill switch):  declines — arrays zeroed, actual=0, nothing"
  echo "                      enumerated; out-params still defined             OK"
else
  echo "  OFF (kill switch):  expected a clean decline with actual=0 (rc=$orc):"
  printf '%s\n' "$off" | sed 's/^/      /'; fail=1
fi

[ "$fail" = 0 ] && echo "fscatinfo-bulk: PASS" || echo "fscatinfo-bulk: FAIL"
exit $fail
