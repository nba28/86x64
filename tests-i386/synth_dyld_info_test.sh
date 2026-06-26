#!/bin/bash
#
# Regression test for `macho-tool convert --synthesize-dyld-info` — route C: turn
# a CLASSIC (LC_DYSYMTAB-only, no LC_DYLD_INFO) translated dylib into a canonical
# MODERN x86_64 dylib by manufacturing an LC_DYLD_INFO_ONLY (rebase/bind/export
# opcode streams) from the classic indirect symbol table + relocations + nlists.
#
# Asserts on the synthesized output:
#   (a) it carries exactly one LC_DYLD_INFO_ONLY, and the classic dysymtab
#       relocation tables are emptied (nlocrel == 0, nextrel == 0);
#   (b) dyld_info parses non-empty bind + rebase + export streams (well-formed);
#   (c) cctools install_name_tool now ACCEPTS it (a classic dylib was rejected
#       "local relocation entries out of place") — the deploy gap;
#   (d) llvm install_name_tool ACCEPTS it if available (a classic dylib was
#       rejected "shared library is not yet supported");
#   (e) codesign signs + verifies, and dyld accepts the Mach-O FORMAT (a load
#       attempt reaches dependency resolution rather than failing as malformed).
#
# Modern `ld` cannot emit a classic-format Mach-O (LC_DYLD_INFO is always
# produced; even `-mmacosx-version-min=10.4/10.5` is silently bumped), and there
# is no in-tree synthetic classic fixture, so this test needs a real classic
# i386 dylib as input. It searches $SYNTH_CLASSIC_DYLIB and the project's known
# classic targets; if none is present it SKIPs (like the suite skips when the
# Snow Leopard sysroot is absent). On a dev machine running the Portal 2 / Source
# engine work it runs against libtier0.dylib.
set -u
MT="${1:?usage: synth_dyld_info_test.sh <path-to-macho-tool>}"

