#!/bin/bash
#
# Regression test for GENERALIZED SysV struct-by-value ARG marshalling
# (abigen.cc byval_classify / byval_flat_convert, 2026-07-05).
#
# abigen used to handle exactly two by-value shapes — {1-2 long fields}
# (CFRange/NSRange) and homogeneous-FP CGFloat/double structs (CG geometry) —
# and SKIPPED every other struct-by-value function. The skipped set included
# the classic Carbon Point-by-value family (DragWindow / PtInRect /
# FindControlUnderMouse / HandleControlClick / TEClick / ContextualMenuSelect),
# struct in_addr (inet_ntoa), CFUUIDBytes (16 UInt8 fields), CFGregorianDate
# (mixed int+double), and NSCreateMapTableWithZone (callback structs by value).
# An unshimmed function binds NATIVE -> the i386 cdecl call reaches it raw ->
# over-pop / garbage-register crash the moment the target calls it.
#
# Self-contained: abigen parses a synthetic header (no i386 sysroot needed).
# Asserts on the generated asm:
#   (a) Point-family {short,short} byval arg is emitted, packed into ONE
#       INTEGER eightbyte, and the NEXT int arg still lands in esi/rsi;
#   (b) {unsigned int} byval (in_addr) is emitted;
#   (c) mixed int+double byval (CFGregorianDate shape) is emitted and uses
#       BOTH a GP eightbyte and an SSE eightbyte (movsd xmm0);
#   (d) a 24-byte all-int byval is emitted and spilled to the STACK (MEMORY
#       class), not registers;
#   (e) a callback-struct byval (NSMapTableKeyCallBacks shape) is emitted and
#       binds its fn-ptr fields through x64_cb_wrap;
#   (f) a UNION byval is still SKIPPED (negative control — no silent wrong emit);
#   (g) the pre-existing {long,long} (CFRange) byval still emits (no regression).
set -u

PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ABIGEN="$PROJ_ROOT/build/src/abiconv/abigen"
[ -x "$ABIGEN" ] || { echo "SKIP byval-marshal (abigen not built at $ABIGEN)"; exit 0; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/hdr.h" <<'EOF'
typedef struct Point { short v; short h; } Point;
typedef struct InAddr { unsigned int s_addr; } InAddr;
typedef struct Greg { int year; signed char month, day, hour, minute; double second; } Greg;
typedef struct Big { int a, b, c, d, e, f; } Big;                    /* 24B -> MEMORY */
typedef struct CB { unsigned long ver; void (*hash)(int); void (*release)(int); } CB;
typedef union U { int i; float f; } U;
typedef struct Range { long location; long length; } Range;

extern int  TakePt(Point pt, int after);
extern char *TakeAddr(InAddr a);
extern double TakeGreg(Greg g, int after);
extern int  TakeBig(Big b, int after);
extern void TakeCB(CB cb);
extern int  TakeU(U u);
extern long TakeRange(Range r, long after);
EOF

cat > "$TMP/syms" <<'EOF'
_TakePt
_TakeAddr
_TakeGreg
_TakeBig
_TakeCB
_TakeU
_TakeRange
EOF

"$ABIGEN" -o "$TMP/out.asm" -s "$TMP/syms" "$TMP/hdr.h" 2> "$TMP/err" || {
   echo "FAIL byval-marshal: abigen exited nonzero"; cat "$TMP/err"; exit 1;
}

# extract one shim's body (from its label to the closing `jmp r11`)
body() { awk "/^___$1:/{f=1} f{print} f&&/jmp\tr11/{exit}" "$TMP/out.asm"; }

fail=0
note() { echo "FAIL byval-marshal: $1"; fail=1; }

# (0) the generated asm must ASSEMBLE (strongest self-contained proof the
#     staged-image + eightbyte-load + stack-spill sequences are well-formed).
NASM="$(command -v nasm || true)"
if [ -n "$NASM" ]; then
   if ! "$NASM" -f macho64 "$TMP/out.asm" -o "$TMP/out.o" 2> "$TMP/nasm.err"; then
      note "generated asm does not assemble"; cat "$TMP/nasm.err"
   fi
else
   echo "note byval-marshal: nasm not found, skipping assemble check"
fi

# (a) Point {short,short}: emitted; packed into ONE INTEGER eightbyte in rdi;
#     `after` in esi/rsi (structure took exactly one GP reg)
grep -q '^___TakePt:' "$TMP/out.asm" || note "TakePt not emitted"
body TakePt | grep -q 'mov[[:space:]]*rdi,' || note "TakePt: struct eightbyte not loaded into rdi"
body TakePt | grep -Eq 'mov(sx)?[[:space:]]*(esi|rsi),' || note "TakePt: arg after struct not in esi/rsi"

# (b) in_addr shape emitted
grep -q '^___TakeAddr:' "$TMP/out.asm" || note "TakeAddr (struct{uint}) not emitted"

# (c) mixed int+double: GP eightbyte + SSE eightbyte
grep -q '^___TakeGreg:' "$TMP/out.asm" || note "TakeGreg (mixed int+double) not emitted"
body TakeGreg | grep -q 'movsd[[:space:]]*xmm0,' || note "TakeGreg: double eightbyte not in xmm0"
body TakeGreg | grep -q 'mov[[:space:]]*rdi,'    || note "TakeGreg: int eightbyte not in rdi"

# (d) 24B all-int struct: MEMORY class -> spilled to outgoing stack via r11, and
#     the following int arg takes edi/rdi (structs in MEMORY consume no GP regs)
grep -q '^___TakeBig:' "$TMP/out.asm" || note "TakeBig (24B) not emitted"
body TakeBig | grep -Eq 'mov[[:space:]]*qword +\[rsp\+0\],[[:space:]]*r11' \
   || note "TakeBig: image not spilled to stack"
body TakeBig | grep -Eq 'mov(sx)?[[:space:]]*(edi|rdi),' || note "TakeBig: arg after MEMORY struct not in edi/rdi"

# (e) callback-struct byval: fn-ptr fields bridge through x64_cb_wrap
grep -q '^___TakeCB:' "$TMP/out.asm" || note "TakeCB (callback struct) not emitted"
body TakeCB | grep -q '_x64_cb_wrap' || note "TakeCB: fn-ptr field not bound to cb trampoline"

# (f) union byval still skipped (negative control)
if grep -q '^___TakeU:' "$TMP/out.asm"; then note "TakeU (union byval) wrongly emitted"; fi
grep -q 'skipping _TakeU' "$TMP/err" || note "TakeU skip not reported"

# (g) {long,long} regression: still emitted, both eightbytes in GP regs, and the
#     trailing long arg takes edx/rdx (movsx is the widening form)
grep -q '^___TakeRange:' "$TMP/out.asm" || note "TakeRange (CFRange shape) not emitted"
body TakeRange | grep -q 'mov[[:space:]]*rdi,' || note "TakeRange: eightbyte0 not in rdi"
body TakeRange | grep -q 'mov[[:space:]]*rsi,' || note "TakeRange: eightbyte1 not in rsi"
body TakeRange | grep -Eq 'mov(sx)?[[:space:]]*(edx|rdx),' || note "TakeRange: arg after struct not in rdx"

if [ "$fail" -eq 0 ]; then
   echo "PASS byval-marshal"
   exit 0
fi
exit 1
