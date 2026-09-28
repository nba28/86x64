#!/bin/bash
#
# pgmemo_saturation_test.sh — A/B guard for the BOUNDED probe window of
# libabiconv's page-readability memo (objc_shim.c page_readable_len).
#
# The memo is an open-addressed table whose entries are never deleted, so it
# only fills. With an unbounded linear probe a lookup of an absent page walked
# the whole 262144-entry table once it was nearly full. MEASURED (Quinn, 10.5 h
# session): 66% of main-thread samples in that loop, the game crawled.
#
# The fixture saturates the memo with 300000 readable pages, then times 2000
# lookups of unseen pages.
#   ON  (shipped):                    bounded window, fast
#   OFF (M64_NO_PGMEMO_WINDOW=1):     the full walk, >= 20x slower
# Native x86_64 only (dlopen of the REAL libabiconv; no i386 sysroot).
set -u
cd "$(dirname "$0")"
LIBABICONV="${1:?usage: pgmemo_saturation_test.sh <path-to-libabiconv.dylib>}"
fail() { echo "FAIL pgmemo-saturation: $1"; exit 1; }
mkdir -p build
# libabiconv maps its proxy arena in the low 4GB at load; a default x86_64
# host's 4GB __PAGEZERO forbids that (same as snapshot-compat).
clang -arch x86_64 -O2 -Wl,-pagezero_size,0x1000 \
      -o build/pgmemo_saturation src/pgmemo_saturation_fixture.c || fail compile

run() { env "$@" perl -e 'my $p = fork; if (!$p) { exec @ARGV or exit 127 }
          $SIG{ALRM} = sub { kill "KILL", $p }; alarm 120; waitpid($p, 0);
          exit($? & 127 ? 128 + ($? & 127) : $? >> 8)' \
          build/pgmemo_saturation "$LIBABICONV" 2>/dev/null; }
off=$(run M64_NO_PGMEMO_WINDOW=1) || fail "OFF arm did not complete: $off"
on=$(run) || fail "ON arm did not complete: $on"      # ★ON arm last

echo "  ON  (bounded window):   ${on} us for 2000 misses on a saturated memo"
echo "  OFF (unbounded walk):   ${off} us"
[ "$off" -ge $((on * 20)) ] || fail "OFF arm is not >=20x slower (guard inert or fix ineffective)"
echo "PASS pgmemo-saturation"
