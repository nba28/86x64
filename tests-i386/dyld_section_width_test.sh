#!/bin/bash
#
# dyld-section-width: a classic 8-byte __DATA,__dyld (two i386 crt slots) is
# padded to 16 bytes in the M64 output (Archive::widen_classic_dyld_section).
# The host dyld writes its native func_lookup as an 8-byte pointer at __dyld+8;
# in an 8-byte section that stomped the NEXT section's first slot (Halo/Civ IV:
# __mod_init_func[0] / __nl_symbol_ptr[0]).
#   shape arm: the i386 input's __dyld is 8 bytes;
#   fix arm:   the translated dylib's __dyld is 16 bytes and keeps both crt
#              placeholder words at +0/+4.
set -u
cd "$(dirname "$0")"
MT="${1:-../build/src/macho-tool/macho-tool}"
IN=build/33_dyld_section_patch.i386
make -s "$IN" >/dev/null 2>&1   # an explicit target, so make keeps it
[ -f "$IN" ] || { echo "SKIP dyld-section-width (cannot build $IN; needs the i386 sysroot)"; exit 0; }
fail() { echo "FAIL dyld-section-width ($1)"; exit 1; }

T=build/dyld_section_width; rm -rf $T; mkdir -p $T
"$MT" rebasify "$IN" $T/r32 2>$T/err &&
"$MT" -- transform $T/r32 $T/t64 2>>$T/err &&
"$MT" -- modify --insert load-dylib,name=@rpath/libabiconv.dylib $T/t64 $T/m64 2>>$T/err &&
"$MT" convert --archive DYLIB --synthesize-dyld-info $T/m64 $T/d64 2>>$T/err ||
   { sed 's/^/    /' $T/err; fail translate; }

size_of() { otool -l "$1" | grep -A5 "sectname __dyld" | awk '/ size /{print $2; exit}'; }
[ "$(size_of "$IN")" = "0x00000008" ] || fail "shape arm: input __dyld is $(size_of "$IN"), not 8 bytes"
[ "$(size_of $T/d64)" = "0x0000000000000010" ] || fail "fix arm: output __dyld is $(size_of $T/d64)"
words=$(otool -s __DATA __dyld $T/d64 | tail -1 | awk '{print $2$3$4$5, $6$7$8$9}')
[ "$words" = "0010e08f 0810e08f" ] || fail "crt placeholder words moved: $words"
echo "PASS dyld-section-width"
