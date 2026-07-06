#!/bin/bash
#
# Regression guard for the extended callback-bridge marshalling (C4): a long-long
# RETURN (CBR_I64) and a small by-VALUE struct callback ARG.
#
# abigen's cb bridge previously threw on both, and convert_fnptr then fell back to
# passing the RAW i386 fn-ptr (NO trampoline) -> native code calls the i386
# callback with the x86_64 ABI -> crash. The fixes:
#   CBR_I64 : _86x64_call_i386 now folds the i386 edx:eax into rax, and
#             cb_ret_code_for maps `long long`/`unsigned long long` -> CBR_I64
#             (funopen fpos_t seekfn, CGDataProvider off_t skipForward).
#   struct arg: an all-integer aggregate <=8 bytes is one SysV eightbyte -> one
#             GP register, byte-identical to a 32/64-bit int, so cb_arg_code_for
#             maps it to CBA_I32 (<=4B) / CBA_I64 (5-8B) — no dispatcher change
#             (CGScreenUpdateMoveDelta {int,int}; classic Carbon Point {short,short}).
# FP-bearing or >8B structs arrive in xmm / span multiple regs and stay
# unsupported (raw fallback), so they must NOT be encoded here.
#
# Self-contained: abigen parses a synthetic header + nasm assembles the emitted
# callback-signature descriptors (no i386 sysroot). The descriptor blob layout is
# `dd nargs; dd ret_kind; db arg_kinds[16]` (cb_sig_emit); codes match cb_bridge.c
# (CBA_I32=0 CBA_I64=1 CBA_PTR=2; CBR_I32=1 CBR_I64=5).
set -u

PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ABIGEN="$PROJ_ROOT/build/src/abiconv/abigen"
[ -x "$ABIGEN" ] || { echo "SKIP cb-extended (abigen not built at $ABIGEN)"; exit 0; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/hdr.h" <<'EOF'
typedef long long fpos_t;
typedef struct { int dX; int dY; } CGScreenUpdateMoveDelta;   /* 8B all-int */
typedef struct { short v; short h; } Point;                    /* 4B all-int */
typedef struct { float x; float y; } FPair;                    /* 8B FP -> unsupported */
/* CBR_I64 return callback (funopen seekfn shape): */
extern void RegSeek(fpos_t (*seekfn)(void *cookie, fpos_t off, int whence));
/* 8B all-int struct-by-value callback arg -> CBA_I64: */
extern void RegMove(void (*cb)(CGScreenUpdateMoveDelta delta, void *userInfo));
/* 4B all-int struct-by-value callback arg -> CBA_I32: */
extern void RegPoint(void (*cb)(Point where, int modifiers));
/* FP-bearing struct arg must remain unsupported (raw fallback): */
extern void RegFP(void (*cb)(FPair f, int m));
EOF
printf '_RegSeek\n_RegMove\n_RegPoint\n_RegFP\n' > "$TMP/syms"

"$ABIGEN" -o "$TMP/out.asm" -s "$TMP/syms" "$TMP/hdr.h" 2> "$TMP/err" \
   || { echo "FAIL cb-extended: abigen exited nonzero"; cat "$TMP/err"; exit 1; }

fail=0
note() { echo "FAIL cb-extended: $1"; fail=1; }

# collect the emitted descriptor blobs: map "nargs|ret|arg0,arg1,..." per sig index
descs="$(awk '
  /_x64_cbsig_[0-9]+:/ { gsub(/[^0-9]/,"",$0); idx=$0; n=""; r=""; a=""; next }
  /^\tdd / && n=="" { n=$2; next }
  /^\tdd / && r=="" { r=$2; next }
  /^\tdb / { sub(/^\tdb /,""); a=$0; print idx"|"n"|"r"|"a }
' "$TMP/out.asm")"

# (0) assemble
NASM="$(command -v nasm || true)"
if [ -n "$NASM" ]; then
   "$NASM" -f macho64 "$TMP/out.asm" -o "$TMP/out.o" 2> "$TMP/nasm.err" \
      || { note "generated asm does not assemble"; cat "$TMP/nasm.err"; }
fi

# (a) RegSeek + RegMove + RegPoint must NOT hit the raw-pointer fallback
for fn in RegSeek RegMove RegPoint; do
   grep -q "fn-ptr arg.*: .*$fn" "$TMP/err" 2>/dev/null
done
if grep -qE "'(long long|.*CGScreenUpdateMoveDelta|.*Point[^e])" "$TMP/err" 2>/dev/null; then :; fi
# precise: seekfn's long long return + the two int-structs must not appear as unsupported
grep -q 'callback return type unsupported: long long' "$TMP/err" \
   && note "long long callback return still unsupported (CBR_I64 not wired)"
grep -q 'callback arg type unsupported: struct CGScreenUpdateMoveDelta' "$TMP/err" \
   && note "8B int-struct callback arg still unsupported (CBA_I64 not wired)"
grep -q 'callback arg type unsupported: Point' "$TMP/err" \
   && note "4B int-struct callback arg still unsupported (CBA_I32 not wired)"

# descriptor rows are "idx|nargs|ret|arg0, arg1, ...".
# (b) a CBR_I64 (ret=5) descriptor whose seekfn args are PTR(2),I64(1),I32(0):
if ! printf '%s\n' "$descs" | grep -qE '\|5\|2, 1, 0,'; then
   note "no CBR_I64 (ret=5) descriptor with [PTR,I64,I32] args (seekfn) emitted"
fi

# (c) the 8B struct arg -> first arg code I64(1), then PTR(2):
if ! printf '%s\n' "$descs" | grep -qE '\|0\|1, 2,'; then
   note "no [I64, PTR] descriptor (8B CGScreenUpdateMoveDelta struct arg -> CBA_I64)"
fi

# (d) the 4B struct arg -> first arg code I32(0), then I32(0):
if ! printf '%s\n' "$descs" | grep -qE '\|0\|0, 0,'; then
   note "no [I32, I32] descriptor (4B Point struct arg -> CBA_I32)"
fi

# (e) the FP-bearing struct MUST remain unsupported (raw fallback) — not encoded
grep -q 'callback arg type unsupported: FPair' "$TMP/err" \
   || note "FP-bearing struct arg was wrongly encoded (would arrive in xmm, mis-marshalled)"

if [ "$fail" -eq 0 ]; then echo "PASS cb-extended"; exit 0; fi
exit 1
