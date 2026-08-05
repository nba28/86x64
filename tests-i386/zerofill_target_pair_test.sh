#!/bin/bash
# zerofill_target_pair_test.sh — A/B guard: the translator must not "rebase" two
# adjacent u16 fields whose bytes happen to spell an address into ZERO-FILL
# memory (__bss/__common).
#
# Halo CE died on exactly this every run (task #25): tag-class descriptor records
# carry u16 fields at +0xa/+0xc/+0xe, and in the 'scen'/'lifi' records the PAIR at
# +0xc spells 0x0048021C / 0x005802D0 — both inside __DATA,__common — so both were
# slid: (540,72) -> (27356,4272), (720,88) -> (27536,4288).
#
# The poisonous value must EQUAL a link-time address, so it cannot be a C
# constant. This script patches it into the built i386 binary (pointing at the
# INTERIOR of g_zf — a symbol sits at its start, and an exact symbol hit is
# legitimately accepted as evidence), then translates the SAME bytes twice, one
# env var apart.
#
# ON  : the pair survives  == the i386 value.
# OFF : M64_NO_ZEROFILL_TARGET_GATE=1 — the pair is mangled (defect reproduces).
# The CONTROL record (pair 0x00780234, above any image) must be intact in BOTH,
# so a green ON cannot just mean "the translator changed nothing".
set -u
cd "$(dirname "$0")"
ROOT=..
BUILD=build
SRC=src/99_zerofill_target_pair.c
I386=$BUILD/99_zerofill_target_pair.i386
MT=$ROOT/build/src/macho-tool/macho-tool

if [ ! -x "$MT" ]; then echo "zerofill-target-pair: SKIP (no macho-tool)"; exit 0; fi
if [ ! -f "$I386" ]; then echo "zerofill-target-pair: SKIP (no $I386 — sysroot missing?)"; exit 0; fi

# ---- patch the pair to spell an address inside g_zf's interior --------------
patched=$(python3 - "$I386" <<'PY'
import sys, struct, subprocess, re
p = sys.argv[1]
d = bytearray(open(p, 'rb').read())
# locate the ZERO-FILL section from the section table (no symbol needed: the
# fixture links with -x precisely so none exists there)
zf = None
out = subprocess.run(["otool", "-arch", "i386", "-l", p], capture_output=True, text=True).stdout
sec = {}
for line in out.splitlines():
    t = line.strip()
    if t.startswith("sectname"): sec = {"name": t.split()[1]}
    elif t.startswith("addr ") and sec: sec["addr"] = int(t.split()[1], 16)
    elif t.startswith("size ") and sec: sec["size"] = int(t.split()[1], 16)
    elif t.startswith("flags 0x") and sec.get("addr") is not None:
        if (int(t.split()[1], 16) & 0xff) in (1, 12) and sec.get("size", 0) > 0x2000:
            zf = sec["addr"]; break
        sec = {}
if zf is None:
    print("NOZF"); raise SystemExit
target = (zf + 0x1234) & ~3           # inside the zero-fill span, 4-byte aligned
magic = struct.pack("<I", 0x5A46504B)  # 'ZFPK' as stored little-endian
i = d.find(magic)
if i < 0:
    print("NOREC"); raise SystemExit
# record layout: magic is at +4, so the pair at +0xc is magic_off + 8
struct.pack_into("<I", d, i + 8, target)
open(p, 'wb').write(d)
print("0x%08X" % target)
PY
)
case "$patched" in
  NOZF)  echo "zerofill-target-pair: SKIP (no sizeable zero-fill section)"; exit 0;;
  NOREC) echo "zerofill-target-pair: SKIP (record magic not found)"; exit 0;;
esac
echo "  patched pair -> $patched (symbol-free zero-fill space)"

read_pair() {  # $1 = dylib, $2 = magic hex ; prints "lo hi"
  python3 - "$1" "$2" <<'PY'
import sys, struct
d = open(sys.argv[1], 'rb').read()
i = d.find(struct.pack("<I", int(sys.argv[2], 16)))
if i < 0: print("MISSING"); raise SystemExit
lo, hi = struct.unpack_from("<HH", d, i + 8)
print("%d %d" % (lo, hi))
PY
}

fail=0
for arm in on off; do
  rm -f "$BUILD/99_zerofill_target_pair.$arm.dylib"
  ENVV=""; [ "$arm" = off ] && ENVV="M64_NO_ZEROFILL_TARGET_GATE=1"
  env $ENVV bash $ROOT/src/86x64/86x64.sh -m "$MT" \
      -l $ROOT/build/src/abiconv/libabiconv.dylib \
      -w $ROOT/build/src/86x64/libwrapper.a \
      -i $ROOT/build/src/86x64/libinterpose.dylib \
      -o "$BUILD/99_zerofill_target_pair.$arm.x86_64" "$I386" >/dev/null 2>&1
  cp "$BUILD/99_zerofill_target_pair.$arm.x86_64.dylib" \
     "$BUILD/99_zerofill_target_pair.$arm.dylib" 2>/dev/null
done

exp_lo=$(( $(printf '%d' "$patched") & 0xFFFF ))
exp_hi=$(( ($(printf '%d' "$patched") >> 16) & 0xFFFF ))

on=$(read_pair "$BUILD/99_zerofill_target_pair.on.dylib" 0x5A46504B 2>/dev/null)
off=$(read_pair "$BUILD/99_zerofill_target_pair.off.dylib" 0x5A46504B 2>/dev/null)
ctl_on=$(read_pair "$BUILD/99_zerofill_target_pair.on.dylib" 0x4C54434B 2>/dev/null)
ctl_off=$(read_pair "$BUILD/99_zerofill_target_pair.off.dylib" 0x4C54434B 2>/dev/null)

if [ "$on" = "$exp_lo $exp_hi" ]; then
  echo "  ON  (gate armed):   u16 pair preserved as ($exp_lo, $exp_hi)              OK"
else
  echo "  ON  (gate armed):   expected ($exp_lo, $exp_hi), got ($on) — the pair was"
  echo "                      relocated despite the gate"; fail=1
fi
if [ -n "$off" ] && [ "$off" != "$exp_lo $exp_hi" ] && [ "$off" != "MISSING" ]; then
  echo "  OFF (kill switch):  pair mangled to ($off) — the defect reproduces      OK"
else
  echo "  OFF (kill switch):  expected the pair to be mangled; got ($off)."
  echo "                      The guard is not exercising the gate."; fail=1
fi
if [ "$ctl_on" = "564 120" ] && [ "$ctl_off" = "564 120" ]; then
  echo "  CONTROL:            out-of-range pair (564,120) intact in BOTH arms      OK"
else
  echo "  CONTROL:            expected (564,120) in both arms; on=($ctl_on) off=($ctl_off)"; fail=1
fi

[ "$fail" = 0 ] && echo "zerofill-target-pair: PASS" || echo "zerofill-target-pair: FAIL"
exit $fail
