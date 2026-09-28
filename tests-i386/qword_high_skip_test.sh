#!/bin/bash
#
# qword_high_skip_test.sh — A/B guard for the QWORD-HIGH-HALF skip in the
# JUMP-TABLE COHESION gate (ParseEnv::code_alias_run_contradicted,
# src/core/parse.cc). Kill switch M64_NO_QWORD_HIGH_SKIP=1.
#
# THE BLIND SPOT. An M32 image has no 8-byte pointers, so a const array of 64-bit
# INTEGERS is 8-aligned {lo, hi} pairs whose small entries put a ZERO in every
# odd word. The cohesion walk treated that zero as an OBJECT EDGE and stopped on
# it, so each low half was judged with block == 1 and interior_edge == false and
# the proven mid-instruction siblings two words away were invisible. In
# __TEXT,__const the func-entry and code-entry gates are deliberately disarmed,
# and the record-field gate is vetoed as soon as two entries of the same column
# alias code at a boundary — each declassifies the other. So any entry landing on
# an exact instruction BOUNDARY was rebased into a translated code address.
#
# ★MEASURED (Quinn, 2026-09-23): the 32-entry u64 knapsack vector at
# __TEXT,__const 0xb2c60 that obfuscates the saved highscore file. Entries 7
# (0x29a5) and 9 (0xbd3c) are exact instruction boundaries and became
# 0x10001ee7 / 0x10011d2a. 63 of the 102 stored 8-byte records then decrypted to
# garbage, the NSArchiver stream came back with a mangled "streamtyped"
# signature, and every highscore was lost on restart.
#
# ★THREE ARMS:
#   on        shipped config              -> subjects preserved, table untouched
#   off       M64_NO_QWORD_HIGH_SKIP=1    -> subject A REBASED (defect reproduced; B see below)
#   nocohesion M64_NO_JT_COHESION=1       -> subject A REBASED too. Proves the OFF
#             arm is not inert for some unrelated reason: the whole gate being
#             disabled must look exactly like this one skip being disabled.
#
# ★THE GATE RUNS AT TRANSLATE TIME, so every arm must TRANSLATE, not merely run.
# ★The positive control (a 12-entry stride-4 jump table, no zero high halves) is
#  asserted STATICALLY from the translated bytes: its entries are alias targets,
#  not real basic blocks, so it must never be executed. It must stay REBASED in
#  every arm — a "fix" that ignored every zero would demote real switch dispatch.
# ★The ON arm runs LAST.
set -u
cd "$(dirname "$0")"
ROOT=$(cd .. && pwd)

MACHO_TOOL="$ROOT/build/src/macho-tool/macho-tool"
LIBABICONV="$ROOT/build/src/abiconv/libabiconv.dylib"
LIBWRAPPER="$ROOT/build/src/86x64/libwrapper.a"
LIBINTERPOSE="$ROOT/build/src/86x64/libinterpose.dylib"
PIPELINE="$ROOT/src/86x64/86x64.sh"

I386=build/99_qword_high_skip.i386
if [ ! -f "$I386" ]; then
  echo "qword-high-skip: FAIL (build/99_qword_high_skip.i386 missing — run \`make qword-high-skip\`)"
  echo "  ⚠A MISSING FIXTURE IS A FAILURE, NOT A SKIP."
  exit 1
fi

fail=0

# --- precondition: the constants really do alias code as claimed -------------
dis=$(otool -tV "$I386" 2>/dev/null)
is_boundary() { printf '%s\n' "$dis" | awk -v a="$1" '$1==a{print "yes"; exit}'; }
pre_ok=1
for a in 00008002 00008004 00009002 00009018; do          # must be BOUNDARIES
  [ "$(is_boundary $a)" = "yes" ] || { echo "  precondition: 0x$a is NOT a boundary"; pre_ok=0; }
done
for a in 00008001 00008003 00008005 00008007; do          # must be INTERIOR
  [ -z "$(is_boundary $a)" ] || { echo "  precondition: 0x$a IS a boundary, want interior"; pre_ok=0; }
