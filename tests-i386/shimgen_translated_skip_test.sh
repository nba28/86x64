#!/bin/bash
#
# shimgen-translated-skip — two-arm guard for src/86x64/shimgen.py.
#
# THE BUG. shimgen collects a binary's dyld binds and, for anything missing
# from the CURRENT on-disk/system provider, generates a native-ABI stub
# (<FW>ShimAuto.dylib) and queues the symbol in shimdb/observed.json. It used
# to do this for EVERY target handed to it, including a TRANSLATED image (i386
# rewritten to x86_64 by macho-tool, identifiable structurally: it links
# libabiconv.dylib, and carries the translator's own __DATA,__86x64_pcmap
# section). A translated image's calls still carry i386-frame semantics, so a
# missing symbol there needs a libabiconv bridge / `m64 reinterpose`, never a
# native stub (a stub's 8-byte `ret` over-pops the caller's 4-byte-return
# frame). Feeding shimgen a translated image polluted observed.json and
# generated/*ShimAuto.m with translated-only symbols (DateToSeconds, NewAlias,
# ResolveAlias, create_fftsetup — see "How it works" in the README).
#
# THE FIX: shimgen's main() now skips a binary that links libabiconv.dylib
# (is_translated_consumer) BEFORE collecting its binds, at the single
# enumeration point every caller funnels through.
#
# ARMS (one shimgen --scan-only invocation, two fixture consumers of the SAME
# missing symbol from the SAME fake framework):
#   NATIVE     consumer (no libabiconv dep): the missing bind IS reported.
#   TRANSLATED consumer (links a libabiconv.dylib stand-in): the missing bind
#              is NOT reported, and shimgen prints one "skipped" line for it.
#
# Self-contained: builds two tiny x86_64 fixtures + a fake framework whose
# on-disk copy is missing a symbol the fixtures were linked against (the
# standard "app was linked against an older, richer framework" shape).
set -u
cd "$(dirname "$0")"
PROJ_ROOT="$(cd .. && pwd)"
SHIMGEN="$PROJ_ROOT/src/86x64/shimgen.py"

T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT

cat > "$T/fw_full.c"  <<'SRC'
void FakePresent(void) {}
void FakeMissing(void) {}
SRC
cat > "$T/fw_stub.c"  <<'SRC'
void FakePresent(void) {}
SRC
cat > "$T/abiconv_stub.c" <<'SRC'
void abiconv_marker(void) {}
SRC
cat > "$T/consumer.c" <<'SRC'
extern void FakePresent(void);
extern void FakeMissing(void);
int main(void) { FakePresent(); FakeMissing(); return 0; }
SRC

fail=0
build() {
    clang -arch x86_64 "$@" 2>>"$T/cc.log" || { fail=1; }
}

# A fake framework linked with BOTH symbols present (what the fixtures bind
# against at build time), then replaced on disk with a copy missing
# FakeMissing (what shimgen's probe sees "on this system" — the exact
# shape of a removed/renamed OS symbol).
build -dynamiclib -install_name @rpath/libFakeFW.dylib \
      -o "$T/libFakeFW_full.dylib" "$T/fw_full.c"
build -dynamiclib -install_name @rpath/libabiconv.dylib \
      -o "$T/libabiconv.dylib" "$T/abiconv_stub.c"

mkdir -p "$T/native_dir" "$T/translated_dir"
build -o "$T/native_dir/native_consumer" "$T/consumer.c" \
      -L"$T" -lFakeFW_full
build -o "$T/translated_dir/translated_consumer" "$T/consumer.c" \
      -L"$T" -lFakeFW_full -labiconv

# Now install the STUB (FakeMissing genuinely absent) as what shimgen probes.
build -dynamiclib -install_name @rpath/libFakeFW.dylib \
      -o "$T/libFakeFW.dylib" "$T/fw_stub.c"

if [ "$fail" = 1 ]; then
    echo "shimgen-translated-skip: FAIL (fixture build)"; head -20 "$T/cc.log"
    exit 1
fi

# Sanity: translated_consumer must actually carry a libabiconv load command
# (the exact structural test the fix keys on), or this guard tests nothing.
if ! otool -L "$T/translated_dir/translated_consumer" | grep -q libabiconv; then
    echo "shimgen-translated-skip: FAIL — translated_consumer does not link" \
         "libabiconv; fixture is broken"
    exit 1
fi
if otool -L "$T/native_dir/native_consumer" | grep -q libabiconv; then
    echo "shimgen-translated-skip: FAIL — native_consumer links libabiconv;" \
         "fixture is broken"
    exit 1
fi

out="$(python3 "$SHIMGEN" --scan-only "$T" \
       native_dir/native_consumer translated_dir/translated_consumer 2>&1)"
echo "$out" | sed 's/^/  /'

# ── NATIVE arm: the missing bind IS surfaced
if ! echo "$out" | grep -q "native_consumer:.*missing from libFakeFW.*FakeMissing"; then
    echo "shimgen-translated-skip: FAIL — native consumer's missing bind was" \
         "not reported"
    fail=1
fi

# ── TRANSLATED arm: the missing bind is NOT surfaced, and the skip is counted
if echo "$out" | grep -q "translated_consumer:.*missing from"; then
    echo "shimgen-translated-skip: FAIL — translated consumer's bind was" \
         "collected (should have been skipped)"
    fail=1
fi
if ! echo "$out" | grep -qE "skipped 1 translated image"; then
    echo "shimgen-translated-skip: FAIL — no 'skipped 1 translated image'" \
         "summary line"
    fail=1
fi

if [ "$fail" = 0 ]; then
    echo "shimgen-translated-skip: PASS"
    exit 0
fi
exit 1
