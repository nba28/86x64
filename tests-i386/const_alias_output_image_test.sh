#!/bin/bash
# const_alias_output_image_test.sh — A/B guard: the M64 RE-PARSE must not
# reclassify an integer CONSTANT as a pointer just because its value aliases the
# 0x10000000-based TRANSLATED image.
#
# Every false-positive discriminator in Section::DataParser is M32-only (each
# needs the ORIGINAL i386 image's relocs/symbols/decode). The later pipeline
# stages — modify / strip-bind / static-interpose / convert — RE-PARSE the
# already-translated M64 image with the bare in-range heuristic, so a constant
# the M32 pass correctly left alone (it sat far ABOVE the little i386 image)
# reads as a fine translated address and gets "rebased" by the next layout
# shift.
#
# MEASURED: Civ IV Steam 738 reclassified constants, iPhoto 143 (13 of them
# round LSDA constants in __DATA,__gcc_except_tab), iMovie 10, Halo CE 1
# (__TEXT,__const+0x2440: 0x10080808 -> 0x10080780).
#
# ★WHY THE ARMS ARE THE RE-LAYOUT AND NOT TWO FULL PIPELINE RUNS. The defect is
# a MISCLASSIFICATION; it only becomes VISIBLE when a stage re-lays the image
# out, because a word wrongly bound to a blob is then re-emitted at that blob's
# NEW address. A fixture this small does not shift on its own (the 1 KB
# headerpad absorbs the pipeline's own load-command growth), so the guard
# translates once to produce the base image (and its __86x64_cpin provenance
# table), then drives the SAME bytes through the re-parsing stage TWICE, one env
# var apart, forcing a deterministic re-layout with MACHO_HEADERPAD. That is
# exactly the stage the defect lives in.
#
# ON  : every aliasing constant survives the re-layout unchanged.
# OFF : M64_NO_CONST_PIN=1 — constants are rebased (the defect reproduces).
# CONTROL A: a value above ANY image is intact in every arm, so a green ON
#            cannot mean "the re-layout changed nothing".
# CONTROL B: a GENUINE pointer is relocated by the re-layout in BOTH arms and to
#            the SAME address — the pin removes only false positives and never
#            costs a real pointer its tracking.
set -u
cd "$(dirname "$0")"
ROOT=..
BUILD=build
I386=$BUILD/100_const_alias_output_image.i386
MT=$ROOT/build/src/macho-tool/macho-tool

if [ ! -x "$MT" ]; then echo "const-alias-output-image: SKIP (no macho-tool)"; exit 0; fi
if [ ! -f "$I386" ]; then echo "const-alias-output-image: SKIP (no $I386 — sysroot missing?)"; exit 0; fi

BASE=$BUILD/100_const_alias.base.dylib
rm -f "$BASE" "$BUILD/100_const_alias.on.dylib" "$BUILD/100_const_alias.off.dylib"

bash $ROOT/src/86x64/86x64.sh -m "$MT" \
    -l $ROOT/build/src/abiconv/libabiconv.dylib \
    -w $ROOT/build/src/86x64/libwrapper.a \
    -i $ROOT/build/src/86x64/libinterpose.dylib \
    -o "$BUILD/100_const_alias.base.x86_64" "$I386" >/dev/null 2>&1
cp "$BUILD/100_const_alias.base.x86_64.dylib" "$BASE" 2>/dev/null
if [ ! -f "$BASE" ]; then
  echo "const-alias-output-image: SKIP (translation produced no dylib)"; exit 0
fi

# The provenance table must actually be there, or the ON arm would be green for
# the wrong reason (nothing to disarm).
if ! otool -l "$BASE" 2>/dev/null | grep -q "__86x64_cpin"; then
  echo "  __DATA,__86x64_cpin: ABSENT — the M32 verdicts were never recorded"
  echo "const-alias-output-image: FAIL"; exit 1
fi
echo "  base image carries __DATA,__86x64_cpin (M32 constant verdicts)"

