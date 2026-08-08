#!/bin/bash
#
# translated-provider-defer — two-arm guard for
#   src/abiconv/posix_shim.c: translated_provider_for / lookup_by_name_target
#
# THE BLOCKER, reproduced (Civilization IV, 2026-08-08):
#   SIGSEGV KERN_INVALID_ADDRESS at 0x11
#     #0  ?+0x100a338c8                        (native-convention call debris)
#     #1  libabiconv+0x22db86  ___Py_Initialize+0x33     <- OUR abigen bridge
#
#   An abigen bridge `___X` is an i386-callable entry whose whole purpose is to
#   reach a NATIVE `_X`: it lifts the i386 cdecl frame to the x86_64 SysV ABI,
#   `call`s out (8-byte return address), and returns with the i386 4-byte `ret`.
#   Apple removed Python 2 in macOS 12.3, so no native `_Py_Initialize` remains;
#   the bundle vendors our own TRANSLATED Python 2.6, whose code still speaks the
#   i386 convention. Put the bridge in front of it and every call double-converts.
#
#   ⚠The bind tables are innocent. `Civilization IV.dylib` lazy-binds
#   `Python/_Py_Initialize` correctly; `_Py_Initialize` is already in
#   libabiconv.nulljump so static-interpose never redirected it; NOTHING in the
#   bundle binds `___Py_Initialize`. The detour is minted at RUN TIME by the
#   by-name lookup family (Civ imports ___NSIsSymbolNameDefined /
#   ___NSLookupAndBindSymbol / ___NSAddressOfSymbol), whose resolver PREFERS
#   libabiconv's interpose shim. That preference is the documented fix for the
#   raw-thunk defects (shim_dlsym c62c908, ns_symbol_resolve 632b1e5, the
#   CFBundle twin b30ccd9). This is its MIRROR IMAGE: when the provider is itself
#   TRANSLATED, the resolver must DEFER.
#
# THE FIX (universal, structural, keyed on Mach-O shape and nothing else): before
#   returning a bridge, ask whether a TRANSLATED image in this process defines
#   the symbol — translated iff it carries an LC_LOAD_DYLIB naming libabiconv —
#   and if so hand back that definition. Deferral cannot fire for an app with no
#   translated provider, so every other target keeps its bridges.
#   Kill switch: M64_NO_TRANSLATED_PROVIDER_DEFER=1.
#
# ARMS (the SAME translated binary, run twice; the OFF arm must disagree):
#   ON  : the by-name lookup returns the fixture's own translated
#         `_PyInt_AsLong` -> 21 doubles to 42.
#   OFF : the lookup returns ___PyInt_AsLong; the fixture calls the bridge with
#         an i386 frame; the bridge re-enters that same translated function
#         through its <flat-namespace>/_PyInt_AsLong lazy bind with the NATIVE
#         convention, so the callee reads a stack slot that carries no argument.
#         THE DOUBLE CONVERSION, EXECUTED — not merely an address comparison.
set -u
cd "$(dirname "$0")"

BIN=build/99_translated_provider_defer.x86_64
LIBABICONV=../build/src/abiconv/libabiconv.dylib

if [ ! -x "$BIN" ]; then
    echo "translated-provider-defer: SKIP (no $BIN — needs the i386 sysroot;"
    echo "  run 'make sysroot' then 'make $BIN')"
    exit 0
fi

# The guard is only meaningful while libabiconv still carries a bridge for the
# chosen name; without one there is nothing to defer FROM and both arms agree.
if ! nm -gU "$LIBABICONV" 2>/dev/null | grep -q " T ___PyInt_AsLong$"; then
    echo "translated-provider-defer: SKIP — libabiconv exports no ___PyInt_AsLong"
    echo "  bridge. Pick another name libabiconv bridges and that no native"
    echo "  library defines, and update the fixture to match."
    exit 0
fi

fail=0

on_out="$("$BIN" 2>&1)";                                   on_rc=$?
off_out="$(M64_NO_TRANSLATED_PROVIDER_DEFER=1 "$BIN" 2>&1)"; off_rc=$?

echo "$on_out"  | sed 's/^/  ON  /'
echo "$off_out" | sed 's/^/  OFF /'

# ── ON: the lookup must return the translated provider, and the call must work
if [ "$on_rc" = 0 ] && [ "${on_out#*is_provider=1}" != "$on_out" ] \
                    && [ "${on_out#*byname=42}"     != "$on_out" ]; then
    echo "  ON  (deferral active): by-name lookup returned the TRANSLATED"
    echo "                         provider; 21 -> 42                        OK"
else
    echo "  ON  (deferral active): expected is_provider=1 and byname=42 (exit 0),"
    echo "                         got exit $on_rc — the fix is NOT working"
    fail=1
fi

# ── OFF: the bridge comes back instead, and the call is double-converted.
# A crash is an equally valid reproduction (whether the callee's short `ret`
# lands on a valid PC depends on where libabiconv happens to be mapped), so the
# assertion is "did NOT return the provider, and did NOT produce 42".
if [ "$off_rc" != 0 ] && [ "${off_out#*is_provider=1}" = "$off_out" ] \
                      && [ "${off_out#*byname=42}"     = "$off_out" ]; then
    echo "  OFF (kill switch):     lookup returned libabiconv's ___PyInt_AsLong"
    echo "                         bridge and the call double-converted — the"
    echo "                         unfixed behaviour reproduces               OK"
else
    echo "  OFF (kill switch):     expected the bridge (is_provider=0) and a"
    echo "                         wrong result, got exit $off_rc — this guard is"
    echo "                         NOT exercising the deferral"
    fail=1
fi

# The two arms MUST differ, or the guard is inert whatever it asserts.
if [ "$on_out" = "$off_out" ]; then
    echo "  both arms identical — the kill switch has no effect"
    fail=1
fi

# Static wiring: the kill switch has to exist in the BUILT libabiconv, not just
# in the source tree (a stale dylib would make the OFF arm silently green).
if ! strings -a "$LIBABICONV" 2>/dev/null | grep -q "M64_NO_TRANSLATED_PROVIDER_DEFER"; then
    echo "  kill-switch string absent from the built libabiconv (stale build?)"
    fail=1
fi

if [ "$fail" = 0 ]; then
    echo "translated-provider-defer: PASS"
else
    echo "translated-provider-defer: FAIL"
fi
exit $fail
