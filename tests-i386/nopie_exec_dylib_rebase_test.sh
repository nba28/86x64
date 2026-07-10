#!/bin/bash
#
# Regression test for the NON-PIE-executable -> dylib REBASE-SYNTHESIS fix
# (core archive.cc synthesize_dyld_info rebase_only path).
#
# A non-PIE i386 EXECUTABLE (MH_PIE clear) links with an EMPTY rebase table
# (rebase_size 0) even though it carries an LC_DYLD_INFO_ONLY: as a fixed-base
# image its internal sliding pointers are recorded as INDIRECT_SYMBOL_LOCAL
# __nl_symbol_ptr slots resolved absolute at link time — no rebase opcodes are
# emitted because the executable never slides. When the 86x64 pipeline converts
# that executable into a DYLIB (which dyld ALWAYS slides), every un-rebased
# absolute internal pointer is left at its original pre-slide vmaddr ->
# EXC_BAD_ACCESS on first deref.
#
# Root cause: Civ IV Steam (i386, non-PIE, LC_DYLD_INFO with rebase_size 0,
# 5992 INDIRECT_SYMBOL_LOCAL __nl_symbol_ptr slots — boost.python vtable/GOT
# locals) crashed at a CFBundleGetFunctionPointerForName plugin-loader path
# reading a stale __nl_symbol_ptr (movl (%rdi),%eax, rdi = original vmaddr
# 0x11f9e6f0 that should have slid to 0x6dc76f0). The classic (LC_DYSYMTAB-only)
# synth path already emitted these rebases; the modern-with-empty-rebase path
# early-returned and emitted none.
#
# Fix: synthesize_dyld_info now detects an existing LC_DYLD_INFO whose rebase
# table is EMPTY and MERGES synthesized REBASE opcodes (one per LOCAL
# __nl_symbol_ptr slot, derived from the indirect symbol table) into that stream,
# leaving the image's already-correct bind/export streams untouched.
#
# The LOCAL-__nl_symbol_ptr shape is emitted only by the old GCC/boost.python
# codegen (modern clang stores file-scope function-pointer tables as plain
# __data words with no indirect markers), so there is no compact synthetic
# fixture. Like synth-dyld-info (which needs a real classic Portal 2 dylib), this
# test uses a REAL non-PIE i386 executable that carries LOCAL __nl_symbol_ptr
# slots + rebase_size 0, and asserts the converted dylib gains a non-empty rebase
# stream. Searches $NOPIE_REBASE_INPUT and the known Civ IV Steam binary; SKIPs
# if none is present.
set -u
MT="${1:?usage: nopie_exec_dylib_rebase_test.sh <path-to-macho-tool>}"

fail() { echo "FAIL nopie-exec-dylib-rebase: $1"; exit 1; }

# --- Find a real non-PIE i386 executable with LOCAL __nl_symbol_ptr + no rebases.
CANDIDATES=(
   "${NOPIE_REBASE_INPUT:-}"
   "$HOME/Library/Application Support/Steam/steamapps/common/Sid Meier's Civilization IV 34440/Civilization IV.app/Contents/MacOS/Civilization IV"
)
IN=""
for c in "${CANDIDATES[@]}"; do
   [ -n "$c" ] && [ -f "$c" ] || continue
   file "$c" 2>/dev/null | grep -q 'i386' || continue
   # must be modern (LC_DYLD_INFO), empty rebase table, and have LOCAL nl slots.
   HAS_DI=$(otool -arch i386 -l "$c" 2>/dev/null | grep -c 'cmd LC_DYLD_INFO')
   [ "$HAS_DI" -gt 0 ] || continue
   RB=$(otool -arch i386 -l "$c" 2>/dev/null | awk '/rebase_size/ {print $2; exit}')
   [ "${RB:-1}" = "0" ] || continue
   LOC=$(otool -arch i386 -Iv "$c" 2>/dev/null | grep -c 'LOCAL')
   [ "${LOC:-0}" -gt 0 ] || continue
   IN="$c"; break
done
if [ -z "$IN" ]; then
   echo "SKIP nopie-exec-dylib-rebase (no non-PIE i386 exec with LOCAL __nl_symbol_ptr found; set NOPIE_REBASE_INPUT=<path>)"
   exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
export PATH="$(cd "$(dirname "$MT")" && pwd):$PATH"

# Thin to the i386 slice if fat.
if file "$IN" 2>/dev/null | grep -q 'universal\|fat'; then
   lipo "$IN" -thin i386 -output "$TMP/in.i386" 2>/dev/null || cp "$IN" "$TMP/in.i386"
else
   cp "$IN" "$TMP/in.i386"
fi

# (a) Confirm the trigger on the input.
IN_REBASE=$(otool -l "$TMP/in.i386" 2>/dev/null | awk '/rebase_size/ {print $2; exit}')
[ "${IN_REBASE:-1}" = "0" ] || fail "input rebase_size=$IN_REBASE (expected 0)"
IN_LOCAL=$(otool -Iv "$TMP/in.i386" 2>/dev/null | grep -c 'LOCAL')
[ "${IN_LOCAL:-0}" -gt 0 ] || fail "input has no LOCAL __nl_symbol_ptr slots (wrong fixture)"

# --- Translate to a DYLIB with dyld-info synthesis. ---
"$MT" rebasify "$TMP/in.i386" "$TMP/in.rebase" >/dev/null 2>&1 || fail "rebasify"
"$MT" -- transform "$TMP/in.rebase" "$TMP/in.transform" >/dev/null 2>&1 || fail "transform"
"$MT" convert --archive DYLIB --synthesize-dyld-info \
   "$TMP/in.transform" "$TMP/in.dylib" >/dev/null 2>&1 \
   || fail "convert --archive DYLIB --synthesize-dyld-info"

# (b) The converted dylib MUST now carry a non-empty rebase stream.
OUT_REBASE=$(otool -l "$TMP/in.dylib" 2>/dev/null | awk '/rebase_size/ {print $2; exit}')
[ -n "$OUT_REBASE" ] || fail "converted dylib has no LC_DYLD_INFO rebase_size field"
if [ "$OUT_REBASE" = "0" ]; then
   fail "converted dylib rebase_size=0 (fix did not synthesize rebases for the \
non-PIE exec's $IN_LOCAL LOCAL __nl_symbol_ptr slots — they would fault under slide)"
fi

echo "nopie-exec-dylib-rebase: input rebase_size=0 with $IN_LOCAL LOCAL slots -> output rebase_size=$OUT_REBASE bytes; OK"
exit 0
