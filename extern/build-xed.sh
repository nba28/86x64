#!/bin/sh

if [ $# -ne 2 ]; then
    echo "usage: $0 <xed-src-path> <xed-build-path>"
    exit 1;
fi

SOURCE="$1"
BUILD="$2"

# xed's mfile.py invokes `llvm-ar` directly. On macOS the system `ar`
# works fine, but the build script looks up llvm-ar by name. Make sure
# the Homebrew LLVM bin dir is on PATH if it exists.
for d in /usr/local/opt/llvm/bin /opt/homebrew/opt/llvm/bin; do
    if [ -x "$d/llvm-ar" ]; then
        PATH="$d:$PATH"
        export PATH
        break
    fi
done

if ! [ -r "$BUILD/obj/libxed.a" -a -r "$BUILD/obj/wkit/include/xed/xed-interface.h" ]; then
    cd "$BUILD"
    # We need libxed to match the rest of the project (x86_64). On
    # Apple Silicon mfile.py would otherwise produce arm64 objects
    # that fail to link.
    "$SOURCE/mfile.py" --jobs=8 -s \
        --host-cpu=x86-64 \
        --cc='clang -arch x86_64' \
        --cxx='clang++ -arch x86_64'
fi
