#!/bin/bash
#
# Rewrite lazy-bind entries for the listed symbols so they point at
# <prefix><sym> in <dylib> (the abiconv shim). Symbols are passed as
# positional arguments after the input archive, OR on stdin (one per line)
# if no positional symbols are given.
#
# A symbol is only rewritten if <prefix><sym> actually exists as an export
# of <dylib>. The original tool blindly rewrote every lazy-bind, which
# caused dyld to silently fail to resolve symbols the shim doesn't cover
# (e.g. anything ObjC) — the resulting NULL la_symbol_ptr then jumped the
# program to address 0 on first call.

usage() {
    cat <<EOF
usage: $0 [-h] -l interpose_dylib [-n install_name] [-p prefix] [-o outpath] inpath [sym...]
       (if no syms given, reads from stdin one per line)

  -l  filesystem path to the shim dylib (read for its exports)
  -n  load-command name of the shim dylib as embedded in inpath
      (defaults to -l value; pass this when they differ, e.g. when the
      load command uses an @loader_path/@rpath install name)
EOF
}

OUTPATH="a.out"
DYLIB=""
DYLIB_NAME=""
PREFIX="__"

while getopts "ho:l:n:p:" OPTION; do
    case $OPTION in
        h) usage; exit 0 ;;
        o) OUTPATH="$OPTARG" ;;
        l) DYLIB="$OPTARG" ;;
        n) DYLIB_NAME="$OPTARG" ;;
        p) PREFIX="$OPTARG" ;;
        "?") usage >&2; exit 1 ;;
    esac
done

shift $((OPTIND-1))

if [ -z "$DYLIB" ]; then
    echo "$0: specify static interpose target dylib with '-l <dylib>'" >&2
    exit 1
fi

# The load command embedded in the archive may carry an install name
# (@loader_path/@rpath) that differs from the on-disk path used to read
# exports. Fall back to the path when no explicit name is given.
[ -z "$DYLIB_NAME" ] && DYLIB_NAME="$DYLIB"

INPATH="$1"
shift 1
if ! [ "$INPATH" ]; then
    echo "$0: missing positional argument input archive" >&2
    exit 1
fi

ORD=$(macho-tool translate --load-dylib "$DYLIB_NAME" "$INPATH")
if [[ $? != 0 ]]; then
    echo "$0: failed to translate dylib ordinal" >&2
    exit 2
fi

# Build the set of symbols actually exported by the shim dylib. nm prints
# each external defined symbol as "<addr> T <name>" (or other types) — we
# accept anything that's not undefined (U).
SHIM_EXPORTS=$(mktemp)
trap "rm -f $SHIM_EXPORTS" EXIT
nm -gU "$DYLIB" 2>/dev/null | awk '{print $NF}' | sort -u > "$SHIM_EXPORTS"

# Sanity check: empty SHIM_EXPORTS would cause every interpose check to
# fail silently. Common cause: caller passed -l with a wrong/missing path.
if ! [ -s "$SHIM_EXPORTS" ]; then
    echo "$0: ERROR: no exports found in shim dylib '$DYLIB' (file missing or not a dylib?)" >&2
    exit 3
fi

# ── NULL-JUMP BRIDGES: never redirect a bind into one ──────────────────────
# abigen generates ___X for anything in its consider set with a parseable 10.6
# declaration. That is right while a NATIVE _X still exists to call. When Apple
# REMOVES the API the bridge survives, the pipeline weakens the now-dangling
# native bind to NULL, and the bridge's `call` jumps to 0 -- so redirecting here
# does not merely fail to help, it REPLACES whatever the app would have bound to
# with a guaranteed crash.
#
# Measured 2026-08-03: 507 such bridges, and 358 of them shadow a WORKING
# implementation the app itself ships -- 347 from its own bundled (translated)
# QuickTime.framework, 11 from its bundled Python. Civ IV died at
# QTNewDataReferenceFromFSRef for exactly this reason while its own QuickTime
# defined the symbol at 0x100aa300.
#
# Skipping them is strictly better in BOTH cases:
#   * app ships an implementation -> the bind resolves to real, working code.
#   * nothing ships one          -> the bind stays weak and resolves to NULL,
#     which is what a classic app's `if (SomeAPI != NULL)` availability check
#     needs to see. Our bridge is non-NULL, so interposing actively DEFEATS that
#     check: the app concludes the API exists and calls into a jump-to-zero.
#
# The list is computed at BUILD time (find_null_jump_bridges.py --emit) because
# condition 3 -- "does _X resolve on this OS" -- is a runtime dlsym question.
# Hand-written shims are absent from it by construction: they implement the work
# themselves and leave no undefined native, so quicktime_image.c and the golden
# QuickTime surface keep their interposition.
NULLJUMP="${NULLJUMP:-$(dirname "$DYLIB")/libabiconv.nulljump}"
NULLJUMP_N=0
if [ -s "$NULLJUMP" ]; then
    NULLJUMP_N=$(wc -l < "$NULLJUMP" | tr -d ' ')
fi

# Build the set of symbols bound through the WEAK bind table (their ORIGINAL,
# pre-rewrite names — the same form as the incoming symbol list). These need
# the extra weak_bind rewrite below; almost no symbol is in this table (it is
# just the C++ weak-coalesced runtime externals: operator new/delete and any
# weakly-referenced template/vtable symbol), so computing the membership set
# once and emitting the weak --update ONLY for members is far cheaper than an
# optional weak --update per symbol (which would 1.5× the modify arg list — an
# ARG_MAX risk on a binary with thousands of imports — and force thousands of
# no-op table searches). A classic (LC_DYSYMTAB-only) binary has no dyld_info
# weak_bind table; print fails harmless and the set is empty.
WEAK_SYMS=$(mktemp)
trap "rm -f $SHIM_EXPORTS $WEAK_SYMS" EXIT
macho-tool print --weak-bind "$INPATH" 2>/dev/null | tail +2 | cut -d" " -f5 \
    | sort -u > "$WEAK_SYMS"

