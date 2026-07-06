#!/bin/bash
#
# Regression guard for the record-VALUE data-shadow fix (abigen.cc
# handle_var_decl CXType_Record case + objc_slide.c load-time redirect).
#
# A small struct-VALUE data constant — e.g. HIToolbox's
# `const HIViewID kHIViewWindowContentID` = { OSType signature; SInt32 id }, 8B —
# is read by i386 as `movl slot,%reg; movl (%reg); movl 0x4(%reg)` (load &var,
# then read the fields). The real 64-bit &var truncates on the 4-byte load and
# the field read faults (Civ EULADialog+356 -> HIViewFindByID). abigen shadows
# object, scalar and opaque-CF data constants but hit `default: return` for a
# record VALUE global, leaving it native-bound -> truncation crash. The fix adds
# a CXType_Record case that shadows small (<=8B) record constants exactly like
# the scalar case (a low-4GB value COPY, info = byte size, memcpy'd at runtime).
#
# Self-contained: abigen parses a synthetic header (no i386 sysroot). Asserts:
#   (a) an 8-byte record VALUE constant IS shadowed (___SYM emitted, table
#       info = 8 so x64_init_data_shadows/x64_value_data_shadow memcpy 8 bytes);
#   (b) a >8-byte record is NOT shadowed (fits-the-8B-slot cap, mirrors scalars).
set -u

PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ABIGEN="$PROJ_ROOT/build/src/abiconv/abigen"
[ -x "$ABIGEN" ] || { echo "SKIP record-shadow (abigen not built at $ABIGEN)"; exit 0; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/hdr.h" <<'EOF'
typedef unsigned int OSType;
typedef int SInt32;
struct ViewIDLike { OSType signature; SInt32 id; };   /* 8B record, HIViewID-shaped */
extern const struct ViewIDLike kMyViewID;
struct BigRec { long long a; long long b; };            /* 16B record — too big */
extern const struct BigRec kMyBigRec;
EOF
printf '_kMyViewID\n_kMyBigRec\n' > "$TMP/syms"

"$ABIGEN" -o "$TMP/out.asm" -s "$TMP/syms" "$TMP/hdr.h" 2> "$TMP/err" \
   || { echo "FAIL record-shadow: abigen exited nonzero"; cat "$TMP/err"; exit 1; }

fail=0
note() { echo "FAIL record-shadow: $1"; fail=1; }

# (0) assembles
NASM="$(command -v nasm || true)"
if [ -n "$NASM" ]; then
   "$NASM" -f macho64 "$TMP/out.asm" -o "$TMP/out.o" 2> "$TMP/nasm.err" \
      || { note "generated asm does not assemble"; cat "$TMP/nasm.err"; }
fi

# (a) the 8B record is shadowed with a low-4GB copy slot + table info = 8
grep -Eq '^___kMyViewID: dq 0' "$TMP/out.asm" \
   || note "8B record kMyViewID: shadow ___kMyViewID NOT emitted (record case missed)"
# the table triple is: dq ___kMyViewID / dq _x64_dsn_N / dq <info>
info="$(awk '/dq ___kMyViewID$/{getline; getline; print}' "$TMP/out.asm" | grep -oE '[0-9]+' | head -1)"
[ "$info" = "8" ] \
   || note "8B record kMyViewID: table info=$info, expected 8 (memcpy width wrong)"

# (b) the 16B record is NOT shadowed (only records that fit the 8-byte slot)
grep -q 'kMyBigRec' "$TMP/out.asm" \
   && note ">8B record kMyBigRec wrongly shadowed (must respect the 8-byte cap)"

if [ "$fail" -eq 0 ]; then echo "PASS record-shadow"; exit 0; fi
exit 1
