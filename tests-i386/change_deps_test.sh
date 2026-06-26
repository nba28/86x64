#!/bin/bash
#
# Regression test for `macho-tool change-deps` — the classic-dylib-safe
# dependency-path rewriter (the deploy fix for translated CLASSIC dylibs that
# cctools/llvm install_name_tool refuse).
#
# Builds a native x86_64 dylib (no i386 sysroot needed) that records an absolute
# LC_LOAD_DYLIB dependency, rewrites it to @rpath/ with change-deps, and asserts:
#   (a) otool -L shows the new @rpath path and the old absolute path is gone;
#   (b) the __text vmaddr is UNCHANGED — change-deps preserves the header-region
#       size so no code/data moves (byte-stable, the core safety property);
#   (c) the rewritten dylib still loads via @rpath and its symbol resolves.
#
# The fixture is linked -mmacosx-version-min=10.9 so the linker emits the
# LC_DYLD_INFO form that macho-tool models (the default toolchain now emits
# LC_DYLD_CHAINED_FIXUPS, which translated binaries never carry).
set -u
MT="${1:?usage: change_deps_test.sh <path-to-macho-tool>}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
OLD="-mmacosx-version-min=10.9"
ABS="/nonexistent/place/libdep.dylib"

fail() { echo "FAIL change-deps: $1"; exit 1; }

echo 'int dep_answer(void){return 42;}' > "$TMP/dep.c"
cc -arch x86_64 $OLD -dynamiclib -install_name "$ABS" \
   -o "$TMP/libdep.dylib" "$TMP/dep.c" || fail "could not build libdep.dylib"

printf 'extern int dep_answer(void);\nint main_answer(void){return dep_answer();}\n' > "$TMP/main.c"
cc -arch x86_64 $OLD -dynamiclib -install_name "@rpath/libmain.dylib" \
   -o "$TMP/libmain.dylib" "$TMP/main.c" "$TMP/libdep.dylib" || fail "could not build libmain.dylib"

otool -L "$TMP/libmain.dylib" | grep -qF "$ABS" || fail "setup: $ABS not recorded in libmain"
text_before=$(otool -l "$TMP/libmain.dylib" | awk '/sectname __text/{f=1} f&&/ addr /{print $2; exit}')

"$MT" change-deps -q --change "$ABS=@rpath/libdep.dylib" \
   "$TMP/libmain.dylib" "$TMP/libmain.cd.dylib" 2>/dev/null \
   || fail "change-deps returned nonzero"

# (a) path rewritten
otool -L "$TMP/libmain.cd.dylib" | grep -qF "@rpath/libdep.dylib" \
   || fail "@rpath/libdep.dylib missing after rewrite"
otool -L "$TMP/libmain.cd.dylib" | grep -qF "$ABS" \
   && fail "old absolute path still present after rewrite"

# (b) layout byte-stable: __text vmaddr unchanged
text_after=$(otool -l "$TMP/libmain.cd.dylib" | awk '/sectname __text/{f=1} f&&/ addr /{print $2; exit}')
[ "$text_before" = "$text_after" ] \
   || fail "__text vmaddr moved $text_before -> $text_after (layout not preserved)"

# (c) still loads via @rpath and the symbol resolves
cp "$TMP/libmain.cd.dylib" "$TMP/libmain.dylib"
codesign -f -s - "$TMP/libmain.dylib" 2>/dev/null
codesign -f -s - "$TMP/libdep.dylib" 2>/dev/null
printf '#include <dlfcn.h>\n#include <stdio.h>\nint main(void){void*h=dlopen("@rpath/libmain.dylib",RTLD_NOW);if(!h){printf("dlopen:%%s\\n",dlerror());return 1;}int(*f)(void)=dlsym(h,"main_answer");if(!f){printf("dlsym\\n");return 1;}printf("%%d\\n",f());return 0;}\n' > "$TMP/host.c"
cc -arch x86_64 -Wl,-rpath,"$TMP" -o "$TMP/host" "$TMP/host.c" || fail "host build failed"
out=$("$TMP/host")
[ "$out" = "42" ] || fail "rewritten dylib returned '$out' (want 42)"

echo "PASS change-deps (path rewrite + layout-stable + loads via @rpath)"