# Collect symbols: prefer positional args, otherwise fall back to stdin.
if [ "$#" -gt 0 ]; then
    SYM_INPUT=$(printf '%s\n' "$@")
else
    SYM_INPUT=$(cat)
fi

ARGS=""
SKIPPED=0
SKIPPED_NULLJUMP=0
REWRITTEN=0
while IFS= read -r SYM; do
    [ -z "$SYM" ] && continue
    # A bridge that can only jump to NULL must never become a bind target.
    if [ "$NULLJUMP_N" != 0 ] && grep -qFx "$SYM" "$NULLJUMP"; then
        SKIPPED_NULLJUMP=$((SKIPPED_NULLJUMP + 1))
        SKIPPED=$((SKIPPED + 1))
        continue
    fi
    REPLACEMENT="${PREFIX}${SYM}"
    if ! grep -qFx "$REPLACEMENT" "$SHIM_EXPORTS"; then
        # Legacy POSIX conformance-variant symbols (_open$UNIX2003,
        # _stat$INODE64, _opendir$INODE64$UNIX2003, _foo$1050) are the SAME
        # function as their base name — only errno/struct-size conformance
        # differs, which the abigen base shim already marshals. The variant
        # form has no shim of its own, so it would otherwise fall through to
        # NATIVE libSystem and be called with the i386 ABI (4-byte stack ret),
        # which the native 64-bit `ret` over-pops -> fused PC crash. Strip the
        # $… suffix(es) and retry against the base shim. Generic to any i386
        # binary built against the 10.5-era UNIX2003/INODE64 variant symbols.
        BASE="${SYM%%\$*}"
        REPLACEMENT="${PREFIX}${BASE}"
        if [ "$BASE" = "$SYM" ] || ! grep -qFx "$REPLACEMENT" "$SHIM_EXPORTS"; then
            # Classic Mach-O (no LC_DYLD_INFO) binaries: nm -u emits the raw
            # decorated symbol names exactly as they appear in the nlist table.
            # These already carry their full underscore prefix, so for symbols
            # like ___tolower (= C __tolower), PREFIX+SYM = _____tolower does
            # not exist — but SYM itself (___tolower) may be a direct libabiconv
            # export (the ABI-bridging shim for _tolower that happens to share
            # the name). When SYM is found verbatim, retarget the nlist library
            # ordinal to libabiconv without renaming. This is structurally gated
            # on the symbol being absent from libabiconv under PREFIX+SYM but
            # present directly — harmless for modern (LC_DYLD_INFO) images where
            # the bind stream already uses 1-underscore names that the prefix
            # maps unambiguously.
            if grep -qFx "$SYM" "$SHIM_EXPORTS"; then
                REPLACEMENT="$SYM"
            else
                # No shim available — leave the bind pointing at its original framework.
                SKIPPED=$((SKIPPED + 1))
                continue
            fi
        fi
    fi
    # Redirect the symbol wherever it is bound: functions live in the lazy
    # bind table, external DATA constants (e.g. NSString* consts like
    # NSArgumentDomain) in the non-lazy table. Emit BOTH updates, each marked
    # `optional` so the one targeting the wrong table is silently skipped.
    ARGS="$ARGS --update bind,lazy,optional,old_sym=$SYM,new_sym=$REPLACEMENT,new_dylib=$ORD"
    ARGS="$ARGS --update bind,optional,old_sym=$SYM,new_sym=$REPLACEMENT,new_dylib=$ORD"
    # If this symbol is ALSO bound through the WEAK bind table, redirect that
    # entry too. C++ runtime weak-coalesced externals (operator new/delete:
    # __Znwm/__Znam/__ZdlPv/__ZdaPv, and any weakly-referenced template/vtable
    # symbol) appear in BOTH the regular bind table (rewritten above) AND the
    # weak_bind table, targeting the SAME __la_symbol_ptr slot. dyld processes
    # weak binds AFTER regular binds, so a weak_bind entry left under its
    # ORIGINAL name re-resolves that slot by flat weak coalescing and OVERWRITES
    # the libabiconv shim pointer with the NATIVE definition -> the call reaches
    # native code unrouted -> the i386 4-byte ret is over-popped by the native
    # 8-byte ret -> fused PC crash. Renaming the weak_bind symbol to the shim
    # name (same REPLACEMENT) keeps the coalesced lookup resolving to libabiconv
    # (its only definition). No new_dylib: a weak bind carries no ordinal
    # (implicit BIND_SPECIAL_DYLIB_WEAK_LOOKUP). `optional` is belt-and-braces.
    if grep -qFx "$SYM" "$WEAK_SYMS"; then
        ARGS="$ARGS --update bind,weak,optional,old_sym=$SYM,new_sym=$REPLACEMENT"
    fi
    REWRITTEN=$((REWRITTEN + 1))
done <<< "$SYM_INPUT"

echo "$0: interposing $REWRITTEN symbols; leaving $SKIPPED unbound to their original frameworks ($SKIPPED_NULLJUMP of them null-jump bridges we must not shadow)" >&2

# Use $ARGS unquoted so each "--update arg" expands as separate args.
macho-tool modify $ARGS "$INPATH" "$OUTPATH"
