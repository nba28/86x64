#!/bin/bash
#
# change-deps-sigroom: `macho-tool change-deps` must refuse a rewrite that
# leaves no header room for the code signature. Every translated image is
# ad-hoc signed afterwards, and codesign appends its 16-byte LC_CODE_SIGNATURE
# after the load commands without checking for room: with no slack it
# overwrites the first bytes of __text and still exits 0 (Civ IV's Python
# modules jumped into a header-shaped init after consolidate-runtime).
#
# Native x86_64 fixture (no i386 sysroot): an unsigned dylib linked with no
# header padding, and a dependency rewrite sized so the new load commands fit
# in the header slack with fewer than 16 bytes to spare.
#   ON : change-deps exits nonzero and writes no Mach-O.
#   OFF (M64_NO_CHANGE_DEPS_SIGROOM=1): it writes the image, and signing it
#        overwrites __text (the defect).
set -u
MT="${1:?usage: change_deps_sigroom_test.sh <path-to-macho-tool>}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
OLD="-mmacosx-version-min=10.9"
ABS="/x/libdep.dylib"
fail() { echo "change-deps-sigroom: FAIL ($1)"; exit 1; }

echo 'int dep_answer(void){return 42;}' > "$TMP/dep.c"
echo 'extern int dep_answer(void); int main_answer(void){return dep_answer();}' > "$TMP/main.c"
cc -arch x86_64 $OLD -dynamiclib -install_name "$ABS" -o "$TMP/libdep.dylib" "$TMP/dep.c" \
   || fail "could not build libdep.dylib"
cc -arch x86_64 $OLD -dynamiclib -Wl,-headerpad,0 -Wl,-no_adhoc_codesign 2>/dev/null \
   -install_name @rpath/libmain.dylib -o "$TMP/libmain.dylib" "$TMP/main.c" "$TMP/libdep.dylib" \
   || fail "could not build libmain.dylib"
M="$TMP/libmain.dylib"
otool -l "$M" | grep -q LC_CODE_SIGNATURE && fail "setup: fixture is already signed"

# header slack = first section's file offset - (mach_header_64 + sizeofcmds)
cmds=$(otool -h "$M" | awk 'NR==4{print $7}')
toff=$(otool -l "$M" | awk '/sectname __text/{f=1} f&&/ offset /{print $2; exit}')
slack=$((toff - 32 - cmds))
old=$(otool -l "$M" | awk -v n="$ABS" '/cmd LC_LOAD_DYLIB/{f=1;c=""} f&&/cmdsize/{c=$2} f&&$1=="name"&&$2==n{print c; exit}')
[ -n "$old" ] || fail "setup: $ABS not recorded"
# grow the command by the slack rounded down to 8: it fits, < 16 bytes stay free
grow=$((slack / 8 * 8))
[ $grow -gt 0 ] && [ $((slack - grow)) -lt 16 ] || fail "setup: slack $slack"
len=$((old + grow - 25))                     # cmdsize = 24 + strlen + NUL, no pad
NEW="/$(printf 'p%.0s' $(seq 1 $((len - 14))))/libdep.dylib"
[ ${#NEW} -eq $len ] || fail "setup: path length ${#NEW} != $len"

is_macho() { [ -s "$1" ] && otool -h "$1" >/dev/null 2>&1; }

"$MT" change-deps -q --change "$ABS=$NEW" "$M" "$TMP/on.dylib" 2>/dev/null
rc_on=$?
M64_NO_CHANGE_DEPS_SIGROOM=1 "$MT" change-deps -q --change "$ABS=$NEW" "$M" "$TMP/off.dylib" 2>/dev/null
rc_off=$?

[ $rc_on -ne 0 ] && ! is_macho "$TMP/on.dylib" || fail "ON rewrote the image (rc=$rc_on)"
[ $rc_off -eq 0 ] && is_macho "$TMP/off.dylib" || fail "OFF did not rewrite (rc=$rc_off)"
before=$(dd if="$TMP/off.dylib" bs=1 skip="$toff" count=16 2>/dev/null | xxd -p)
codesign -f -s - "$TMP/off.dylib" 2>/dev/null
after=$(dd if="$TMP/off.dylib" bs=1 skip="$toff" count=16 2>/dev/null | xxd -p)
[ "$before" != "$after" ] || fail "OFF: signing left __text intact (guard inert)"

echo "change-deps-sigroom: PASS (slack $slack, on refused rc=$rc_on, off signed over __text)"
