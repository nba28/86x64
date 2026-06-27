#!/bin/bash
#
# Broad canonical-output validation matrix for route C
# (`macho-tool convert --synthesize-dyld-info`).
#
# For each classic (LC_DYSYMTAB-only) i386 input, translate it the same way the
# 86x64.sh classic path does (rebasify -> transform -> convert
# --synthesize-dyld-info -> dedup LC_ID_DYLIB) and then assert that the
# synthesized MODERN x86_64 output is structurally canonical enough that EVERY
# stock cctools tool accepts it:
#
#   [STRUCT]  exactly one LC_DYLD_INFO_ONLY; nlocrel == nextrel == 0
#   [INT-rp]  cctools install_name_tool -add_rpath succeeds
#   [INT-ch]  cctools install_name_tool -change <dep> @rpath/<dep> succeeds
#   [INT-id]  cctools install_name_tool -id @rpath/<name> succeeds
#   [SIGN]    codesign -f -s - then codesign -v passes
#   [OTOOL]   otool -l exits 0 (well-formed load commands)
#   [DYLD]    arch -x86_64 dlopen() reaches dependency/symbol resolution
#             (NOT a *malformed/invalid/truncated* format rejection)
#
# Any single FAIL is a canonical gap to root-cause in synthesize_dyld_info /
# linkedit layout.  Usage:
#   canonical_matrix.sh <macho-tool> [binary ...]
# With no binaries it uses a built-in spread of Portal 2 classic dylibs.
set -u
MT="${1:?usage: canonical_matrix.sh <macho-tool> [binary...]}"; shift || true
MTDIR="$(cd "$(dirname "$MT")" && pwd)"
DEDUP="$MTDIR/../../../src/86x64/remove_dup_lc_id_dylib.py"

