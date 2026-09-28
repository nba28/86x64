#!/bin/bash
# Build objc_send_bench.m as i386 (translated through the real pipeline) and as
# native x86_64, run both, print side by side.
set -eu
cd "$(dirname "$0")/.."
ROOT=$(cd .. && pwd); O=build/bench; mkdir -p $O
LD="$HOME/projects/Library/Toolchains/sl-ld64/ld-i386"
clang -arch i386 -isysroot "$(xcrun --show-sdk-path)" -mmacosx-version-min=10.6 \
      -fobjc-runtime=macosx-fragile -O2 -c bench/objc_send_bench.m -o $O/bench.o
"$LD" -arch i386 -macos_version_min 10.6 -no_pie -syslibroot /tmp/i386-sysroot -lSystem -lobjc \
      -framework Foundation -framework CoreFoundation -undefined dynamic_lookup -e _main \
      -o $O/bench.i386 $O/bench.o
bash "$ROOT/src/86x64/86x64.sh" -m "$ROOT/build/src/macho-tool/macho-tool" \
     -l "$ROOT/build/src/abiconv/libabiconv.dylib" -w "$ROOT/build/src/86x64/libwrapper.a" \
     -i "$ROOT/build/src/86x64/libinterpose.dylib" -o $O/bench.x86_64 $O/bench.i386 >/dev/null 2>&1
cp "$ROOT/build/src/abiconv/libabiconv.dylib" "$ROOT/build/src/abiconv/libabiconv.nulljump" \
   "$ROOT/build/src/86x64/libinterpose.dylib" $O/
clang -arch x86_64 -O2 -framework Foundation bench/objc_send_bench.m -o $O/bench.native 2>/dev/null
echo "== translated";  $O/bench.x86_64 2>/dev/null | grep -E "ns\/(send|call)"
echo "== native";      $O/bench.native | grep -E "ns\/(send|call)"
