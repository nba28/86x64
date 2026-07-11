#!/bin/bash
#
# Regression test for the STAGE-SHIFT-STALE pointer-immediate bug
# (core instruction.cc, XED_IFORM_MOV_MEMv_IMMz abs32-dest arm).
#
# A non-PIE i386 executable initializes a same-object self-referential pointer
# with `movl $abs32, abs32` (c7 05 disp32 imm32) — the GCC-4 -O2 inlined
# empty-container header init (boost.python registry std::set _Rb_tree
# _M_initialize: `_M_header._M_left = _M_header._M_right = &_M_header`; std::list
# header next/prev likewise).
#
# The M32->M64 transform used to translate this by CLONING the instruction as
# `mov dword [rip+disp], imm32`, leaving the POINTER as a raw imm32 in __text
# (patched once, from the transform-stage layout, by an M64 Immediate blob).
# Later pipeline stages RE-PARSE the M64 archive (modify --insert /
# static-interpose / convert) and re-Build the layout: the rip-relative
# DESTINATION re-resolves structurally, but `c7 05` in 64-bit mode is the
# RIP-base parse path which probes no trailing immediate, so the imm32 kept the
# OLD layout's address. Any stage that shifts __DATA leaves every such store
# stale by exactly the shift — on Civ IV Steam __DATA moved +0x1000, so the
# boost.python converter-registry set header got _M_left/_M_right ==
# &_M_header - 0x1000: _M_insert_unique's `__j == begin()` leftmost guard
# failed -> _Rb_tree_decrement(&header) with parent==0 -> EXC_BAD_ACCESS at 0x4
# in rb_decrement (cxx_shim.c) during static init.
#
# The fix rewrites the abs-dest arm like every other pointer-immediate form:
#   lea r11, [rip+target]    ; pointee, re-resolved on every re-parse
#   mov [rip+dest], r11d     ; destination, likewise
# leaving NO pointer immediate in __text.
#
# This test recreates the trigger deterministically: it transforms the
# 86_selfref_imm_store fixture, FORCES a __DATA layout shift by growing the
# load-command area (repeated `modify --insert load-dylib` with long names —
# the same class of growth the real pipeline's stages cause), converts to a
# dylib, and asserts the self-referential store still targets &_hdr in the
# FINAL layout. Pre-fix this fails with the store exactly one page low.
#
# Needs the i386 sysroot (see Makefile `make sysroot`); SKIPs without.
set -u
MT="${1:?usage: selfref_imm_stage_shift_test.sh <path-to-macho-tool>}"

fail() { echo "FAIL selfref-imm-stage-shift: $1"; exit 1; }

SYSROOT=/tmp/i386-sysroot
if [ ! -d "$SYSROOT/usr/lib" ]; then
   echo "SKIP selfref-imm-stage-shift (no i386 sysroot at $SYSROOT)"
   exit 0
fi

HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/src/86_selfref_imm_store.s"
[ -f "$SRC" ] || fail "fixture $SRC missing"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# --- Build the non-PIE i386 fixture. ---
clang -arch i386 -isysroot "$SYSROOT" -mmacosx-version-min=10.6 \
   -c "$SRC" -o "$TMP/t.o" 2>/dev/null || fail "clang -arch i386"
ld -arch i386 -macos_version_min 10.6 -no_pie -syslibroot "$SYSROOT" \
   -lSystem -e _main -o "$TMP/t.i386" "$TMP/t.o" 2>/dev/null || fail "ld i386"

# --- Translate: rebasify + transform. ---
"$MT" rebasify "$TMP/t.i386" "$TMP/t.rebase" >/dev/null 2>&1 || fail rebasify
"$MT" -- transform "$TMP/t.rebase" "$TMP/t.transform" >/dev/null 2>&1 || fail transform

data_addr() { otool -l "$1" 2>/dev/null | awk '/sectname __data/{f=1} f&&/^ *addr/{print $2; exit}'; }

D0=$(data_addr "$TMP/t.transform")
[ -n "$D0" ] || fail "no __data in transform output"

# --- FORCE a __DATA page shift: grow the load-command area across re-parses.
# Each modify is a full parse->Build->Emit round trip, exactly like the real
# pipeline's post-transform stages. Long dylib names inflate LC_LOAD_DYLIB so
# __TEXT crosses a page boundary and every later segment slides up. ---
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
[ -n "$SHIFTED" ] || fail "could not force a __DATA shift ($D0 unchanged after 8 inserts) — test premise broken, fix the forcing"

