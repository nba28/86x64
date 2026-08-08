#!/bin/bash
# record_field_pair_test.sh — A/B guard: the translator must not "rebase" a u16
# pair held in an INTEGER FIELD of a record array when the address those bytes
# spell is MAPPED.
#
# Sister of zerofill_target_pair_test.sh. There the target is __bss/__common and
# the gate can argue from the absence of an anchoring symbol. Here the target is
# real mapped memory, where that argument does not exist: a __DATA,__data target
# had no discriminator at all, and a __TEXT,__text target held in a __TEXT,__const
# slot escapes the code-entry gates, which are deliberately disarmed there so that
# switch jump tables keep relocating.
#
# Halo CE task #35: an 8-byte record array whose recs 1 and 5 hold the pairs
# 0x00030002 and 0x00380000 had them slid to 0x1003ac81 / 0x104a2000, so a u16
# index became the LOW HALF of a relocated pointer — (0,12,2,3) -> (0,12,44161,4099).
#
# The poisonous value must EQUAL a link-time address, so it cannot be a C
# constant. This script patches it into the built i386 binary, then translates the
# SAME bytes twice, one env var apart.
#
#   ON  : the candidate survives == the patched i386 value.
#   OFF : M64_NO_RECORD_FIELD_GATE=1 — it is relocated (the defect reproduces).
#
# ★Because this gate's evidence is POSITIONAL it is easy to make it over-fire,
# and two measured over-fires cost real pointers, so there are three controls
# that must hold in BOTH arms:
#   JUMPTABLE  a dense run of code addresses (a switch table) must STILL relocate
#   PTRRECORD  Quinn's 32-byte record {ptr,0,1,float,float,float,float,magic}
#              must STILL relocate its pointer field
#   INERT      a pair above any image must stay byte-identical
set -u
cd "$(dirname "$0")"
ROOT=..
BUILD=build
I386=$BUILD/99_record_field_pair.i386
MT=$ROOT/build/src/macho-tool/macho-tool

if [ ! -x "$MT" ]; then echo "record-field-pair: SKIP (no macho-tool)"; exit 0; fi
if [ ! -f "$I386" ]; then echo "record-field-pair: SKIP (no $I386 — sysroot missing?)"; exit 0; fi

