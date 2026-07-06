#!/bin/bash
#
# Regression guard for the abigen REGISTER-CLASS struct RETURN + byval struct ARG
# completion (abigen.cc: relaxed the C1/C2 "byval arg with unhandled struct
# return" guard to admit register-class returns; int_reg_struct_return + hi-split).
#
# The C1 byval-arg support skipped ANY function that both took a by-value struct
# arg AND returned a struct, unless the return was the >16B homogeneous-FP MEMORY
# sret. That over-skipped the register-class-return family, which has NO hidden
# sret pointer / NO i386 arg shift and needs only a post-call fix-up:
#   fp_reg  : CGPoint/CGSize in xmm0:xmm1 -> narrow to i386 eax:edx (reuses C2).
#   int_reg : all-integer <=8B i386 (eax:edx) / <=16B x86_64 (rax:rdx). A single
#             x86_64 eightbyte (rax) with an i386 span > 4 bytes needs the high
#             dword split rax[63:32] -> edx (UnsignedWide/AbsoluteTime); a 2-
#             eightbyte return (NSRange/CFRange) already lands in rax:rdx = eax:edx.
#
# Self-contained: abigen parses a synthetic header + nasm assembles (no i386
# sysroot). The fp_reg runtime path is separately value-checked by the full
# translate+run test 51_structret_regclass; this guard also covers the int_reg
# hi-split, which has no convenient deterministic native returner to run.
set -u

PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ABIGEN="$PROJ_ROOT/build/src/abiconv/abigen"
[ -x "$ABIGEN" ] || { echo "SKIP structret-regclass (abigen not built at $ABIGEN)"; exit 0; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/hdr.h" <<'EOF'
typedef double CGFloat;
typedef struct { CGFloat x, y; } CGPoint;
typedef struct { CGFloat a,b,c,d,tx,ty; } CGAffineTransform;
typedef struct { unsigned int hi, lo; } UnsignedWide;   /* 8B all-int -> hi-split */
typedef struct { unsigned long location, length; } NSRange; /* 16B -> rax:rdx      */
/* fp_reg return + byval args: */
extern CGPoint ApplyPt(CGPoint p, CGAffineTransform t);
/* int_reg 8-byte return (single eightbyte in rax) + byval arg: */
extern UnsignedWide AddW(UnsignedWide a, UnsignedWide b);
/* int_reg 16-byte return (rax:rdx) + byval arg: */
extern NSRange IsectRange(NSRange a, NSRange b);
/* int_reg 8-byte return, NO byval arg (also needs the hi-split): */
extern UnsignedWide UpTimeW(void);
EOF
printf '_ApplyPt\n_AddW\n_IsectRange\n_UpTimeW\n' > "$TMP/syms"

"$ABIGEN" -o "$TMP/out.asm" -s "$TMP/syms" "$TMP/hdr.h" 2> "$TMP/err" \
   || { echo "FAIL structret-regclass: abigen exited nonzero"; cat "$TMP/err"; exit 1; }

body() { awk "/^___$1:/{f=1} f{print} f&&/jmp\tr11/{exit}" "$TMP/out.asm"; }
fail=0
note() { echo "FAIL structret-regclass: $1"; fail=1; }

# (0) nothing in this set may be skipped anymore
if grep -q 'skipping' "$TMP/err"; then
   note "a function was skipped (guard not relaxed)"; grep 'skipping' "$TMP/err"
fi

# (0b) assembles
NASM="$(command -v nasm || true)"
if [ -n "$NASM" ]; then
   "$NASM" -f macho64 "$TMP/out.asm" -o "$TMP/out.o" 2> "$TMP/nasm.err" \
      || { note "generated asm does not assemble"; cat "$TMP/nasm.err"; }
else
   echo "note structret-regclass: nasm not found, skipping assemble check"
fi

# (a) fp_reg return + byval arg: emitted with the SSE-pair -> eax:edx narrow
grep -q '^___ApplyPt:' "$TMP/out.asm" || note "ApplyPt not emitted (fp_reg+byval still skipped)"
b="$(body ApplyPt)"
echo "$b" | grep -q 'cvtsd2ss[[:space:]]*xmm0,[[:space:]]*xmm0' || note "ApplyPt: fp_reg narrow missing"
echo "$b" | grep -Eq 'movd[[:space:]]*edx,[[:space:]]*xmm1' || note "ApplyPt: field1 not narrowed to edx"

# (b) int_reg 8-byte return + byval arg: emitted with the rax[63:32] -> edx split
grep -q '^___AddW:' "$TMP/out.asm" || note "AddW not emitted (int_reg+byval still skipped)"
ab="$(body AddW)"
echo "$ab" | grep -Eq 'mov[[:space:]]*rdx,[[:space:]]*rax' || note "AddW: hi-split (mov rdx,rax) missing"
echo "$ab" | grep -Eq 'shr[[:space:]]*rdx,[[:space:]]*32' || note "AddW: hi-split (shr rdx,32) missing"

# (c) int_reg 16-byte return (2 eightbytes): emitted, NO split (rax:rdx = eax:edx)
grep -q '^___IsectRange:' "$TMP/out.asm" || note "IsectRange not emitted"
if body IsectRange | grep -Eq 'shr[[:space:]]*rdx,[[:space:]]*32'; then
   note "IsectRange: spurious hi-split on a 2-eightbyte return"
fi

# (d) int_reg 8-byte return WITHOUT a byval arg also gets the hi-split (latent
#     correctness: single eightbyte in rax must fill i386 edx)
grep -q '^___UpTimeW:' "$TMP/out.asm" || note "UpTimeW not emitted"
body UpTimeW | grep -Eq 'shr[[:space:]]*rdx,[[:space:]]*32' \
   || note "UpTimeW: no-byval 8-byte int return missing the hi-split"

if [ "$fail" -eq 0 ]; then echo "PASS structret-regclass"; exit 0; fi
exit 1
