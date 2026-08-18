#!/bin/bash
#
# jt_cohesion_test.sh — A/B guard for the JUMP-TABLE COHESION gate
# (ParseEnv::code_alias_run_contradicted, src/core/parse.cc).
#
# THE BLIND SPOT. For __TEXT-resident data the func-entry gate
# (code_alias_is_constant) and the code-entry gate
# (code_alias_lacks_entry_evidence) are DELIBERATELY disarmed, because switch
# jump tables live in __TEXT,__const and target MID-function basic-block heads
# with neither an nlist nor a `55 89 e5` prologue. That leaves only
# code_interior_alias, which fires only for a value strictly INSIDE a decoded
# instruction — so an INTEGER aliasing an exact instruction BOUNDARY passes
# every gate and is silently rebased. Measured twice on Halo CE: the audio
# sample-rate table (aa203f3) and the vertex-STRIDE table (6acb4ef).
#
# ★FOUR ARMS, because this gate can fail in BOTH directions and three of the
# arms exist to prove the fourth is not a fluke:
#   on       shipped config          -> subject preserved, jump table untouched
#   off      M64_NO_JT_COHESION=1    -> subject REBASED (the defect)
#   legacy   ..._NEIGHBOURS_ONLY=1   -> subject REBASED TOO. This is the whole
#            point of 6acb4ef: in a packed u16 table every boundary word has one
#            contradicting AND one corroborating neighbour, so the old +-4 form
#            with its `!corroborated` veto could never fire on this shape.
#   overfire ..._MAX_BLOCK=100       -> the jump table gets DEMOTED. Proves the
#            positive control is LIVE. A first cut of the fix demoted three
#            genuine Halo jump tables (64/56/44 entries) while every
#            "the constant survived" arm stayed green; without this arm the
#            guard would have blessed that regression.
#
# ★THE GATE RUNS AT TRANSLATE TIME, so every arm must TRANSLATE, not merely run.
# ★The jump table's fate is asserted STATICALLY, from the translated bytes: its
# entries are alias targets, not real basic blocks, so it must never be executed.
set -u
cd "$(dirname "$0")"
ROOT=$(cd .. && pwd)

MACHO_TOOL="$ROOT/build/src/macho-tool/macho-tool"
LIBABICONV="$ROOT/build/src/abiconv/libabiconv.dylib"
LIBWRAPPER="$ROOT/build/src/86x64/libwrapper.a"
LIBINTERPOSE="$ROOT/build/src/86x64/libinterpose.dylib"
PIPELINE="$ROOT/src/86x64/86x64.sh"

I386=build/96_jt_cohesion.i386
if [ ! -f "$I386" ]; then
  echo "jt-cohesion: FAIL (build/96_jt_cohesion.i386 missing — run \`make jt-cohesion\`)"
  echo "  ⚠A MISSING FIXTURE IS A FAILURE, NOT A SKIP. The previous version of this"
  echo "   script exited 0 here; its fixture source was a .s eaten by the blanket"
  echo "   *.s gitignore, so the guard reported PASS while never once running."
  exit 1
fi

fail=0

# --- precondition: the constants really do alias code as claimed -------------
# Re-derived from the disassembly every run, so a layout change FAILS LOUDLY
# instead of silently testing nothing.
dis=$(otool -tV "$I386" 2>/dev/null)
is_boundary() { printf '%s\n' "$dis" | awk -v a="$1" '$1==a{print "yes"; exit}'; }
pre_ok=1
for a in 00008002 00008004 00009002 00009018; do          # must be BOUNDARIES
  [ "$(is_boundary $a)" = "yes" ] || { echo "  precondition: 0x$a is NOT a boundary"; pre_ok=0; }
done
for a in 00008001 00008007 00008009; do                   # must be INTERIOR
  [ -z "$(is_boundary $a)" ] || { echo "  precondition: 0x$a IS a boundary, want interior"; pre_ok=0; }
