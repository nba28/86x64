#!/bin/bash
#
# Regression test for the STRIPPED-binary stack-arg ProcPtr relocation fix
# (core: instruction.cc stack-arg path; Halo CE renderer-check event-handler
# crash, 2026-07-05).
#
# A locals-stripped non-PIE i386 binary stores a callback ProcPtr as a stack
# argument (`movl $handler,(%esp)` = c7 04 24 <i386 addr>). Before the fix the
# raw i386 __text address survives translation UNRELOCATED (the parser cannot
# tell a ProcPtr from an integer constant without a symbol); Halo then jmps to
# that unmapped address when Carbon invokes the handler. The fix recognizes the
# target structurally as a function ENTRY by its `55 89 e5` prologue and
# relocates it (rewriting the immediate store to a slide-correct lea+store).
#
# Asserts STRUCTURALLY on the translated output (the runtime bug only manifests
# through the native callback path, not a pure translated `call *reg`, which
# pcmap-translates): the raw i386 `_handler` vmaddr must NOT survive in the
# translated __text as a `c7 04 24 <vmaddr>` immediate. Needs the SL i386
# sysroot; SKIPs without it (like the suite).
# i386 linking needs the Snow Leopard ld64-95 wrapper: modern ld dropped -arch i386
# (same resolution as the Makefile's LD). Override with LD=... in the environment.
. "$(dirname "$0")/../src/86x64/paths.sh"   # M64_* local paths
LD="${LD:-$M64_I386_LD}"; [ -x "$LD" ] || LD=ld

set -u
cd "$(dirname "$0")"

PROJ_ROOT="$(cd .. && pwd)"
MT="${1:-$PROJ_ROOT/build/src/macho-tool/macho-tool}"
LIBABICONV="$PROJ_ROOT/build/src/abiconv/libabiconv.dylib"
LIBWRAPPER="$PROJ_ROOT/build/src/86x64/libwrapper.a"
LIBINTERPOSE="$PROJ_ROOT/build/src/86x64/libinterpose.dylib"
PIPELINE="$PROJ_ROOT/src/86x64/86x64.sh"
SYSROOT=/tmp/i386-sysroot

if [ ! -f "$SYSROOT/usr/lib/libSystem.dylib" ] && [ ! -f "$SYSROOT/usr/lib/libSystem.B.dylib" ]; then
   echo "SKIP stripped-stackarg-procptr (no i386 sysroot at $SYSROOT; run 'make sysroot')"
   exit 0
fi
for f in "$MT" "$LIBABICONV" "$LIBWRAPPER" "$LIBINTERPOSE"; do
   [ -e "$f" ] || { echo "SKIP stripped-stackarg-procptr (missing $f; build first)"; exit 0; }
done

mkdir -p build
fail() { echo "FAIL stripped-stackarg-procptr ($1)"; exit 1; }

clang -arch i386 -isysroot "$SYSROOT" -mmacosx-version-min=10.6 \
   -c src/stripped_stackarg_procptr.s -o build/stripped_stackarg_procptr.o \
   2>/dev/null || fail assemble
# non-PIE MH_EXECUTE (the fixed-load-address shape the relocation heuristic gates on)
"$LD" -arch i386 -macos_version_min 10.6 -no_pie -syslibroot "$SYSROOT" \
   -lSystem -e _main -o build/stripped_stackarg_procptr.i386 \
   build/stripped_stackarg_procptr.o 2>/dev/null || fail link

# Record _handler's i386 vmaddr from the linked binary BEFORE stripping.
HANDLER_VMADDR="$(nm build/stripped_stackarg_procptr.i386 2>/dev/null \
   | awk '$3=="_handler"{print $1}')"
[ -n "$HANDLER_VMADDR" ] || fail "could not read _handler vmaddr"

# Strip local symbols so _handler leaves no func_syms nlist (the Halo shape;
# only .globl _main survives) — this is what arms the stripped-binary path.
strip -x build/stripped_stackarg_procptr.i386 2>/dev/null || fail strip
if nm build/stripped_stackarg_procptr.i386 2>/dev/null | grep -q " _handler$"; then
   fail "_handler still symboled after strip"
fi

bash "$PIPELINE" -m "$MT" -l "$LIBABICONV" -w "$LIBWRAPPER" -i "$LIBINTERPOSE" \
   -o build/stripped_stackarg_procptr.x86_64 build/stripped_stackarg_procptr.i386 \
   >/dev/null 2>&1 || fail translate

# The raw ProcPtr immediate as it would appear if UNRELOCATED: c7 04 24 <LE vmaddr>.
RESULT="$(python3 - "$HANDLER_VMADDR" build/stripped_stackarg_procptr.x86_64.dylib <<'PY'
import sys, struct
va = int(sys.argv[1], 16)
path = sys.argv[2]
try:
    d = open(path, 'rb').read()
except FileNotFoundError:
    # some pipelines leave the translated code in the wrapper .x86_64 itself
    d = open(sys.argv[2].rsplit('.dylib', 1)[0], 'rb').read()
needle = b'\xc7\x04\x24' + struct.pack('<I', va)   # movl $va, (%esp)
print('RAW_PRESENT' if needle in d else 'RELOCATED')
PY
)" || fail "inspection error"

if [ "$RESULT" = "RELOCATED" ]; then
   echo "PASS stripped-stackarg-procptr"
   exit 0
fi
echo "FAIL stripped-stackarg-procptr (raw i386 ProcPtr immediate $HANDLER_VMADDR survived unrelocated in __text)"
exit 1
