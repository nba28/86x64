#!/bin/bash
#
# print_exports_trie.sh — print exported symbol names from one or more
# system dynamic libraries, one symbol per line.
#
# On macOS 11+ the system dylibs no longer exist as standalone files on
# disk: they are shipped only inside the dyld shared cache. So when the
# requested .dylib path is missing we fall back to the matching .tbd
# stub file in the active SDK and extract symbols from it (YAML-ish
# format, see tapi-tbd v4).

set -e

usage() {
    cat<<EOF
usage: $0 path...
EOF
}

while getopts "h" OPTCHAR; do
    case $OPTCHAR in
        h)
            usage
            exit 0
            ;;
        "?")
            usage 2>&1
            exit 1
            ;;
    esac
done

shift $((OPTIND-1))

SDKROOT="${SDKROOT:-$(xcrun --show-sdk-path)}"

# Map a /usr/lib/.../foo.dylib path to the matching .tbd inside the SDK.
tbd_for() {
    local p="$1"
    case "$p" in
        /*) echo "${SDKROOT}${p%.dylib}.tbd" ;;
        *)  echo "${p%.dylib}.tbd" ;;
    esac
}

# Extract underscore-prefixed exported symbols from a .tbd file.
# Strategy: pull out the bracketed symbol lists, split on commas/whitespace,
# strip surrounding quotes, and keep tokens beginning with '_'.
extract_tbd_symbols() {
    local tbd="$1"
    python3 - "$tbd" <<'PY'
import sys, re
try:
    import yaml  # type: ignore
except ImportError:
    yaml = None

path = sys.argv[1]
with open(path, 'r') as f:
    text = f.read()

symbols = set()

if yaml is not None:
    try:
        for doc in yaml.safe_load_all(text):
            if not isinstance(doc, dict):
                continue
            for section in ('exports', 'reexports'):
                for entry in doc.get(section) or []:
                    if not isinstance(entry, dict):
                        continue
                    for s in entry.get('symbols') or []:
                        if isinstance(s, str) and s.startswith('_'):
                            symbols.add(s)
    except Exception:
        symbols.clear()

if not symbols:
    # Fallback: regex over the file. Capture both quoted ('_foo') and
    # bare (_foo) underscore-prefixed tokens.
    for m in re.finditer(r"'(_[^']+)'|(?<![A-Za-z0-9_])(_[A-Za-z0-9_$.]+)", text):
        sym = m.group(1) or m.group(2)
        if sym:
            symbols.add(sym)

for s in sorted(symbols):
    print(s)
PY
}

for LIBPATH in "$@"; do
    if [ -r "$LIBPATH" ]; then
        # Old behavior: real on-disk Mach-O dylib. Apple objdump uses
        # `--macho --exports-trie`.
        objdump --macho --exports-trie "$LIBPATH" \
            | tr -s " " \
            | cut -f 2 -d " " \
            | grep "^_"
    else
        TBD="$(tbd_for "$LIBPATH")"
        if [ -r "$TBD" ]; then
            extract_tbd_symbols "$TBD"
        else
            echo "print_exports_trie.sh: cannot find dylib or tbd for: $LIBPATH" 1>&2
            echo "  tried:    $LIBPATH" 1>&2
            echo "  also:     $TBD" 1>&2
            exit 1
        fi
    fi
done
