#!/bin/bash
#
# cfdata_bytebuf_bounce_test.sh — A/B guard proving src/abiconv/cfdata_bytebuf_shim.c
# is what keeps CFDataGetBytePtr/CFDataGetMutableBytePtr honest about a byte
# buffer's REAL length.
#
# abigen's generic return-value rule treats any `char*`/`unsigned char*`
# return as a C string and bounces a high native pointer through a
# strlen(p)+1 copy. That is correct for glGetString but wrong for a buffer
# whose length is carried separately (CFDataGetLength) — a 0x00 byte inside
# it is content, not a terminator. MEASURED on Halo CE: its 88-byte
# "Graphics Options" preference starts with 0x00, so the cstring bounce
# copied ONE byte and Halo's own memcpy(dst, ptr, CFDataGetLength(v)) read 87
# bytes of low-heap garbage past it, silently skipping the Graphics Settings
# dialog on every launch.
#
# The ON side is the main-suite fixture 99_cfdata_bytebuf_bounce. This script
# adds the OFF side: re-run the SAME translated binary under
# M64_NO_CFDATA_BYTEBUF_BOUNCE=1 (which makes the hand shim reproduce the old
# generic-cstring-bounce behaviour exactly) and assert BOTH checks come back
# broken. Without that half, a passing fixture would only prove the ON
# behaviour exists, not that this fix is what produces it.
#
# Needs the i386 sysroot; SKIPs when the binary has not been built.
set -u
cd "$(dirname "$0")"

BIN=build/99_cfdata_bytebuf_bounce.x86_64
if [ ! -x "$BIN" ]; then
  echo "cfdata-bytebuf-bounce: SKIP (build/99_cfdata_bytebuf_bounce.x86_64 missing;"
  echo "                             run \`make 99_cfdata_bytebuf_bounce\` first)"
  exit 0
fi

fail=0
has() { printf '%s\n' "$1" | grep -q "^$2\$"; }

# --- ON: real length used -> every byte round-trips, both immutable and the
#         mutable write-through across a SEPARATE later bridge call ----------
on=$("$BIN" 2>/dev/null); on_rc=$?
if [ "$on_rc" = 0 ] &&
   has "$on" 'len=88' && has "$on" 'immutable_roundtrip=1' &&
   has "$on" 'mutable_writethrough=1' && has "$on" 'done=1'; then
  echo "  ON  (length-aware bounce):  full 88-byte immutable round-trip and mutable"
  echo "                              write-through both survive                  OK"
else
  echo "  ON  (length-aware bounce):  expected success, got rc=$on_rc:"
  printf '%s\n' "$on" | sed 's/^/      /'
  fail=1
fi

# --- OFF: kill switch -> strlen-truncated copy, buffer corrupted/short -------
# The mutable check performs a real heap OOB write in this arm (the pre-fix
# defect IS a heap-corrupting bug, not just a wrong-answer one), so the
# process may crash instead of printing every line; a nonzero exit code or a
# missing/failed check line both count as the expected divergence.
off=$(M64_NO_CFDATA_BYTEBUF_BOUNCE=1 "$BIN" 2>/dev/null); off_rc=$?
if [ "$off_rc" != 0 ] || ! has "$off" 'immutable_roundtrip=1' ||
   ! has "$off" 'mutable_writethrough=1' || ! has "$off" 'done=1'; then
  echo "  OFF (kill switch):          the truncated buffer failed to round-trip"
  echo "                              (rc=$off_rc) — the unfixed behaviour           OK"
else
  echo "  OFF (kill switch):          expected the strlen-truncated copy to lose data"
  echo "                              (or crash). It fully survived (rc=$off_rc), so"
  echo "                              this guard is NOT exercising the length bounce."
  printf '%s\n' "$off" | sed 's/^/      /'
  fail=1
fi

if [ "$fail" = 0 ]; then
  echo "cfdata-bytebuf-bounce: PASS"
else
  echo "cfdata-bytebuf-bounce: FAIL"
fi
exit $fail
