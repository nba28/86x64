#!/bin/bash

LINKDST="$(readlink "$0")"
export ROOTDIR="$(dirname "${LINKDST:-"$0"}")"

# Reserve header slack between the load commands and the first __TEXT section.
# Every pipeline output is codesigned (deploy) and path-patched (install_name_tool
# rpath-fix) downstream — both GROW the load-command region. With zero slack the
# first section (__text, holding the entry point) starts immediately after the
# LCs, so codesign's appended LC_CODE_SIGNATURE (+16B) overwrites the entry and
# the binary SIGILLs on launch (seen on small dylibs like Portal 2's
# portal2_osx.dylib, whose __text began exactly where the LCs ended). 1024 matches
# Apple's ld -headerpad_max_install_names. Honor an explicit override if set.
export MACHO_HEADERPAD="${MACHO_HEADERPAD:-1024}"

usage() {
    cat<<EOF
usage: 86x64 [-hv] [-k] [-o archive64] [-r root] [-l libabiconv] [-w wrapper] [-i libinterpose] [-a archive64.dylib] [-m macho-tool] archive32

Translates an i386 Mach-O to x86_64. Produces two output files when -w is given:
  <archive64>          — x86_64 wrapper executable (entry _main_wrapper)
  <archive64>.dylib    — the translated dylib the wrapper loads
or just <archive64.dylib> when -w is omitted (dylib-only mode).

All intermediate stages are written to a temporary directory that is removed
on exit. Set -k to keep the intermediates (the dir path is printed on stderr).

The translated dylib has LC_CODE_SIGNATURE stripped and any duplicate
LC_ID_DYLIB removed so install_name_tool can edit it without re-signing first.
EOF
}

error() {
    echo "86x64: error encountered, exiting..." >&2
    exit 1
}

abort() {
    echo "86x64: $1" >&2
    usage >&2
    exit 1
}

ARCHIVE64="a.out"
LIBABICONV="$ROOTDIR/libabiconv.dylib"
WRAPPER_OBJ="$ROOTDIR/libwrapper.a"
LIBINTERPOSE="$ROOTDIR/libinterpose.dylib"
DYLIB64=""
MACHO_TOOL="macho-tool"
KEEP_TMP=""

while getopts "hvko:l:w:i:a:m:" OPTION; do
    case $OPTION in
        h)
            usage
            exit 0
            ;;
        o)
            ARCHIVE64="$OPTARG"
            ;;
        l)
            LIBABICONV="$OPTARG"
            ;;
        w)
            WRAPPER_OBJ="$OPTARG"
            ;;
        i)
            LIBINTERPOSE="$OPTARG"
            ;;
        a)
            DYLIB64="$OPTARG"
            ;;
        v)
            VERBOSE="1"
            ;;
        k)
            KEEP_TMP="1"
            ;;
        m)
            MACHO_TOOL="$OPTARG"
            ;;
        "?")
            usage >&2
            exit 1
    esac
done

shift $((OPTIND-1))

[ "$DYLIB64" ] || DYLIB64="$ARCHIVE64.dylib"

# Make the chosen macho-tool reachable for the sub-scripts (static-interpose.sh
# calls a bare `macho-tool`). The tests-i386 Makefile exports PATH; do the same
# here so callers like translate-bundle don't have to.
if [ "$MACHO_TOOL" != "macho-tool" ] && [ -x "$MACHO_TOOL" ]; then
    export PATH="$(cd "$(dirname "$MACHO_TOOL")" && pwd):$PATH"
fi

ARCHIVE32="$1"
shift 1
[ "$ARCHIVE32" ] || abort "missing positional parameter 'archive32'"

# A dylib/bundle input can never produce an executable wrapper (it has no
# `_main`), so force dylib-only mode regardless of -w. Without this, callers
# that always pass -w (m64's standalone path) try to link an exec wrapper
# against the translated dylib and fail with `ld: _main undefined` — even
# though the translated dylib itself was already produced. Detected on the
# thin i386 input (m64 thins fat wrappers before calling us).
if file "$ARCHIVE32" 2>/dev/null | grep -qi "shared library\|bundle"; then
    [ "$VERBOSE" ] && echo "86x64: dylib/bundle input -> dylib-only mode (no exec wrapper)"
    WRAPPER_OBJ=""
