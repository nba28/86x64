#!/bin/bash
#
# arena_publish_toctou_test.sh — regression guard for the arena_init shared-ctrl
# PUBLISH TOCTOU (objc_shim.c arena_init; the residual rc=139 Civ IV ~1/5
# startup crash after the proc_img cross-copy CLAIM fix — 2026-07-23).
#
# COMPANION to dyld_multicopy_reprocess_test.sh. That test proves the per-image
# proc_img CLAIM works GIVEN one shared objc_shared_ctrl. This test proves
# arena_init actually produces exactly ONE shared ctrl across all libabiconv
# copies even when a second copy's adopt races the first copy's publish.
#
# ROOT: arena_init publishes the shared ctrl via setenv(OBJC_CTRL_ENV,&ctrl) and
# adopts via getenv — a setenv/getenv TOCTOU. arena_init is reached at message
# time (objc_bridge_prep*) on any thread and has NO lock around its check-then-
# act create path, so a second copy that misses the not-yet-visible env ALSO
# creates a ctrl -> two proc_img tables -> both copies "win" the same image's
# claim -> its static initializers run TWICE -> Civ's same-image FConsole
# intrusive-list registry is spliced twice -> NULL link deref at +0x88/+0x8c.
#
# FIX (Option A, coordinator's objc_shim.c): replace the setenv/getenv publish
# with a fixed low-4GB rendezvous word + atomic compare_exchange (winner installs,
# losers atomic_load the winner) -> always exactly one shared ctrl.
#
# A/B (needs the coordinator-added ABICONV_ARENA_TRACE hook: one line
# `[arena] create <addr>` on the ctrl-create path, `[arena] attach <addr>` on the
# adopt path; and the ABICONV_ARENA_PUBLISH_GAP pre-fix arm that makes the FIRST
# copy's create path STAGE-but-not-publish the env, modeling the unseen setenv):
#   - POST-FIX (default): exactly ONE `[arena] create` across both copies.
#   - PRE-FIX arm (ABICONV_ARENA_PUBLISH_GAP=1): the second copy also creates ->
#     TWO `[arena] create` lines (the duplicate-ctrl root of the crash).
# If ABICONV_ARENA_TRACE emits nothing (hook not yet in the built libabiconv),
# the test SKIPs (so it is inert until the coordinator lands the hook + fix).
#
# Needs the Snow Leopard i386 sysroot; SKIPs without it.
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
IMG=arena_publish_toctou_fixture.x86_64.dylib

if [ ! -f "$SYSROOT/usr/lib/libSystem.dylib" ] && [ ! -f "$SYSROOT/usr/lib/libSystem.B.dylib" ]; then
   echo "SKIP arena-publish-toctou (no i386 sysroot at $SYSROOT; run 'make sysroot')"
   exit 0
fi
for f in "$MT" "$LIBABICONV" "$LIBWRAPPER" "$LIBINTERPOSE"; do
   [ -e "$f" ] || { echo "SKIP arena-publish-toctou (missing $f; build the project first)"; exit 0; }
done

mkdir -p build
fail() { echo "FAIL arena-publish-toctou ($1)"; exit 1; }

clang -arch i386 -isysroot "$SYSROOT" -mmacosx-version-min=10.6 \
   -c src/arena_publish_toctou_fixture.c \
   -o build/arena_publish_toctou_fixture.o 2>/dev/null || fail compile
"$LD" -arch i386 -macos_version_min 10.6 -no_pie -syslibroot "$SYSROOT" \
   -lSystem -e _main -o build/arena_publish_toctou_fixture.i386 \
   build/arena_publish_toctou_fixture.o 2>/dev/null || fail link
bash "$PIPELINE" -m "$MT" -l "$LIBABICONV" -w "$LIBWRAPPER" -i "$LIBINTERPOSE" \
   -o build/arena_publish_toctou_fixture.x86_64 \
   build/arena_publish_toctou_fixture.i386 >/dev/null 2>&1 || fail translate
chmod +x build/arena_publish_toctou_fixture.x86_64

# The copy next to the fixture MUST be the freshly built libabiconv (baked
# @loader_path). The second copy: same content, distinct inode -> distinct image.
cp -f "$LIBABICONV" build/libabiconv.dylib
codesign -f -s - build/libabiconv.dylib >/dev/null 2>&1
cp -f "$LIBABICONV" build/libabiconv_copy2.dylib
codesign -f -s - build/libabiconv_copy2.dylib >/dev/null 2>&1

# Count `[arena] create` lines (how many DISTINCT shared ctrls were built).
count_create() {   # $1 = extra env
   env $1 ABICONV_RUN_INITS=1 ABICONV_ARENA_TRACE=1 \
      DYLD_MULTICOPY_LIB="$PWD/build/libabiconv_copy2.dylib" \
      build/arena_publish_toctou_fixture.x86_64 2>&1 >/dev/null \
      | grep -c '^\[arena\] create'
}

# Hook-presence probe: if ABICONV_ARENA_TRACE produces NO arena lines at all,
# the coordinator's trace hook is not in this libabiconv build yet -> SKIP.
PROBE="$(env ABICONV_RUN_INITS=1 ABICONV_ARENA_TRACE=1 \
   DYLD_MULTICOPY_LIB="$PWD/build/libabiconv_copy2.dylib" \
   build/arena_publish_toctou_fixture.x86_64 2>&1 >/dev/null \
   | grep -c '^\[arena\] \(create\|attach\)')"
if [ "$PROBE" = "0" ]; then
   echo "SKIP arena-publish-toctou (ABICONV_ARENA_TRACE hook absent in libabiconv; add the arena_init trace hook)"
   exit 0
fi

# --- POST-FIX arm (default): one shared ctrl -> exactly ONE create. ---
N_FIX="$(count_create "")"
[ "$N_FIX" = "1" ] || fail "post-fix: $N_FIX ctrls created (expected 1 shared ctrl across both copies — the publish adopt should converge)"

# --- PRE-FIX arm: force the publish gap -> the second copy also creates. ---
N_PRE="$(count_create "ABICONV_ARENA_PUBLISH_GAP=1")"
[ "$N_PRE" -ge 2 ] || fail "pre-fix: $N_PRE ctrls created (expected >=2 — the publish gap is not exercising the duplicate-ctrl race; is ABICONV_ARENA_PUBLISH_GAP wired?)"

echo "PASS arena-publish-toctou"
