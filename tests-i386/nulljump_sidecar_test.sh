#!/bin/bash
# nulljump_sidecar_test.sh — A/B guard that the NULL-jump exclusion list TRAVELS
# with libabiconv, and that its absence is LOUD rather than silent.
#
# static-interpose.sh resolves the list as
# `dirname(libabiconv.dylib)/libabiconv.nulljump`. A translate driven against a
# BUNDLE-LOCAL libabiconv copy therefore finds nothing unless the sidecar was
# copied beside it — and the old code then set NULLJUMP_N=0 and carried on with
# NO diagnostic, so the whole f71a3c1 protection evaporated invisibly.
#
# ★MEASURED, Halo 2026-08-05: the deployed bundle had 4 libabiconv.dylib and 0
# libabiconv.nulljump. Its translated QuickTime.framework consequently got
# _ResolveAliasFile redirected into a NULL-jump bridge and died live with rip=0
# ([rsp] named ___ResolveAliasFile.1). Same family as the libabiconv multi-copy gotcha.
#
# ARMS:
#   PRESENT : `m64 resync` on a throwaway bundle must place libabiconv.nulljump
#             beside EVERY libabiconv.dylib it syncs.
#   ABSENT  : with the sidecar removed, static-interpose must WARN on stderr.
#             Silence here is the defect.
set -u
cd "$(dirname "$0")"
ROOT=..
LIB=$ROOT/build/src/abiconv/libabiconv.dylib
NJ=$ROOT/build/src/abiconv/libabiconv.nulljump
SI=$ROOT/src/86x64/static-interpose.sh
WORK="${TMPDIR:-/tmp}/nulljump_sidecar.$$"

if [ ! -f "$LIB" ] || [ ! -s "$NJ" ]; then
  echo "nulljump-sidecar: SKIP (build libabiconv + the abiconv_nulljump target first)"; exit 0
fi
mkdir -p "$WORK"; trap 'rm -rf "$WORK"' EXIT
fail=0

# ---- PRESENT arm: a bundle shaped like a real one, with copies at two depths --
APP="$WORK/T.app"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks/Q.framework/Versions/A"
cp "$LIB" "$APP/Contents/MacOS/libabiconv.dylib"
cp "$LIB" "$APP/Contents/Frameworks/Q.framework/Versions/A/libabiconv.dylib"
python3 "$ROOT/src/86x64/m64" resync "$APP" >"$WORK/resync.out" 2>&1
ncopy=$(find "$APP" -name libabiconv.dylib | wc -l | tr -d ' ')
nside=$(find "$APP" -name libabiconv.nulljump | wc -l | tr -d ' ')
if [ "$ncopy" -gt 0 ] && [ "$nside" = "$ncopy" ]; then
  echo "  PRESENT (m64 resync): all $ncopy libabiconv copies got the exclusion list  OK"
else
  echo "  PRESENT (m64 resync): $nside of $ncopy copies got libabiconv.nulljump —"
  echo "                        a translate against the others silently interposes"
  echo "                        NULL-jump bridges"
  sed -n 1,6p "$WORK/resync.out" | sed 's/^/      /'
  fail=1
fi

# ---- ABSENT arm: the warning must actually be emitted ------------------------
# Drive static-interpose with a libabiconv that has NO sidecar and an empty
# symbol list, so it does no real work and we are asserting purely on the
# diagnostic. It may exit nonzero for unrelated reasons; only stderr matters.
# static-interpose calls `macho-tool` off PATH.
export PATH="$(cd "$ROOT/build/src/macho-tool" && pwd):$PATH"
BARE="$WORK/bare"; mkdir -p "$BARE"; cp "$LIB" "$BARE/libabiconv.dylib"
: > "$WORK/empty.syms"
bash "$SI" -l "$BARE/libabiconv.dylib" -n "@loader_path/libabiconv.dylib" \
     -p "__" -o "$WORK/out.bin" "$BARE/libabiconv.dylib" \
     < "$WORK/empty.syms" >"$WORK/si.out" 2>"$WORK/si.err" || true
if grep -q "no NULL-jump exclusion list" "$WORK/si.err"; then
  echo "  ABSENT  (no sidecar):  static-interpose WARNS instead of silently"
  echo "                        disabling the gate                                OK"
else
  echo "  ABSENT  (no sidecar):  expected a warning on stderr; got none. A missing"
  echo "                        list silently disables the whole exclusion."
  sed -n 1,6p "$WORK/si.err" | sed 's/^/      /'
  fail=1
fi

[ "$fail" = 0 ] && echo "nulljump-sidecar: PASS" || echo "nulljump-sidecar: FAIL"
exit $fail
