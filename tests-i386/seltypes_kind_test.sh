#!/bin/bash
# seltypes_kind_test.sh — A/B guard for src/99_objc_seltypes_kind.m (objc_shim.c
# seltypes_refines). A legacy `-addObject:(void *)` must not turn the object arg
# of -[NSMutableArray addObject:] into a raw pointer.
# ON: exact expected output. OFF (M64_NO_SELTYPES_KIND_GUARD=1 at run time):
# anything else (crash or wrong element).
set -u
cd "$(dirname "$0")"
# CC / LD / LDF come from the Makefile target (seltypes-kind).
T=99_objc_seltypes_kind; B=build/$T
$CC -c src/$T.m -o $B.o || { echo "seltypes-kind: FAIL (compile)"; exit 1; }
$LD $LDF -e _main -o $B.i386 $B.o || { echo "seltypes-kind: FAIL (link)"; exit 1; }
bash ../src/86x64/86x64.sh -m ../build/src/macho-tool/macho-tool \
  -l ../build/src/abiconv/libabiconv.dylib -w ../build/src/86x64/libwrapper.a \
  -i ../build/src/86x64/libinterpose.dylib -o $B.x86_64 $B.i386 >/dev/null 2>&1 \
  || { echo "seltypes-kind: FAIL (translate)"; exit 1; }
run() { { env "$@" ./$B.x86_64 2>/dev/null; echo "exit_code: $?"; } | tr '\n' ' '; }
want=$(tr '\n' ' ' < expected/$T.txt)
on=$(run X=1); off=$(run M64_NO_SELTYPES_KIND_GUARD=1)
echo "  ON : $on"; echo "  OFF: $off"
fail=0
[ "$on" = "$want" ] || { echo "  ON arm wrong"; fail=1; }
[ "$off" != "$want" ] || { echo "  OFF arm passes — guard inert"; fail=1; }
[ $fail = 0 ] && echo "seltypes-kind: PASS" || echo "seltypes-kind: FAIL"
exit $fail
