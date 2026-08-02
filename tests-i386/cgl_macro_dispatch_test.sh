#!/bin/bash
#
# cgl_macro_dispatch_test.sh — A/B guard for the <OpenGL/CGLMacro.h> DIRECT-DISPATCH
# contract (cgl_macro_shim.c + gli_tramp.asm).
#
# A 32-bit app compiled with CGLMacro.h never calls a GL symbol: every glXxx() is
# #defined to jump through its CGLContextObj's embedded GLIFunctionDispatch table.
# So the context handed to translated code CANNOT be a proxy arena handle — read as
# a struct _CGLContextObject that yields 0 for every dispatch slot, and the app
# executes `jmp *0`. Measured live in Halo CE: rip=0, call site Halo.dylib+0x3a09a6,
# with arg1 = 0x1F03 = GL_EXTENSIONS.
#
# The main-suite fixture 99_cgl_macro_dispatch runs the ON side (a real dispatch
# slot at the hardcoded i386 offset 0x1D8, called, returning REAL GL strings). This
# script adds the OFF side: re-run the SAME translated binary with the kill switch
# M64_NO_CGL_MACRO=1 and assert the slot is 0 again — i.e. that the fixture is
# genuinely exercising the shadow and cannot pass inertly.
#
# Needs the i386 sysroot; SKIPs when the binary has not been built.
set -u
cd "$(dirname "$0")"

BIN=build/99_cgl_macro_dispatch.x86_64
if [ ! -x "$BIN" ]; then
  echo "cgl-macro-dispatch: SKIP (build/99_cgl_macro_dispatch.x86_64 missing;"
  echo "                          run \`make 99_cgl_macro_dispatch\` first)"
  exit 0
fi

fail=0
has() { printf '%s\n' "$1" | grep -q "^$2\$"; }

# --- ON: the context is an i386-layout shadow with a live dispatch table ------
on=$("$BIN" 2>/dev/null)
if has "$on" 'pixfmt=1' && has "$on" 'ctx=1' && has "$on" 'slot=1' &&
   has "$on" 'islot=1' && has "$on" 'maxtex=1'; then
  echo "  ON  (shim armed):    disp slots at the hardcoded i386 offsets 0x1D8/0x1A4 are"
  echo "                       live thunks; calling one the CGLMacro way read back a real"
  echo "                       hardware limit, twice, with the stack balanced           OK"
  printf '%s\n' "$on" | grep -E '^MAX_TEXTURE_SIZE=' | sed 's/^/      /'
else
  echo "  ON  (shim armed):    expected callable dispatch slots and a real GL limit, got:"
  printf '%s\n' "$on" | sed 's/^/      /'
  fail=1
fi

# --- OFF: kill switch -> a proxy arena handle -> the pre-fix `jmp *0` ---------
off=$(M64_NO_CGL_MACRO=1 "$BIN" 2>/dev/null)
# The context must still be CREATED in both arms (the kill switch changes only what
# the i386 side is HANDED, not whether CGL works), so ctx=1 is deliberately an
# invariant and is not what distinguishes the arms.
if has "$off" 'ctx=1' && has "$off" 'slot=0' && has "$off" 'islot=0' &&
   has "$off" 'maxtex=0'; then
  echo "  OFF (kill-switch):   the arena handle has 0 at offset 0x1D8 — the unfixed"
  echo "                       'jmp *0' state                                        OK"
else
  echo "  OFF (kill-switch):   expected slot=0 (the unfixed behaviour); the guard is NOT"
  echo "                       exercising the dispatch shadow. Got:"
  printf '%s\n' "$off" | sed 's/^/      /'
  fail=1
fi

# --- the two arms MUST differ, or neither proves anything --------------------
if [ "$on" = "$off" ]; then
  echo "  ARMS IDENTICAL — the kill switch changed nothing; the guard is inert."
  fail=1
fi

if [ "$fail" = 0 ]; then echo "cgl-macro-dispatch: PASS"; else echo "cgl-macro-dispatch: FAIL"; fi
exit $fail
