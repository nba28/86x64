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

# Collect symbols: prefer positional args, otherwise fall back to stdin.
if [ "$#" -gt 0 ]; then
    SYM_INPUT=$(printf '%s\n' "$@")
else
    SYM_INPUT=$(cat)
fi

ARGS=""
SKIPPED=0
REWRITTEN=0
while IFS= read -r SYM; do
    [ -z "$SYM" ] && continue
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
    REWRITTEN=$((REWRITTEN + 1))
done <<< "$SYM_INPUT"

echo "$0: interposing $REWRITTEN symbols; leaving $SKIPPED unbound to their original frameworks" >&2

# Use $ARGS unquoted so each "--update arg" expands as separate args.
macho-tool modify $ARGS "$INPATH" "$OUTPATH"