fi

v() {
    [ "$VERBOSE" ] && echo "$@"
    "$@"
}

# All pipeline intermediates land in a tempdir; cleaned on exit unless -k.
# Previous behaviour was to drop _rebase / _transform / _abi / _dollar /
# _interpose / _dyld next to the output, polluting any bundle the user
# pointed -o at. The author's intent (visible in the file's history with
# the commented-out mktemp+trap lines) was always tempdirs — completing it.
TMPDIR_PIPE="$(mktemp -d -t 86x64.XXXXXX)"
if [ "$KEEP_TMP" ]; then
    echo "86x64: keeping intermediates at $TMPDIR_PIPE" >&2
else
    trap 'rm -rf "$TMPDIR_PIPE"' EXIT
fi

# Stage names are derived from the i386 input basename so multiple
# concurrent pipeline invocations on different inputs in the same tempdir
# would not clash (defensive — current invocations get one tempdir each).
STEM="$(basename "$ARCHIVE32")"
REBASE32="$TMPDIR_PIPE/${STEM}.rebase"
TRANSFORM64="$TMPDIR_PIPE/${STEM}.transform"
ABI64="$TMPDIR_PIPE/${STEM}.abi"
DOLLAR64="$TMPDIR_PIPE/${STEM}.dollar"
INTERPOSE64="$TMPDIR_PIPE/${STEM}.interpose"
DYLD64="$TMPDIR_PIPE/${STEM}.dyld"

# rebasify 32-bit archive
v "$MACHO_TOOL" rebasify "$ARCHIVE32" "$REBASE32" || error

# transform 32-bit rebased archive to 64-bit archive
v "$MACHO_TOOL" -- transform "$REBASE32" "$TRANSFORM64" || error

# link 64-bit archive with libabiconv.dylib. The inserted load-command name
# must be resolvable at runtime relative to the binary (not the build-time
# filesystem path), so the result is deployable into an app bundle.
LIBABICONV_NAME="@loader_path/$(basename "$LIBABICONV")"
v "$MACHO_TOOL" -- modify --insert load-dylib,name="$LIBABICONV_NAME" "$TRANSFORM64" "$ABI64" || error

# strip the $UNIX2003 bind suffix (legacy 10.5-era libSystem export names
# that no longer exist on modern macOS). No-op for classic Mach-O binaries
# (handled by StripBind::workT itself).
v "$MACHO_TOOL" modify --update strip-bind,suffix='$UNIX2003' "$ABI64" "$DOLLAR64" || error

# Detect whether this binary uses modern (LC_DYLD_INFO) or classic
# (LC_DYSYMTAB-only) binding. The classic path predates Snow Leopard; iWeb's
# MobileMe.framework and the entire Source-engine (Portal 2) dylib set land
# here. BOTH paths interpose to libabiconv — they only differ in how the
# import symbol list is gathered and how the bind is rewritten:
#   modern : symbols come from the dyld_info lazy/non-lazy bind opcode stream;
#            static-interpose rewrites the bind opcode (old_sym -> __sym).
#   classic: there is no bind opcode stream — imports are resolved by dyld
#            from the symbol + indirect symbol tables, keyed by (name, library
#            ordinal). Symbols come from nm; static-interpose (via macho-tool's
#            classic_symbind fallback) renames the undefined nlist + retargets
#            its library ordinal so the la/nl_symbol_ptr slot binds to the shim.
# WITHOUT this, classic binaries call native libSystem with the i386 ABI and
# crash at the first cross-ABI call (e.g. dlopen in the Portal 2 launcher).
HAS_DYLD_INFO=$(otool -l "$DOLLAR64" 2>/dev/null | grep -c "cmd LC_DYLD_INFO" || true)

