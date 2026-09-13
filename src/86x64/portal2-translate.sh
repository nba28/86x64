#!/usr/bin/env bash
# Portal 2 (Source engine, i386) full-tree translate.
#
# Portal 2 is NOT an .app bundle, so `m64 translate <bundle>` does not apply: the
# game is a Steam folder holding an i386 main exec at its root plus ~42 i386
# dylibs in bin/osx32/. We translate each binary STANDALONE into a staging dir,
# then optionally deploy the staging dir into bin/osx64/ (the x86_64 tree dyld
# actually loads, via the wrapper's baked @rpath + DYLD_LIBRARY_PATH).
#
# ⚠ PRISTINE-SOURCE BOUNDARY (2026-09-12): bin/osx32/ and the game-root
# portal2_osx are the READ-ONLY i386 originals. Steam re-verify is the only way
# back if they are clobbered, so this script refuses to write to either, and
# every translate output is addressed explicitly with `-o <stage>/...`.
#
# Usage:
#   portal2-translate.sh [--stage DIR] [--jobs N] [--deploy] [--only NAME]...
#                        [--include-cef] [--no-exec]
#
#   --stage DIR     staging dir for translated output (default: a mktemp dir)
#   --jobs N        parallel translations (default: 8)
#   --deploy        after a clean batch, install the staging dir into bin/osx64
#   --deploy-only   skip translating; just install an EXISTING --stage into
#                   bin/osx64 (requires --stage)
#   --only NAME     translate just these osx32 basenames (repeatable)
#   --include-cef   also translate libcef.dylib (29 MB of embedded Chromium;
#                   a known data-in-code rabbit hole, and it is only the in-game
#                   Steam-Workshop/browser UI — not needed for gameplay)
#   --no-exec       skip the game-root portal2_osx main executable
#   --exec-only     translate ONLY the game-root exec (no dylibs). Use this when a
#                   change lands in libwrapper (wrapper_setup.c): the wrapper is
#                   statically linked into the exec, so `m64 resync` cannot deliver
#                   it -- but the 41 dylibs are untouched and need no retranslate.
set -uo pipefail

P2="${P2:-$HOME/Library/Application Support/Steam/steamapps/common/Portal 2}"
SRC32="$P2/bin/osx32"
DST64="$P2/bin/osx64"
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
M64="${M64:-m64}"

STAGE=""
JOBS=8
DEPLOY=0
DEPLOY_ONLY=0
INCLUDE_CEF=0
DO_EXEC=1
EXEC_ONLY=0
ONLY=()

while [ $# -gt 0 ]; do
  case "$1" in
    --stage) STAGE="$2"; shift 2;;
    --jobs|-j) JOBS="$2"; shift 2;;
    --deploy) DEPLOY=1; shift;;
    --deploy-only) DEPLOY=1; DEPLOY_ONLY=1; shift;;
    --only) ONLY+=("$2"); shift 2;;
    --include-cef) INCLUDE_CEF=1; shift;;
    --no-exec) DO_EXEC=0; shift;;
    --exec-only) EXEC_ONLY=1; shift;;
    -h|--help) sed -n '2,26p' "$0"; exit 0;;
    *) echo "unknown arg: $1" >&2; exit 2;;
  esac
done

[ -d "$SRC32" ] || { echo "FATAL: no such dir: $SRC32" >&2; exit 1; }
[ -x "$P2/portal2_osx" ] || { echo "FATAL: no game-root portal2_osx in $P2" >&2; exit 1; }

if [ -z "$STAGE" ]; then
  STAGE="$(mktemp -d "${TMPDIR:-/tmp}/p2stage.XXXXXX")"
fi
mkdir -p "$STAGE"