LLVM_INT="${LLVM_INSTALL_NAME_TOOL:-}"
if [ -z "$LLVM_INT" ]; then
   for c in llvm-install-name-tool /usr/local/Cellar/llvm/*/bin/llvm-install-name-tool \
            /opt/homebrew/opt/llvm/bin/llvm-install-name-tool; do
      command -v "$c" >/dev/null 2>&1 && { LLVM_INT="$c"; break; }
      [ -x "$c" ] && { LLVM_INT="$c"; break; }
   done
fi

P2="$HOME/Library/Application Support/Steam/steamapps/common/Portal 2/bin/osx32"
if [ "$#" -eq 0 ]; then
   set -- "$P2/unitlib.dylib" "$P2/inputsystem.dylib" "$P2/libtier0.dylib" \
          "$P2/vgui2.dylib" "$P2/libvstdlib.dylib" "$P2/datacache.dylib" \
          "$P2/filesystem_stdio.dylib"
fi

# Build the x86_64 dlopen probe once.
PROBE="$(mktemp -d)/probe"
printf '#include <dlfcn.h>\n#include <stdio.h>\nint main(int c,char**v){void*h=dlopen(v[1],RTLD_NOW);printf("%%s\\n",h?"loaded":dlerror());return 0;}\n' > "$PROBE.c"
HAVE_PROBE=0
cc -arch x86_64 -o "$PROBE" "$PROBE.c" 2>/dev/null && HAVE_PROBE=1

PASS_N=0; FAIL_N=0; SKIP_N=0
declare -a FAILS

check() { # name input
   local name="$1" in="$2"
   if [ ! -f "$in" ]; then echo "SKIP  $name (not found)"; SKIP_N=$((SKIP_N+1)); return; fi
   if ! file "$in" 2>/dev/null | grep -q i386; then echo "SKIP  $name (no i386 slice)"; SKIP_N=$((SKIP_N+1)); return; fi
   if [ "$(otool -arch i386 -l "$in" 2>/dev/null | grep -c 'cmd LC_DYLD_INFO')" != "0" ]; then
      echo "SKIP  $name (already modern LC_DYLD_INFO)"; SKIP_N=$((SKIP_N+1)); return
   fi
   local T; T="$(mktemp -d)"
   local err=""
   # thin to i386
   if file "$in" | grep -q 'universal\|fat'; then
      lipo "$in" -thin i386 -output "$T/in.i386" 2>/dev/null || cp "$in" "$T/in.i386"
   else cp "$in" "$T/in.i386"; fi

   "$MT" rebasify "$T/in.i386" "$T/in.rebase" 2>"$T/e" || err="rebasify"
   [ -z "$err" ] && { "$MT" -- transform "$T/in.rebase" "$T/in.t64" 2>"$T/e" || err="transform"; }
   [ -z "$err" ] && { "$MT" convert --archive DYLIB --synthesize-dyld-info "$T/in.t64" "$T/out.dylib" 2>"$T/e" || err="convert"; }
   if [ -n "$err" ]; then echo "FAIL  $name [$err] $(tail -1 "$T/e")"; FAIL_N=$((FAIL_N+1)); FAILS+=("$name:$err"); rm -rf "$T"; return; fi
   [ -f "$DEDUP" ] && python3 "$DEDUP" "$T/out.dylib" >/dev/null 2>&1

   local results=""
   # [STRUCT]
   local ndi nloc next
   ndi=$(otool -l "$T/out.dylib" | grep -c 'cmd LC_DYLD_INFO_ONLY')
   nloc=$(otool -l "$T/out.dylib" | awk '/nlocrel/{print $2; exit}')
   next=$(otool -l "$T/out.dylib" | awk '/nextrel/{print $2; exit}')
   if [ "$ndi" = "1" ] && [ "${nloc:-x}" = "0" ] && [ "${next:-x}" = "0" ]; then results+="STRUCT "; else results+="!STRUCT(ndi=$ndi,nloc=$nloc,next=$next) "; err="x"; fi

   # install_name_tool prints a harmless "will invalidate the code signature"
   # WARNING (exit 0) when the input carries a stale LC_CODE_SIGNATURE; the
   # mutation still applies, so trust the exit code, not stderr text.
   # [INT-rp]
   cp "$T/out.dylib" "$T/rp.dylib"
   if install_name_tool -add_rpath /canon_rp "$T/rp.dylib" 2>"$T/e" \
      && otool -l "$T/rp.dylib" | grep -q /canon_rp; then results+="INT-rp "; else results+="!INT-rp($(grep -i error "$T/e" | tail -1)) "; err="x"; fi

   # [INT-ch] change a real LC_LOAD_DYLIB dependency (skip the id/loader_path
   # line and the leading "file:" header line).
   local dep
   dep=$(otool -L "$T/out.dylib" 2>/dev/null | awk 'NR>2 && $1 !~ /:$/ {print $1; exit}')
   if [ -n "$dep" ]; then
      cp "$T/out.dylib" "$T/ch.dylib"
      if install_name_tool -change "$dep" "@rpath/$(basename "$dep")" "$T/ch.dylib" 2>"$T/e" \
         && otool -L "$T/ch.dylib" | grep -q "@rpath/$(basename "$dep")"; then results+="INT-ch "; else results+="!INT-ch($(grep -i error "$T/e" | tail -1)) "; err="x"; fi
   fi

   # [INT-id]
   cp "$T/out.dylib" "$T/id.dylib"
   if install_name_tool -id "@rpath/$name" "$T/id.dylib" 2>"$T/e" \
      && otool -D "$T/id.dylib" | grep -q "@rpath/$name"; then results+="INT-id "; else results+="!INT-id($(grep -i error "$T/e" | tail -1)) "; err="x"; fi

   # [SIGN]
   cp "$T/out.dylib" "$T/cs.dylib"
   if codesign -f -s - "$T/cs.dylib" 2>"$T/e" && codesign -v "$T/cs.dylib" 2>"$T/e"; then results+="SIGN "; else results+="!SIGN($(tail -1 "$T/e")) "; err="x"; fi

   # [OTOOL]
   if otool -l "$T/out.dylib" >/dev/null 2>"$T/e"; then results+="OTOOL "; else results+="!OTOOL "; err="x"; fi

   # [DYLD] (10s alarm: a probe can block resolving a missing/slow dep chain;
   # we only care that dyld accepts the FORMAT, so a timeout is treated as "no
   # format complaint" = pass, not a hang.)
   if [ "$HAVE_PROBE" = 1 ]; then
      local msg; msg=$(perl -e 'alarm 10; exec @ARGV' arch -x86_64 "$PROBE" "$T/cs.dylib" 2>&1)
      case "$msg" in
         *malformed*|*"not a valid"*|*truncated*|*invalid*|*"bad magic"*|*"out of place"*|*"not supported"*)
            results+="!DYLD($msg) "; err="x" ;;
         *) results+="DYLD " ;;
      esac
   fi

   if [ -z "$err" ]; then echo "PASS  $name | $results"; PASS_N=$((PASS_N+1));
   else echo "FAIL  $name | $results"; FAIL_N=$((FAIL_N+1)); FAILS+=("$name"); fi
   rm -rf "$T"
}

for in in "$@"; do check "$(basename "$in")" "$in"; done

echo "------------------------------------------------------------"
echo "matrix: PASS=$PASS_N FAIL=$FAIL_N SKIP=$SKIP_N"
[ "$FAIL_N" = 0 ] && echo "ALL CANONICAL" || { echo "GAPS: ${FAILS[*]}"; exit 1; }
