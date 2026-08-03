#!/bin/bash
#
# cfstring_range_test.sh — A/B guard proving src/abiconv/cfstring_range_shim.c is
# what keeps a translated classic app alive when it asks CFStringGetCharacters
# for a range past the end of a string.
#
# Classic CoreFoundation never validated the range, so length+1 (to pick up a
# NUL terminator) was a normal 32-bit-era idiom. Modern CoreFoundation validates
# in SOME concrete string classes only:
#
#     -[NSTaggedPointerString getCharacters:range:]  -> _CFThrowFormattedException
#     __NSCFString                                   -> tolerated silently
#
# so the SAME call site is fatal for a short string and harmless for a longer
# one. The throw is an ObjC exception crossing a C frame that cannot catch it,
# so it reaches _objc_terminate and aborts the process — which is exactly how
# Civilization IV died on every launch of the game proper (2026-08-03).
#
# The ON side is the main-suite fixture 99_cfstring_range_overrun. This script
# adds the OFF side: re-run the SAME translated binary under
# M64_NO_CFSTRING_RANGE_CLAMP=1 and assert the abort comes back. Without that
# half a passing fixture would only prove that CF tolerated the range on this
# particular OS build, not that our clamp is doing the work.
#
# Needs the i386 sysroot; SKIPs when the binary has not been built.
set -u
cd "$(dirname "$0")"

BIN=build/99_cfstring_range_overrun.x86_64
if [ ! -x "$BIN" ]; then
  echo "cfstring-range: SKIP (build/99_cfstring_range_overrun.x86_64 missing;"
  echo "                      run \`make 99_cfstring_range_overrun\` first)"
  exit 0
fi

fail=0
has() { printf '%s\n' "$1" | grep -q "^$2\$"; }

# --- ON: the clamp is armed -> survives, copies the real characters, and the
#         tail the string does not have comes back deterministically zeroed ----
on=$("$BIN" 2>/dev/null); on_rc=$?
if [ "$on_rc" = 0 ] &&
   has "$on" 'len=6'      && has "$on" 'c1_survived=1' && has "$on" 'c1_chars=1' &&
   has "$on" 'c1_term=1'  && has "$on" 'c2_chars=1'    && has "$on" 'c2_zeros=1' &&
   has "$on" 'c3_zeros=1' && has "$on" 'done=1'; then
  echo "  ON  (clamp armed):  length+1, partial overrun and wholly-past-the-end all"
  echo "                      survive; real characters copied, tail zero-filled     OK"
else
  echo "  ON  (clamp armed):  expected survival with a zero-filled tail, got rc=$on_rc:"
  printf '%s\n' "$on" | sed 's/^/      /'
  fail=1
fi

# --- OFF: kill switch -> CF validates the raw range and the process aborts ----
# 134 = 128 + SIGABRT. Any clean exit means the arms did NOT differ.
off=$(M64_NO_CFSTRING_RANGE_CLAMP=1 "$BIN" 2>/dev/null); off_rc=$?
if [ "$off_rc" != 0 ] && ! has "$off" 'c1_survived=1'; then
  echo "  OFF (kill switch):  CFStringGetCharacters(range=len+1) threw and aborted the"
  echo "                      process (rc=$off_rc) — the unfixed behaviour             OK"
else
  echo "  OFF (kill switch):  expected the length+1 request to ABORT (the unfixed"
  echo "                      behaviour). It survived (rc=$off_rc), so this guard is NOT"
  echo "                      exercising the clamp. Most likely CF stopped backing a"
  echo "                      6-character string with a tagged pointer — see the note"
  echo "                      in the fixture about why the string must stay SHORT."
  printf '%s\n' "$off" | sed 's/^/      /'
  fail=1
fi

if [ "$fail" = 0 ]; then
  echo "cfstring-range: PASS"
else
  echo "cfstring-range: FAIL"
fi
exit $fail