# --- pristine-source guard ------------------------------------------------
# Refuse to run at all if the staging dir resolves inside the read-only i386
# tree, or is the game root itself (which would target portal2_osx).
stage_abs="$(cd "$STAGE" && pwd -P)"
src_abs="$(cd "$SRC32" && pwd -P)"
p2_abs="$(cd "$P2" && pwd -P)"
case "$stage_abs" in
  "$src_abs"|"$src_abs"/*) echo "FATAL: stage dir is inside the PRISTINE i386 tree ($src_abs). Refusing." >&2; exit 1;;
  "$p2_abs") echo "FATAL: stage dir is the game root (would overwrite the pristine portal2_osx). Refusing." >&2; exit 1;;
esac

if [ "$DEPLOY_ONLY" -eq 1 ] && [ ! -d "$STAGE" ]; then
  echo "FATAL: --deploy-only needs an existing --stage DIR" >&2; exit 1
fi

LOGS="$STAGE/.logs"; mkdir -p "$LOGS"
echo "==> repo      $REPO ($(git -C "$REPO" rev-parse --short HEAD 2>/dev/null))"
echo "==> source    $SRC32  (READ-ONLY)"
echo "==> stage     $STAGE"
echo "==> jobs      $JOBS"

if [ "$DEPLOY_ONLY" -eq 1 ]; then
  echo "==> deploy-only: not translating, installing what is already in the stage"
  nok=$(ls -1 "$STAGE" | wc -l | tr -d ' '); nbad=0; nfail=0
fi

# --- build the work list --------------------------------------------------
# ⚠ SKIPPING A DEPENDENCY SILENTLY BREAKS EVERY DEPENDENT.
# libcef.dylib is excluded by default (29 MB of embedded Chromium), but
# vguimatsurface.dylib BINDS 19 cef_* symbols from it, so dyld refuses to load
# vguimatsurface AT ALL -- which made CSourceAppSystemGroup::Create() fail,
# AddSystems() fail, and Portal 2 tear down, with no error text anywhere. That
# presented for months as "the main thread idle-parks in startup"; it was the app
# quitting and then hanging in CBaseFileSystem::ShutdownAsync.
# Tell the pipeline the dep is deliberately absent so _weaken_dangling_deps.py
# weakens BOTH the load command and the binds attributed to it: the module then
# loads, and a real call into CEF faults at 0x0 -- loudly, and exactly where CEF
# was needed -- rather than silently doing nothing. Cleared by --include-cef.
if [ "$INCLUDE_CEF" -eq 0 ]; then
  export M64_ABSENT_DEPS="${M64_ABSENT_DEPS:+$M64_ABSENT_DEPS,}libcef"
fi

targets=()
if [ "$DEPLOY_ONLY" -eq 0 ]; then
if [ "$EXEC_ONLY" -eq 1 ]; then
  :                      # targets stays empty; only the exec below is translated
elif [ ${#ONLY[@]} -gt 0 ]; then
  for n in "${ONLY[@]}"; do
    [ -f "$SRC32/$n" ] || { echo "FATAL: --only $n not found in osx32" >&2; exit 1; }
    targets+=("$n")
  done
else
  while IFS= read -r n; do
    [ "$n" = "libcef.dylib" ] && [ "$INCLUDE_CEF" -eq 0 ] && continue
    # Only Mach-O i386 inputs are translatable. The mss*.asi/.mix Miles plugins
    # are Windows PE32 DLLs that the engine loads as opaque data — copy, never
    # translate.
    case "$(file -b "$SRC32/$n")" in *i386*) targets+=("$n");; esac
  done < <(ls -1 "$SRC32")
fi

# --- arch audit ------------------------------------------------------------
# An input that ALREADY ships an x86_64 slice is a decision point, never a
# silent translate: `file -b` says "i386" for a FAT binary too, which is how
# libsteam_api.dylib (i386 + x86_64 + arm64) got translated by accident.
#
# We still translate it, deliberately: Portal 2's callers are TRANSLATED i386
# code, and a call from translated code into a natively-built third-party dylib
# needs an ABI bridge that libabiconv only provides for system frameworks — so
# for a non-system dylib the i386 slice is the ABI-consistent choice. Deploying
# the native slice instead would mean hand-bridging its whole exported surface
# (the SteamAPI_* C API). Print it so the choice stays visible and reviewable.
native_capable=()
for n in ${targets[@]+"${targets[@]}"}; do
  case "$(lipo -info "$SRC32/$n" 2>/dev/null)" in *x86_64*) native_capable+=("$n");; esac
done
if [ ${#native_capable[@]} -gt 0 ]; then
  echo "==> ⚠ ships a NATIVE x86_64 slice, translating the i386 slice anyway:"
  for n in "${native_capable[@]}"; do
    echo "      $n  [$(lipo -info "$SRC32/$n" 2>/dev/null | sed 's/.*are: //')]"
  done
fi

echo "==> translating ${#targets[@]} dylibs$([ "$DO_EXEC" -eq 1 ] && echo " + portal2_osx (exec)")"

# --- translate one binary -------------------------------------------------
# Invoked one-per-process by xargs -P, as: translate_one <in> <out> <name>
translate_one() {
  local in="$1"
  local out="$2"
  local name="$3"
  local log="$LOGS/$name.log"
  if "$M64" translate "$in" -o "$out" >"$log" 2>&1; then
    # m64 can exit 0 having produced nothing usable; demand a real x86_64 Mach-O.
    if [ -f "$out" ] && file -b "$out" | grep -q 'Mach-O 64-bit.*x86_64'; then
      echo "OK   $name"
    else
      echo "BAD  $name  (exit 0 but no x86_64 output)"
    fi
  else
    echo "FAIL $name  (see $log)"
  fi
}
export -f translate_one
export M64 LOGS

# --- run the batch in parallel -------------------------------------------
# NUL-delimited so the Steam path's spaces survive; xargs -P does the slotting.
results="$STAGE/.results"
: > "$results"
{
  for n in ${targets[@]+"${targets[@]}"}; do
    printf '%s\0%s\0%s\0' "$SRC32/$n" "$STAGE/$n" "$n"
  done
  if [ "$DO_EXEC" -eq 1 ]; then
    # The exec pipeline emits BOTH the x86_64 wrapper exec `portal2_osx` and its
    # translated payload `portal2_osx.dylib`.
    printf '%s\0%s\0%s\0' "$P2/portal2_osx" "$STAGE/portal2_osx" "portal2_osx"
  fi
} | xargs -0 -n 3 -P "$JOBS" bash -c 'translate_one "$0" "$1" "$2"' >>"$results"

# --- copy the non-Mach-O engine payloads through -------------------------
if [ ${#ONLY[@]} -eq 0 ]; then
  for n in $(ls -1 "$SRC32"); do
    case "$(file -b "$SRC32/$n")" in *i386*) ;; *) cp -p "$SRC32/$n" "$STAGE/$n";; esac
  done
fi

# --- report ---------------------------------------------------------------
sort -o "$results" "$results"
nok=$(grep -c '^OK'   "$results" || true)
nbad=$(grep -c '^BAD' "$results" || true)
nfail=$(grep -c '^FAIL' "$results" || true)
echo
echo "===================== RESULT: $nok ok / $nbad bad / $nfail fail ====================="
grep -v '^OK' "$results" || true
echo "(full list: $results ; logs: $LOGS)"

fi   # end: translate pass (skipped by --deploy-only)

if [ "$nbad" -ne 0 ] || [ "$nfail" -ne 0 ]; then
  echo "!! not clean — NOT deploying" >&2
  [ "$DEPLOY" -eq 1 ] && exit 1
  exit 0
fi

# --- deploy --------------------------------------------------------------
if [ "$DEPLOY" -eq 1 ]; then
  echo
  echo "==> deploying $STAGE -> $DST64"
  mkdir -p "$DST64"
  for f in "$STAGE"/*; do
    [ -f "$f" ] || continue
    cp -p "$f" "$DST64/$(basename "$f")"
  done
  # libabiconv + libinterpose must match the macho-tool that produced the code.
  "$M64" resync "$DST64" || echo "!! m64 resync failed — check by CONTENT before measuring" >&2
  echo "==> deployed. Verify the deployed libabiconv is YOURS before measuring:"
  echo "    nm -a '$DST64/libabiconv.dylib' | grep -c shim_"
fi
