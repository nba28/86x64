#!/bin/bash
#
# Regression guard for the legacy fixed-32 by-value-struct SIZING fix (abigen
# byval_field_is_int32; Civ's HIViewFindByID wall).
#
# The legacy (-arch i386) header parse canonicalizes the fixed-width 32-bit Mac
# typedefs (SInt32/UInt32/OSType/FourCharCode/OSStatus/...) to `long` (4 bytes on
# i386). abigen sized the x86_64 field from that canonical `long` = 8 bytes, so an
# all-int 8-byte record like HIViewID/ControlID {OSType signature; SInt32 id}
# became 16 bytes -> TWO SysV eightbytes -> TWO GP regs, shifting the NEXT arg.
# But native x86_64 (built from the modern headers where these are `int`) passes
# the 8-byte record in ONE GP reg. The fix: a `long`-canonical field whose
# AS-WRITTEN typedef chain ends in "32" is a fixed-32 type worth 4 bytes on x86_64.
#
# This guard mimics the legacy canonical with `typedef long SInt32` (name ends
# "32", canonical long) so it reproduces WITHOUT the 10.6 SDK, and uses a genuine
# pointer-width `typedef long CFIndex` (name does NOT end "32") as the control that
# must STILL span two eightbytes. Self-contained (abigen + nasm; no i386 sysroot).
set -u

PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ABIGEN="$PROJ_ROOT/build/src/abiconv/abigen"
[ -x "$ABIGEN" ] || { echo "SKIP byval-int32 (abigen not built at $ABIGEN)"; exit 0; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/hdr.h" <<'EOF'
typedef long          SInt32;     /* fixed-32: name ends "32", canonical long    */
typedef unsigned long UInt32;     /* fixed-32: name ends "32"                     */
typedef UInt32        FourCharCode;
typedef FourCharCode  OSType;     /* fixed-32 via chain OSType->FourCharCode->UInt32 */
typedef long          CFIndex;    /* pointer-width: name does NOT end "32"        */

/* all-int 8-byte fixed-32 record ARG followed by a POINTER arg: the record must
 * ride ONE GP reg and the pointer must NOT be shifted. */
struct ControlID { OSType signature; SInt32 id; };
extern int HIViewFindByID_probe(void *inStartView, struct ControlID inID, void **outView);

/* CONTROL: a genuine pointer-width {long,long} record (CFRange shape) must STILL
 * occupy TWO eightbytes (must NOT be shrunk to one). */
struct CFRange { CFIndex location; CFIndex length; };
extern int TakeRange_probe(struct CFRange r, void *after);
EOF
printf '_HIViewFindByID_probe\n_TakeRange_probe\n' > "$TMP/syms"

"$ABIGEN" -o "$TMP/out.asm" -s "$TMP/syms" "$TMP/hdr.h" 2> "$TMP/err" \
   || { echo "FAIL byval-int32: abigen exited nonzero"; cat "$TMP/err"; exit 1; }

body() { awk "/^___$1:/{f=1} f{print} f&&/call _$1/{exit}" "$TMP/out.asm"; }
fail=0
note() { echo "FAIL byval-int32: $1"; fail=1; }

# (0) assembles
NASM="$(command -v nasm || true)"
if [ -n "$NASM" ]; then
   "$NASM" -f macho64 "$TMP/out.asm" -o "$TMP/out.o" 2> "$TMP/nasm.err" \
      || { note "generated asm does not assemble"; cat "$TMP/nasm.err"; }
fi

hv="$(body HIViewFindByID_probe)"
tr="$(body TakeRange_probe)"

# (a) the fixed-32 record's fields are staged as 4-byte DWORD stores (not qword)
echo "$hv" | grep -Eq 'mov[[:space:]]*dword +\[rsp\+' \
   || note "ControlID fixed-32 fields not staged as 4-byte dword (still sized as 8-byte long?)"
# (b) it rides exactly ONE eightbyte in a GP reg (rsi = 2nd integer arg), loaded
#     from the staging slot [rsp+N]
echo "$hv" | grep -Eq 'mov[[:space:]]*rsi,[[:space:]]*qword +\[rsp\+[0-9]+\]' \
   || note "ControlID did not load its single eightbyte into rsi"
# outView (3rd arg) must reach rdx (as the pointer r11), not be shifted to rcx
echo "$hv" | grep -Eq 'mov[[:space:]]*rdx,[[:space:]]*r11' \
   || note "outView pointer arg did not reach rdx (shifted?)"
# the record must NOT spill a SECOND eightbyte into rdx from the staging slot
# (the over-sized-to-16-bytes bug signature)
if echo "$hv" | grep -Eq 'mov[[:space:]]*rdx,[[:space:]]*qword +\[rsp\+[0-9]+\]'; then
   note "ControlID wrongly loaded a SECOND eightbyte into rdx (over-sized to 16 bytes)"
fi

# (c) CONTROL: the genuine {long,long} record IS staged as 8-byte qword and rides
#     TWO eightbytes (rdi + rsi) — the fix must not over-shrink pointer-width longs
echo "$tr" | grep -Eq 'mov[[:space:]]*qword +\[rsp\+' \
   || note "CFRange genuine long fields not staged as 8-byte qword"
echo "$tr" | grep -Eq 'mov[[:space:]]*rdi,[[:space:]]*qword' || note "CFRange eightbyte0 not in rdi"
echo "$tr" | grep -Eq 'mov[[:space:]]*rsi,[[:space:]]*qword' \
   || note "CFRange eightbyte1 not in rsi (wrongly shrunk to one eightbyte)"

if [ "$fail" -eq 0 ]; then echo "PASS byval-int32"; exit 0; fi
exit 1
