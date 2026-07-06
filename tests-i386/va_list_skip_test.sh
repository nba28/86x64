#!/bin/bash
#
# Regression guard for the abigen va_list-arg SKIP (C6 marshalling-completeness).
#
# abigen cannot marshal a va_list arg: an i386 va_list is a plain char* pointing
# at the stack varargs, whereas x86_64's is a __va_list_tag register-save-area
# struct, and translating it needs the arg TYPES (only the callee's format string
# yields those at runtime). abigen's generic Pointer/ConstantArray path used to
# deep-copy the i386 char* AS a __va_list_tag (16 garbage bytes) -> native va_arg
# dereferences a fused overflow_arg_area -> SIGSEGV (Portal-2 class). abigen now
# DETECTS a va_list arg (structurally, by the __va_list_tag record) and SKIPS the
# function so no actively-broken shim is emitted; correct v* functions get a
# per-function hand-shim (printf-conv.cc; CFStringCreateWithFormatAndArguments in
# objc_shim.c, exercised at runtime by 53_cf_format_valist).
#
# Self-contained (abigen only; no i386 sysroot). Asserts a va_list-taking function
# is SKIPPED (reported) and NOT emitted with a __va_list_tag deep-copy, while an
# ordinary (non-va_list) function IS still emitted.
set -u

PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ABIGEN="$PROJ_ROOT/build/src/abiconv/abigen"
[ -x "$ABIGEN" ] || { echo "SKIP va-list-skip (abigen not built at $ABIGEN)"; exit 0; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/hdr.h" <<'EOF'
typedef __builtin_va_list va_list;
extern int myvfn(const char *fmt, va_list ap);       /* va_list arg -> SKIP */
extern int myplain(const char *fmt, int a, int b);   /* ordinary -> emitted */
EOF
printf '_myvfn\n_myplain\n' > "$TMP/syms"

"$ABIGEN" -o "$TMP/out.asm" -s "$TMP/syms" "$TMP/hdr.h" 2> "$TMP/err" \
   || { echo "FAIL va-list-skip: abigen exited nonzero"; cat "$TMP/err"; exit 1; }

fail=0
note() { echo "FAIL va-list-skip: $1"; fail=1; }

# (a) the va_list function is reported skipped as un-marshallable
grep -q 'va_list arg not marshallable' "$TMP/err" \
   || note "va_list function was NOT reported as skipped (detection missing)"

# (b) NO shim was emitted for it, and specifically NO __va_list_tag deep-copy
grep -q '^___myvfn:' "$TMP/out.asm" \
   && note "a shim was emitted for the va_list function (should be skipped)"
grep -q "convert 'struct __va_list_tag" "$TMP/out.asm" \
   && note "the broken __va_list_tag deep-copy was emitted"

# (c) the ordinary function is still emitted (skip is scoped to va_list args)
grep -q '^___myplain:' "$TMP/out.asm" \
   || note "the ordinary (non-va_list) function was NOT emitted (over-broadened)"

# (d) whatever WAS emitted still assembles
NASM="$(command -v nasm || true)"
if [ -n "$NASM" ]; then
   "$NASM" -f macho64 "$TMP/out.asm" -o "$TMP/out.o" 2> "$TMP/nasm.err" \
      || { note "generated asm does not assemble"; cat "$TMP/nasm.err"; }
fi

if [ "$fail" -eq 0 ]; then echo "PASS va-list-skip"; exit 0; fi
exit 1
