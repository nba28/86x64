#!/bin/bash
#
# Regression test for the CROSS-COPY slide_objc per-image processing claim
# (objc_shim.c objc_shared_ctrl.proc_img + _86x64_objc_shared_claim_image,
# consulted at the top of objc_slide.c slide_objc; 2026-07-17).
#
# A bundle co-locates one libabiconv per translated binary/framework
# (the libabiconv multi-copy gotcha; Civ IV = 9 copies). dyld runs each copy's
# ctor, and each calls _dyld_register_func_for_add_image(&slide_objc) — so a
# SECOND copy's registration re-fires slide_objc over every already-mapped image
# with its OWN (private) processed-set, re-walking metadata the first copy
# already processed / is mid-processing. That read a not-yet-ready pointer field
# back as NULL and dereferenced it at +offset: the intermittent slide_objc
# `[rbx+0x88]` rbx=0 SIGSEGV that killed ~40% of Civ launches before its version
# check. The fix is a process-global processed-set in the shared ctrl: the first
# copy to reach an image wins and processes it; later copies' re-scans skip it.
#
# The fixture (src/dyld_multicopy_reprocess_fixture.c) makes no libabiconv call
# (a translated i386 binary can't, without an abigen bridge) — it just dlopen()s
# a SECOND libabiconv copy so that copy's ctor re-fires slide_objc over this
# already-mapped image. slide_objc emits one env-gated `[xcopy] process|skip
# <image>` line per (copy,image) decision; the harness counts the `process`
# lines for THIS fixture:
#   - POST-FIX: the re-scan is SKIPPED -> exactly ONE process line.
#   - PRE-FIX arm (ABICONV_NO_XCOPY_CLAIM=1 disables the dedup): the re-scan
#     RE-PROCESSES -> TWO process lines (the crash's root double-process).
# Needs the Snow Leopard i386 sysroot; SKIPs without it.
# i386 linking needs the Snow Leopard ld64-95 wrapper: modern ld dropped -arch i386
# (same resolution as the Makefile's LD). Override with LD=... in the environment.
LD="${LD:-$HOME/projects/Library/Toolchains/sl-ld64/ld-i386}"; [ -x "$LD" ] || LD=ld

set -u
cd "$(dirname "$0")"

PROJ_ROOT="$(cd .. && pwd)"
MT="${1:-$PROJ_ROOT/build/src/macho-tool/macho-tool}"
LIBABICONV="$PROJ_ROOT/build/src/abiconv/libabiconv.dylib"
LIBWRAPPER="$PROJ_ROOT/build/src/86x64/libwrapper.a"
LIBINTERPOSE="$PROJ_ROOT/build/src/86x64/libinterpose.dylib"
PIPELINE="$PROJ_ROOT/src/86x64/86x64.sh"
SYSROOT=/tmp/i386-sysroot
IMG=dyld_multicopy_reprocess_fixture.x86_64.dylib

if [ ! -f "$SYSROOT/usr/lib/libSystem.dylib" ] && [ ! -f "$SYSROOT/usr/lib/libSystem.B.dylib" ]; then
   echo "SKIP dyld-multicopy-reprocess (no i386 sysroot at $SYSROOT; run 'make sysroot')"
   exit 0
fi
for f in "$MT" "$LIBABICONV" "$LIBWRAPPER" "$LIBINTERPOSE"; do
   [ -e "$f" ] || { echo "SKIP dyld-multicopy-reprocess (missing $f; build the project first)"; exit 0; }
done

mkdir -p build
fail() { echo "FAIL dyld-multicopy-reprocess ($1)"; exit 1; }

clang -arch i386 -isysroot "$SYSROOT" -mmacosx-version-min=10.6 \
   -c src/dyld_multicopy_reprocess_fixture.c \
   -o build/dyld_multicopy_reprocess_fixture.o 2>/dev/null || fail compile
"$LD" -arch i386 -macos_version_min 10.6 -no_pie -syslibroot "$SYSROOT" \
   -lSystem -e _main -o build/dyld_multicopy_reprocess_fixture.i386 \
   build/dyld_multicopy_reprocess_fixture.o 2>/dev/null || fail link
bash "$PIPELINE" -m "$MT" -l "$LIBABICONV" -w "$LIBWRAPPER" -i "$LIBINTERPOSE" \
   -o build/dyld_multicopy_reprocess_fixture.x86_64 \
   build/dyld_multicopy_reprocess_fixture.i386 >/dev/null 2>&1 || fail translate
chmod +x build/dyld_multicopy_reprocess_fixture.x86_64

# Copy 1: the translated fixture bakes in @loader_path/libabiconv.dylib, so the
# copy next to it MUST be the freshly built one (not a stale leftover).
cp -f "$LIBABICONV" build/libabiconv.dylib
codesign -f -s - build/libabiconv.dylib >/dev/null 2>&1
# The second libabiconv copy: same content, distinct inode -> distinct image.
cp -f "$LIBABICONV" build/libabiconv_copy2.dylib
codesign -f -s - build/libabiconv_copy2.dylib >/dev/null 2>&1

# Count `[xcopy] process <fixture>` lines (how many copies actually processed
# THIS image). stderr carries the trace; stdout carries "copy2: loaded".
count_process() {   # $1 = extra env
   env $1 ABICONV_RUN_INITS=1 ABICONV_XCOPY_TRACE=1 \
      DYLD_MULTICOPY_LIB="$PWD/build/libabiconv_copy2.dylib" \
      build/dyld_multicopy_reprocess_fixture.x86_64 2>&1 >/dev/null \
      | grep -c "^\[xcopy\] process $IMG$"
}

# --- POST-FIX arm (default): the cross-copy claim holds -> processed ONCE. ---
N_FIX="$(count_process "")"
[ "$N_FIX" = "1" ] || fail "post-fix: image processed $N_FIX times (expected 1 — the second copy's re-scan should be skipped by the cross-copy claim)"

# --- PRE-FIX arm: disable the dedup -> the second copy re-processes (twice). ---
N_PRE="$(count_process "ABICONV_NO_XCOPY_CLAIM=1")"
[ "$N_PRE" -ge 2 ] || fail "pre-fix: image processed $N_PRE times (expected >=2 — the A/B is not exercising the second-copy re-scan)"

echo "PASS dyld-multicopy-reprocess"
