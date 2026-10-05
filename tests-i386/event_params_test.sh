#!/bin/bash
# event_params_test.sh — guard for carbon_event_param_shim.c: Carbon event
# parameters whose layout differs between i386 and x86_64 (references,
# CGFloat geometry, HICommand) are converted; same-layout types pass through.
# hipoint_coerced fails on the plain bridge (native coercion to a 16-byte
# double HIPoint does not fit the i386 8-byte buffer). Headless.
set -u
cd "$(dirname "$0")"
BIN=build/99_event_params.x86_64
[ -x "$BIN" ] || { echo "event-params: FAIL ($BIN missing)"; exit 1; }
out=$("$BIN" 2>/dev/null); rc=$?
fail=0
for k in event=1 qdpoint_same=1 hipoint_coerced=1 cfref_roundtrip=1 hicommand_size14=1 hicommand_roundtrip=1 hicommand_no_overrun=1; do
  grep -qx "$k" <<<"$out" || { echo "  missing $k"; fail=1; }
done
[ $rc = 0 ] || fail=1
# OFF arm: the old 16-byte i386 HICommand must reproduce the dead command handler.
off=$(M64_NO_HICMD_PACK2=1 "$BIN" 2>/dev/null)
if grep -qx "hicommand_roundtrip=1" <<<"$off"; then
  echo "  OFF (M64_NO_HICMD_PACK2=1) still round-trips a 14-byte HICommand: guard inert"; fail=1
else echo "  OFF arm: the 16-byte assumption rejects a real sizeof(HICommand)  OK"; fi
if [ $fail = 0 ]; then echo "  refs, HIPoint (coerced), HICommand and pass-through types all cross intact  OK"; echo "event-params: PASS"
else printf '%s\n' "$out" | sed 's/^/      /'; echo "event-params: FAIL"; fi
exit $fail
