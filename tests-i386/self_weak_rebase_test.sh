#!/bin/bash
#
# self-weak-rebase: A TRANSLATED IMAGE MUST SATISFY ITS OWN WEAK BINDS LOCALLY.
#
# WHY THIS EXISTS.  A C++ `linkonce_odr` symbol (inline member function, vtable,
# typeinfo, template instantiation) is a WEAK definition, and a reference to its
# ADDRESS goes through a __DATA,__nl_symbol_ptr / __la_symbol_ptr slot serviced
# by the WEAK_BIND opcode stream so every image in the process coalesces on one
# definition.  A self-contained C++ image therefore both SUPPLIES and CONSUMES
# weak-def coalescing: it weak-binds to symbols it defines itself.
#
# src/core/transform.cc clears MH_WEAK_DEFINES on every M64 emit, so our i386
# code can never satisfy a NATIVE x86_64 image's coalesced bind and truncate its
# rbp (the cross-ABI weak-def coalesce bug -- the Portal 2 GPU-driver crash).
# That directionality is correct and this fix does NOT touch it.  But the same
# bit also removes the image from the candidate set for its OWN binds: dyld
# finds no definition anywhere, the slot keeps its FILE value -- the UNSLID
# preferred-base address -- and the first call through it jumps into unmapped
# low memory at 0x100000xx.
#
# ★MEASURED 2026-09-23 on 32_member_func_ptr: 7 weak binds, every symbol `T` in
# the same image; M64_FAULT_REPORT gave rip=0x10000e14 = the unslid
# __ZN4MathC1Ei, i.e. the process died on `Math m(10)` before printing anything.
# Six C++ fixtures had been carried as "the known 6" in exactly this state.
#
# THE FIX (src/core/archive.cc, Archive<M64>::resolve_self_weak_binds): a weak
# bind whose target slot is a SymbolPointer with a non-null `pointee` already
# has its answer -- the translator resolved the target to a blob in THIS image,
# and SymbolPointer::raw_data() emits that blob's unslid vmaddr into the slot.
# Drop the bind, emit a plain local REBASE, and dyld just adds the slide.  No
# coalescing consumed; MH_WEAK_DEFINES stays cleared.
#
# ASSERTS, per fixture, BOTH ARMS:
#   (a) ON  (default): the translated dylib's WEAK_BIND stream has NO entry that
#       names a symbol the SAME image defines, and every such slot carries
#       exactly ONE rebase (a second rebase would apply the slide twice);
#   (b) ON: the fixture runs and its output matches expected/<t>.txt;
#   (c) OFF (M64_NO_SELF_WEAK_REBASE=1): the self weak binds are BACK in the
#       stream and the fixture fails again exactly as it did before the fix.
#
# The ON arm is (re)built LAST so the suite inherits the ON artifact.
# Needs the C++ sysroot (`make sysroot-cpp`); SKIPs without.
set -u

MT="${1:?usage: self_weak_rebase_test.sh <macho-tool> <libabiconv> <libwrapper.a> <libinterpose>}"
LIBABICONV="${2:?}"
LIBWRAPPER="${3:?}"
LIBINTERPOSE="${4:?}"

HERE="$(cd "$(dirname "$0")" && pwd)"
PIPELINE="$HERE/../src/86x64/86x64.sh"
BUILD="$HERE/build"

# The six fixtures that were failing on this defect.  Each must FAIL in the OFF
# arm and PASS in the ON arm.
FIXTURES="28_weak_bind 32_member_func_ptr 52_eh_throw_object \
96_rtti_const_vtable_bind 97_dynamic_cast_crosscast 98_dynamic_cast_corrupt_vtable"

# Control: a C++ fixture that ALREADY passed before the fix.  It must pass in
# BOTH arms -- so a "fix" that merely traded one broken fixture for another, or
# that perturbs an image with no self-satisfiable weak binds, is caught here.
CONTROLS="29_weak_new_delete 63_cpp_static_map_rbtree"

fail() { echo "FAIL self-weak-rebase: $1"; exit 1; }

for t in $FIXTURES $CONTROLS; do
   [ -f "$BUILD/$t.i386" ] || { echo "SKIP self-weak-rebase (no $BUILD/$t.i386; run 'make cpp' / 'make sysroot-cpp' first)"; exit 0; }
done

