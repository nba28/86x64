#!/bin/bash
# carbon_datetime_test.sh — A/B guard for the classic Date & Time Utilities.
# These were REMOVED from modern macOS; abigen's bridge calls a bind the
# pipeline weakened to NULL, so the classic call jumps to 0 (rip=0). Measured on
# Halo CE 2026-08-03 via M64_FAULT_REPORT, which named ___SecondsToDate.l1.
# ON: conversions round-trip. OFF (M64_NO_CARBON_DATETIME=1): out-params come
# back ZEROED — defined, per rule A, but not converted.
set -u
cd "$(dirname "$0")"
BIN=build/99_carbon_datetime.x86_64
if [ ! -x "$BIN" ]; then
  echo "carbon-datetime: SKIP (build/99_carbon_datetime.x86_64 missing)"; exit 0
fi
fail=0
has() { printf '%s\n' "$1" | grep -q "^$2\$"; }
on=$("$BIN" 2>/dev/null); rc=$?
want='survived=1 year_sane=1 month_sane=1 day_sane=1 dow_sane=1 roundtrip=1
long_year=1 long_month=1 long_dow=1 long_doy=1 long_pm=1 long_roundtrip=1 done=1'
miss=""; for k in $want; do has "$on" "$k" || miss="$miss $k"; done
if [ "$rc" = 0 ] && [ -z "$miss" ]; then
  echo "  ON  (shim armed):   SecondsToDate/DateToSeconds and the Long* pair convert"
  echo "                      and round-trip; fields internally consistent          OK"
else
  echo "  ON  (shim armed):   rc=$rc missing:$miss"; printf '%s\n' "$on" | sed 's/^/      /'; fail=1
fi
off=$(M64_NO_CARBON_DATETIME=1 "$BIN" 2>/dev/null); orc=$?
# Must still SURVIVE (rule A: declining leaves out-params defined, never a jump
# to NULL) but must NOT convert.
if [ "$orc" = 0 ] && has "$off" 'survived=1' && ! has "$off" 'roundtrip=1' && ! has "$off" 'year_sane=1'; then
  echo "  OFF (kill switch):  out-params come back ZEROED — defined but unconverted OK"
else
  echo "  OFF (kill switch):  expected zeroed, unconverted out-params (rc=$orc):"
  printf '%s\n' "$off" | sed 's/^/      /'; fail=1
fi
[ "$fail" = 0 ] && echo "carbon-datetime: PASS" || echo "carbon-datetime: FAIL"
exit $fail
