#!/bin/bash
# nulljump_sidecar_test.sh — A/B guard that a MISSING NULL-jump exclusion list is
# LOUD rather than silent.
#
# static-interpose.sh resolves the list as
# `dirname(libabiconv.dylib)/libabiconv.nulljump`. The normal pipeline always
# passes the BUILD-TREE libabiconv, which sits beside the freshly generated list,
# so in practice it is found. But when it is NOT — a hand-run translate, a
# relocated dylib, a build tree where the abiconv_nulljump target was never
# built — the old code set NULLJUMP_N=0 and carried on with NO diagnostic, so the
# entire f71a3c1 protection evaporated invisibly. Silent loss of a safety gate is
# the defect this pins down.
#
# ⚠The list is deliberately NOT copied into deployed bundles: unnecessary (nothing
# translates against a bundle-local copy) and it BREAKS SIGNING — a stray file at
# a framework's version root fails `codesign --verify --deep --strict` with
# "a sealed resource is missing or invalid". See the note in m64.
#
# ARMS:
#   PRESENT : list beside the dylib  -> no warning, and it is actually consulted.
#   ABSENT  : list removed           -> static-interpose WARNS on stderr.
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
export PATH="$(cd "$ROOT/build/src/macho-tool" && pwd):$PATH"   # static-interpose calls it off PATH
: > "$WORK/empty.syms"

# The check runs before any translation work, so an input that macho-tool cannot
# process is fine — we are asserting purely on the diagnostic.
run_si() {   # $1 = dir holding libabiconv.dylib (and maybe the sidecar)
  bash "$SI" -l "$1/libabiconv.dylib" -n "@loader_path/libabiconv.dylib" \
       -p "__" -o "$WORK/out.bin" "$1/libabiconv.dylib" \
       < "$WORK/empty.syms" >"$WORK/si.out" 2>"$WORK/si.err" || true
}

WITH="$WORK/with"; mkdir -p "$WITH"; cp "$LIB" "$WITH/"; cp "$NJ" "$WITH/"
run_si "$WITH"
if grep -q "no NULL-jump exclusion list" "$WORK/si.err"; then
  echo "  PRESENT (list beside dylib): warned anyway — the check is misfiring"; fail=1
else
  echo "  PRESENT (list beside dylib): no warning; the list is consulted        OK"
fi

BARE="$WORK/bare"; mkdir -p "$BARE"; cp "$LIB" "$BARE/"
run_si "$BARE"
if grep -q "no NULL-jump exclusion list" "$WORK/si.err"; then
  echo "  ABSENT  (no list):           static-interpose WARNS instead of silently"
  echo "                               disabling the gate                       OK"
else
  echo "  ABSENT  (no list):           expected a warning on stderr; got none. A"
  echo "                               missing list silently disables the gate."
  sed -n 1,6p "$WORK/si.err" | sed 's/^/      /'
  fail=1
fi

[ "$fail" = 0 ] && echo "nulljump-sidecar: PASS" || echo "nulljump-sidecar: FAIL"
exit $fail
