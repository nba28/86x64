#!/bin/bash
# halo_cfstr_capture.sh — name the TRANSLATED CALL SITE behind a bad object/CF
# argument (nil or a sub-page value) that a native framework then faults on.
#
# NO DEBUGGER. An earlier lldb version of this script was wrong twice: the
# condition `$rdi < 0x1000` matches every legitimate CFStringCreateWithCString(
# NULL, ...) call (a NULL CFAllocatorRef IS kCFAllocatorDefault), so it fired
# hundreds of times on innocent NATIVE callers; and `lldb -b` exits at the first
# unhandled stop, which killed Halo before the UI could be driven. The capture
# now lives inside libabiconv (objc_shim.c _86x64_unwrap_obj_arg) and costs one
# predictable branch when disabled.
#
# Why a plain backtrace cannot do this: the crashlog unwinder keeps only the
# frames ABOVE our abigen `.l1` shim (it cannot walk the i386 frame below it),
# and ABICONV_ARGSTR_TRACE's caller= field is the unwrap wrapper's own return
# address — identical on every line.
#
#   ABICONV_CALLSITE_TRACE=1  non-nil SUB-PAGE args (never legitimate; low noise)
#   ABICONV_CALLSITE_NIL=1    also nil args (legitimate for allocators => noisy)
#
# Usage:  bash halo_cfstr_capture.sh [logfile] [--nil]
# Then:   drive Halo to the failing UI (Graphics Settings -> OK).
# Output: [callsite] arg=0x… -> 0x…  i386ret=0x…  Halo.dylib+0xOFFSET
# Decode: disassemble at 0x10000000+OFFSET (the translated __TEXT vmaddr; the
#         layout mirrors the i386 original), e.g.
#   objdump -d --start-address=$((0x10000000+OFFSET-0x60)) \
#              --stop-address=$((0x10000000+OFFSET+0x10)) <app>/Contents/MacOS/Halo.dylib
set -u
APP="${HALO_APP:-$HOME/projects/translations/Apps64/Halo.app}"
BIN="$APP/Contents/MacOS/Halo"
LOG="${1:-/tmp/halo_callsite.log}"
[ "${2:-}" = "--nil" ] && export ABICONV_CALLSITE_NIL=1

pkill -9 -x Halo 2>/dev/null; sleep 1     # Halo is a singleton

echo "Running Halo. Drive it to Graphics Settings -> OK, then quit or let it crash."
echo "Log: $LOG"
ABICONV_CALLSITE_TRACE=1 ABICONV_CTRL_TRACE=1 "$BIN" >"$LOG" 2>&1
rc=$?

echo
echo "exit rc=$rc"
echo "--- [callsite] hits (the bad args and who passed them) ---"
grep '\[callsite\]' "$LOG" | sort | uniq -c | sort -rn | head -20 || echo "(none)"
echo "--- last control-data traffic before the end ---"
grep '\[ctrl\]' "$LOG" | tail -10 || echo "(none)"
