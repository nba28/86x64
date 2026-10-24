#!/bin/bash
# nulljump_underscore_test.sh — A/B guard: a bridge whose native C name begins
# with '_' (Mach-O `__X`, bridge `____X`) is listed as a NULL-jump when nothing
# provides it (find_null_jump_bridges.py; Civ IV's __PyType_Lookup was missed, so
# static-interpose sent its Python C-API calls into a jump to nothing).
# ON lists __m64_nj_probe; OFF (M64_NO_NULLJUMP_UNDERSCORE=1) does not.
set -u
cd "$(dirname "$0")"
ROOT="$(cd .. && pwd)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
cat > "$TMP/b.c" <<'C'
extern int _m64_nj_probe(void);
int ___m64_nj_probe(void) __asm("____m64_nj_probe");
int ___m64_nj_probe(void) { return _m64_nj_probe(); }
C
clang -arch x86_64 -dynamiclib -undefined dynamic_lookup -o "$TMP/libb.dylib" "$TMP/b.c" 2>"$TMP/cc.log" \
  || { echo "nulljump-underscore: FAIL (build)"; cat "$TMP/cc.log"; exit 1; }
on=$(python3 "$ROOT/src/86x64/find_null_jump_bridges.py" --emit "$TMP/libb.dylib" 2>/dev/null | grep -cx '__m64_nj_probe')
off=$(M64_NO_NULLJUMP_UNDERSCORE=1 python3 "$ROOT/src/86x64/find_null_jump_bridges.py" --emit "$TMP/libb.dylib" 2>/dev/null | grep -cx '__m64_nj_probe')
echo "  ON : $on   OFF: $off"
[ "$on" = 1 ] && [ "$off" = 0 ] && { echo "nulljump-underscore: PASS"; exit 0; }
echo "nulljump-underscore: FAIL"; exit 1
