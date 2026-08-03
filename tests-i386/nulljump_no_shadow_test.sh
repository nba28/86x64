#!/bin/bash
#
# nulljump_no_shadow_test.sh — A/B guard proving static-interpose refuses to
# redirect a bind into a bridge that can only jump to NULL.
#
# abigen emits ___X for anything in its consider set with a parseable 10.6
# declaration; when Apple REMOVES the native _X the bridge survives, the pipeline
# weakens the dangling native bind to NULL, and the bridge's call jumps to 0.
# Redirecting the app's bind into that bridge REPLACES whatever the app would
# have bound to with a guaranteed crash. Measured 2026-08-03: 507 such bridges,
# 358 of them shadowing a WORKING implementation the app itself ships (347 from
# its own bundled translated QuickTime.framework, 11 from its bundled Python) —
# Civ IV's QTNewDataReferenceFromFSRef death.
#
# ARMS (both are BUILDS of the same source; the assertion is on the bind table):
#   ON  : normal build. The bind must NOT point at libabiconv.
#   OFF : NULLJUMP=/dev/null, which empties the exclusion list. The bind MUST
#         point at libabiconv — i.e. the defect reproduces.
#
# ⚠Read with `dyld_info -fixups`. `nm -m` / `otool -Iv` report the NLIST table,
#  which static-interpose does not rewrite, and answer this wrongly in BOTH arms.
set -u
cd "$(dirname "$0")"

SYM=CTabChanged
NJ=../build/src/abiconv/libabiconv.nulljump
SRC=src/99_nulljump_no_shadow.c

if [ ! -s "$NJ" ]; then
  echo "nulljump-no-shadow: SKIP (no $NJ — build libabiconv first)"; exit 0
fi
# The guard is only meaningful while the chosen symbol is still an orphan.
if ! grep -qFx "_$SYM" "$NJ"; then
  echo "nulljump-no-shadow: SKIP — _$SYM is no longer in libabiconv.nulljump."
  echo "  It was probably implemented. Pick another symbol still in that list"
  echo "  (the point is that it has NO provider) and update SYM here + the fixture."
  exit 0
fi

fail=0
build_arm() {   # $1 = tag, $2 = NULLJUMP value
  rm -f "build/99_nulljump_no_shadow.$1.x86_64" build/99_nulljump_no_shadow.i386 \
        build/99_nulljump_no_shadow.o build/99_nulljump_no_shadow.x86_64 2>/dev/null
  NULLJUMP="$2" make -s build/99_nulljump_no_shadow.x86_64 >/dev/null 2>&1
  cp build/99_nulljump_no_shadow.x86_64.dylib "build/99_nulljump_no_shadow.$1.dylib" 2>/dev/null \
    || cp build/99_nulljump_no_shadow.x86_64 "build/99_nulljump_no_shadow.$1.dylib" 2>/dev/null
}
# Detect the RENAME, not the dylib name. static-interpose rewrites _X -> ___X
# and (for two-level images) also retargets the ordinal; a dynamic_lookup fixture
# like this one keeps a <flat-namespace> bind, where the rename alone decides the
# outcome: ___X resolves to libabiconv's NULL-jump bridge, _X stays unresolved
# and therefore NULL, which is what the app's weak check needs.
redirected_to_bridge() {   # $1 = dylib
  dyld_info -fixups "$1" 2>/dev/null | grep -q "/___$SYM\b"
}

build_arm on ""            # default: the real list is consulted
if [ -f build/99_nulljump_no_shadow.on.dylib ] && ! redirected_to_bridge build/99_nulljump_no_shadow.on.dylib; then
  echo "  ON  (list honoured): _$SYM left bound to its original framework, so the"
  echo "                       app's weak NULL-check still works                  OK"
else
  echo "  ON  (list honoured): _$SYM was renamed to ___$SYM, i.e. redirected into the NULL-jump"
  echo "                       bridge — the shadowing defect is NOT fixed"
  fail=1
fi

build_arm off /dev/null    # empty list: the pre-fix behaviour
if [ -f build/99_nulljump_no_shadow.off.dylib ] && redirected_to_bridge build/99_nulljump_no_shadow.off.dylib; then
  echo "  OFF (list emptied):  _$SYM renamed to ___$SYM (the NULL-jump bridge) — the unfixed"
  echo "                       behaviour reproduces                               OK"
else
  echo "  OFF (list emptied):  expected _$SYM to be renamed to ___$SYM."
  echo "                       It was not, so this guard is NOT exercising the"
  echo "                       exclusion (is $SYM still imported by the fixture?)"
  fail=1
fi

if [ "$fail" = 0 ]; then echo "nulljump-no-shadow: PASS"; else echo "nulljump-no-shadow: FAIL"; fi
exit $fail
