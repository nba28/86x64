#!/bin/bash
#
# Regression test for the SSE-register FP-pair struct RETURN fix (abigen.cc
# fp_reg_return; 2026-07-05). The CGPoint/CGSize/NSPoint/NSSize family: x86_64
# returns the two CGFloats in xmm0:xmm1 (a <=16-byte SSE-class aggregate), but
# i386 Darwin returns the 8-byte {float,float} struct in the INTEGER pair
# eax:edx. abigen used to emit NO return conversion for such a register-returned
# record -> the native callee left the point in xmm0:xmm1 and the i386 caller
# read eax:edx = garbage (CGContextGetTextPosition, CGPointApplyAffineTransform,
# CGContextConvertPointToUserSpace, CGLayerGetSize, ...).
#
# The i386 return conventions were verified empirically (clang -arch i386 disasm):
#   struct {float x,y}  (8B, >=2 fields)  -> eax:edx  (eax=x, edx=y)
#   struct {double}     (single FP field) -> st0      (scalar path, NOT eax:edx)
#   struct {4 CGFloat}  (CGRect, >16B x64) -> memory  (hidden sret pointer)
# so the fix fires ONLY for the multi-field, <=8B-i386 / <=16B-x64 homogeneous-FP
# case, and NOT for single-FP-field structs nor for the >16B CGRect sret family.
#
# Self-contained: abigen parses a synthetic header + nasm assembles the output.
set -u

PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ABIGEN="$PROJ_ROOT/build/src/abiconv/abigen"
[ -x "$ABIGEN" ] || { echo "SKIP fpreg-return (abigen not built at $ABIGEN)"; exit 0; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/hdr.h" <<'EOF'
typedef double CGFloat;                                  /* x86_64: CGFloat = double */
typedef struct CGPoint { CGFloat x; CGFloat y; } CGPoint;
typedef struct CGSize  { CGFloat width; CGFloat height; } CGSize;
typedef struct CGRect  { CGPoint origin; CGSize size; } CGRect;   /* >16B x64 -> sret */
typedef struct D1      { double d; } D1;                          /* single FP -> st0 */
extern CGPoint GetPt(void *ctx);
extern CGSize  GetSz(void *ctx);
extern CGRect  GetRect(void *ctx);
extern D1      GetD1(void);
EOF

cat > "$TMP/syms" <<'EOF'
_GetPt
_GetSz
_GetRect
_GetD1
EOF

"$ABIGEN" -o "$TMP/out.asm" -s "$TMP/syms" "$TMP/hdr.h" 2> "$TMP/err" || {
   echo "FAIL fpreg-return: abigen exited nonzero"; cat "$TMP/err"; exit 1;
}

body() { awk "/^___$1:/{f=1} f{print} f&&/jmp\tr11/{exit}" "$TMP/out.asm"; }

fail=0
note() { echo "FAIL fpreg-return: $1"; fail=1; }

# (0) generated asm must assemble
NASM="$(command -v nasm || true)"
if [ -n "$NASM" ]; then
   "$NASM" -f macho64 "$TMP/out.asm" -o "$TMP/out.o" 2> "$TMP/nasm.err" \
      || { note "generated asm does not assemble"; cat "$TMP/nasm.err"; }
else
   echo "note fpreg-return: nasm not found, skipping assemble check"
fi

# (a) CGPoint: narrows xmm0:xmm1 -> eax:edx
grep -q '^___GetPt:' "$TMP/out.asm" || note "GetPt not emitted"
b="$(body GetPt)"
echo "$b" | grep -q 'cvtsd2ss[[:space:]]*xmm0,[[:space:]]*xmm0' || note "GetPt: xmm0 not narrowed"
echo "$b" | grep -Eq 'movd[[:space:]]*eax,[[:space:]]*xmm0' || note "GetPt: field0 not moved to eax"
echo "$b" | grep -q 'cvtsd2ss[[:space:]]*xmm1,[[:space:]]*xmm1' || note "GetPt: xmm1 not narrowed"
echo "$b" | grep -Eq 'movd[[:space:]]*edx,[[:space:]]*xmm1' || note "GetPt: field1 not moved to edx"

# (b) CGSize: same treatment
grep -q '^___GetSz:' "$TMP/out.asm" || note "GetSz not emitted"
body GetSz | grep -Eq 'movd[[:space:]]*edx,[[:space:]]*xmm1' || note "GetSz: not narrowed to eax:edx"

# (c) CGRect (>16B x64): fp_sret path, NOT the eax:edx pair
grep -q '^___GetRect:' "$TMP/out.asm" || note "GetRect not emitted"
body GetRect | grep -q 'narrow x86_64 FP-struct sret' || note "GetRect: not using sret narrow path"
body GetRect | grep -Eq 'movd[[:space:]]*eax,[[:space:]]*xmm0' && note "GetRect: wrongly used eax:edx pair"

# (d) single-double struct: must NOT use the eax:edx narrow (i386 returns it in st0)
grep -q '^___GetD1:' "$TMP/out.asm" || note "GetD1 not emitted"
body GetD1 | grep -q 'narrow x86_64 SSE-pair record return' && note "GetD1: single-FP wrongly narrowed to eax:edx"

if [ "$fail" -eq 0 ]; then echo "PASS fpreg-return"; exit 0; fi
exit 1