# --- Final conversion (another re-parse). ---
"$MT" convert --archive DYLIB "$CUR" "$TMP/t.dylib" >/dev/null 2>&1 || fail convert

D1=$(data_addr "$TMP/t.dylib")
echo "selfref-imm-stage-shift: __data ${D0} (transform) -> ${D1} (final dylib)"

# --- Assert: the self-referential stores must target &_hdr in the FINAL layout.
python3 - "$TMP/t.dylib" <<'EOF' || exit 1
import struct, subprocess, sys

path = sys.argv[1]
data = open(path, 'rb').read()

# _hdr address from the symbol table
hdr = None
for line in subprocess.run(['nm', path], capture_output=True, text=True).stdout.splitlines():
    parts = line.split()
    if len(parts) >= 3 and parts[2] == '_hdr':
        hdr = int(parts[0], 16)
if hdr is None:
    print("FAIL selfref-imm-stage-shift: _hdr not in final symtab"); sys.exit(1)

# __TEXT,__text bounds
magic, = struct.unpack_from('<I', data, 0)
assert magic == 0xfeedfacf, hex(magic)
ncmds, = struct.unpack_from('<I', data, 16)
off = 32
text = None
for _ in range(ncmds):
    cmd, csz = struct.unpack_from('<II', data, off)
    if cmd == 0x19:
        nsects, = struct.unpack_from('<I', data, off + 64)
        soff = off + 72
        for _ in range(nsects):
            sect = data[soff:soff+16].rstrip(b'\0').decode()
            seg  = data[soff+16:soff+32].rstrip(b'\0').decode()
            addr, size = struct.unpack_from('<QQ', data, soff + 32)
            fo, = struct.unpack_from('<I', data, soff + 48)
            if seg == '__TEXT' and sect == '__text':
                text = (addr, size, fo)
            soff += 80
    off += csz
assert text, "no __text"
tvm, tsz, tfo = text
seg = data[tfo:tfo+tsz]

def scan(pat, ln):
    i = 0
    while True:
        i = seg.find(pat, i)
        if i < 0 or i + ln > len(seg):
            return
        yield i
        i += 1

bad = 0
# (1) Legacy shape: `c7 05 disp32 imm32` whose DESTINATION is inside the _hdr
#     object must carry imm32 == &_hdr (the value stored is the self-reference).
for i in scan(b'\xc7\x05', 10):
    disp, imm = struct.unpack_from('<iI', seg, i + 2)
    target = tvm + i + 10 + disp
    if hdr <= target < hdr + 16:
        if imm != hdr:
            print("FAIL selfref-imm-stage-shift: store @%#x -> [%#x] carries STALE imm %#x "
                  "(expected &_hdr=%#x, delta %#x — the Civ IV boost.python registry bug)"
                  % (tvm + i, target, imm, hdr, hdr - imm))
            bad += 1

# (2) Fixed shape: lea r11,[rip+&_hdr] (4c 8d 1d) followed by
#     mov [rip+_hdr+8/12], r11d (44 89 1d). Count correct pairs.
pairs = 0
for i in scan(b'\x4c\x8d\x1d', 7):
    disp, = struct.unpack_from('<i', seg, i + 3)
    if tvm + i + 7 + disp != hdr:
        continue
    j = i + 7
    if seg[j:j+3] == b'\x44\x89\x1d':
        disp2, = struct.unpack_from('<i', seg, j + 3)
        tgt = tvm + j + 7 + disp2
        if hdr + 8 <= tgt < hdr + 16:
            pairs += 1

if bad:
    sys.exit(1)
if pairs < 2:
    # No stale c7 05 stores and no lea+store pairs either: the fixture's
    # stores vanished — layout assumptions broke, make the test see them again.
    print("FAIL selfref-imm-stage-shift: found neither c7 05 stores into _hdr "
          "nor %d lea+store pairs (got %d) — fixture/scan mismatch" % (2, pairs))
    sys.exit(1)
print("selfref-imm-stage-shift: %d slide-correct lea+store self-references, 0 stale imm stores; OK" % pairs)
EOF
RC=$?
[ $RC -eq 0 ] || exit 1
exit 0