# ---- patch the fixture ------------------------------------------------------
# siblings  -> symbol-free zero-fill addresses (zerofill_target_unattested
#              declassifies them, and they ARE address-shaped, which is what the
#              gate demands as evidence)
# candidate -> a mid-function __text instruction BOUNDARY with no symbol and no
#              `55 89 e5` prologue, i.e. exactly what code_interior_alias passes
#              and code_target_has_entry_evidence rejects
# g_jt      -> real instruction boundaries in a dense run (a switch table)
patched=$(python3 - "$I386" <<'PY'
import sys, struct, subprocess
p = sys.argv[1]
d = bytearray(open(p, 'rb').read())

# ---- section table ----------------------------------------------------------
out = subprocess.run(["otool", "-arch", "i386", "-l", p],
                     capture_output=True, text=True).stdout
secs, cur = [], {}
for line in out.splitlines():
    t = line.strip()
    if t.startswith("sectname"):
        cur = {"sect": t.split()[1]}
    elif t.startswith("segname") and cur:
        cur["seg"] = t.split()[1]
    elif t.startswith("addr ") and cur:
        cur["addr"] = int(t.split()[1], 16)
    elif t.startswith("size ") and cur:
        cur["size"] = int(t.split()[1], 16)
    elif t.startswith("offset ") and cur:
        cur["off"] = int(t.split()[1])
    elif t.startswith("flags 0x") and cur.get("addr") is not None:
        cur["flags"] = int(t.split()[1], 16)
        secs.append(cur); cur = {}

def find(seg, sect):
    for s in secs:
        if s.get("seg") == seg and s.get("sect") == sect:
            return s
    return None

zf = None
for s in secs:
    if (s.get("flags", 0) & 0xff) in (1, 12) and s.get("size", 0) > 0x2000:
        zf = s; break
if zf is None:
    print("NOZF"); raise SystemExit

tconst = find("__TEXT", "__const")
if tconst is None:
    print("NOTCONST"); raise SystemExit

# ---- the POISON array must live in __TEXT,__const ---------------------------
magic = struct.pack("<I", 0x5246504B)
i = d.find(magic)
if i < 0:
    print("NOREC"); raise SystemExit
va = tconst["addr"] + (i - tconst["off"])
if not (tconst["off"] <= i < tconst["off"] + tconst["size"]):
    print("NOTINCONST"); raise SystemExit

# ---- pick a mid-function __text instruction boundary ------------------------
# `otool -tv` sweeps linearly from the section start, exactly as our xed sweep
# does, so its instruction addresses are the boundaries the translator sees.
dis = subprocess.run(["otool", "-arch", "i386", "-tvV", p],
                     capture_output=True, text=True).stdout
addrs = []
for line in dis.splitlines():
    tok = line.split()
    if not tok:
        continue
    h = tok[0].rstrip(":")
    if len(h) >= 6 and all(c in "0123456789abcdefABCDEF" for c in h):
        try:
            addrs.append(int(h, 16))
        except ValueError:
            pass
text = find("__TEXT", "__text")
if text is None or len(addrs) < 40:
    print("NOTEXT"); raise SystemExit

# symbols, so we never pick an address that carries one (that would be positive
# entry evidence and the gate would correctly refuse to demote it)
nm = subprocess.run(["nm", "-n", "-arch", "i386", p],
                    capture_output=True, text=True).stdout
symaddrs = set()
for line in nm.splitlines():
    tok = line.split()
    if len(tok) >= 3:
        try:
            symaddrs.add(int(tok[0], 16))
        except ValueError:
            pass

def usable(a):
    if a in symaddrs or a < 0x1000:
        return False
    if not (text["addr"] <= a < text["addr"] + text["size"]):
        return False
    fo = text["off"] + (a - text["addr"])
    if fo + 12 > len(d):
        return False
    if d[fo] == 0x55 and d[fo+1] == 0x89 and d[fo+2] == 0xe5:
        return False                      # frame-setup prologue = entry evidence
    if d[fo] in (0x83, 0x81) and d[fo+1] in (0x44, 0x6c) and d[fo+2] == 0x24:
        j = fo + (5 if d[fo] == 0x83 else 8)
        if j < len(d) and d[j] in (0xe9, 0xeb):
            return False                  # C++ adjustor thunk = entry evidence
    return True

cands = [a for a in addrs if usable(a)]
if len(cands) < 10:
    print("NOCAND"); raise SystemExit
cand = cands[len(cands) // 2]

# ---- write the POISON array -------------------------------------------------
# record r's field is at word index 2r+1
zfbase = zf["addr"]
for r in range(8):
    off = i + (2 * r + 1) * 4
    if r == 3:
        struct.pack_into("<I", d, off, cand)                    # CANDIDATE
    else:
        struct.pack_into("<I", d, off, (zfbase + 0x1000 + r * 0x40) & ~3)

# ---- write the JUMP-TABLE control ------------------------------------------
# Layout (word indices from the array start): 0..47 filler, 48 magic,
# 49..64 the jump table, 65..112 filler. The filler becomes symbol-free
# zero-fill addresses so that a gate which goes fishing across strides finds a
# clean stride-64 "sibling set" and demotes a real code pointer.
JT_FILL, JT_N = 48, 16
jm = d.find(struct.pack("<I", 0x4A544B4C))
if jm < 0:
    print("NOJT"); raise SystemExit
run = [a for a in cands if a != cand][:JT_N]
if len(run) < JT_N:
    print("NOJTRUN"); raise SystemExit
arr = jm - JT_FILL * 4
for k in range(JT_FILL + 1 + JT_N + JT_FILL):
    if k == JT_FILL:
        continue                                   # the locator magic
    off = arr + k * 4
    if JT_FILL < k <= JT_FILL + JT_N:
        struct.pack_into("<I", d, off, run[k - JT_FILL - 1])
    else:
        struct.pack_into("<I", d, off, (zfbase + 0x20000 + k * 0x40) & ~3)

# the witness entry: word 57 = jump-table index 8, the one whose stride-8
# siblings are all jump-table entries and whose stride-64 siblings are all filler
jt_witness = run[8]

open(p, 'wb').write(d)
print("0x%08X 0x%08X %s" % (cand, jt_witness, hex(va)))
PY
)
case "$patched" in
  NOZF)        echo "record-field-pair: SKIP (no sizeable zero-fill section)"; exit 0;;
  NOTCONST)    echo "record-field-pair: SKIP (no __TEXT,__const)"; exit 0;;
  NOREC)       echo "record-field-pair: SKIP (record magic not found)"; exit 0;;
  NOTINCONST)  echo "record-field-pair: SKIP (poison array is not in __TEXT,__const)"; exit 0;;
  NOTEXT|NOCAND|NOJT|NOJTRUN)
               echo "record-field-pair: SKIP (no usable __text boundary: $patched)"; exit 0;;
esac
set -- $patched
CAND=$1; JT0=$2
echo "  candidate -> $CAND (mid-function __text boundary, no symbol/prologue)"

read_word() {  # $1 = dylib, $2 = magic hex, $3 = word index from the magic
  python3 - "$1" "$2" "$3" <<'PY'
import sys, struct
d = open(sys.argv[1], 'rb').read()
i = d.find(struct.pack("<I", int(sys.argv[2], 16)))
if i < 0:
    print("MISSING"); raise SystemExit
print("0x%08X" % struct.unpack_from("<I", d, i + int(sys.argv[3]) * 4)[0])
PY
}