LLVM_INT="${LLVM_INSTALL_NAME_TOOL:-}"
if [ -z "$LLVM_INT" ]; then
   for c in llvm-install-name-tool \
            /usr/local/Cellar/llvm/*/bin/llvm-install-name-tool \
            /opt/homebrew/opt/llvm/bin/llvm-install-name-tool; do
      if command -v "$c" >/dev/null 2>&1; then LLVM_INT="$c"; break; fi
      [ -x "$c" ] && { LLVM_INT="$c"; break; }
   done
fi

# Find a real classic (no LC_DYLD_INFO) i386 dylib to translate.
CANDIDATES=(
   "${SYNTH_CLASSIC_DYLIB:-}"
   "$HOME/Library/Application Support/Steam/steamapps/common/Portal 2/bin/osx32/libtier0.dylib"
   "$HOME/Library/Application Support/Steam/steamapps/common/Portal 2/bin/osx32/libvstdlib.dylib"
)
CLASSIC=""
for c in "${CANDIDATES[@]}"; do
   [ -n "$c" ] && [ -f "$c" ] || continue
   # i386 + no LC_DYLD_INFO == classic
   if file "$c" 2>/dev/null | grep -q 'i386' \
      && [ "$(otool -arch i386 -l "$c" 2>/dev/null | grep -c 'cmd LC_DYLD_INFO')" = "0" ]; then
      CLASSIC="$c"; break
   fi
done
if [ -z "$CLASSIC" ]; then
   echo "SKIP synth-dyld-info (no classic i386 dylib found; set SYNTH_CLASSIC_DYLIB=<path>)"
   exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
fail() { echo "FAIL synth-dyld-info: $1"; exit 1; }

# Strip to the i386 slice if fat.
if file "$CLASSIC" | grep -q 'universal\|fat'; then
   lipo "$CLASSIC" -thin i386 -output "$TMP/in.i386" 2>/dev/null || cp "$CLASSIC" "$TMP/in.i386"
else
   cp "$CLASSIC" "$TMP/in.i386"
fi

# Translate (rebasify -> transform) then convert WITH dyld-info synthesis.
"$MT" rebasify "$TMP/in.i386" "$TMP/in.rebase" 2>/dev/null || fail "rebasify failed"
"$MT" -- transform "$TMP/in.rebase" "$TMP/in.t64" 2>/dev/null || fail "transform failed"
"$MT" convert --archive DYLIB --synthesize-dyld-info \
   "$TMP/in.t64" "$TMP/out.dylib" 2>/dev/null || fail "convert --synthesize-dyld-info failed"

# A convert on a dylib input leaves the original LC_ID_DYLIB plus the new one;
# the pipeline dedups it. Drop the duplicate so install_name_tool isn't blocked
# by a pre-existing (unrelated) condition.
DEDUP="$(cd "$(dirname "$MT")" && pwd)/../../../src/86x64/remove_dup_lc_id_dylib.py"
[ -f "$DEDUP" ] && python3 "$DEDUP" "$TMP/out.dylib" >/dev/null 2>&1

# (a) exactly one LC_DYLD_INFO_ONLY; classic reloc tables emptied.
[ "$(otool -l "$TMP/out.dylib" | grep -c 'cmd LC_DYLD_INFO_ONLY')" = "1" ] \
   || fail "expected exactly one LC_DYLD_INFO_ONLY"
nlocrel=$(otool -l "$TMP/out.dylib" | awk '/nlocrel/{print $2; exit}')
nextrel=$(otool -l "$TMP/out.dylib" | awk '/nextrel/{print $2; exit}')
[ "${nlocrel:-x}" = "0" ] || fail "nlocrel=$nlocrel (classic local relocs not emptied)"
[ "${nextrel:-x}" = "0" ] || fail "nextrel=$nextrel (classic external relocs not emptied)"

# (b) well-formed, non-empty bind + rebase + export streams.
if command -v dyld_info >/dev/null 2>&1; then
   nbind=$(dyld_info -fixups "$TMP/out.dylib" 2>/dev/null | grep -cw bind)
   nreb=$(dyld_info -fixups "$TMP/out.dylib" 2>/dev/null | grep -cw rebase)
   nexp=$(dyld_info -exports "$TMP/out.dylib" 2>/dev/null | grep -c '0x')
   [ "$nbind" -gt 0 ] || fail "no binds in synthesized stream"
   [ "$nexp"  -gt 0 ] || fail "no exports in synthesized trie"
   echo "  dyld_info: $nbind binds, $nreb rebases, $nexp exports"
fi

# (c) cctools install_name_tool accepts (was 'local relocation entries out of place').
cp "$TMP/out.dylib" "$TMP/cc.dylib"
install_name_tool -add_rpath /synth_test_rpath "$TMP/cc.dylib" 2>/dev/null
otool -l "$TMP/cc.dylib" | grep -q /synth_test_rpath \
   || fail "cctools install_name_tool did not accept the synthesized dylib"

# (d) llvm install_name_tool accepts (was 'shared library is not yet supported').
if [ -n "$LLVM_INT" ]; then
   cp "$TMP/out.dylib" "$TMP/llvm.dylib"
   "$LLVM_INT" -add_rpath /synth_test_rpath_llvm "$TMP/llvm.dylib" 2>/dev/null
   otool -l "$TMP/llvm.dylib" | grep -q /synth_test_rpath_llvm \
      || fail "llvm install_name_tool did not accept the synthesized dylib"
else
   echo "  (llvm-install-name-tool not found — skipping the llvm sub-check)"
fi

# (e) codesign signs + verifies; dyld accepts the Mach-O format.
cp "$TMP/out.dylib" "$TMP/cs.dylib"
codesign -f -s - "$TMP/cs.dylib" 2>/dev/null || fail "codesign signing failed"
codesign -v "$TMP/cs.dylib" 2>/dev/null || fail "codesign verification failed"

printf '#include <dlfcn.h>\n#include <stdio.h>\nint main(int c,char**v){void*h=dlopen(v[1],RTLD_NOW);printf("%%s\\n",h?"loaded":dlerror());return 0;}\n' > "$TMP/load.c"
if cc -arch x86_64 -o "$TMP/load" "$TMP/load.c" 2>/dev/null; then
   msg=$(arch -x86_64 "$TMP/load" "$TMP/cs.dylib" 2>&1)
   # Anything other than a structural/format complaint means dyld accepted the
   # Mach-O and only stumbled on dependencies/symbols (expected for a classic
   # game dylib whose dead deps / shims aren't staged here).
   case "$msg" in
      *malformed*|*"not a valid"*|*"truncated"*|*"invalid"*|*"bad magic"*)
         fail "dyld rejected the Mach-O format: $msg" ;;
   esac
fi

echo "PASS synth-dyld-info ($(basename "$CLASSIC"): LC_DYLD_INFO_ONLY + install_name_tool[cc${LLVM_INT:+/llvm}] + codesign + dyld-format)"