# ---------------------------------------------------------------- helpers ----
# Print "<n_self_weak_binds> <n_double_rebased_slots>" for a translated dylib:
#   n_self_weak_binds   = WEAK_BIND entries naming a symbol this image DEFINES
#   n_double_rebased    = __DATA offsets appearing more than once in REBASE
scan() {
python3 - "$1" "$2" <<'PY'
import sys, struct, subprocess

path, defs_path = sys.argv[1], sys.argv[2]
data = open(path, 'rb').read()
defined = set(l.strip() for l in open(defs_path) if l.strip())

ncmds = struct.unpack_from('<I', data, 16)[0]
off, info = 32, None
for _ in range(ncmds):
    cmd, sz = struct.unpack_from('<II', data, off)
    if cmd in (0x22, 0x80000022):
        info = struct.unpack_from('<10I', data, off + 8)
    off += sz
if info is None:
    print("0 0"); raise SystemExit
reb_off, reb_sz, _b_off, _b_sz, w_off, w_sz = info[:6]


def uleb(b, i):
    r = s = 0
    while True:
        c = b[i]; i += 1; r |= (c & 0x7f) << s; s += 7
        if not c & 0x80:
            return r, i


def weak_syms(o, s):
    b, i, sym, out = data[o:o + s], 0, None, []
    while i < len(b):
        op = b[i]; hi, imm = op & 0xf0, op & 0x0f; i += 1
        if hi == 0x40:
            j = i
            while b[i]:
                i += 1
            sym = b[j:i].decode(); i += 1
        elif hi in (0x20, 0x60, 0x70, 0x80, 0xa0):
            _, i = uleb(b, i)
        elif hi == 0xc0:
            _, i = uleb(b, i); _, i = uleb(b, i)
        elif hi == 0x90:
            out.append(sym)
    return out


def rebase_offs(o, s):
    b, i, cur, out = data[o:o + s], 0, None, []
    while i < len(b):
        op = b[i]; hi, imm = op & 0xf0, op & 0x0f; i += 1
        if hi == 0x20:
            v, i = uleb(b, i); cur = [imm, v]
        elif hi == 0x30:
            v, i = uleb(b, i); cur[1] += v
        elif hi == 0x40:
            cur[1] += imm * 8
        elif hi == 0x50:
            for _ in range(imm):
                out.append(tuple(cur)); cur[1] += 8
        elif hi == 0x60:
            n, i = uleb(b, i)
            for _ in range(n):
                out.append(tuple(cur)); cur[1] += 8
        elif hi == 0x70:
            out.append(tuple(cur)); v, i = uleb(b, i); cur[1] += 8 + v
        elif hi == 0x80:
            n, i = uleb(b, i); sk, i = uleb(b, i)
            for _ in range(n):
                out.append(tuple(cur)); cur[1] += 8 + sk
    return out


n_self = sum(1 for s in weak_syms(w_off, w_sz) if s in defined)
offs = rebase_offs(reb_off, reb_sz)
n_dup = len(offs) - len(set(offs))
print("%d %d" % (n_self, n_dup))
PY
}

translate() { # translate <out> <in.i386>   (env of caller applies)
   bash "$PIPELINE" -m "$MT" -l "$LIBABICONV" -w "$LIBWRAPPER" -i "$LIBINTERPOSE" \
        -o "$1" "$2" >/dev/null 2>&1
}

run_deadlined() { # run_deadlined <binary> <outfile>; echoes the exit code
   ( perl -e 'alarm 20; exec @ARGV' "$1" > "$2" 2>/dev/null ) 2>/dev/null
   local rc=$?
   echo "exit_code: $rc" >> "$2"
   echo "$rc"
}

# ------------------------------------------------------------- the A/B -------
for t in $FIXTURES $CONTROLS; do
   case " $CONTROLS " in *" $t "*) is_control=1;; *) is_control=0;; esac
   exp="$HERE/expected/$t.txt"
   [ -f "$exp" ] || fail "$t has no expected/ file"

   # ---- OFF arm: the pre-fix behaviour must come back -----------------------
   off="$BUILD/${t}.swroff"
   M64_NO_SELF_WEAK_REBASE=1 translate "$off" "$BUILD/$t.i386" \
      || fail "$t: OFF-arm translate failed"
   chmod +x "$off"
   nm -g "$off.dylib" 2>/dev/null | awk '$2=="T"||$2=="W"{print $3}' > "$BUILD/$t.defs"
   read -r off_self off_dup <<EOF
$(scan "$off.dylib" "$BUILD/$t.defs")
EOF
   if [ "$is_control" = "0" ] && [ "${off_self:-0}" -le 0 ]; then
      fail "$t: OFF arm has NO self weak binds -- the kill switch is inert, so the ON arm proves nothing"
   fi

   off_rc=$(run_deadlined "$off" "$BUILD/$t.swroff.out")
   if diff -q "$exp" "$BUILD/$t.swroff.out" >/dev/null 2>&1; then
      off_verdict="passes"
      [ "$is_control" = "1" ] || fail "$t: OFF arm PASSED -- the fixture does not actually depend on the fix"
   else
      off_verdict="fails"
      [ "$is_control" = "0" ] || fail "$t: control fixture FAILED in the OFF arm (exit $off_rc) -- it should be unaffected by this fix"
   fi

   # ---- ON arm (default), built LAST so the suite inherits it ---------------
   on="$BUILD/$t.x86_64"
   translate "$on" "$BUILD/$t.i386" || fail "$t: ON-arm translate failed"
   chmod +x "$on"
   read -r on_self on_dup <<EOF
$(scan "$on.dylib" "$BUILD/$t.defs")
EOF
   # (a) BY CONTENT: no self-satisfiable weak bind survives, no slot rebased twice
   [ "${on_self:-1}" = "0" ] || fail "$t: ON arm still has $on_self weak bind(s) to in-image definitions"
   [ "${on_dup:-1}" = "0" ] || fail "$t: ON arm rebases $on_dup slot(s) twice (slide applied twice)"

   # (b) and it actually runs
   on_rc=$(run_deadlined "$on" "$BUILD/$t.swron.out")
   diff -q "$exp" "$BUILD/$t.swron.out" >/dev/null 2>&1 \
      || fail "$t: ON arm output does not match expected/$t.txt (exit $on_rc)"

   [ "$is_control" = "1" ] && tag="control " || tag=""
   echo "  ${tag}$t: OFF $off_self self weak bind(s), exit $off_rc ($off_verdict) -> ON 0 self weak bind(s), 0 double rebases, exit $on_rc (matches expected)"
   rm -f "$off" "$off.dylib" "$BUILD/$t.swroff.out" "$BUILD/$t.swron.out" "$BUILD/$t.defs"
done

echo "PASS self-weak-rebase (6 fixtures fail with M64_NO_SELF_WEAK_REBASE=1 and pass without it; 2 controls pass in both arms)"
