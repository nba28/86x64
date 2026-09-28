#!/bin/bash
#
# Regression test for the zerofill-section double-slide fix (objc_slide.c
# slide_data_fnptrs skips S_ZEROFILL / S_GB_ZEROFILL / S_THREAD_LOCAL_ZEROFILL).
#
# slide_data_fnptrs slides every 4-byte word in a writable segment whose value
# lands in the image's pre-slide vmaddr window. A __bss/__common (zerofill)
# section has NO file bytes, so a translate-time-relocated static pointer can
# NEVER live there — every word is a RUNTIME value. With >1 co-located
# libabiconv copy (multicopy deploy), the second copy's add-image re-scan
# (private processed-set) re-runs the pass over an already-initialized image;
# by then the vacated preferred-vmaddr window is recycled by the low-4GB heap,
# so a live runtime pointer in a zerofill slot is re-slid into a wild address.
# On Civ IV that corrupted GTokenizer::FreeAllBuffers()'s __common buffer array
# -> write-into-read-only-__TEXT EXC_BAD_ACCESS at exit.
#
# The tests-i386 harness always loads translated no_pie images at their
# preferred base (real slide == 0 -> the scan early-returns), so the double
# slide cannot be reproduced end-to-end. The fixture instead drives the exact
# scan directly via the test-only export _86x64_test_slide_data_fnptrs over a
# hand-built in-memory Mach-O with a chosen nonzero slide and two writable
# sections (S_REGULAR + S_ZEROFILL) holding the same in-window pointer. Fixed:
# the regular slot is slid, the zerofill slot is preserved. Unfixed: BOTH are
# slid (the zerofill slot is double-slid).
#
# Needs the Snow Leopard i386 sysroot (SKIPs without it, like the suite).
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
   echo "SKIP zerofill-reslide (no i386 sysroot at $SYSROOT; run 'make sysroot')"
   exit 0
fi
for f in "$MT" "$LIBABICONV" "$LIBWRAPPER" "$LIBINTERPOSE"; do
   [ -e "$f" ] || { echo "SKIP zerofill-reslide (missing $f; build the project first)"; exit 0; }
done

mkdir -p build
fail() { echo "FAIL zerofill-reslide ($1)"; exit 1; }

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
[ -f "$OBJC_SLIDE_O" ] || { echo "SKIP zerofill-reslide (objc_slide.c.o not found; build the project first)"; exit 0; }

# objc_slide.c's image-list helpers live in their own object (dyld_image_list.c).
IMGLIST_O="$(dirname "$OBJC_SLIDE_O")/dyld_image_list.c.o"
clang -arch x86_64 -o build/zerofill_reslide_test \
   src/zerofill_reslide_fixture.c "$OBJC_SLIDE_O" "$IMGLIST_O" \
   2>build/zerofill_reslide_cc.log || { sed 's/^/    /' build/zerofill_reslide_cc.log; fail compile; }
codesign -f -s - build/zerofill_reslide_test >/dev/null 2>&1

ACTUAL="$(build/zerofill_reslide_test 2>/dev/null)"
EC=$?

EXPECTED="regular slot: slid
zerofill slot: preserved"

if [ $EC -ne 0 ]; then
   fail "exit code $EC"
fi
if [ "$ACTUAL" != "$EXPECTED" ]; then
   echo "FAIL zerofill-reslide (output mismatch)"
   diff <(echo "$EXPECTED") <(echo "$ACTUAL") | sed 's/^/    /'
   exit 1
fi
echo "PASS zerofill-reslide"