# Re-parse + re-layout, twice, one env var apart. The bigger headerpad pushes
# every section down, so anything bound to a blob moves with it.
for arm in on off; do
  ENVV=""; [ "$arm" = off ] && ENVV="M64_NO_CONST_PIN=1"
  env $ENVV MACHO_HEADERPAD=8192 "$MT" modify \
      --insert load-dylib,name=/usr/lib/libSystem.B.dylib \
      "$BASE" "$BUILD/100_const_alias.$arm.dylib" >/dev/null 2>&1
done

python3 - "$BASE" "$BUILD/100_const_alias.on.dylib" "$BUILD/100_const_alias.off.dylib" <<'PY'
import struct, sys

SWEEP_N = 1025            # 1024-entry sweep + Halo's own 0x10080808
BRACKETS = {
    'sweep': (0x4E495043, 0x4350494E, SWEEP_N),
    'ctlh':  (0x484C5443, 0x4354484C, 2),
    'ctlp':  (0x504C5443, 0x4354504C, 1),
}

def table(path, which):
    head, tail, n = BRACKETS[which]
    d = open(path, 'rb').read()
    m = struct.pack('<I', head)
    i = d.find(m)
    while i >= 0:
        if len(d) >= i + 4 + 4*n + 4 and \
           struct.unpack_from('<I', d, i + 4 + 4*n)[0] == tail:
            return list(struct.unpack_from('<%dI' % n, d, i + 4))
        i = d.find(m, i + 1)
    return None

base, on, off = sys.argv[1], sys.argv[2], sys.argv[3]
fail = 0

t = {}
for tag, p in (('base', base), ('on', on), ('off', off)):
    for w in BRACKETS:
        t[(tag, w)] = table(p, w)
        if t[(tag, w)] is None:
            print("  %-18s %s table MISSING from %s" % ('LOCATE:', w, tag))
            fail = 1
if fail:
    print("const-alias-output-image: FAIL"); sys.exit(1)

b, o, f = t[('base','sweep')], t[('on','sweep')], t[('off','sweep')]
d_on  = [i for i in range(SWEEP_N) if o[i] != b[i]]
d_off = [i for i in range(SWEEP_N) if f[i] != b[i]]

if not d_on:
    print("  ON  (gate armed):   all %d aliasing constants preserved across the"
          " re-layout   OK" % SWEEP_N)
else:
    print("  ON  (gate armed):   %d of %d constants were REBASED despite the pin"
          % (len(d_on), SWEEP_N))
    for i in d_on[:5]:
        print("                      idx %4d  %08X -> %08X" % (i, b[i], o[i]))
    fail = 1

if d_off:
    print("  OFF (kill switch):  %d of %d constants rebased — the defect"
          " reproduces         OK" % (len(d_off), SWEEP_N))
    for i in d_off[:3]:
        print("                      idx %4d  %08X -> %08X" % (i, b[i], f[i]))
else:
    print("  OFF (kill switch):  expected constants to be rebased; none were.")
    print("                      The guard is not exercising the gate.")
    fail = 1

hb, ho, hf = t[('base','ctlh')], t[('on','ctlh')], t[('off','ctlh')]
if hb == ho == hf == [0x7F123456, 0x7EFEFEFE]:
    print("  CONTROL A:          out-of-range constants intact in BOTH arms"
          "                OK")
else:
    print("  CONTROL A:          expected 7F123456/7EFEFEFE everywhere; "
          "base=%s on=%s off=%s" % (hb, ho, hf))
    fail = 1

pb, po, pf = t[('base','ctlp')][0], t[('on','ctlp')][0], t[('off','ctlp')][0]
if po != pb and pf != pb and po == pf:
    print("  CONTROL B:          the GENUINE pointer relocated %08X -> %08X in"
          " both arms  OK" % (pb, po))
else:
    print("  CONTROL B:          a real pointer must be relocated identically in"
          " both arms;")
    print("                      base=%08X on=%08X off=%08X" % (pb, po, pf))
    fail = 1

print("const-alias-output-image: %s" % ("PASS" if fail == 0 else "FAIL"))
sys.exit(fail)
PY
exit $?
