#!/bin/bash
#
# mp_allocate_buffer_test.sh — A/B guard proving mp_memory_shim.c is what makes
# Carbon Multiprocessing Services' allocator hand back real memory.
#
# `MPAllocateAligned` returns `LogicalAddress` = `typedef void *`, so abigen's
# generic void*-return rule (abigen.cc:1644-1656) wrapped its >4GB native
# pointer into a 32-bit PROXY ARENA HANDLE — an 8-byte arena slot handed to a
# caller that believes it owns N bytes. MEASURED on Halo CE (2026-08-08):
# consecutive 64KB requests came back 8 BYTES APART (handle=0x808095b8,
# handle=0x808095c0, ...); Halo wrote audio data over the live handle table and
# died in memset, whose arg unwrap resolved the "buffer" back through the arena
# to 0 -> SIGSEGV writing to 0x0.
#
# ON  = mp_memory_shim.c: allocate from libabiconv's low-4GB heap, honour the
#       alignment exponent, and let MPFree actually reclaim.
# OFF = M64_NO_MP_LOW_HEAP=1: call native MPAllocateAligned and x64_objc_wrap the
#       result, i.e. exactly what the generated bridge did. The OFF arm must NOT
#       come out clean — otherwise this guard is not exercising the shim.
#
# Needs the i386 sysroot; SKIPs when the binary has not been built.
set -u
cd "$(dirname "$0")"

BIN=build/99_mp_allocate_buffer.x86_64
if [ ! -x "$BIN" ]; then
  echo "mp-allocate-buffer: SKIP (build/99_mp_allocate_buffer.x86_64 missing;"
  echo "                          run \`make 99_mp_allocate_buffer\` first)"
  exit 0
fi

fail=0
has() { printf '%s\n' "$1" | grep -q "^$2\$"; }

# --- ON: the allocator is ours -> real, distinct, reclaimed memory -----------
on=$("$BIN" 2>/dev/null); on_rc=$?
want='a_nonnull=1 b_nonnull=1 distinct=1 a_intact=1 b_intact=1 size_exact=1
readback=1 pg_nonnull=1 pg_aligned=1 cleared=1 mp_nonnull=1 mp_size=1
reclaim_same_addr=1 churn=1 done=1'
missing=""
for k in $want; do has "$on" "$k" || missing="$missing $k"; done
if [ "$on_rc" = 0 ] && [ -z "$missing" ]; then
  echo "  ON  (MP on low heap): blocks are distinct and independent, exact size,"
  echo "                        every byte writable, 4096B alignment honoured,"
  echo "                        clear mask works, MPFree reclaims (same address"
  echo "                        back) and 512 alloc/free cycles hold             OK"
else
  echo "  ON  (MP on low heap): rc=$on_rc, missing/failed:$missing"
  printf '%s\n' "$on" | sed 's/^/      /'
  fail=1
fi

# --- OFF: kill switch -> native MPAllocateAligned + proxy-handle wrap --------
# The block "returned" is an 8-byte arena slot, so two live N-byte allocations
# land 8 bytes apart and writing one destroys the other. The process may also
# die outright once it scribbles over the proxy arena; either is acceptable
# evidence, but a clean, CORRECT run is not.
off=$(M64_NO_MP_LOW_HEAP=1 "$BIN" 2>/dev/null); off_rc=$?
if [ "$off_rc" != 0 ] || ! has "$off" 'distinct=1' || ! has "$off" 'done=1'; then
  gap=$(printf '%s\n' "$off" | sed -n 's/^gap=//p')
  echo "  OFF (kill switch):    native MPAllocateAligned -> >4GB pointer minted as"
  echo "                        an 8-byte proxy-arena handle; the two live blocks"
  echo "                        overlap (gap=${gap:-?}, want >=4096), rc=$off_rc   OK"
else
  echo "  OFF (kill switch):    expected the proxy-handle path to hand back"
  echo "                        overlapping 8-byte arena slots. It produced real,"
  echo "                        distinct blocks (rc=$off_rc), so this guard is NOT"
  echo "                        exercising mp_memory_shim.c."
  printf '%s\n' "$off" | sed 's/^/      /'
  fail=1
fi

if [ "$fail" = 0 ]; then
  echo "mp-allocate-buffer: PASS"
else
  echo "mp-allocate-buffer: FAIL"
fi
exit $fail