done
if [ "$pre_ok" = 1 ]; then
  echo "  precondition:  subject words are BOUNDARIES, their terminators are"
  echo "                 MID-instruction, jump-table entries are BOUNDARIES  OK"
else
  echo "  precondition:  FAILED — the fixture layout moved, so the constants no"
  echo "                 longer alias code as required. Re-derive them; do NOT"
  echo "                 trust the arms below."
  fail=1
fi

translate() { local a=$1; shift
  rm -f "build/96_jt.$a.x86_64" "build/96_jt.$a.x86_64.dylib"
  env "$@" bash "$PIPELINE" -m "$MACHO_TOOL" -l "$LIBABICONV" -w "$LIBWRAPPER" \
      -i "$LIBINTERPOSE" -o "build/96_jt.$a.x86_64" "$I386" >/dev/null 2>&1
  chmod +x "build/96_jt.$a.x86_64" 2>/dev/null
}
translate on
translate off      M64_NO_JT_COHESION=1
translate legacy   M64_JT_COHESION_NEIGHBOURS_ONLY=1
translate overfire M64_JT_COHESION_MAX_BLOCK=100

for a in on off legacy overfire; do
  [ -x "build/96_jt.$a.x86_64" ] || { echo "  $a arm: translation FAILED"; fail=1; }
done
[ "$fail" = 0 ] || { echo "jt-cohesion: FAIL"; exit 1; }

# report <arm> -> "<subject preserved 0-2> <jumptable still-pointer 0-12>"
report() {
  python3 - "$1" <<'PY'
import struct,subprocess,re,sys
def const(f):
    out=subprocess.run(['otool','-l',f],capture_output=True,text=True).stdout; cur={}
    for L in out.splitlines():
        L=L.strip(); m=re.match(r'(sectname|segname|addr|size|offset) (.+)',L)
        if m:
            cur[m.group(1)]=m.group(2).strip()
            if m.group(1)=='offset' and cur.get('segname')=='__TEXT' and cur.get('sectname')=='__const':
                return int(cur['size'],16),int(cur['offset'])
sz,of=const('build/96_jt_cohesion.i386'); n=sz//4
w=struct.unpack_from('<%dI'%n,open('build/96_jt_cohesion.i386','rb').read(),of)
f='build/96_jt.%s.x86_64.dylib'%sys.argv[1]
s2,o2=const(f); w2=struct.unpack_from('<%dI'%n,open(f,'rb').read(),o2)
print(sum(1 for k in (3,4) if w2[k]==w[k]), sum(1 for k in range(8,20) if w2[k]!=w[k]))
PY
}

for arm in on off legacy overfire; do
  ./build/96_jt.$arm.x86_64 >/dev/null 2>&1; eval "rc_$arm=$?"
  eval "st_$arm=\"\$(report $arm)\""
done

check() {   # <arm> <want_rc> <want_subject> <want_jt> <description>
  local arm=$1 wrc=$2 wsub=$3 wjt=$4 desc=$5
  local rc sub jt; eval "rc=\$rc_$arm"; eval "read sub jt <<< \"\$st_$arm\""
  if [ "$rc" = "$wrc" ] && [ "$sub" = "$wsub" ] && [ "$jt" = "$wjt" ]; then
    printf '  %-9s %s  OK\n' "$arm" "$desc"
  else
    printf '  %-9s %s\n' "$arm" "$desc"
    printf '            got rc=%s subject=%s/2 jumptable=%s/12, want rc=%s %s/2 %s/12\n' \
           "$rc" "$sub" "$jt" "$wrc" "$wsub" "$wjt"
    fail=1
  fi
}

check on       0 2 12 "subject preserved, jump table left alone"
check off      1 0 12 "gate disarmed -> subject REBASED (defect reproduced)"
check legacy   1 0 12 "+-4 form is BLIND to a packed table (that is why it was widened)"
check overfire 0 2  0 "block bar raised -> jump table demoted (positive control is LIVE)"

if [ "$fail" = 0 ]; then echo "jt-cohesion: PASS"; else echo "jt-cohesion: FAIL"; fi
exit $fail
