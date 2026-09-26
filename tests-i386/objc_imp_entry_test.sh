#!/bin/bash
# objc_imp_entry_test.sh — A/B guard for src/guard/99_objc_imp_noprologue.m.
# ON: exit 0. OFF (M64_NO_OBJC_IMP_ENTRY=1 at translate time): nonzero.
set -u
cd "$(dirname "$0")"
# CC / LD / LDF come from the Makefile target (objc-imp-entry).
B=build/99_objc_imp_noprologue
$CC -fomit-frame-pointer -c src/guard/99_objc_imp_noprologue.m -o $B.o || exit 1
$LD $LDF -e _main -o $B.i386 $B.o && strip -x $B.i386 || exit 1
imp_word() {  # $1 = binary; prints the IMP word of the one __cls_meth entry
  python3 - "$1" <<'PY'
import sys, struct, subprocess
p = sys.argv[1]
out = subprocess.run(["otool", "-l", p], capture_output=True, text=True).stdout.split("\n")
for i, l in enumerate(out):
    if l.strip() == "sectname __cls_meth":
        off = int([x for x in out[i:i+8] if x.strip().startswith("offset")][0].split()[1])
        d = open(p, "rb").read()
        # objc_method_list {obsolete, count, {name, types, imp}}: imp at +16
        print("0x%x" % struct.unpack_from("<I", d, off + 16)[0]); break
PY
}
raw=$(imp_word $B.i386)
fail=0
for arm in on off; do
  e=""; [ $arm = off ] && e="M64_NO_OBJC_IMP_ENTRY=1"
  env $e bash ../src/86x64/86x64.sh -m ../build/src/macho-tool/macho-tool \
    -l ../build/src/abiconv/libabiconv.dylib -w ../build/src/86x64/libwrapper.a \
    -i ../build/src/86x64/libinterpose.dylib -o $B.$arm $B.i386 >/dev/null 2>&1
  w=$(imp_word $B.$arm.dylib)
  if [ $arm = on ]; then
    [ -n "$w" ] && [ $((w)) -ge $((0x10000000)) ] && echo "  ON : IMP $raw -> $w  OK" \
      || { echo "  ON : IMP $raw -> '$w' (not relocated)"; fail=1; }
  else
    [ "$w" = "$raw" ] && echo "  OFF: IMP left raw $w — the defect reproduces  OK" \
      || { echo "  OFF: IMP '$w' (raw $raw) — guard inert"; fail=1; }
  fi
done
[ $fail = 0 ] && echo "objc-imp-entry: PASS" || echo "objc-imp-entry: FAIL"
exit $fail
