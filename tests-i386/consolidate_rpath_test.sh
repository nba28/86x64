#!/bin/bash
#
# consolidate-rpath: `m64 consolidate-runtime` repoints an image whose header
# has no room for the longer @loader_path spelling of the home runtime copy
# through the shorter @rpath/<lib>, when the main executable's rpaths reach the
# home copy (Civ IV's deep Python modules: install_name_tool and change-deps
# both refuse, change-deps because signing would overwrite __text).
#
# Native x86_64 bundle (no i386 sysroot): T.app whose main executable
# (rpath @loader_path/../Frameworks) dlopens a plugin 45 directories deep. The
# plugin loads `@loader_path/libabiconv.dylib`, a copy of
# Contents/Frameworks/libabiconv.dylib, and has 128 bytes of header slack:
# room for the @rpath spelling plus the __86x64_abs32 section change-deps
# adds, none for the 144 extra bytes of the deep @loader_path spelling.
#   ON : consolidate-runtime succeeds, the plugin names @rpath/libabiconv.dylib,
#        its local copy is gone, and the app still runs (exit 42).
#   OFF (M64_NO_CONSOLIDATE_RPATH=1): consolidate-runtime fails ("cannot
#        repoint").
set -u
M64="${1:?usage: consolidate_rpath_test.sh <path-to-m64>}"
TMP="$(mktemp -d)"
[ -n "${KEEP:-}" ] && echo "kept $TMP" || trap 'rm -rf "$TMP"' EXIT
OLD="-mmacosx-version-min=10.9"
fail() { echo "consolidate-rpath: FAIL ($1)"; exit 1; }

DEEP="Resources$(printf '/d%.0s' $(seq 1 45))"
make_app() {   # make_app DIR: the fixture bundle at DIR/T.app
   local A="$1/T.app/Contents"
   mkdir -p "$A/MacOS" "$A/Frameworks" "$A/$DEEP"
   cat > "$A/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict><key>CFBundleExecutable</key><string>main</string></dict></plist>
EOF
   echo 'int abi_answer(void){return 42;}' > "$1/abi.c"
   cc -arch x86_64 $OLD -dynamiclib -install_name @rpath/libabiconv.dylib \
      -o "$A/Frameworks/libabiconv.dylib" "$1/abi.c" || return 1
   cp "$A/Frameworks/libabiconv.dylib" "$A/$DEEP/libabiconv.dylib"
   echo 'extern int abi_answer(void); int plug_answer(void){return abi_answer();}' > "$1/plug.c"
   cc -arch x86_64 $OLD -dynamiclib -Wl,-headerpad,0x60 \
      -install_name @rpath/plug.dylib -o "$A/$DEEP/plug.dylib" "$1/plug.c" "$A/$DEEP/libabiconv.dylib" || return 1
   install_name_tool -change @rpath/libabiconv.dylib @loader_path/libabiconv.dylib \
      "$A/$DEEP/plug.dylib" 2>/dev/null || return 1
   codesign -f -s - "$A/$DEEP/plug.dylib" 2>/dev/null
   cat > "$1/main.c" <<EOF
#include <dlfcn.h>
int main(void) {
   void *h = dlopen("@executable_path/../$DEEP/plug.dylib", RTLD_NOW);
   int (*f)(void) = h ? (int (*)(void))dlsym(h, "plug_answer") : 0;
   return f ? f() : 1;
}
EOF
   cc -arch x86_64 $OLD -Wl,-rpath,@loader_path/../Frameworks -o "$A/MacOS/main" "$1/main.c" || return 1
}

for arm in on off; do
   mkdir -p "$TMP/$arm"
   make_app "$TMP/$arm" || fail "could not build the fixture"
   P="$TMP/$arm/T.app/Contents/$DEEP/plug.dylib"
   otool -L "$P" | grep -q "@loader_path/libabiconv.dylib" || fail "setup: plugin dependency"
   "$TMP/$arm/T.app/Contents/MacOS/main"; [ $? = 42 ] || fail "setup: the app does not run before"
   e=""; [ $arm = off ] && e="M64_NO_CONSOLIDATE_RPATH=1"
   env $e python3 "$M64" consolidate-runtime "$TMP/$arm/T.app" >"$TMP/$arm.log" 2>&1
   eval "rc_$arm=$?"
done

[ $rc_on = 0 ] || fail "ON: consolidate-runtime rc=$rc_on: $(tail -1 "$TMP/on.log")"
P="$TMP/on/T.app/Contents/$DEEP"
grep -q "repointed via change-deps" "$TMP/on.log" || fail "ON: did not take the change-deps path"
otool -L "$P/plug.dylib" | grep -q "@rpath/libabiconv.dylib" || fail "ON: plugin not repointed to @rpath"
[ -e "$P/libabiconv.dylib" ] && fail "ON: the plugin's local runtime copy survived"
"$TMP/on/T.app/Contents/MacOS/main"; r=$?
[ $r = 42 ] || fail "ON: the consolidated app exits $r (want 42)"
[ $rc_off != 0 ] || fail "OFF: consolidate-runtime succeeded without the fallback (guard inert)"
grep -q "cannot repoint" "$TMP/off.log" || fail "OFF: unexpected failure: $(tail -1 "$TMP/off.log")"

echo "consolidate-rpath: PASS (on: @rpath, app exits 42; off: cannot repoint)"
