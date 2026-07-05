#!/bin/bash
#
# Regression test for the opaque-Handle-in-struct marshalling fix (abigen
# typeconv.cc is_opaque_handle_type / convert_record; Civ IV 'oapp'-launch
# AppleEvent crash, 2026-07-05).
#
# A classic Mac OS Memory Manager Handle (AEDesc::dataHandle is
# AEDataStorage = Ptr* = char**) is an OPAQUE token — a pointer to a
# relocatable master pointer. abigen used to DEEP-COPY the struct field-by-
# field, and for the Handle field convert_pointer(char**) DEREFERENCED the
# handle to read the master pointer — the handle value is a token, not a
# readable data pointer -> EXC_BAD_ACCESS at runtime. The fix recognizes the
# Handle family (typedef named Handle/AEDataStorage/*Handle whose canonical is
# a T** pointer-to-pointer) and marshals the pointer VALUE instead of deep-
# copying/dereferencing it.
#
# Self-contained: abigen parses a synthetic header (no i386 sysroot needed).
# Asserts on the generated asm:
#   (a) the shim for a function taking a struct with a `Handle` field marshals
#       the Handle as a pointer value (the "opaque Handle field" path fires);
#   (b) a struct with a GENUINE `int **` out-field (NOT a Handle typedef) is
#       still deep-copied (the fix is NAME+STRUCTURE scoped, not "any T**").
set -u

MT="${1:-}"
PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ABIGEN="$PROJ_ROOT/build/src/abiconv/abigen"
[ -x "$ABIGEN" ] || { echo "SKIP handle-marshal (abigen not built at $ABIGEN)"; exit 0; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/hdr.h" <<'EOF'
typedef char *Ptr;
typedef Ptr  *Handle;                 /* classic Memory Manager Handle */
typedef Handle AEDataStorage;         /* AEDesc's field type (aliased) */
/* AEDesc-shaped: i386 {4,4}=8 vs x86_64 {4,8}=16 -> layouts differ -> deep-copy */
struct MyAEDesc { unsigned int descriptorType; AEDataStorage dataHandle; };
extern int probe_ae(struct MyAEDesc *desc);
/* CONTROL: a genuine int** out-field (NOT a Handle) must still be deep-copied */
struct MyOut { unsigned int n; int **rows; };
extern int probe_out(struct MyOut *o);
EOF
printf '_probe_ae\n_probe_out\n' > "$TMP/syms.txt"

"$ABIGEN" -s "$TMP/syms.txt" -o "$TMP/out.asm" "$TMP/hdr.h" 2>/dev/null \
   || { echo "FAIL handle-marshal (abigen error)"; exit 1; }

# probe_ae's shim body (up to the next global) — the AEDataStorage Handle field.
ae_body="$(awk '/^___probe_ae:/{f=1} f{print} /^___probe_out:/{exit}' "$TMP/out.asm")"
out_body="$(awk '/^___probe_out:/{f=1} f{print}' "$TMP/out.asm")"

ae_handle="$(printf '%s\n' "$ae_body" | grep -c 'opaque Handle field')"
out_handle="$(printf '%s\n' "$out_body" | grep -c 'opaque Handle field')"

if [ "$ae_handle" -lt 1 ]; then
   echo "FAIL handle-marshal (AEDataStorage Handle field was NOT marshalled as a value — deep-copied/dereferenced)"
   exit 1
fi
if [ "$out_handle" -ne 0 ]; then
   echo "FAIL handle-marshal (genuine int** field wrongly treated as an opaque Handle — over-broadened)"
   exit 1
fi
echo "PASS handle-marshal"