done
# the subject column must really be 8-ALIGNED with ZERO high halves, or the skip
# it guards is not the thing under test.
align_ok=$(python3 - <<'PY'
import struct,subprocess,re
def sect(f,seg,sec):
    out=subprocess.run(['otool','-l',f],capture_output=True,text=True).stdout; cur={}
    for L in out.splitlines():
        L=L.strip(); m=re.match(r'(sectname|segname|addr|size|offset) (.+)',L)
        if m:
            cur[m.group(1)]=m.group(2).strip()
            if m.group(1)=='offset' and cur.get('segname')==seg and cur.get('sectname')==sec:
                return int(cur['addr'],16),int(cur['size'],16),int(cur['offset'])
ad,sz,of=sect('build/99_qword_high_skip.i386','__TEXT','__const')
w=struct.unpack_from('<%dI'%(sz//4),open('build/99_qword_high_skip.i386','rb').read(),of)
try: b=w.index(0x7f7f7f7f)            # kQwords[0]
except ValueError: print("no"); raise SystemExit
if (ad+4*b) % 8: print("no"); raise SystemExit
ok = (w[b+4]==0x8002 and w[b+8]==0x8004 and
      w[b+5]==0 and w[b+9]==0 and w[b+13]==0)
print("yes" if ok else "no")
PY
)
[ "$align_ok" = yes ] || { echo "  precondition: the u64 column is not 8-aligned / not {lo,0} pairs"; pre_ok=0; }
if [ "$pre_ok" = 1 ]; then
  echo "  precondition:  subjects are BOUNDARIES in an 8-aligned {lo,0} column,"
  echo "                 their terminators are MID-instruction, table entries"
  echo "                 are BOUNDARIES                                        OK"
else
  echo "  precondition:  FAILED — the fixture layout moved. Re-derive it; do NOT"
  echo "                 trust the arms below."
  fail=1
fi

translate() { local a=$1; shift
  rm -f "build/99_qhs.$a.x86_64" "build/99_qhs.$a.x86_64.dylib"
  env "$@" bash "$PIPELINE" -m "$MACHO_TOOL" -l "$LIBABICONV" -w "$LIBWRAPPER" \
      -i "$LIBINTERPOSE" -o "build/99_qhs.$a.x86_64" "$I386" >/dev/null 2>&1
  chmod +x "build/99_qhs.$a.x86_64" 2>/dev/null
}
translate off        M64_NO_QWORD_HIGH_SKIP=1
translate nocohesion M64_NO_JT_COHESION=1
translate on                                   # ★ON arm last

for a in off nocohesion on; do
  [ -x "build/99_qhs.$a.x86_64" ] || { echo "  $a arm: translation FAILED"; fail=1; }
done
[ "$fail" = 0 ] || { echo "qword-high-skip: FAIL"; exit 1; }

# report <arm> -> "<subjects preserved 0-2> <table still-rebased 0-12>"
report() {
  python3 - "$1" <<'PY'
import struct,subprocess,re,sys
def sect(f):
    out=subprocess.run(['otool','-l',f],capture_output=True,text=True).stdout; cur={}
    for L in out.splitlines():
        L=L.strip(); m=re.match(r'(sectname|segname|addr|size|offset) (.+)',L)
        if m:
            cur[m.group(1)]=m.group(2).strip()
            if m.group(1)=='offset' and cur.get('segname')=='__TEXT' and cur.get('sectname')=='__const':
                return int(cur['size'],16),int(cur['offset'])
f0='build/99_qword_high_skip.i386'
sz,of=sect(f0); n=sz//4
w=struct.unpack_from('<%dI'%n,open(f0,'rb').read(),of)
f='build/99_qhs.%s.x86_64.dylib'%sys.argv[1]
s2,o2=sect(f); w2=struct.unpack_from('<%dI'%n,open(f,'rb').read(),o2)
b=w.index(0x7f7f7f7f)                      # kQwords[0]
t=w.index(0x00009002)                       # kTable[2]
print(sum(1 for k in (b+4,b+8) if w2[k]==w[k]),
      sum(1 for k in range(t,t+12) if w2[k]!=w[k]))
PY
}

for arm in off nocohesion on; do
  perl -e 'alarm 20; exec @ARGV' "./build/99_qhs.$arm.x86_64" >/dev/null 2>&1
  eval "rc_$arm=$?"
  eval "st_$arm=\"\$(report $arm)\""
done

check() {   # <arm> <want_rc> <want_subject> <want_table> <description>
  local arm=$1 wrc=$2 wsub=$3 wtb=$4 desc=$5
  local rc sub tb; eval "rc=\$rc_$arm"; eval "read sub tb <<< \"\$st_$arm\""
  if [ "$rc" = "$wrc" ] && [ "$sub" = "$wsub" ] && [ "$tb" = "$wtb" ]; then
    printf '  %-11s %s  OK\n' "$arm" "$desc"
  else
    printf '  %-11s %s\n' "$arm" "$desc"
    printf '              got rc=%s subject=%s/2 table_rebased=%s/12, want rc=%s %s/2 %s/12\n' \
           "$rc" "$sub" "$tb" "$wrc" "$wsub" "$wtb"
    fail=1
  fi
}

# Subject B stays preserved in the OFF arms: it has a full stride-8 sibling set
# (4 proven-interior words vs 1 boundary), so the record-field gate's majority
# rule (PvZ lenfix, M64_RECFIELD_STRICT_VETO) also protects it. Subject A is
# the first record of __TEXT,__const, has no full sibling set, and only the
# qword-high skip protects it — that is the defect the OFF arms reproduce.
check off        1 1 12 "skip disarmed -> subject A REBASED (defect reproduced)"
check nocohesion 1 1 12 "whole gate disarmed -> same corruption (OFF arm is not inert)"
check on        42 2 12 "u64 low halves preserved, jump table left alone"

if [ "$fail" = 0 ]; then echo "qword-high-skip: PASS"; else echo "qword-high-skip: FAIL"; fi
exit $fail
