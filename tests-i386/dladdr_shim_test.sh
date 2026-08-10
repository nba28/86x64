#!/bin/bash
#
# dladdr-shim — two-arm guard for src/abiconv/posix_shim.c: shim_dladdr
#   (+ maptable_tramp.asm MTSHIM ___dladdr)
#
# THE BLOCKER (Civilization IV, 2026-08-09/10). libabiconv hand-shimmed
# dlopen/dlsym/dlclose/dlerror and NOT dladdr, so static-interpose had no
# `___dladdr` to redirect to and a translated `_dladdr` bind stayed on NATIVE
# libSystem. The i386 caller passes arguments in 4-byte STACK slots; native
# dladdr reads them from %rdi/%rsi. Caught under lldb:
#     frame #0  dyld`dyld4::APIs::dladdr + 635
#     ->  movq 0x8(%rbx), %rcx    rbx = 0x4e   (0x4e + 8 = 0x56 = fault addr)
#         rdi = 0x8aee8378 (a stack address, read as `addr`)
#         rsi = 0x8        (read as `Dl_info *`)
# CPython 2.6 calls dladdr during start-up, so it landed inside Py_Initialize and
# read for a long time as a Python-bridge defect. `bt 40` returned ONLY frame #0
# — lldb cannot unwind out to a translated 4-byte frame — so the ARGUMENTS, not
# the backtrace, identified it. It presents as a RACE (11/11 unhosted,
# intermittent under a debugger) because the garbage in rdi/rsi varies.
#
# ⚠The RECORD needs converting, not just the call. Dl_info is four pointers:
# 16 bytes on i386, 32 on x86_64, so a forwarding shim would overrun the caller's
# buffer by 16 bytes AND read dli_fbase out of the HIGH HALF of dli_fname.
#
# ARMS (same translated binary, run twice):
#   ON  : ret=1 fbase_ok=1 fname_ok=1 — real image base, readable bounced path.
#   OFF : M64_NO_DLADDR_MARSHAL=1 copies the native record verbatim; the fields
#         land at the wrong offsets and fname_ok goes to 0. Defect reproduces.
#
# Also asserts STATICALLY that the fixture's bind actually reaches
# `libabiconv/___dladdr` — read with `dyld_info -fixups`, since `nm -m`/`otool -Iv`
# report the NLIST table that static-interpose does not rewrite.
set -u
cd "$(dirname "$0")"

BIN=build/99_dladdr_shim.x86_64
LIBABICONV=../build/src/abiconv/libabiconv.dylib

[ -x "$BIN" ] || { echo "dladdr-shim: SKIP (no $BIN — needs the i386 sysroot)"; exit 0; }

# ── STALENESS IS THE FAILURE MODE THAT ALMOST LANDED THIS FIX UNVERIFIED ──────
# `make dladdr-shim` passed twice, for two people, against a fixture binary built
# before the shim existed. A guard that can pass against a stale artifact is
# worse than no guard: it certifies the thing it never exercised. So refuse to
# report anything if the binary is older than either the runtime it links or the
# source it was built from. (The Makefile dependency exists; this is the check
# that does not depend on the dependency being right.)
for newer in "$LIBABICONV" src/99_dladdr_shim.c; do
    if [ -e "$newer" ] && [ "$newer" -nt "$BIN" ]; then
        echo "dladdr-shim: FAIL — $BIN is STALE (older than $newer)."
        echo "  It was NOT rebuilt against the current libabiconv, so any result"
        echo "  below would describe a binary that no longer exists. Rebuild:"
        echo "     rm -f build/99_dladdr_shim.* && make build/99_dladdr_shim.x86_64"
        exit 1
    fi
done

# The shim has to be REACHABLE, not merely present. If libabiconv carries no
# ___dladdr then static-interpose had nothing to redirect to, both arms run the
# same unshimmed path, and the guard would be inert rather than failing.
if ! nm -gU "$LIBABICONV" 2>/dev/null | grep -q " T ___dladdr$"; then
    echo "dladdr-shim: FAIL — libabiconv exports no ___dladdr trampoline, so"
    echo "  nothing can route to the shim. Is MTSHIM ___dladdr wired in"
    echo "  maptable_tramp.asm, and was libabiconv rebuilt?"
    exit 1
fi

fail=0
on_out="$("$BIN" 2>&1)";                              on_rc=$?
off_out="$(M64_NO_DLADDR_MARSHAL=1 "$BIN" 2>&1)";     off_rc=$?

echo "$on_out"  | sed 's/^/  ON  /'
echo "$off_out" | sed 's/^/  OFF /'

if [ "$on_rc" = 0 ] && [ "${on_out#*fname_ok=1}" != "$on_out" ] \
                    && [ "${on_out#*saddr_ok=1}" != "$on_out" ] \
                    && [ "${on_out#*fbase_ok=1}" != "$on_out" ]; then
    echo "  ON  (marshalled):  Dl_info came back with a real low-4GB image base"
    echo "                     and a readable path string                       OK"
else
    echo "  ON  : expected ret/saddr_ok/fbase_ok/fname_ok all 1 and exit 0, got exit $on_rc"
    fail=1
fi

if [ "$off_rc" != 0 ] && [ "${off_out#*saddr_ok=1}" = "$off_out" ] \
                      && [ "${off_out#*fname_ok=1}" = "$off_out" ]; then
    echo "  OFF (unmarshalled): the native 32-byte record smeared over the i386"
    echo "                      16-byte struct — the defect reproduces          OK"
else
    echo "  OFF : expected fname_ok=0 and a non-zero exit with"
    echo "        M64_NO_DLADDR_MARSHAL=1, got exit $off_rc — guard not exercising"
    fail=1
fi

[ "$on_out" = "$off_out" ] && { echo "  both arms identical — kill switch inert"; fail=1; }

# The whole fix is worthless if the bind never reaches the shim.
if ! dyld_info -fixups "$BIN.dylib" 2>/dev/null | grep -q "libabiconv/___dladdr"; then
    echo "  the fixture's _dladdr bind was NOT redirected to libabiconv/___dladdr"
    echo "  (static-interpose found no such export — is MTSHIM ___dladdr wired?)"
    fail=1
fi
nm -gU "$LIBABICONV" 2>/dev/null | grep -q " T ___dladdr$" || {
    echo "  built libabiconv exports no ___dladdr trampoline"; fail=1; }
nm -gU "$LIBABICONV" 2>/dev/null | grep -q " T _shim_dladdr$" || {
    echo "  built libabiconv exports no _shim_dladdr"; fail=1; }
strings -a "$LIBABICONV" 2>/dev/null | grep -q "M64_NO_DLADDR_MARSHAL" || {
    echo "  kill-switch string absent from the built libabiconv"; fail=1; }

if [ "$fail" = 0 ]; then echo "dladdr-shim: PASS"; else echo "dladdr-shim: FAIL"; fi
exit $fail
