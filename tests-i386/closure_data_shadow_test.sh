#!/bin/bash
# closure_data_shadow_test.sh — END-TO-END guard for header-typed OBJECT
# data-shadows of a CLOSURE-DISCOVERED framework constant (the Numbers/SFTabular
# AddressBook kAB* wall).
#
# The crash it recreates: SFTabular's __static_initialization_and_destruction_0
# copies AddressBook kAB*Property NSString constants. Any kAB* symbol missing
# from libabiconv's shadow set binds to NATIVE AddressBook (>4GB); the
# translated i386 `movl slot,%reg; movl (%reg),%reg` double-deref truncates the
# 64-bit address and faults. The fix chain under test:
#   1. _i386_closure.py expands targets to their co-translated closure, so
#      SFTabular's kAB* imports enter the abigen consider set
#      (abigen_modern_manifest.py; needs the source pool's i386 SF originals);
#   2. the AddressBook umbrella mapping gives abigen the ObjC-object decls;
#   3. abigen emits low-4GB ___kAB* shadows (handle-wrap, filled by
#      x64_init_data_shadows), static-interpose redirects the bind.
# This guard builds an i386 program that reads kABTitleProperty exactly like
# SFTabular does, translates it, runs it, and expects the real value ("Title").
#
# FAIL (not SKIP) when libabiconv lacks ___kABTitleProperty: that means the
# closure discovery regressed OR the source pool's SF originals lost their i386
# slice (see gotcha: an agent once translated the pool's SFTabular IN PLACE).
# i386 linking needs the Snow Leopard ld64-95 wrapper: modern ld dropped -arch i386
# (same resolution as the Makefile's LD). Override with LD=... in the environment.
. "$(dirname "$0")/../src/86x64/paths.sh"   # M64_* local paths
LD="${LD:-$M64_I386_LD}"; [ -x "$LD" ] || LD=ld

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ_ROOT="$(cd "$HERE/.." && pwd)"

MACHO_TOOL="$PROJ_ROOT/build/src/macho-tool/macho-tool"
LIBABICONV="$PROJ_ROOT/build/src/abiconv/libabiconv.dylib"
LIBWRAPPER="$PROJ_ROOT/build/src/86x64/libwrapper.a"
LIBINTERPOSE="$PROJ_ROOT/build/src/86x64/libinterpose.dylib"
PIPELINE="$PROJ_ROOT/src/86x64/86x64.sh"
SYSROOT=/tmp/i386-sysroot
SDK106="$M64_SDK106"

[ -x "$MACHO_TOOL" ]   || { echo "SKIP closure-data-shadow (macho-tool not built)"; exit 0; }
[ -f "$LIBABICONV" ]   || { echo "SKIP closure-data-shadow (libabiconv not built)"; exit 0; }
[ -f "$LIBWRAPPER" ]   || { echo "SKIP closure-data-shadow (libwrapper not built)"; exit 0; }
[ -f "$SYSROOT/System/Library/Frameworks/Foundation.framework/Foundation" ] \
   || { echo "SKIP closure-data-shadow (i386 sysroot — run 'make sysroot-objc')"; exit 0; }

# ---- 1: build-level assertion — the shadow must exist. RED pre-fix.
if ! nm -gU "$LIBABICONV" | grep -q ' ___kABTitleProperty$'; then
   echo "FAIL closure-data-shadow: libabiconv exports no ___kABTitleProperty shadow."
   echo "  -> closure import-discovery regressed (abigen_modern_manifest/_i386_closure),"
   echo "     AddressBook umbrella mapping lost, or the source pool's SFTabular original"
   echo "     lost its i386 slice ($M64_FRAMEWORKS/iLife11/SFTabular.framework)."
   exit 1
fi

# link stub: stage the 10.6 SDK's i386 AddressBook stub into the scratch sysroot
ABFW="$SYSROOT/System/Library/Frameworks/AddressBook.framework"
if [ ! -f "$ABFW/AddressBook" ]; then
   [ -f "$SDK106/System/Library/Frameworks/AddressBook.framework/Versions/A/AddressBook" ] \
      || { echo "SKIP closure-data-shadow (no AddressBook link stub: 10.6 SDK absent)"; exit 0; }
   mkdir -p "$ABFW/Versions/A"
   cp "$SDK106/System/Library/Frameworks/AddressBook.framework/Versions/A/AddressBook" \
      "$ABFW/Versions/A/AddressBook"
   ln -sf Versions/A/AddressBook "$ABFW/AddressBook"
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/t.m" <<'EOF'
#import <Foundation/Foundation.h>
/* exactly SFTabular's read shape: an extern ObjC-object data constant from
 * AddressBook, reached through the non-lazy pointer double-deref. */
extern NSString * const kABTitleProperty;
int main(void) {
   NSString *s = kABTitleProperty;
   printf("len=%u eq=%d val=%s\n",
          (unsigned)[s length],
          [s isEqualToString:@"Title"] ? 1 : 0,
          [s UTF8String]);
   /* suite convention: the exec wrapper enters _main via jmp (no return
    * address) — end with exit(), never return. */
   exit(0);
}
EOF

HOST_SDK="$(xcrun --show-sdk-path)"
clang -arch i386 -isysroot "$HOST_SDK" -mmacosx-version-min=10.6 \
      -fobjc-runtime=macosx-fragile -c "$TMP/t.m" -o "$TMP/t.o" 2> "$TMP/cc.err" \
   || { echo "SKIP closure-data-shadow (i386 objc compile unavailable)"; exit 0; }
"$LD" -arch i386 -macos_version_min 10.6 -no_pie -syslibroot "$SYSROOT" \
   -e _main -o "$TMP/t.i386" "$TMP/t.o" \
   -lSystem -lobjc -framework Foundation -framework CoreFoundation \
   -framework AddressBook 2> "$TMP/ld.err" \
   || { echo "FAIL closure-data-shadow (i386 link)"; cat "$TMP/ld.err"; exit 1; }

bash "$PIPELINE" -m "$MACHO_TOOL" -l "$LIBABICONV" -w "$LIBWRAPPER" \
     -i "$LIBINTERPOSE" -o "$TMP/t.x86_64" "$TMP/t.i386" > "$TMP/pipe.log" 2>&1 \
   || { echo "FAIL closure-data-shadow (translate)"; tail -5 "$TMP/pipe.log"; exit 1; }
chmod +x "$TMP/t.x86_64"
# the translated exec loads libabiconv/libinterpose via @loader_path
cp "$LIBABICONV" "$LIBINTERPOSE" "$TMP/"

"$TMP/t.x86_64" > "$TMP/actual" 2>/dev/null
echo "exit_code: $?" >> "$TMP/actual"

printf 'len=5 eq=1 val=Title\nexit_code: 0\n' > "$TMP/expected"
if diff -u "$TMP/expected" "$TMP/actual" > "$TMP/diff"; then
   echo "PASS closure-data-shadow"
   exit 0
else
   echo "FAIL closure-data-shadow: translated read of kABTitleProperty wrong"
   sed 's/^/    /' "$TMP/diff" | head -10
   exit 1
fi