if [ "$HAS_DYLD_INFO" -gt 0 ]; then
    # gather bound symbols: lazy binds are function imports (bound via stubs),
    # non-lazy binds are DATA imports (e.g. NSString* consts like
    # NSArgumentDomain). static-interpose redirects both to libabiconv shims /
    # data-constant shadows, so feed it the union of the two tables.
    # Exclude dyld_stub_binder: it is a non-lazy bind that libabiconv DOES
    # provide a __-prefixed shim for, so leaving it in would make
    # static-interpose rename it early and the dedicated rebind step below
    # (which still looks for the original name) would fail to find it.
    SYMS=$( { "$MACHO_TOOL" print --lazy-bind "$DOLLAR64" | tail +2;
              "$MACHO_TOOL" print --bind "$DOLLAR64" | tail +2; } \
            | cut -d" " -f5 | grep -vx 'dyld_stub_binder' | sort -u )
else
    # classic Mach-O: the import set is the undefined external symbols. nm
    # prints them with the leading underscore (e.g. `_dlopen`), exactly the
    # form static-interpose expects (it prepends `__` -> `___dlopen`, the
    # libabiconv shim export). Classic binaries reference dyld's internal
    # lazy-binding helper, not the dyld_stub_binder symbol, so there is no
    # separate dyld_stub_binder rebind step.
    SYMS=$( nm -u "$DOLLAR64" 2>/dev/null | awk '{print $NF}' \
            | grep -vx 'dyld_stub_binder' | sort -u )
fi

# statically interpose imported symbols to libabiconv (both binding flavors)
v "$ROOTDIR"/static-interpose.sh -l "$LIBABICONV" -n "$LIBABICONV_NAME" -p "__" -o "$INTERPOSE64" "$DOLLAR64" $SYMS || error

if [ "$HAS_DYLD_INFO" -gt 0 ]; then
    # interpose dyld_stub_binder (modern lazy-binding entry point)
    DYLD_ORD=$("$MACHO_TOOL" translate --load-dylib "$LIBABICONV_NAME" "$INTERPOSE64")
    v "$MACHO_TOOL" modify --update bind,old_sym="dyld_stub_binder",new_sym="__dyld_stub_binder",new_dylib="$DYLD_ORD" "$INTERPOSE64" "$DYLD64" || error
else
    DYLD64="$INTERPOSE64"
fi

# convert result to dylib (final output)
v "$MACHO_TOOL" convert --archive DYLIB "$DYLD64" "$DYLIB64" || error

# Post-process the dylib so install_name_tool can edit it later:
#   1) strip LC_CODE_SIGNATURE (otherwise install_name_tool refuses with
#      "code signature data out of place" once the LC layout shifts);
#   2) drop the duplicate LC_ID_DYLIB the pipeline appends (@rpath/X.dylib
#      next to the original install_name) which install_name_tool also
#      can't deal with.
# This is the same logic remove_dup_lc_id_dylib.py applied as a manual
# post-step; folding it in so every consumer doesn't have to know.
python3 "$ROOTDIR/remove_dup_lc_id_dylib.py" "$DYLIB64" >/dev/null || error

# If no wrapper requested (dylib-only mode), we're done.
if [ ! -f "$WRAPPER_OBJ" ]; then
    [ "$VERBOSE" ] && echo "86x64: dylib-only mode, output at $DYLIB64"
    exit 0
fi

# link wrapper exec. Modern ld (Xcode 15+) requires -platform_version and
# -syslibroot; derive both from xcrun.
SDK_VER=$(xcrun --show-sdk-version)
SDK_PATH=$(xcrun --show-sdk-path)
v ld -arch x86_64 \
    -syslibroot "$SDK_PATH" \
    -platform_version macos 11.0 "$SDK_VER" \
    -rpath "$(dirname $LIBINTERPOSE)" -rpath "$(dirname $ARCHIVE64)" \
    -rpath @loader_path \
    -rpath @loader_path/../Frameworks \
    -pagezero_size 0x1000 -lSystem -e _main_wrapper \
    -o "$ARCHIVE64" "$WRAPPER_OBJ" "$LIBINTERPOSE" "$DYLIB64" || error
