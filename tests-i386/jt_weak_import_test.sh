#!/bin/bash
#
# Regression test for the WEAK-IMPORT jump-table fix (the keystone that lets
# images with REMOVED-from-modern-macOS Carbon/QuickDraw/Sound imports LOAD
# instead of dyld hard-failing "Symbol not found"). It exercises the classic
# i386 `__IMPORT,__jump_table` (S_SYMBOL_STUBS + S_ATTR_SELF_MODIFYING_CODE)
# stub path end to end:
#
#   lift_jump_table_targets  -> marks each UNDEFINED jump-table import N_WEAK_REF
#                               + synthesizes a NULL-checking diagnostic
#                               JumpStubBlob trampoline + a __jt_ptrs bound slot
#   synthesize_dyld_info     -> N_WEAK_REF => BIND_SYMBOL_FLAGS_WEAK_IMPORT
#
# Asserts:
#   (a) after transform, the jump-table undef imports are WEAK external nlists
#       (nm -m "weak external") == N_WEAK_REF set;
#   (b) the synthesized __TEXT,__jt_tramp holds the fixed NULL-checking
#       diagnostic trampolines (each ends in `ud2`, loads its slot into r11);
#   (c) the trampolines SURVIVE the convert/modify reparse byte-for-byte in
#       structure (same `ud2` count) -- the inline-name variant did NOT, this
#       pure-code form does;
#   (d) the final modern dylib's bind stream carries WEAK_IMPORT for those
#       jump-table imports (so a removed one binds NULL and the image LOADS).
#
# Modern `ld` rewrites the classic `__IMPORT,__jump_table` self-modifying stubs
# into ordinary lazy stubs, so there is no way to BUILD a jump-table fixture
# from source; like synth_dyld_info_test.sh this needs a real classic binary. It
# searches $JT_CLASSIC_BINARY and known on-disk classic i386 binaries that carry
# a `__jump_table`; if none is present it SKIPs.
set -u
MT="${1:?usage: jt_weak_import_test.sh <path-to-macho-tool>}"

# Find a real classic i386 binary with a __IMPORT,__jump_table section.
CANDIDATES=(
   "${JT_CLASSIC_BINARY:-}"
   "$HOME/projects/Library/Frameworks/iLife11/eOkaoCom.dylib"
   "$HOME/projects/Library/Frameworks/iLife11/eOkaoPt.dylib"
   "$HOME/projects/Library/Frameworks/iLife11/eOkaoDt.dylib"
   "$HOME/projects/Library/Frameworks/iLife11/SFWebView.framework/Versions/A/SFWebView"
)
JT=""
for c in "${CANDIDATES[@]}"; do
   [ -n "$c" ] && [ -f "$c" ] || continue
   if file "$c" 2>/dev/null | grep -q 'i386' \
      && otool -arch i386 -l "$c" 2>/dev/null | grep -q '__jump_table'; then
      JT="$c"; break
   fi
done
if [ -z "$JT" ]; then
   echo "SKIP jt-weak-import (no classic i386 __jump_table binary found; set JT_CLASSIC_BINARY=<path>)"
   exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
fail() { echo "FAIL jt-weak-import: $1"; exit 1; }

# i386 slice only.
if file "$JT" | grep -q 'universal\|fat'; then
   lipo "$JT" -thin i386 -output "$TMP/in.i386" 2>/dev/null || fail "lipo -thin i386 failed"
else
   cp "$JT" "$TMP/in.i386"
fi

# Names imported through the __IMPORT,__jump_table self-modifying stubs.
JT_SYMS=$(otool -arch i386 -Iv "$TMP/in.i386" 2>/dev/null \
   | awk '/Indirect symbols for \(__IMPORT,__jump_table\)/{f=1;next} /Indirect symbols for/{f=0} f && $3 ~ /^_/ {print $3}')
[ -n "$JT_SYMS" ] || { echo "SKIP jt-weak-import ($JT has __jump_table but no undefined imports)"; exit 0; }
NSYM=$(printf '%s\n' "$JT_SYMS" | grep -c .)

# rebasify -> transform -> convert (the real pipeline's macho-tool steps).
"$MT" rebasify "$TMP/in.i386" "$TMP/in.rebase" 2>/dev/null || fail "rebasify failed"
"$MT" -- transform "$TMP/in.rebase" "$TMP/in.t64" 2>/dev/null || fail "transform failed"

