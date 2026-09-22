#!/bin/bash
#
# Regression test for the __86x64_pcmap / __86x64_ehlsda STAGE-SHIFT-STALE bug
# (core section.cc re-parse routing + PcmapBlob/EhlsdaBlob::Parse).
#
# The C++ exception unwinder (libabiconv eh_shim.c) maps translated PCs to the
# original i386 PC space through __DATA,__86x64_pcmap, and finds each function's
# LSDA through __DATA,__86x64_ehlsda. Both tables are synthesized once by
# Archive::inject_pcmap_section / inject_ehlsda_section at the first M64 Build
# (the transform stage), and inject is IDEMPOTENT — a later stage that re-parses
# the M64 archive (modify --insert / static-interpose / convert) must therefore
# ROUND-TRIP the existing tables.
#
# Pre-fix they round-tripped through the generic DataParser, which
# pointer-detects every 4-byte aligned word that aliases a segment vmaddr:
#  - `orig` fields are ORIGINAL-i386 vmaddrs (~0x1000-0x4000+) that alias the
#    translated image's own low vmaddr range, and
#  - `trans_off` fields can alias too,
# so a stage that SHIFTS the layout re-emitted those words "rebased" by the
# shift delta: the pcmap silently mapped translated PCs to WRONG original PCs
# and every C++ throw in the deployed binary found the wrong LSDA/landing pad
# (latent EH-correctness gap for every C++ target; invisible to stale-check —
# baked bytes, not binds). The fix lifts both sections back into live
# PcmapBlob/EhlsdaBlob objects on re-parse (the __86x64_xrel/__86x64_abs32
# pattern) whose Parse re-resolves each row to the instruction blob it names,
# so every re-Build re-emits rows against the FINAL layout.
#
# This test recreates the trigger deterministically: build the existing
# 51_eh_throw_int C++ fixture as an i386 DYLIB linked at -image_base
# 0x10000000 — the M64 layout window — so the table's original-i386 vmaddrs
# ALIAS the translated image's own segment range (the structural trigger;
# prebound-era i386 frameworks legitimately carry bases across this space).
# Then: translate, snapshot both tables, FORCE a layout shift by growing the
# load-command area across repeated `modify --insert` re-parses (the same
# growth class the real pipeline's stages cause), convert to a dylib (another
# re-parse), and assert both tables carry the SAME rows in the final image.
# Rows are compared in (trans_off, orig) form — section-relative offsets are
# invariant across a pure segment shift and orig values live in the FROZEN
# i386 address space, so any drift = the bug. Pre-fix this fails with ~70
# drifted rows (origs non-uniformly rebased by the DataParser round-trip).
#
# Needs the i386 sysroot + staged libstdc++ (see Makefile sysroot/sysroot-cpp);
# SKIPs without.
# i386 linking needs the Snow Leopard ld64-95 wrapper: modern ld dropped -arch i386
# (same resolution as the Makefile's LD). Override with LD=... in the environment.
LD="${LD:-$HOME/projects/Library/Toolchains/sl-ld64/ld-i386}"; [ -x "$LD" ] || LD=ld

set -u
MT="${1:?usage: pcmap_stage_shift_test.sh <path-to-macho-tool>}"

fail() { echo "FAIL pcmap-stage-shift: $1"; exit 1; }

SYSROOT=/tmp/i386-sysroot
if [ ! -d "$SYSROOT/usr/lib" ] || [ ! -f "$SYSROOT/usr/lib/libstdc++.dylib" ]; then
   echo "SKIP pcmap-stage-shift (no i386 sysroot / libstdc++ at $SYSROOT)"
   exit 0
fi

HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/src/51_eh_throw_int.cc"
[ -f "$SRC" ] || fail "fixture $SRC missing"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

HOST_SDK="$(xcrun --show-sdk-path)"

# --- Build the i386 C++ fixture as a DYLIB whose base ALIASES the M64
#     layout window (see header comment). ---
clang++ -arch i386 -isysroot "$HOST_SDK" -mmacosx-version-min=10.6 \
   -c "$SRC" -o "$TMP/t.o" 2>/dev/null || fail "clang++ -arch i386"
"$LD" -arch i386 -macos_version_min 10.6 -dylib -image_base 0x10000000 \
   -syslibroot "$SYSROOT" -lSystem -lstdc++ -o "$TMP/t.i386" "$TMP/t.o" \
   2>/dev/null || fail "ld i386 dylib"

# --- Translate: rebasify + transform (first M64 Build injects the tables). ---
"$MT" rebasify "$TMP/t.i386" "$TMP/t.rebase" >/dev/null 2>&1 || fail rebasify
"$MT" -- transform "$TMP/t.rebase" "$TMP/t.transform" >/dev/null 2>&1 || fail transform

# The shift probe is the fixture's FIRST __DATA section, whichever it is: a
# C++ EH fixture need not have a __data section at all (today's clang++ gives it
# __nl_symbol_ptr/__la_symbol_ptr only), and the test just needs one __DATA
# address that moves when the load commands grow.
sect_addr() { otool -l "$1" 2>/dev/null | awk -v want="$2" \
   '/^ *sectname /{s=$2} /^ *segname /{g=$2} /^ *addr /{if(g=="__DATA"&&s==want){print $2; exit}}'; }
