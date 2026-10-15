#!/bin/bash
# jt_lea_sum_test.sh — A/B guard: a PIC switch that sums entry and anchor with
# `lea (%t,%anchor),%d` (unoptimised code; src/99_jt_lea_sum_dispatch.s has the
# story) must be claimed as a jump table. The suite run checks exit 42.
#
# ON  : the i386 detector pass claims the table (one dispatch).
# OFF : M64_NO_JT_LEA_SUM=1 — no dispatch; the table bytes are parsed as code.
set -u
cd "$(dirname "$0")"
MT=../build/src/macho-tool/macho-tool
I386=build/99_jt_lea_sum_dispatch.i386
[ -f "$I386" ] || { echo "jt-lea-sum: FAIL (no $I386)"; exit 1; }
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
det() { env "$@" MACHO_TRACE_JUMPTABLE=1 "$MT" transform "$I386" "$TMP/out" 2>&1 \
          | grep -c '^\[jumptable\] dispatch@.* count=4'; }
on=$(det); off=$(det M64_NO_JT_LEA_SUM=1)
echo "  ON: $on table(s)   OFF: $off table(s)"
[ "$on" -ge 1 ] && [ "$off" = 0 ] && { echo "jt-lea-sum: PASS"; exit 0; }
echo "jt-lea-sum: FAIL"; exit 1
