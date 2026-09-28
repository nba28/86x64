#!/bin/bash
#
# init-slot-rebase: every widened 8-byte __mod_init_func slot of a translated
# image carries exactly one REBASE, even when the input already had a few.
#
# Archive::synthesize_dyld_info used to add rebases only when the existing
# table was EMPTY (its proxy for a non-PIE input). But transform itself rebases
# the lazy pointers it re-points at the translated stub_helper, so a non-PIE
# C++ executable reaches convert with a non-empty table and its init slots got
# none: dyld slid the dylib and the slot kept the preferred address (hidden only
# by objc_slide.c's runtime recovery of unrebased init slots).
#
# Two arms over 69_cpp_ios_base_init (non-PIE, one static ctor):
#   shape: the transform output HAS rebases and none on __mod_init_func — the
#          input still presents the case the old empty-table check skipped;
#   fix:   the final dylib rebases every __mod_init_func slot exactly once.
set -u
cd "$(dirname "$0")"
MT="${1:-../build/src/macho-tool/macho-tool}"
IN=build/69_cpp_ios_base_init.i386
make -s "$IN" >/dev/null 2>&1   # an explicit target, so make keeps it
[ -f "$IN" ] || { echo "SKIP init-slot-rebase (cannot build $IN; needs the i386 sysroot)"; exit 0; }
fail() { echo "FAIL init-slot-rebase ($1)"; exit 1; }

T=build/init_slot_rebase; rm -rf $T; mkdir -p $T
"$MT" rebasify "$IN" $T/r32 2>$T/err &&
"$MT" -- transform $T/r32 $T/t64 2>>$T/err &&
"$MT" -- modify --insert load-dylib,name=@rpath/libabiconv.dylib $T/t64 $T/m64 2>>$T/err &&
"$MT" convert --archive DYLIB --synthesize-dyld-info $T/m64 $T/d64 2>>$T/err ||
   { sed 's/^/    /' $T/err; fail translate; }

t_all=$(dyld_info -fixups $T/t64 | grep -c " rebase ")
t_init=$(dyld_info -fixups $T/t64 | grep __mod_init_func | grep -c " rebase ")
[ "$t_all" -gt 0 ] && [ "$t_init" -eq 0 ] ||
   fail "shape arm: transform output has $t_all rebases, $t_init on init slots — fixture no longer presents the case"

slots=$(( $(otool -l $T/d64 | grep -A4 "sectname __mod_init_func" | awk '/ size /{print $2}') / 8 ))
d_init=$(dyld_info -fixups $T/d64 | grep __mod_init_func | grep " rebase " | awk '{print $3}')
n=$(echo "$d_init" | grep -c .); u=$(echo "$d_init" | sort -u | grep -c .)
[ "${slots:-0}" -gt 0 ] || fail "no __mod_init_func in output"
[ "$n" -eq "$slots" ] && [ "$u" -eq "$n" ] ||
   fail "fix arm: $slots init slot(s), $n rebase(s), $u distinct"
echo "PASS init-slot-rebase"