read_qptr() {  # $1 = dylib ; prints the pointer field of ALL FOUR qrecs.
  # ★All four, not just the first: records 1 and 2 are the ones whose stride-8
  # siblings lie ENTIRELY inside the array (its own 0/1/float/magic fields),
  # which is the exact divisor-stride shape that cost Quinn 7 pointers. Record 0
  # borrows its negative siblings from whatever precedes the array, so on its own
  # it is not a reliable witness.
  python3 - "$1" <<'PY'
import sys, struct
d = open(sys.argv[1], 'rb').read()
m = struct.pack("<I", 0x51435452)
i = d.find(m)
if i < 0 or i < 0x1c:
    print("MISSING"); raise SystemExit
rec0 = i - 0x1c
out = []
for k in range(4):
    out.append("0x%08X" % struct.unpack_from("<I", d, rec0 + k * 0x20)[0])
print(" ".join(out))
PY
}

fail=0
for arm in on off; do
  rm -f "$BUILD/99_record_field_pair.$arm.dylib"
  ENVV=""; [ "$arm" = off ] && ENVV="M64_NO_RECORD_FIELD_GATE=1"
  env $ENVV bash $ROOT/src/86x64/86x64.sh -m "$MT" \
      -l $ROOT/build/src/abiconv/libabiconv.dylib \
      -w $ROOT/build/src/86x64/libwrapper.a \
      -i $ROOT/build/src/86x64/libinterpose.dylib \
      -o "$BUILD/99_record_field_pair.$arm.x86_64" "$I386" >/dev/null 2>&1
  cp "$BUILD/99_record_field_pair.$arm.x86_64.dylib" \
     "$BUILD/99_record_field_pair.$arm.dylib" 2>/dev/null
done

ON=$BUILD/99_record_field_pair.on.dylib
OFF=$BUILD/99_record_field_pair.off.dylib

cand_on=$(read_word  "$ON"  0x5246504B 7 2>/dev/null)
cand_off=$(read_word "$OFF" 0x5246504B 7 2>/dev/null)
jt_on=$(read_word    "$ON"  0x4A544B4C 9 2>/dev/null)
jt_off=$(read_word   "$OFF" 0x4A544B4C 9 2>/dev/null)
qp_on=$(read_qptr    "$ON"  2>/dev/null)
qp_off=$(read_qptr   "$OFF" 2>/dev/null)
in_on=$(read_word    "$ON"  0x494E5254 1 2>/dev/null)
in_off=$(read_word   "$OFF" 0x494E5254 1 2>/dev/null)

if [ "$cand_on" = "$CAND" ]; then
  echo "  ON  (gate armed):   record field preserved as $CAND                OK"
else
  echo "  ON  (gate armed):   expected $CAND, got $cand_on — the field was"
  echo "                      relocated despite the gate"; fail=1
fi
if [ -n "$cand_off" ] && [ "$cand_off" != "$CAND" ] && [ "$cand_off" != "MISSING" ]; then
  echo "  OFF (kill switch):  field relocated to $cand_off — defect reproduces  OK"
else
  echo "  OFF (kill switch):  expected the field to be relocated; got $cand_off."
  echo "                      The guard is not exercising the gate."; fail=1
fi

# CONTROL 1 — a switch jump table must STILL relocate, in BOTH arms.
if [ "$jt_on" != "MISSING" ] && [ "$jt_on" != "$JT0" ] && [ "$jt_on" = "$jt_off" ]; then
  echo "  CONTROL jumptable:  code pointer relocated $JT0 -> $jt_on in BOTH     OK"
else
  echo "  CONTROL jumptable:  expected $JT0 to relocate identically in both arms;"
  echo "                      on=$jt_on off=$jt_off — the gate is demoting real"
  echo "                      code pointers (this is the 7889-word Halo regression)"; fail=1
fi

# CONTROL 2 — Quinn's 32-byte pointer records must ALL STILL relocate, in BOTH arms.
qp_ok=1
[ "$qp_on" = "MISSING" ] || [ "$qp_on" != "$qp_off" ] && qp_ok=0
for w in $qp_on; do
  [ "$w" = "MISSING" ] && { qp_ok=0; break; }
  [ "$(printf '%d' "$w")" -ge 268435456 ] || qp_ok=0
done
if [ "$qp_ok" = 1 ]; then
  echo "  CONTROL ptrrecord:  all 4 struct pointer fields relocated in BOTH arms  OK"
else
  echo "  CONTROL ptrrecord:  expected all four {ptr,0,1,float...} pointers to"
  echo "                      relocate in both arms; on=($qp_on) off=($qp_off) —"
  echo "                      a divisor stride is sampling sibling FIELDS, not"
  echo "                      sibling RECORDS (the Quinn regression)"; fail=1
fi

# CONTROL 3 — a pair above any image is inert in BOTH arms.
if [ "$in_on" = "0x7F123456" ] && [ "$in_off" = "0x7F123456" ]; then
  echo "  CONTROL inert:      out-of-range pair 0x7F123456 intact in BOTH arms  OK"
else
  echo "  CONTROL inert:      expected 0x7F123456 in both arms; on=$in_on off=$in_off"; fail=1
fi

[ "$fail" = 0 ] && echo "record-field-pair: PASS" || echo "record-field-pair: FAIL"
exit $fail