# (a) each jump-table import is now a WEAK external undef nlist (N_WEAK_REF).
NM_T64=$(nm -m "$TMP/in.t64" 2>/dev/null)
nweak=0
while IFS= read -r s; do
   [ -n "$s" ] || continue
   if printf '%s\n' "$NM_T64" | grep -qE "weak external $s\b"; then
      nweak=$((nweak+1))
   else
      fail "$s is not a weak external after transform (N_WEAK_REF not set)"
   fi
done <<< "$JT_SYMS"
[ "$nweak" = "$NSYM" ] || fail "expected $NSYM weak jump-table imports, got $nweak"

# (b) __TEXT,__jt_tramp holds the NULL-checking diagnostic trampolines: one `ud2`
# per undefined stub, each loading its slot into r11.
TRAMP_T64=$(otool -v -s __TEXT __jt_tramp "$TMP/in.t64" 2>/dev/null)
nud2_t64=$(printf '%s\n' "$TRAMP_T64" | grep -cw 'ud2')
[ "$nud2_t64" = "$NSYM" ] || fail "expected $NSYM ud2 traps in __jt_tramp (transform), got $nud2_t64"
printf '%s\n' "$TRAMP_T64" | grep -q '%r11' || fail "__jt_tramp trampolines do not use r11 (NULL-check form)"

# convert to the final modern dylib.
"$MT" convert --archive DYLIB --synthesize-dyld-info \
   "$TMP/in.t64" "$TMP/out.dylib" 2>/dev/null || fail "convert --synthesize-dyld-info failed"

# (c) the trampolines survive the convert reparse (same ud2 count -> not
# corrupted; the earlier inline-name form mis-decoded here).
nud2_out=$(otool -v -s __TEXT __jt_tramp "$TMP/out.dylib" 2>/dev/null | grep -cw 'ud2')
[ "$nud2_out" = "$NSYM" ] || fail "trampolines corrupted by convert reparse: $nud2_out/$NSYM ud2 survived"

# (d) the final bind stream carries WEAK_IMPORT for the jump-table imports.
NWEAK_BIND=$(python3 - "$TMP/out.dylib" "$JT_SYMS" <<'PY'
import sys,struct
data=open(sys.argv[1],"rb").read()
want=set(s for s in sys.argv[2].split() if s)
ncmds=struct.unpack_from("<I",data,16)[0]
off=32; bind_off=bind_size=0
for _ in range(ncmds):
    cmd,cmdsize=struct.unpack_from("<II",data,off)
    if cmd in (0x22,0x80000022):
        bind_off,bind_size=struct.unpack_from("<II",data,off+8+8)  # bind_off,bind_size
    off+=cmdsize
def uleb(p):
    r=s=0
    while True:
        b=data[p];p+=1;r|=(b&0x7f)<<s
        if not b&0x80:break
        s+=7
    return r,p
p=bind_off; end=bind_off+bind_size; cur=None; weak=False; nweak=0
while p<end:
    b=data[p];op=b&0xF0;imm=b&0x0F;p+=1
    if   op==0x00: pass
    elif op==0x20: _,p=uleb(p)
    elif op==0x40:
        weak=bool(imm&0x1); s=p
        while data[p]!=0:p+=1
        cur=data[s:p].decode();p+=1
    elif op==0x60: _,p=uleb(p)
    elif op==0x70: _,p=uleb(p)
    elif op==0x80: _,p=uleb(p)
    elif op==0x90:
        if cur in want and weak: nweak+=1
    elif op==0xA0:
        _,p=uleb(p)
        if cur in want and weak: nweak+=1
    elif op==0xB0:
        if cur in want and weak: nweak+=1
    elif op==0xC0:
        c,p=uleb(p); _,p=uleb(p)
        if cur in want and weak: nweak+=c
print(nweak)
PY
)
[ "${NWEAK_BIND:-0}" -ge 1 ] 2>/dev/null || fail "no WEAK_IMPORT binds synthesized for jump-table imports"
[ "$NWEAK_BIND" = "$NSYM" ] || fail "expected $NSYM WEAK_IMPORT binds, got $NWEAK_BIND"

echo "PASS jt-weak-import ($(basename "$JT"): $NSYM jump-table imports -> N_WEAK_REF + WEAK_IMPORT bind; $NSYM ud2 diagnostic trampolines survive convert)"
