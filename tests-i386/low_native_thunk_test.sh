#!/bin/bash
#
# low-native-thunk — two-arm guard for
#   src/abiconv/posix_shim.c: fnptr_lookup_result / addr_is_translated_code
#
# THE DEFECT. fnptr_lookup_result used to hand ANY sub-4GB dlsym result straight
# back to the i386 caller, its comment reading "translated/low: callable". That
# conflates two different questions:
#
#     "fits in 32 bits"          -- where dyld happened to map an image
#     "speaks the i386 ABI"      -- how the code was built
#
# They coincide for every image the pipeline PRODUCES (based at 0x10000000) and
# for system frameworks (always in the high shared cache), and they come apart
# the moment dyld maps a NATIVE dylib low, which it does whenever there is room.
#
# MEASURED, Portal 2 2026-09-14: modern steamclient.dylib is x86_64 + arm64 with
# NO i386 slice, and dyld mapped its 24 MB at 0x2143f000. libsteam_api dlsym'd
# `CreateInterface`, got the raw low address back as "callable", and called
# native SysV code with an i386 cdecl frame:
#     SIGBUS err=0x6, `movl $1,(%rbx)`, rbx inside steamclient's OWN r-x __TEXT,
#     while %rsp was still the low, 4-byte-aligned TRANSLATED stack.
# That pairing -- native rip, low 4-byte-aligned rsp -- is the signature of a
# cross-ABI call with no bridge. dlsym is the SECOND delivery mechanism of
# the unbridged native-call bug, and the one unbridged-native-calls.py
# structurally cannot see (it audits static binds; a dlsym result does not exist
# at translate time).
#
# THE FIX asks the question we mean, reusing the structural test posix_shim.c
# already defines for the mirror-image case (translated_provider_for): an image
# is translated iff it carries an LC_LOAD_DYLIB naming libabiconv. A low NATIVE
# function now gets the same callable thunk a high one always did.
#   Kill switch: M64_NO_LOW_NATIVE_THUNK=1.
#
# ARMS (the same translated binary, run twice):
#   ON  : the low native `m64_lownative_triple` is bridged through a thunk that
#         marshals the i386 cdecl frame into SysV -> triple(14) = 42, exit 0.
#   OFF : the raw low native address comes back and the i386 caller calls it
#         directly, so the callee reads %edi instead of the stack argument.
#         Wrong answer or a crash -- the unfixed behaviour, EXECUTED.
#
# ⚠THE GUARD MUST PROVE IT IS EXERCISING THE LOW BRANCH. If the helper ever gets
# mapped ABOVE 4GB, the ON arm would still print 42 via the ordinary high-native
# path and the guard would pass while testing nothing. So the ON arm also asserts
# the "LOW NATIVE function ... bridged via thunk" trace line, which is emitted
# only on the new branch.
set -u
cd "$(dirname "$0")"

BIN=build/99_low_native_thunk.x86_64
HELPER=build/lownative_helper.dylib
LIBABICONV=../build/src/abiconv/libabiconv.dylib

if [ ! -x "$BIN" ] || [ ! -f "$HELPER" ]; then
    echo "low-native-thunk: SKIP (missing $BIN or $HELPER — needs the i386"
    echo "  sysroot; run 'make sysroot' then 'make low-native-thunk')"
    exit 0
fi

# The helper must be genuinely NATIVE: x86_64, and carrying no libabiconv load
# command (that is the exact test the fix keys on).
if ! file "$HELPER" | grep -q "x86_64"; then
    echo "low-native-thunk: FAIL — $HELPER is not an x86_64 dylib"
    exit 1
fi
if otool -L "$HELPER" 2>/dev/null | grep -q libabiconv; then
    echo "low-native-thunk: FAIL — $HELPER links libabiconv, so it would be"
    echo "  classified TRANSLATED and the guard would test nothing"
    exit 1
fi

fail=0

on_out="$(POSIX_TRACE=1 "$BIN" 2>&1)";                          on_rc=$?
off_out="$(POSIX_TRACE=1 M64_NO_LOW_NATIVE_THUNK=1 "$BIN" 2>&1)"; off_rc=$?

echo "$on_out"  | grep -E "LOW NATIVE|triple=|dlsym:" | sed 's/^/  ON  /'
echo "$off_out" | grep -E "LOW NATIVE|triple=|dlsym\(" | sed 's/^/  OFF /'

# ── ON: bridged, correct, and provably via the LOW branch
if [ "$on_rc" = 0 ] && [ "${on_out#*triple=42}" != "$on_out" ]; then
    if [ "${on_out#*LOW NATIVE function}" != "$on_out" ]; then
        echo "  ON  (fix active):  low NATIVE function bridged via thunk;"
        echo "                     triple(14) -> 42                          OK"
    else
        echo "  ON  (fix active):  got 42, but the 'LOW NATIVE function' trace is"
        echo "                     ABSENT — the helper was not mapped below 4GB, so"
        echo "                     this run did NOT exercise the branch under test"
        fail=1
    fi
else
    echo "  ON  (fix active):  expected triple=42 and exit 0, got exit $on_rc —"
    echo "                     the fix is NOT working"
    fail=1
fi

# ── OFF: the raw low native address is called with the i386 frame.
# A crash is an equally valid reproduction (whether the callee's misread frame
# faults or merely returns nonsense is not ours to control), so the assertion is
# "did NOT produce 42".
if [ "${off_out#*triple=42}" = "$off_out" ]; then
    echo "  OFF (kill switch): the raw low native address was called with the"
    echo "                     i386 cdecl frame and did not yield 42 (exit"
    echo "                     $off_rc) — the unfixed behaviour reproduces    OK"
else
    echo "  OFF (kill switch): still produced 42 — the kill switch is not"
    echo "                     reaching the code path, so this guard is INERT"
    fail=1
fi

# The two arms MUST differ, or the guard asserts nothing whatever it prints.
if [ "$on_out" = "$off_out" ]; then
    echo "  both arms identical — the kill switch has no effect"
    fail=1
fi

# Static wiring: the kill switch must exist in the BUILT libabiconv, not merely
# in the source tree — a stale dylib would make the OFF arm silently green.
if [ -f "$LIBABICONV" ] && ! strings "$LIBABICONV" | grep -q M64_NO_LOW_NATIVE_THUNK; then
    echo "  the built libabiconv has no M64_NO_LOW_NATIVE_THUNK — it is STALE"
    fail=1
fi

if [ "$fail" = 0 ]; then echo "low-native-thunk: PASS"; else echo "low-native-thunk: FAIL"; fi
exit $fail
