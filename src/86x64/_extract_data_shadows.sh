#!/bin/bash
#
# Emit the list of external DATA-constant symbols a translated i386->x86_64
# binary reads via a TRUNCATING 32-bit load and that libabiconv does NOT yet
# provide a low-4GB shadow for. Feed the output to abigen --data-shadow-file
# so those symbols get header-less object shadows (see abigen.cc
# forced_object_shadows). Generic: works for any app binary.
#
# The bug it targets: a non-lazy data symbol pointer slot holds the real
# 64-bit address of an external constant (e.g. RedRock's NSString* const
# RKAllProjectsAlbumName, which lives >4GB in a NATIVE x86_64 framework). The
# i386 code reads it `movl slot,%reg` (truncating the address) then derefs ->
# fault. Header-typed NS*/CF* constants already get shadows via abigen's
# consider set; private bundled frameworks have no headers, so they slip
# through. This finds exactly those gaps from the binary itself.
#
# usage: _extract_data_shadows.sh <binary.dylib> <libabiconv.dylib>
set -euo pipefail

BIN="${1:?usage: $0 <binary> <libabiconv>}"
LIBABICONV="${2:?usage: $0 <binary> <libabiconv>}"

# 1. Symbols libabiconv already shadows: it exports `__<sym>` for each. Strip
#    ONE leading underscore to recover the bind name `_<sym>`.
SHADOWED=$(nm -gU "$LIBABICONV" 2>/dev/null | awk '{print $NF}' \
           | grep '^__' | sed 's/^_//' | sort -u)

# A shadow is only needed for a symbol whose defining framework loads HIGH
# (>4GB) — i.e. a NATIVE x86_64 framework. A framework WE translated loads in
# the low 4GB, so its symbol address never truncates; shadowing it would be
# redundant and risks dlsym picking the wrong copy. A translated binary is
# exactly one that links libabiconv. Collect the bundle's translated framework
# binaries by that signal so their symbols can be excluded below.
APP_ROOT="${BIN%.app/*}.app"
TRFILE=$(mktemp)
trap "rm -f $TRFILE" EXIT
if [ -d "$APP_ROOT/Contents/Frameworks" ]; then
   find "$APP_ROOT/Contents/Frameworks" -type f 2>/dev/null | while read -r f; do
      if otool -L "$f" 2>/dev/null | grep -q libabiconv; then basename "$f"; fi
   done | sort -u > "$TRFILE"
fi

# 2. External DATA imports (non-lazy binds = data constants, not functions),
#    keeping only those bound from a NATIVE OBJECTIVE-C framework. Object/CF
#    string constants come from ObjC frameworks; the C/C++ runtime exports DATA
#    globals that must NOT be handle-wrapped (FILE* __stderrp,
#    __DefaultRuneLocale, C++ vtables/typeinfo __ZTV*). Deny those source dylibs
#    (and our own already-shadowing libabiconv + generated *ShimAuto stubs),
#    deny TRANSLATED (low) bundled frameworks, and drop any C++-mangled (__Z*)
#    name as a backstop.
DENY='libabiconv|libstdc\+\+|libc\+\+|libSystem|libsystem|libobjc|libgcc|ShimAuto'
NONLAZY=$(otool -bind_info "$BIN" 2>/dev/null \
          | awk '$2=="__nl_symbol_ptr"{print $(NF-1)" "$NF}' \
          | grep -vE "^[^ ]*($DENY)[^ ]* " \
          | awk -v trf="$TRFILE" 'BEGIN{while((getline l < trf)>0) if(l!="") tr[l]=1}
                                  !($1 in tr){print $2}' \
          | grep -vE '^__Z' | sort -u)

# 3. Symbols read by the exact crashing object-load signature: a 32-bit
#    `movl slot(%rip), %eREG` (truncates the slot address) IMMEDIATELY followed
#    by `movl (%rREG), %r..` (derefs it to read the object pointer). This is how
#    the compiler emits `[obj msg: NSStringConst]` (load &var -> load value).
#    Requiring the double-deref keeps us to genuine object/CF VALUE loads and
#    excludes single-load address-passing (which may not be handle-safe). movq
#    (64-bit, non-truncating) is correctly excluded.
LOADED=$(otool -tV "$BIN" 2>/dev/null | awk '
   match($0, /movl[[:space:]]+0x[0-9a-f]+\(%rip\), %e[a-z][a-z]/) {
      reg = substr($0, RSTART+RLENGTH-3, 3)              # e.g. "edx"
      sub(/^e/, "r", reg)                                # -> "rdx"
      sym = ""
      if ($0 ~ /literal pool symbol address: _/) {
         sym = $0
         sub(/.*literal pool symbol address: /, "", sym)
         sub(/[^A-Za-z0-9_].*$/, "", sym)
      }
      pend_reg = reg; pend_sym = sym; next
   }
   pend_sym != "" && index($0, "movl\t(%" pend_reg "),") { print pend_sym }
   { pend_reg = ""; pend_sym = "" }
' | sort -u)

# result = (data imports that are truncate-loaded) minus already-shadowed.
comm -23 <(comm -12 <(printf '%s\n' "$NONLAZY") <(printf '%s\n' "$LOADED")) \
         <(printf '%s\n' "$SHADOWED")