# (the SEGMENT load command also prints a segname line, so only a segname that
#  directly follows a sectname names a section.)
first_data_sect() { otool -l "$1" 2>/dev/null | awk \
   '/^ *sectname /{s=$2; p=1; next} /^ *segname /{if(p&&$2=="__DATA"){print s; exit}; p=0}'; }

SECT=$(first_data_sect "$TMP/t.transform")
[ -n "$SECT" ] || fail "no __DATA section in transform output"
data_addr() { sect_addr "$1" "$SECT"; }

D0=$(data_addr "$TMP/t.transform")
[ -n "$D0" ] || fail "no $SECT address in transform output"

# --- Row dumper: prints "pcmap <trans_off> <orig>" and
#     "ehlsda <func_off> <orig_func> <lsda_off>" lines, sorted. ---
dump_tables() {
python3 - "$1" <<'EOF'
import struct, sys
data = open(sys.argv[1], 'rb').read()
magic, = struct.unpack_from('<I', data, 0)
assert magic == 0xfeedfacf, hex(magic)
ncmds, = struct.unpack_from('<I', data, 16)
off = 32
sects = {}
for _ in range(ncmds):
    cmd, csz = struct.unpack_from('<II', data, off)
    if cmd == 0x19:  # LC_SEGMENT_64
        nsects, = struct.unpack_from('<I', data, off + 64)
        soff = off + 72
        for _ in range(nsects):
            sect = data[soff:soff+16].rstrip(b'\0').decode()
            seg  = data[soff+16:soff+32].rstrip(b'\0').decode()
            addr, size = struct.unpack_from('<QQ', data, soff + 32)
            fo, = struct.unpack_from('<I', data, soff + 48)
            sects[(seg, sect)] = (addr, size, fo)
            soff += 80
    off += csz
rows = []
pc = sects.get(('__DATA', '__86x64_pcmap'))
if pc:
    fo = pc[2]
    magic, count = struct.unpack_from('<II', data, fo)
    assert magic == 0x366d6370, "pcmap magic %#x" % magic
    for i in range(count):
        t, o = struct.unpack_from('<iI', data, fo + 8 + i*8)
        rows.append("pcmap %d %#x" % (t, o))
eh = sects.get(('__DATA', '__86x64_ehlsda'))
if eh:
    fo = eh[2]
    magic, count = struct.unpack_from('<II', data, fo)
    assert magic == 0x366c6865, "ehlsda magic %#x" % magic
    for i in range(count):
        f, o, l = struct.unpack_from('<iIi', data, fo + 8 + i*12)
        rows.append("ehlsda %d %#x %d" % (f, o, l))
if not rows:
    print("NOTABLES")
for r in sorted(rows):
    print(r)
EOF
}

dump_tables "$TMP/t.transform" > "$TMP/rows.before" || fail "dump transform tables"
grep -q NOTABLES "$TMP/rows.before" && fail "transform output carries no pcmap/ehlsda tables (fixture premise broken)"
NPC=$(grep -c '^pcmap' "$TMP/rows.before" || true)
NLS=$(grep -c '^ehlsda' "$TMP/rows.before" || true)

# --- FORCE a layout shift: grow the load-command area across re-parses. ---
CUR="$TMP/t.transform"
SHIFTED=""
for i in 1 2 3 4 5 6 7 8; do
   NAME=$(printf '@rpath/stage_shift_pad_%02d_%0400d.dylib' "$i" 0)
   "$MT" -- modify --insert load-dylib,name="$NAME" "$CUR" "$TMP/t.s$i" \
      >/dev/null 2>&1 || fail "modify --insert #$i"
   CUR="$TMP/t.s$i"
   DN=$(data_addr "$CUR")
   if [ -n "$DN" ] && [ "$DN" != "$D0" ]; then SHIFTED=1; break; fi
done
[ -n "$SHIFTED" ] || fail "could not force a __DATA shift ($SECT $D0 unchanged after 8 inserts) — test premise broken, fix the forcing"

# --- Final conversion (another re-parse; mirror the pipeline's flags). ---
"$MT" convert --archive DYLIB --synthesize-dyld-info "$CUR" "$TMP/t.dylib" \
   >/dev/null 2>&1 || fail convert

D1=$(data_addr "$TMP/t.dylib")
echo "pcmap-stage-shift: $SECT ${D0} (transform) -> ${D1} (final dylib); ${NPC} pcmap + ${NLS} ehlsda rows"

dump_tables "$TMP/t.dylib" > "$TMP/rows.after" || fail "dump final tables"
grep -q NOTABLES "$TMP/rows.after" && fail "final dylib lost the pcmap/ehlsda tables"

if ! diff -u "$TMP/rows.before" "$TMP/rows.after" > "$TMP/rows.diff"; then
   NDIFF=$(grep -cE '^[+-](pcmap|ehlsda)' "$TMP/rows.diff" || true)
   echo "FAIL pcmap-stage-shift: $NDIFF row(s) drifted across the forced-shift re-parses"
   echo "--- first drifted rows (trans_off/orig must be shift-invariant): ---"
   grep -E '^[+-](pcmap|ehlsda)' "$TMP/rows.diff" | head -12
   exit 1
fi

echo "pcmap-stage-shift: all rows identical across shift + convert; OK"
exit 0
