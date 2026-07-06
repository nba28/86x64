#!/bin/bash
#
# Regression test for the bare opaque-Handle ARGUMENT marshalling fix (abigen
# is_opaque_handle_type at the top-level arg dispatch; the companion to
# handle-marshal, which covers a Handle FIELD inside a by-value struct).
#
# A classic Mac OS Memory Manager Handle (char**, spelled Handle / AEDataStorage
# / a *Handle typedef) passed DIRECTLY as a function argument is an OPAQUE token
# — a pointer to a relocatable master pointer. abigen's generic Pointer arg path
# would convert_pointer(char**) = DEREFERENCE the handle to read the master
# pointer (mov rN,[rbp+off]; mov ...,[rN]), and the handle value is a token, not
# a readable data pointer -> EXC_BAD_ACCESS. The fix recognizes the Handle family
# (same is_opaque_handle_type predicate as the struct-field case) and marshals
# the pointer VALUE (mov reg,[rbp+off]) instead, with no copy-back.
#
# Real imports served: GetDialogItemText/SetDialogItemText (Halo), HLock/HUnlock/
# DisposeHandle/HandleToHandle (Resource+Memory Manager), AppleEvent AEDesc
# handles, control/menu *Handle Toolbox calls.
#
# Self-contained: abigen parses a synthetic header + nasm assembles the output
# (no i386 sysroot). Asserts on the generated asm:
#   (a) a bare Handle arg fires the "opaque Handle arg" pointer-VALUE path;
#   (b) a *Handle-suffixed opaque handle typedef (ControlHandle) fires it too;
#   (c) the Handle shim does NOT dereference the handle (no `[rN+0]` read of the
#       loaded handle pointer), unlike a genuine out-pointer;
#   (d) a genuine `int **` out-param is STILL deep-copied (name+structure scoped,
#       not "any T**") — the fix does not over-broaden.
set -u

PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ABIGEN="$PROJ_ROOT/build/src/abiconv/abigen"
[ -x "$ABIGEN" ] || { echo "SKIP handle-arg (abigen not built at $ABIGEN)"; exit 0; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/hdr.h" <<'EOF'
typedef char *Ptr;
typedef Ptr  *Handle;                       /* classic Memory Manager Handle */
typedef Ptr  *AEDataStorage;                /* AppleEvent handle alias        */
typedef unsigned char Str255[256];
typedef struct OpaqueControlHandle **ControlHandle;   /* *Handle-suffixed typedef */
extern void GetDialogItemText(Handle item, Str255 text);
extern void HLock(Handle h);
extern void DisposeHandle(Handle h);
extern void AEStash(AEDataStorage s);
extern void SizeControl(ControlHandle c, short w, short h);
/* CONTROL: a genuine int** out-param (NOT a Handle typedef) must be deep-copied */
extern void GetCount(int **outCount);
EOF
printf '_GetDialogItemText\n_HLock\n_DisposeHandle\n_AEStash\n_SizeControl\n_GetCount\n' > "$TMP/syms.txt"

"$ABIGEN" -s "$TMP/syms.txt" -o "$TMP/out.asm" "$TMP/hdr.h" 2>/dev/null \
   || { echo "FAIL handle-arg (abigen error)"; exit 1; }

body() { awk "/^___$1:/{f=1} f{print} f&&/jmp\tr11/{exit}" "$TMP/out.asm"; }
fail=0
note() { echo "FAIL handle-arg: $1"; fail=1; }

# (0) the generated asm must assemble
NASM="$(command -v nasm || true)"
if [ -n "$NASM" ]; then
   "$NASM" -f macho64 "$TMP/out.asm" -o "$TMP/out.o" 2> "$TMP/nasm.err" \
      || { note "generated asm does not assemble"; cat "$TMP/nasm.err"; }
else
   echo "note handle-arg: nasm not found, skipping assemble check"
fi

# (a) bare Handle args fire the opaque-Handle pointer-value path
for fn in GetDialogItemText HLock DisposeHandle AEStash; do
   body "$fn" | grep -q 'opaque Handle arg' || note "$fn: Handle arg not value-marshalled (deep-copied/dereferenced)"
done

# (b) *Handle-suffixed opaque typedef also fires it
body SizeControl | grep -q 'opaque Handle arg' || note "SizeControl: ControlHandle not recognized as opaque Handle"

# (c) the Handle shim must NOT dereference the loaded handle. The generic deep-
#     copy loads the handle into a scratch GP reg then reads THROUGH it ([rN+0]);
#     the value path only does `mov <argreg>,[rbp+off]`. Assert no through-reg
#     read appears in HLock (a lone Handle arg, nothing else to marshal).
if body HLock | grep -Eq 'mov[[:space:]]+e?[a-d]x,[[:space:]]*dword +\[r(ax|bx|cx|dx|1[0-5])\+0\]'; then
   note "HLock: handle is dereferenced (deep-copy) instead of value-marshalled"
fi

# (d) genuine int** out-param is still deep-copied (dereferences through a reg)
grep -q '^___GetCount:' "$TMP/out.asm" || note "GetCount not emitted"
body GetCount | grep -q 'opaque Handle arg' && note "GetCount: int** wrongly treated as an opaque Handle (over-broadened)"
body GetCount | grep -Eq '\[r(ax|1[0-5])\+0\]' || note "GetCount: int** out-param not deep-copied (dereference missing)"

if [ "$fail" -eq 0 ]; then echo "PASS handle-arg"; exit 0; fi
exit 1
