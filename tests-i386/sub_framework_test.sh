#!/bin/bash
#
# Regression test for LC_SUB_FRAMEWORK (0x12) support in macho-tool's load-command
# parser (src/core/lc.cc). An umbrella framework's SUB-frameworks carry an
# LC_SUB_FRAMEWORK command naming their umbrella (e.g. iMovie's
# Helium.framework/.../HeliumRender.framework -> umbrella "Helium"). The parser's
# default case used to `throw error("load command 0x%x not supported")` on it,
# aborting the whole translate ("libc++abi: terminating due to uncaught exception
# of type MachO::error: load command 0x12 not supported"). LC_SUB_FRAMEWORK (and
# its siblings LC_SUB_UMBRELLA/CLIENT/LIBRARY) share the {cmd,cmdsize,lc_str}
# layout of LC_LOAD_DYLINKER/LC_RPATH, so they parse/emit through DylinkerCommand.
#
# Builds a tiny i386 dylib with `-Wl,-umbrella,Helium` (which emits
# LC_SUB_FRAMEWORK), runs the pipeline's rebasify + M32->M64 transform, and
# asserts: (a) transform exits 0 (no "not supported" throw); (b) the translated
# x86_64 output still carries LC_SUB_FRAMEWORK naming the "Helium" umbrella.
# Needs the i386 sysroot; SKIPs without it.
set -u
MT="${1:?usage: sub_framework_test.sh <path-to-macho-tool>}"
SYSROOT="${I386_SYSROOT:-/tmp/i386-sysroot}"

if [ ! -d "$SYSROOT" ]; then
   echo "SKIP sub-framework (no i386 sysroot at $SYSROOT)"
   exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
fail() { echo "FAIL sub-framework: $1"; exit 1; }

echo 'int helium_render(void){return 42;}' > "$TMP/r.c"
clang -arch i386 -isysroot "$SYSROOT" -mmacosx-version-min=10.6 -dynamiclib \
   -Wl,-umbrella,Helium -install_name "@rpath/HeliumRender" \
   -o "$TMP/HeliumRender" "$TMP/r.c" 2>/dev/null \
   || fail "could not build i386 sub-framework fixture"

otool -arch i386 -l "$TMP/HeliumRender" | grep -q "LC_SUB_FRAMEWORK" \
   || fail "setup: fixture lacks LC_SUB_FRAMEWORK (toolchain change?)"

# The pipeline path that used to throw: rebasify then transform (M32->M64).
"$MT" rebasify "$TMP/HeliumRender" "$TMP/HeliumRender.rb" 2>"$TMP/err1" \
   || { cat "$TMP/err1"; fail "rebasify threw (LC_SUB_FRAMEWORK not supported?)"; }
"$MT" -- transform "$TMP/HeliumRender.rb" "$TMP/HeliumRender.m64" 2>"$TMP/err2" \
   || { cat "$TMP/err2"; fail "transform threw (LC_SUB_FRAMEWORK not supported?)"; }

# The translated x86_64 output must PRESERVE LC_SUB_FRAMEWORK + the umbrella name.
otool -l "$TMP/HeliumRender.m64" | grep -q "LC_SUB_FRAMEWORK" \
   || fail "translated output dropped LC_SUB_FRAMEWORK"
otool -l "$TMP/HeliumRender.m64" | grep -qE "umbrella Helium" \
   || fail "translated output lost the 'Helium' umbrella name"

echo "PASS sub-framework (LC_SUB_FRAMEWORK parsed + preserved through transform)"
exit 0
