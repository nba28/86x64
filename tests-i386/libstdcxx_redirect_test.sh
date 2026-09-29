#!/bin/bash
#
# Regression guard for the libstdc++-redirect fix (Portal 2, 2026-09-29):
# see src/99_libstdcxx_redirect.cc for the mechanism writeup.
#
# ON arm  = the translated fixture's bind on /usr/lib/libstdc++.6.dylib is
#           redirected (src/86x64/abi-hazard-vendor.py) to our own translated
#           i386 libstdc++ (a sibling translated module -> same 4-byte-return
#           convention, correct i386 object layout) -> expect exit 42.
# OFF arm = the SAME translated fixture, UNTOUCHED: its bind reaches the
#           NATIVE x86_64 system libstdc++ raw (i386 cdecl call into a SysV
#           callee whose `ret` pops 8 bytes where the translator pushed 4 ->
#           fused PC, see unbridged_native_call_bug) -> expect exit != 42.
#
# Needs the i386 sysroot + staged libstdc++ (make sysroot-cpp) to link the
# i386 fixture, AND a built golden translated libstdc++ (m64 translate
# <i386-original> -o ~/projects/Library/Frameworks/libstdc++-x86_64/
# libstdc++.6.dylib) for the ON arm's redirect target. SKIPs without either.
set -u
MT="${1:?usage: libstdcxx_redirect_test.sh <path-to-macho-tool>}"

fail() { echo "FAIL libstdcxx-redirect: $1"; exit 1; }

HERE="$(cd "$(dirname "$0")" && pwd)"
SYSROOT=/tmp/i386-sysroot
GOLDEN="$HOME/projects/Library/Frameworks/libstdc++-x86_64/libstdc++.6.dylib"
if [ ! -f "$SYSROOT/usr/lib/libstdc++.dylib" ]; then
    echo "SKIP libstdcxx-redirect (no i386 sysroot / libstdc++ at $SYSROOT)"
    exit 0
fi
if [ ! -f "$GOLDEN" ]; then
    echo "SKIP libstdcxx-redirect (no golden translated libstdc++ at $GOLDEN)"
    exit 0
fi

PROJ_ROOT="$(cd "$HERE/.." && pwd)"
LIBABICONV="$PROJ_ROOT/build/src/abiconv/libabiconv.dylib"
LIBWRAPPER="$PROJ_ROOT/build/src/86x64/libwrapper.a"
LIBINTERPOSE="$PROJ_ROOT/build/src/86x64/libinterpose.dylib"
PIPELINE="$PROJ_ROOT/src/86x64/86x64.sh"
VENDOR="$PROJ_ROOT/src/86x64/abi-hazard-vendor.py"
[ -f "$LIBABICONV" ] || fail "missing $LIBABICONV (run make first)"

LD="${LD:-$HOME/projects/Library/Toolchains/sl-ld64/ld-i386}"; [ -x "$LD" ] || LD=ld
HOST_SDK="$(xcrun --show-sdk-path)"
# The modern SDK ships ONLY libc++ headers (usr/include/c++/v1) -- #include
# <sstream> etc. against them would mangle names in the std::__1:: inline
# namespace, which is not what libstdc++.6.dylib (GCC 4.2 ABI) exports and not
# what a real i386 app calls. Compile against the preserved GNU 4.2.1 headers
# instead (same convention as 66_cpp_cow_string.cc's writeup), so the object
# code's mangled names match the real GNU libstdc++ entry points.
GCC421="$HOME/projects/Library/SDKs/MacOSX10.6.sdk/usr/include/c++/4.2.1"
if [ ! -d "$GCC421" ]; then
    echo "SKIP libstdcxx-redirect (no GNU libstdc++ 4.2.1 headers at $GCC421)"
    exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

SRC="$HERE/src/99_libstdcxx_redirect.cc"
[ -f "$SRC" ] || fail "fixture $SRC missing"

clang++ -arch i386 -isysroot "$HOST_SDK" -mmacosx-version-min=10.6 \
    -nostdinc++ -I "$GCC421" -I "$GCC421/i686-apple-darwin10" \
    -c "$SRC" -o "$TMP/t.o" 2>/dev/null || fail "clang++ -arch i386"
"$LD" -arch i386 -macosx_version_min 10.6 -syslibroot "$SYSROOT" \
    -e _main -lSystem -lstdc++ -dead_strip_dylibs \
    -o "$TMP/t.i386" "$TMP/t.o" 2>/dev/null || fail "ld i386"

# The wrapper (-w libwrapper.a exec mode) emits a small front-end exec plus
# the real translated payload beside it (<name>.dylib, co-located like every
# other translated dylib) -- the payload is the actual libabiconv-linked
# consumer that binds libstdc++, not the front-end. 86x64.sh bakes the
# OUTPUT DIRECTORY itself into an absolute LC_RPATH entry (ahead of
# @loader_path in search order), so translating once and copying the pair
# into two subdirs does not work -- the absolute rpath from the first
# translate would keep resolving to that original directory. Translate
# TWICE instead, once per arm, so each pair's baked absolute rpath matches
# its own directory (harmless -- same target as @loader_path there).
mkdir -p "$TMP/off" "$TMP/on"
bash "$PIPELINE" -m "$MT" -l "$LIBABICONV" -w "$LIBWRAPPER" -i "$LIBINTERPOSE" \
    -o "$TMP/off/t.x86_64" "$TMP/t.i386" >/dev/null 2>&1 || fail "translate (off)"
bash "$PIPELINE" -m "$MT" -l "$LIBABICONV" -w "$LIBWRAPPER" -i "$LIBINTERPOSE" \
    -o "$TMP/on/t.x86_64" "$TMP/t.i386" >/dev/null 2>&1 || fail "translate (on)"
chmod +x "$TMP/off/t.x86_64" "$TMP/on/t.x86_64"
cp "$LIBABICONV" "$TMP/off/" && cp "$LIBABICONV" "$TMP/on/"
python3 "$VENDOR" "$TMP/on" >/dev/null 2>&1 || fail "abi-hazard-vendor.py"
# Confirm the redirect actually landed (co-located copy + rewritten bind) --
# else the ON arm is silently identical to OFF and the guard is inert.
[ -f "$TMP/on/libstdc++.6.dylib" ] || fail "redirect did not co-locate a copy"
otool -L "$TMP/on/t.x86_64.dylib" | grep -q '@loader_path/libstdc++.6.dylib' \
    || fail "redirect did not rewrite the bind"

FIXTURE_DEADLINE=30
RUN() { perl -e 'my $t = shift; my $p = fork; if (!$p) { exec @ARGV or exit 127 }
    $SIG{ALRM} = sub { kill "KILL", $p }; alarm $t; waitpid($p, 0);
    exit($? & 127 ? 128 + ($? & 127) : $? >> 8)' "$FIXTURE_DEADLINE" "$@"; }

RUN "$TMP/on/t.x86_64";  r_on=$?
RUN "$TMP/off/t.x86_64"; r_off=$?

if [ "$r_on" = 42 ] && [ "$r_off" != 42 ]; then
    echo "PASS libstdcxx-redirect (on=$r_on off=$r_off)"
    exit 0
else
    echo "FAIL libstdcxx-redirect (on=$r_on off=$r_off)"
    exit 1
fi
