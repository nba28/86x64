#!/bin/bash
# Guard for openssl_shim.c (see the Makefile `openssl-shim` target).
set -u
cd "$(dirname "$0")"
ROOT=$(cd .. && pwd)
B=build; SRC=src/99_openssl_shim.c
SYSROOT=/tmp/i386-sysroot
LD=$HOME/projects/Library/Toolchains/sl-ld64/ld-i386
LIB=${LIBABICONV:-$ROOT/build/src/abiconv/libabiconv.dylib}
[ -f "$SYSROOT/usr/lib/libSystem.dylib" ] || [ -f "$SYSROOT/usr/lib/libSystem.B.dylib" ] || { echo "SKIP openssl-shim (no i386 sysroot)"; exit 0; }
# Link-time stand-in for /usr/lib/libcrypto.0.9.7.dylib (the real one refuses
# direct linking). Same install name, so the binary imports exactly like Quinn.
printf 'void SHA1(void){} void EVP_sha1(void){} void HMAC(void){} void RAND_pseudo_bytes(void){}\n' > $B/crypto_stub.c
clang -arch i386 -isysroot "$SYSROOT" -mmacosx-version-min=10.6 -c $B/crypto_stub.c -o $B/crypto_stub.o || exit 1
$LD -arch i386 -dylib -macos_version_min 10.6 -syslibroot "$SYSROOT" -lSystem \
    -install_name /usr/lib/libcrypto.0.9.7.dylib -o $B/libcrypto.0.9.7.dylib $B/crypto_stub.o || exit 1
clang -arch i386 -isysroot "$SYSROOT" -mmacosx-version-min=10.6 -c $SRC -o $B/99_openssl_shim.o || exit 1
$LD -arch i386 -macos_version_min 10.6 -no_pie -syslibroot "$SYSROOT" -lSystem \
    $B/libcrypto.0.9.7.dylib -e _main -o $B/99_openssl_shim.i386 $B/99_openssl_shim.o || exit 1
arm() {  # $1 = libabiconv, $2 = out suffix
   bash $ROOT/src/86x64/86x64.sh -m $ROOT/build/src/macho-tool/macho-tool -l "$1" \
      -w $ROOT/build/src/86x64/libwrapper.a -i $ROOT/build/src/86x64/libinterpose.dylib \
      -o $B/99_openssl_shim.$2 $B/99_openssl_shim.i386 >/dev/null 2>&1 || { echo "translate failed ($2)"; return 99; }
   ( cd $B && M64_ABSENT_DEPS=libcrypto ./99_openssl_shim.$2 ); return $?
}
arm "$LIB" on; on=$?
# The crash itself (a classic __IMPORT,__jump_table import jumping raw into
# native libcrypto) cannot be rebuilt here: ld64-95 emits modern lazy binds,
# which the stub binder already bridges. So this guard proves the shims are
# BOUND (by content) and CORRECT (test vectors), not a two-arm repro.
nbound=$(dyld_info -fixups $B/99_openssl_shim.on.dylib 2>/dev/null | grep -cE "___(SHA1|EVP_sha1|HMAC|RAND_pseudo_bytes)")
echo "openssl-shim: exit=$on (want 42), shim binds=$nbound (want 4)"
[ $on -eq 42 ] && [ "$nbound" -eq 4 ] && echo "PASS openssl-shim" || { echo "FAIL openssl-shim"; exit 1; }
