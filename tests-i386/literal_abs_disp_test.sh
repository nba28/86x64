#!/bin/bash
#
# literal_abs_disp_test.sh — A/B guard for the LITERAL-POOL absolute-displacement
# gate (Instruction::Parse's dest_is_data test, src/core/instruction.cc).
#
# i386 ModR/M mod=00 r/m=101 is "absolute disp32"; the identical encoding in
# 64-bit is "[rip+disp32]". Every absolute memory reference must therefore have
# its displacement rewritten at translate time, or the translated instruction
# reads whatever byte sits at that offset from rip. The gate deciding that used
# to enumerate data section TYPES -- S_REGULAR / S_ZEROFILL / S_GB_ZEROFILL --
# which excludes every LITERAL POOL (__literal4 = S_4BYTE_LITERALS, __cstring =
# S_CSTRING_LITERALS). Found by a whole-image pcmap audit of Halo CE: four float
# compares against pooled constants read __text, __DATA,__data and the
# translator's own __86x64_pcmap instead of 0.0f / -0.05f / 0.0005f.
#
# ★THE GATE RUNS AT TRANSLATE TIME, so both arms must TRANSLATE, not merely run:
# an .x86_64 already built cannot be A/B'd by an env var at run time.
#
# TWO ARMS:
#   on   shipped config             -> literal-pool read correct (exit 0)
#   off  M64_NO_LITERAL_ABS_DISP=1  -> restores the type allowlist; the
#                                      literal-pool read is wrong (exit 1)
# The fixture's __DATA control must pass in BOTH arms (exit 2 = control broke,
# i.e. the fixture stopped measuring the gate). An `off` arm that exits 0 means
# the guard has gone inert -- the displacement happened to land on the right
# bytes -- and is reported as a failure, not a pass.
set -u
cd "$(dirname "$0")"
ROOT=$(cd .. && pwd)

MACHO_TOOL="$ROOT/build/src/macho-tool/macho-tool"
LIBABICONV="$ROOT/build/src/abiconv/libabiconv.dylib"
LIBWRAPPER="$ROOT/build/src/86x64/libwrapper.a"
LIBINTERPOSE="$ROOT/build/src/86x64/libinterpose.dylib"
PIPELINE="$ROOT/src/86x64/86x64.sh"

I386=build/99_literal_abs_disp.i386
if [ ! -f "$I386" ]; then
  echo "literal-abs-disp: FAIL (build/99_literal_abs_disp.i386 missing — run \`make literal-abs-disp\`)"
  echo "  ⚠A MISSING FIXTURE IS A FAILURE, NOT A SKIP."
  exit 1
fi

fail=0

# --- precondition: the fixture really does use [abs32]+imm8 into a POOL ------
# Re-derived every run from the binary itself, so a compiler/layout change fails
# loudly instead of leaving the arms below testing nothing.
pre=$(python3 - "$I386" <<'PY'
import re,struct,subprocess,sys
f=sys.argv[1]
out=subprocess.run(['otool','-l',f],capture_output=True,text=True).stdout
cur={}; pool=None; text=None
for L in out.splitlines():
    L=L.strip(); m=re.match(r'(sectname|segname|addr|size|offset|flags) (.+)',L)
    if not m: continue
    cur[m.group(1)]=m.group(2).strip()
    if m.group(1)=='flags' and {'sectname','addr','size','offset'} <= set(cur):
        rec=(int(cur['addr'],16),int(cur['size'],16),int(cur['offset']),
             int(cur['flags'],16))
        if cur.get('sectname')=='__cstring': pool=rec
        if cur.get('sectname')=='__text':    text=rec
        cur.pop('sectname',None)   # segment 'flags' lines must not re-fire
if pool is None or text is None: print("NOSECT"); raise SystemExit
addr,size,off,flags=pool
if flags & 0xff != 0x02:            # S_CSTRING_LITERALS
    print("BADTYPE %#x"%flags); raise SystemExit
# count `cmpb $imm8, [abs32]` (80 3d disp32 imm8) whose disp lands in the pool
data=open(f,'rb').read()[text[2]:text[2]+text[1]]
n=0
for m in re.finditer(b'\x80\x3d', data):
    d,=struct.unpack_from('<I',data,m.start()+2)
    if addr<=d<addr+size: n+=1
print("OK %d %#x"%(n,flags))
PY
)
case "$pre" in
  OK\ [1-9]*) echo "  precondition:  ${pre#OK } cmpb \$imm8,[abs32] refs into an S_CSTRING_LITERALS pool  OK" ;;
  *) echo "  precondition:  FAILED ($pre) — the fixture no longer emits an"
     echo "                 [abs32]+imm8 reference into a literal-pool section. Do NOT"
     echo "                 trust the arms below."
     fail=1 ;;
esac

translate() { local a=$1; shift
  rm -f "build/99_lit.$a.x86_64" "build/99_lit.$a.x86_64.dylib"
  env "$@" bash "$PIPELINE" -m "$MACHO_TOOL" -l "$LIBABICONV" -w "$LIBWRAPPER" \
      -i "$LIBINTERPOSE" -o "build/99_lit.$a.x86_64" "$I386" >/dev/null 2>&1
  chmod +x "build/99_lit.$a.x86_64" 2>/dev/null
}
translate on
translate off M64_NO_LITERAL_ABS_DISP=1

for a in on off; do
  [ -x "build/99_lit.$a.x86_64" ] || { echo "  $a arm: translation FAILED"; fail=1; }
done
[ "$fail" = 0 ] || { echo "literal-abs-disp: FAIL"; exit 1; }

for a in on off; do
  out=$(./build/99_lit.$a.x86_64 2>&1); rc=$?
  eval "rc_$a=$rc"; eval "out_$a=\$out"
done

check() {  # <arm> <want_rc> <description>
  local arm=$1 want=$2 desc=$3 rc out; eval "rc=\$rc_$arm"; eval "out=\$out_$arm"
  if [ "$rc" = "$want" ]; then printf '  %-4s %s  OK\n' "$arm" "$desc"
  else
    printf '  %-4s %s\n' "$arm" "$desc"
    printf '       got rc=%s want %s; output: %s\n' "$rc" "$want" "$out"
    [ "$rc" = 2 ] && printf '       rc=2 is the __DATA CONTROL failing: the fixture is broken,\n       not the gate.\n'
    fail=1
  fi
}

check on  0 "literal-pool [abs32] relocated (shipped behaviour)"
check off 1 "type allowlist restored -> pool read is WRONG (defect reproduced)"

if [ "$fail" = 0 ]; then echo "literal-abs-disp: PASS"; else echo "literal-abs-disp: FAIL"; fi
exit $fail
