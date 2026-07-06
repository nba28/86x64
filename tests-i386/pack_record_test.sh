#!/bin/bash
#
# Regression guard for the #pragma pack record-LAYOUT fix (abigen typeconv.cc
# convert_record + loc.cc align_field pack_cap; Civ 'oapp' AEGetParamDesc crash).
#
# Classic Carbon AppleEvent structs are #pragma pack(2): AEDesc is
# { DescType descriptorType@0; AEDataStorage dataHandle@+4 }, sizeof 12 — the
# Handle sits at +4, NOT the natural +8. abigen built the native x86_64 record
# with NATURAL alignment (dataHandle @+8), so the native AE callee reading it at
# +4 straddled two fields -> a garbage handle pointer -> EXC_BAD_ACCESS
# (AEGetParamDesc + 820, addr 0x..._00000008). The fix caps each field's
# alignment at the struct's effective packing (clang_Type_getAlignOf, which folds
# in the pragma): 2 for AEDesc, so dataHandle lands at +4 on BOTH the x86_64 and
# i386 sides. Unpacked structs are unaffected (cap == natural max-field-align).
#
# Self-contained: abigen parses a synthetic header + nasm assembles (no i386
# sysroot). Asserts, in the generated deep-copy shim:
#   (a) a pack(2) AEDesc-shaped record writes its Handle field at dst [+4];
#   (b) an otherwise identical NATURALLY-aligned record writes it at dst [+8]
#       (the fix is scoped to packed structs, not "shift every struct").
set -u

PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ABIGEN="$PROJ_ROOT/build/src/abiconv/abigen"
[ -x "$ABIGEN" ] || { echo "SKIP pack-record (abigen not built at $ABIGEN)"; exit 0; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/hdr.h" <<'EOF'
typedef unsigned int DescType;
typedef char  *Ptr;
typedef Ptr   *AEDataStorage;              /* a classic Memory Manager Handle */
#pragma pack(push, 2)
struct AEDesc { DescType descriptorType; AEDataStorage dataHandle; };  /* pack(2): 12B, handle @+4 */
#pragma pack(pop)
struct NatDesc { DescType descriptorType; AEDataStorage dataHandle; }; /* natural: 16B, handle @+8 */
extern int probe_ae(struct AEDesc *d);
extern int probe_nat(struct NatDesc *d);
EOF
printf '_probe_ae\n_probe_nat\n' > "$TMP/syms"

"$ABIGEN" -o "$TMP/out.asm" -s "$TMP/syms" "$TMP/hdr.h" 2> "$TMP/err" \
   || { echo "FAIL pack-record: abigen exited nonzero"; cat "$TMP/err"; exit 1; }

body() { awk "/^___$1:/{f=1} f{print} /^___$2:/{exit}" "$TMP/out.asm"; }
fail=0
note() { echo "FAIL pack-record: $1"; fail=1; }

# (0) assembles
NASM="$(command -v nasm || true)"
if [ -n "$NASM" ]; then
   "$NASM" -f macho64 "$TMP/out.asm" -o "$TMP/out.o" 2> "$TMP/nasm.err" \
      || { note "generated asm does not assemble"; cat "$TMP/nasm.err"; }
else
   echo "note pack-record: nasm not found, skipping assemble check"
fi

ae="$(body probe_ae probe_nat)"
nat="$(awk '/^___probe_nat:/{f=1} f{print} f&&/jmp\tr11/{exit}' "$TMP/out.asm")"

# (a) pack(2): the Handle field (8 bytes on x86_64) is stored at dst [r11+4]
echo "$ae" | grep -Eq 'mov[[:space:]]*qword +\[r11\+4\]' \
   || note "pack(2) AEDesc: dataHandle NOT written at packed offset +4"
echo "$ae" | grep -Eq 'mov[[:space:]]*qword +\[r11\+8\]' \
   && note "pack(2) AEDesc: dataHandle wrongly written at natural +8"

# (b) natural: the Handle field IS at dst [r11+8]
echo "$nat" | grep -Eq 'mov[[:space:]]*qword +\[r11\+8\]' \
   || note "natural NatDesc: dataHandle NOT at natural offset +8 (fix over-broadened to unpacked structs)"

if [ "$fail" -eq 0 ]; then echo "PASS pack-record"; exit 0; fi
exit 1
