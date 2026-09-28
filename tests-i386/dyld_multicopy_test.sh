#!/bin/bash
#
# Regression test for the multi-libabiconv-copy __dyld+8 mod-init artifact fix
# (objc_slide.c wrap_mod_init_funcs; Halo CE + bundled translated QuickTime,
# 2026-07-05).
#
# patch_dyld_section stamps the i386-cdecl _86x64_dyld_func_lookup shim into
# the 8-byte view of the classic __DATA,__dyld func_lookup slot (__dyld+8) —
# under the classic Csu layout that is __mod_init_func[0]. With >1 libabiconv
# copy loaded (each copy's processed-set is private), the second copy's
# add-image re-scan finds the slot non-NULL again and, unfixed, collects+runs
# the shim as an initializer with an argc=0/argv=NULL i386 frame -> the shim
# writes through *NULL -> SIGSEGV inside dlopen. Fixed, the entry is
# recognized (dladdr: exact exported _86x64_dyld_func_lookup entry outside the
# image) and skipped.
#
# The fixture recreates the artifact state through the live runtime paths
# (see src/dyld_multicopy_fixture.c) and dlopen()s a second libabiconv copy;
# the test asserts the process survives and the artifact entry was skipped.
# Needs the Snow Leopard i386 sysroot (SKIPs without it, like the suite).
# i386 linking needs the Snow Leopard ld64-95 wrapper: modern ld dropped -arch i386
# (same resolution as the Makefile's LD). Override with LD=... in the environment.
LD="${LD:-$HOME/projects/Library/Toolchains/sl-ld64/ld-i386}"; [ -x "$LD" ] || LD=ld

set -u
cd "$(dirname "$0")"

PROJ_ROOT="$(cd .. && pwd)"
MT="${1:-$PROJ_ROOT/build/src/macho-tool/macho-tool}"
LIBABICONV="$PROJ_ROOT/build/src/abiconv/libabiconv.dylib"
LIBWRAPPER="$PROJ_ROOT/build/src/86x64/libwrapper.a"
LIBINTERPOSE="$PROJ_ROOT/build/src/86x64/libinterpose.dylib"
PIPELINE="$PROJ_ROOT/src/86x64/86x64.sh"
SYSROOT=/tmp/i386-sysroot

if [ ! -f "$SYSROOT/usr/lib/libSystem.dylib" ] && [ ! -f "$SYSROOT/usr/lib/libSystem.B.dylib" ]; then
   echo "SKIP dyld-multicopy (no i386 sysroot at $SYSROOT; run 'make sysroot')"
   exit 0
fi
for f in "$MT" "$LIBABICONV" "$LIBWRAPPER" "$LIBINTERPOSE"; do
   [ -e "$f" ] || { echo "SKIP dyld-multicopy (missing $f; build the project first)"; exit 0; }
done

mkdir -p build
fail() { echo "FAIL dyld-multicopy ($1)"; exit 1; }

clang -arch i386 -isysroot "$SYSROOT" -mmacosx-version-min=10.6 \
   -c src/dyld_multicopy_fixture.c -o build/dyld_multicopy_fixture.o \
   2>/dev/null || fail compile
"$LD" -arch i386 -macos_version_min 10.6 -no_pie -syslibroot "$SYSROOT" \
   -lSystem -e _main -o build/dyld_multicopy_fixture.i386 \
   build/dyld_multicopy_fixture.o 2>/dev/null || fail link
bash "$PIPELINE" -m "$MT" -l "$LIBABICONV" -w "$LIBWRAPPER" -i "$LIBINTERPOSE" \
   -o build/dyld_multicopy_fixture.x86_64 build/dyld_multicopy_fixture.i386 \
   >/dev/null 2>&1 || fail translate
chmod +x build/dyld_multicopy_fixture.x86_64

# The second libabiconv copy: same content, distinct inode -> distinct image.
cp -f "$LIBABICONV" build/libabiconv_copy2.dylib
codesign -f -s - build/libabiconv_copy2.dylib >/dev/null 2>&1

# ABICONV_NO_XCOPY_CLAIM: the per-process image claim (dyld-multicopy-reprocess)
# now stops the second copy from re-scanning at all, so the skip under test is
# only reached with the claim off. The verbose log proves it was reached.
ACTUAL="$(ABICONV_NO_XCOPY_CLAIM=1 ABICONV_OBJC_SLIDE_VERBOSE=1 ABICONV_RUN_INITS=1 \
          DYLD_MULTICOPY_LIB="$PWD/build/libabiconv_copy2.dylib" \
          build/dyld_multicopy_fixture.x86_64 2>build/dyld_multicopy_fixture.err)"
EC=$?
grep -q "func_lookup shim artifact" build/dyld_multicopy_fixture.err \
   || fail "the second copy never reached the artifact skip (guard inert)"
EXPECTED="dyld patched: yes
copy2: loaded
slot: artifact-skipped"

if [ $EC -ne 0 ]; then
   fail "exit code $EC (SIGSEGV = the unfixed second-copy re-scan ran the __dyld+8 artifact as an initializer)"
fi
if [ "$ACTUAL" != "$EXPECTED" ]; then
   echo "FAIL dyld-multicopy (output mismatch)"
   diff <(echo "$EXPECTED") <(echo "$ACTUAL") | sed 's/^/    /'
   exit 1
fi
echo "PASS dyld-multicopy"
