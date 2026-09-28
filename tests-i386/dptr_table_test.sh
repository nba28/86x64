#!/bin/bash
#
# Guard for the exact DATA pointer-slot table (__DATA,__86x64_dptr, written by
# Archive::inject_dptr_section and consumed by objc_slide.c slide_data_fnptrs).
# ON: the listed pointers (aligned + unaligned-data) are slid and a constant
# that aliases the image window is preserved. OFF (DPTR_OFF=1, no table): the
# legacy value scan runs and gets both wrong — the arm proves the guard bites.
# Native x86_64 direct-call fixture, see src/dptr_table_fixture.c.
set -u
cd "$(dirname "$0")"

PROJ_ROOT="$(cd .. && pwd)"
MT="${1:-$PROJ_ROOT/build/src/macho-tool/macho-tool}"
MTDIR="$(dirname "$MT")"
LIBABICONV="${LIBABICONV:-$PROJ_ROOT/build/src/abiconv/libabiconv.dylib}"
LIBWRAPPER="${LIBWRAPPER:-$PROJ_ROOT/build/src/86x64/libwrapper.a}"
LIBINTERPOSE="${LIBINTERPOSE:-$PROJ_ROOT/build/src/86x64/libinterpose.dylib}"
PIPELINE="$PROJ_ROOT/src/86x64/86x64.sh"
SYSROOT=/tmp/i386-sysroot
export PATH="$MTDIR:$PATH"

if [ ! -f "$SYSROOT/usr/lib/libSystem.dylib" ] && [ ! -f "$SYSROOT/usr/lib/libSystem.B.dylib" ]; then
   echo "SKIP dptr-table (no i386 sysroot at $SYSROOT; run 'make sysroot')"
   exit 0
fi
for f in "$MT" "$LIBABICONV" "$LIBWRAPPER" "$LIBINTERPOSE"; do
   [ -e "$f" ] || { echo "SKIP dptr-table (missing $f; build the project first)"; exit 0; }
done

mkdir -p build
fail() { echo "FAIL dptr-table ($1)"; exit 1; }

# NATIVE x86_64 unit test. The fixture exercises ONLY the section-scan decision
# in slide_data_fnptrs (pure C over an in-memory Mach-O header; no i386 ABI),
# via the test-only export _86x64_test_slide_data_fnptrs. We link the SHIPPING
# objc_slide.c object file directly (not the whole libabiconv.dylib) so no
# libabiconv constructor runs — the full dylib's load-time objc arena ctor
# aborts without the wrapper's reserved low-4GB region, and the translated
# harness can't force a nonzero image slide (no_pie -> preferred base -> real
# slide==0 -> the scan early-returns), so an end-to-end reproduction is
# impossible here (cf. 33_dyld_section_patch.c: assert the structural fix
# directly when the live path is unreproducible). slide_data_fnptrs's own call
# graph touches only libc (mprotect/strcmp/strncmp); its sibling functions'
# externs are satisfied by weak no-op stubs and are never reached.
OBJC_SLIDE_O="$PROJ_ROOT/build/src/abiconv/CMakeFiles/abiconv.dir/objc_slide.c.o"
[ -f "$OBJC_SLIDE_O" ] || OBJC_SLIDE_O="$(dirname "$LIBABICONV")/CMakeFiles/abiconv.dir/objc_slide.c.o"
[ -f "$OBJC_SLIDE_O" ] || { echo "SKIP dptr-table (objc_slide.c.o not found; build the project first)"; exit 0; }

# objc_slide.c's image-list helpers live in their own object (dyld_image_list.c).
IMGLIST_O="$(dirname "$OBJC_SLIDE_O")/dyld_image_list.c.o"
clang -arch x86_64 -Wl,-pagezero_size,0x1000 -o build/dptr_table_test \
   src/dptr_table_fixture.c "$OBJC_SLIDE_O" "$IMGLIST_O" \
   2>build/dptr_table_cc.log || { sed 's/^/    /' build/dptr_table_cc.log; fail compile; }
codesign -f -s - build/dptr_table_test >/dev/null 2>&1

ON="$(build/dptr_table_test 2>&1)"
OFF="$(DPTR_OFF=1 build/dptr_table_test 2>&1)"
WANT_ON="pointer: slid
constant: preserved
unaligned pointer: slid"
WANT_OFF="pointer: slid
constant: slid
unaligned pointer: unslid"
[ "$ON" = "$WANT_ON" ] || { echo "$ON" | sed 's/^/    /'; fail "table arm"; }
[ "$OFF" = "$WANT_OFF" ] || { echo "$OFF" | sed 's/^/    /'; fail "OFF arm no longer reproduces the scan's errors"; }
echo "PASS dptr-table"
